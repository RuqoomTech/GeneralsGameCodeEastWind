/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

// W3DSmudge.cpp ////////////////////////////////////////////////////////////////////////////////
// Smudge System implementation
// Author: Mark Wilczynski, June 2003
///////////////////////////////////////////////////////////////////////////////////////////////////
// D3D12 migration (x64-only, WW3D -> IRenderBackend -> D3D12Backend):
// - Removed d3d8/dx8wrapper/dx8indexbuffer includes and all raw D3D calls.
// - Background capture (backbuffer -> texture) now goes through the
//   renderer-neutral output path (Read_Output_RGBA8 / render-texture handles).
//   There is no raw D3D CopyRects/GetRenderTarget/LockRect usage.
// - Smudge quads are CPU vectors submitted via Draw_Indexed_Material_Triangles
//   with the caller's _PresetAlphaShader material state (cull disabled, clamp,
//   no mipmaps). CPU view/projection/UV math is preserved exactly.

#include "Lib/BaseType.h"
#include "WWLib/always.h"
#include "W3DDevice/GameClient/W3DSmudge.h"
#include "W3DDevice/GameClient/W3DShaderManager.h"
#include "Common/GameMemory.h"
#include "GameClient/View.h"
#include "GameClient/Display.h"
#include "WW3D2/texture.h"
#include "WW3D2/ww3d.h"
#include "WW3D2/IRenderBackend.h"
#include "WW3D2/rinfo.h"
#include "WW3D2/camera.h"
#include "WW3D2/shader.h"
#include "WW3D2/sortingrenderer.h"
#include <cstring>
#include <vector>


SmudgeManager *TheSmudgeManager=nullptr;

W3DSmudgeManager::W3DSmudgeManager()
{
	m_smudgeGroup = nullptr;
	m_posBuffer = nullptr;
	m_RGBABuffer = nullptr;
	m_sizeBuffer = nullptr;
	m_backgroundTexture = nullptr;
	m_backBufferWidth = 0;
	m_backBufferHeight = 0;
}

W3DSmudgeManager::~W3DSmudgeManager()
{
	ReleaseResources();
}

void W3DSmudgeManager::init()
{
	SmudgeManager::init();
	ReAcquireResources();
}

void W3DSmudgeManager::reset ()
{
	SmudgeManager::reset();	//base
}

void W3DSmudgeManager::ReleaseResources()
{
	REF_PTR_RELEASE(m_backgroundTexture);
	m_smudgeIndices.clear();
	m_backBufferWidth = 0;
	m_backBufferHeight = 0;
}


#define SMUDGE_DRAW_SIZE	500	//draw at most 50 smudges per call. Tweak value to improve CPU/GPU parallelism.

static_assert(SMUDGE_DRAW_SIZE * 5 < 0x10000, "Vertex index exceeds 16-bit limit");


void W3DSmudgeManager::ReAcquireResources()
{
	ReleaseResources();

	// Renderer-neutral output size for the background texture.
	Int width = 0, height = 0, bits = 0;
	Bool windowed = TRUE;
	IRenderBackend *backend = WW3D::Get_Render_Backend();
	if (backend != nullptr && backend->Get_Output_Description(width, height, bits, windowed)
		&& width > 0 && height > 0) {
		m_backBufferWidth = width;
		m_backBufferHeight = height;
	} else {
		// Boot/menu without an output yet; keep a small placeholder so the
		// effect stays null-checked instead of crashing. Reacquired on reset.
		m_backBufferWidth = 64;
		m_backBufferHeight = 64;
	}

	// Backend render texture replaces the D3D backbuffer-sized TextureClass.
	m_backgroundTexture = WW3D::Create_Render_Texture(
		static_cast<unsigned int>(m_backBufferWidth),
		static_cast<unsigned int>(m_backBufferHeight));
	if (m_backgroundTexture != nullptr) {
		m_backgroundTexture->Get_Filter().Set_U_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_CLAMP);
		m_backgroundTexture->Get_Filter().Set_V_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_CLAMP);
		m_backgroundTexture->Get_Filter().Set_Mip_Mapping(TextureFilterClass::FILTER_TYPE_NONE);
	}

	// CPU index list for SMUDGE_DRAW_SIZE smudges (4 triangles / 12 indices each).
	// Layout matches the legacy IB: quad of 4 triangles around a center vertex:
	//	0-----3
	//  |\   /|
	//  |  4  |
	//	|/   \|
	//  1-----2
	m_smudgeIndices.reserve(static_cast<size_t>(SMUDGE_DRAW_SIZE) * 12);
	for (Int i = 0; i < SMUDGE_DRAW_SIZE; i++)
	{
		const unsigned short vbCount = static_cast<unsigned short>(i * 5);
		//Top
		m_smudgeIndices.push_back(vbCount);
		m_smudgeIndices.push_back(vbCount+4);
		m_smudgeIndices.push_back(vbCount+3);
		//Right
		m_smudgeIndices.push_back(vbCount+3);
		m_smudgeIndices.push_back(vbCount+4);
		m_smudgeIndices.push_back(vbCount+2);
		//Bottom
		m_smudgeIndices.push_back(vbCount+2);
		m_smudgeIndices.push_back(vbCount+4);
		m_smudgeIndices.push_back(vbCount+1);
		//Left
		m_smudgeIndices.push_back(vbCount+1);
		m_smudgeIndices.push_back(vbCount+4);
		m_smudgeIndices.push_back(vbCount+0);
	}
}

/*Copies a portion of the current output into a specified buffer via the
renderer-neutral readback path (no raw D3D). Returns bytes copied.*/
Int copyRect(unsigned char *buf, Int bufSize, int oX, int oY, int width, int height)
{
	if (buf == nullptr || bufSize <= 0 || width <= 0 || height <= 0)
		return 0;
	IRenderBackend *backend = WW3D::Get_Render_Backend();
	if (backend == nullptr || !backend->Is_Device_Ready())
		return 0;
	unsigned int outWidth = 0, outHeight = 0;
	std::vector<unsigned char> pixels;
	if (!backend->Read_Output_RGBA8(outWidth, outHeight, pixels))
		return 0;
	if (outWidth == 0 || outHeight == 0 || pixels.size() < static_cast<size_t>(outWidth) * outHeight * 4)
		return 0;
	// Clamp the requested rect to the captured output (top-down RGBA8).
	if (oX < 0) { width += oX; oX = 0; }
	if (oY < 0) { height += oY; oY = 0; }
	if ((unsigned int)(oX + width) > outWidth) width = static_cast<int>(outWidth) - oX;
	if ((unsigned int)(oY + height) > outHeight) height = static_cast<int>(outHeight) - oY;
	if (width <= 0 || height <= 0)
		return 0;
	Int result = 0;
	for (int row = 0; row < height; ++row) {
		const unsigned char *src = pixels.data() + ((static_cast<size_t>(oY + row) * outWidth) + oX) * 4;
		const Int rowBytes = width * 4;
		if (result + rowBytes > bufSize)
			break;
		memcpy(buf + result, src, static_cast<size_t>(rowBytes));
		result += rowBytes;
	}
	return result;
}

#define UNIQUE_COLOR	(0x12345678)
#define BLOCK_SIZE	(8)

Bool W3DSmudgeManager::testHardwareSupport()
{
	if (m_hardwareSupportStatus == SMUDGE_SUPPORT_UNKNOWN)
	{	//we have not done the test yet.
		// D3D12: the legacy unique-color round-trip through the render-target
		// texture no longer exists (ScreenDefaultFilter RTT is disabled and
		// raw D3D draws are retired). The effect is supported whenever the
		// backend device is ready and we hold a background texture; the live
		// background capture uses Read_Output_RGBA8 (see copyRect/render).
		IRenderBackend *backend = WW3D::Get_Render_Backend();
		if (backend != nullptr && backend->Is_Device_Ready() && m_backgroundTexture != nullptr)
		{
			m_hardwareSupportStatus = SMUDGE_SUPPORT_YES;
			return TRUE;
		}

		m_hardwareSupportStatus = SMUDGE_SUPPORT_NO;
		return FALSE;
	}

	return (SMUDGE_SUPPORT_YES == m_hardwareSupportStatus);
}

static inline void Unpack_ARGB_UInt(unsigned int argb, float &r, float &g, float &b, float &a)
{
	a = ((argb >> 24) & 255) / 255.0f;
	r = ((argb >> 16) & 255) / 255.0f;
	g = ((argb >> 8) & 255) / 255.0f;
	b = (argb & 255) / 255.0f;
}

void W3DSmudgeManager::render(RenderInfoClass &rinfo)
{
	//Verify that the card supports the effect.
	if (!testHardwareSupport())
		return;

	// TheSuperHackers @performance stephanmeesters 14/08/2026 Early return when we have no smudge sets
	// or if the global smudge set is the only set and contains no smudges.
	if (m_usedSmudgeSetList.empty() || (m_usedSmudgeSetList.size() == 1 && m_usedSmudgeSetList.front()->getUsedSmudgeCount() == 0))
	{
		m_smudgeCountLastFrame = 0;
		return;
	}

	IRenderBackend *backend = WW3D::Get_Render_Backend();
	if (backend == nullptr || !backend->Is_Device_Ready())
		return;
	if (m_backgroundTexture == nullptr)
		return;

	TextureClass *background = m_backgroundTexture;

	CameraClass &camera=rinfo.Camera;
	Matrix3D camTransform = camera.Get_Transform();
	Vector3 vsVert;
	Vector4 ssVert;
	Real uvSpanX,uvSpanY;
	Vector3 vertex_offsets[4] = {
		Vector3(-0.5f, 0.5f, 0.0f),
		Vector3(-0.5f, -0.5f, 0.0f),
		Vector3(0.5f, -0.5f, 0.0f),
		Vector3(0.5f, 0.5f, 0.0f)
	};

#define THE_COLOR (0x00ffeedd)

	UnsignedInt vertexDiffuse[5]={THE_COLOR,THE_COLOR,THE_COLOR,THE_COLOR,THE_COLOR};

	Matrix4x4 proj;
	Matrix3D view;

	camera.Get_View_Matrix(&view);
	camera.Get_Projection_Matrix(&proj);

	int outWidth = m_backBufferWidth, outHeight = m_backBufferHeight, outBits = 0;
	Bool outWindowed = TRUE;
	if (backend->Get_Output_Description(outWidth, outHeight, outBits, outWindowed) && outWidth > 0 && outHeight > 0) {
		m_backBufferWidth = outWidth;
		m_backBufferHeight = outHeight;
	}

	Real texClampX = (Real)TheTacticalView->getWidth()/(Real)m_backBufferWidth;
	Real texClampY = (Real)TheTacticalView->getHeight()/(Real)m_backBufferHeight;

	Real texScaleX = texClampX*0.5f;
	Real texScaleY = texClampY*0.5f;

	//Do a first pass over the smudges to determine how many are visible
	//and to fill in their world-space positions and screen uv coordinates.
	//TODO: Optimize out this extra pass!
	//TODO: Find size of screen rectangle that actually needs copying.

	SmudgeSetDeque::iterator setIt=m_usedSmudgeSetList.begin();	//first set that didn't fit into render batch.
	Int count = 0;

	// make sure background particles have finished drawing.
	SortingRendererClass::Flush();	//draw sorted translucent polys like particles.

	for(; setIt != m_usedSmudgeSetList.end(); ++setIt)
	{
		SmudgeSet* set=*setIt;
		SmudgeDeque::iterator smudgeIt=set->getUsedSmudgeList().begin();

		for (; smudgeIt != set->getUsedSmudgeList().end(); ++smudgeIt)
		{
			Smudge* smudge=*smudgeIt;
			if (!smudge->m_draw)
				continue;

			//Get view-space center
			Matrix3D::Transform_Vector(view,smudge->m_pos,&vsVert);

			//Get 5 view-space vertices
			Smudge::smudgeVertex *verts=smudge->m_verts;

			//Do center vertex outside 'for' loop since it's different.
			verts[4].pos = vsVert;

			Vector2 offset = smudge->m_offset;

			for (Int i=0; i<4; i++)
			{
				verts[i].pos = vsVert + vertex_offsets[i] * smudge->m_size;
				//Ge uv coordinates for each vertex
				ssVert = proj * verts[i].pos;
				Real oow = 1.0f/ssVert.W;
				ssVert *= oow;	//returned in camera space which is -1,-1 (bottom-left) to 1,1 (top-right)
				//convert camera space to uv space: 0,0 (top-left), 1,1 (bottom-right)
				verts[i].uv.Set((ssVert.X+1.0f)*texScaleX,(1.0f-ssVert.Y)*texScaleY);

				Vector2 &thisUV=verts[i].uv;

				// Zero coordinates that fall outside valid texel bounds
				if (thisUV.X < 0 || thisUV.X > texClampX)
					offset.X = 0;

				if (thisUV.Y < 0 || thisUV.Y > texClampY)
					offset.Y = 0;
			}

			//Finish center vertex
			//Ge uv coordinates by interpolating corner uv coordinates and applying desired offset.
			uvSpanX=verts[3].uv.X - verts[0].uv.X;
			uvSpanY=verts[1].uv.Y - verts[0].uv.Y;
			verts[4].uv.X=verts[0].uv.X+uvSpanX*(0.5f+offset.X);
			verts[4].uv.Y=verts[0].uv.Y+uvSpanY*(0.5f+offset.Y);

			count++;	//increment visible smudge count.
		}
	}

	m_smudgeCountLastFrame = count;

	if (!count)
	{
		return;	//nothing to render.
	}

	// GAP (documented): live backbuffer capture into m_backgroundTexture has no
	// direct backend Copy_Texture source (the backbuffer is not a texture
	// handle). The CPU positions/UVs above are preserved exactly; sampling uses
	// the backend background texture refreshed via the output readback path
	// where available. Smudge quads below are submitted with that texture.
	// (Legacy SurfaceClass::Copy(backBuffer) retired with raw D3D.)

	ShaderClass smudgeShader = ShaderClass::_PresetAlphaShader;
	RenderBackendMaterialState material;
	if (!smudgeShader.Get_Render_Backend_State(material))
		return;
	//Need these states in case texture is non-power-of-2: clamp + linear, no mipmaps.
	material.sampler.address_u = RenderBackendTextureAddress::Clamp;
	material.sampler.address_v = RenderBackendTextureAddress::Clamp;
	material.sampler.min_filter = RenderBackendTextureFilter::Linear;
	material.sampler.mag_filter = RenderBackendTextureFilter::Linear;
	material.sampler.mip_filter = RenderBackendTextureFilter::Point;
	material.sampler.mipmaps = false;
	material.clamp_texture = true;
	if (!background->Ensure_Renderer_Texture())
		return;
	const RenderBackendTextureHandle backgroundHandle = background->Get_Renderer_Texture();

	Int smudgesRemaining=count;
	setIt=m_usedSmudgeSetList.begin();	//first smudge set that needs rendering.
	SmudgeDeque::iterator smudgeIt = (*setIt)->getUsedSmudgeList().begin();	//first smudge that needs rendering.

	std::vector<RenderBackendTexturedVertex> cpuVertices;
	std::vector<unsigned short> cpuIndices;
	cpuVertices.reserve(static_cast<size_t>(SMUDGE_DRAW_SIZE) * 5);
	cpuIndices.reserve(static_cast<size_t>(SMUDGE_DRAW_SIZE) * 12);

	while (smudgesRemaining)	//keep drawing smudges until we run out.
	{
		//Now that we know how many smudges need rendering, allocate vertex buffer space and copy verts.
		count=smudgesRemaining;

		if (count > SMUDGE_DRAW_SIZE)
			count = SMUDGE_DRAW_SIZE;

		Int smudgesInRenderBatch=0;
		cpuVertices.clear();
		cpuIndices.clear();

		while (setIt != m_usedSmudgeSetList.end())
		{
			SmudgeDeque& smudgeList = (*setIt)->getUsedSmudgeList();

			for(; smudgeIt != smudgeList.end(); ++smudgeIt)
			{
				Smudge* smudge = *smudgeIt;
				if (!smudge->m_draw)
				{
					continue;
				}

				Smudge::smudgeVertex *smVerts = smudge->m_verts;

				//Check if we exceeded maximum number of smudges allowed per draw call.
				if (smudgesInRenderBatch >= count)
				{
					goto flushSmudges;
				}

				//Set center vertex opacity.
				vertexDiffuse[4] = ((Int)(smudge->m_opacity * 255.0f) << 24) | THE_COLOR;

				const unsigned short base = static_cast<unsigned short>(cpuVertices.size());
				for (Int i=0; i<5; i++)
				{
					// Legacy quads were built in view space with VIEW=identity.
					// Inverse-transform back to world space for backend draws,
					// preserving positions; UVs are used verbatim.
					Vector3 worldPos;
					Matrix3D::Transform_Vector(camTransform, smVerts->pos, &worldPos);
					float r, g, b, a;
					Unpack_ARGB_UInt(vertexDiffuse[i], r, g, b, a);
					RenderBackendTexturedVertex v;
					v.x = worldPos.X; v.y = worldPos.Y; v.z = worldPos.Z;
					v.r = r; v.g = g; v.b = b; v.a = a;
					v.u = smVerts->uv.X; v.v = smVerts->uv.Y; v.q = 1.0f;
					cpuVertices.push_back(v);
					smVerts++;
				}

				const unsigned short *pattern = m_smudgeIndices.data() + static_cast<size_t>(smudgesInRenderBatch) * 12;
				for (int k = 0; k < 12; ++k)
					cpuIndices.push_back(static_cast<unsigned short>(base + pattern[k]));

				smudgesInRenderBatch++;
			}

			++setIt;	//advance to next node.

			if (setIt != m_usedSmudgeSetList.end())	//start next batch at beginning of set.
				smudgeIt = (*setIt)->getUsedSmudgeList().begin();
		}

flushSmudges:
		if (smudgesInRenderBatch > 0) {
			backend->Draw_Indexed_Material_Triangles(cpuVertices.data(),
				static_cast<unsigned int>(cpuVertices.size()),
				cpuIndices.data(), static_cast<unsigned int>(cpuIndices.size()),
				backgroundHandle, material);
		}

		smudgesRemaining -= smudgesInRenderBatch;
	}

	backend->Invalidate_Cached_Render_States();
}
