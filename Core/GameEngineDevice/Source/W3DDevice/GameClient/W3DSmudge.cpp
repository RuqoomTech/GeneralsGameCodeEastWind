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

#include "Lib/BaseType.h"
#include "WWLib/always.h"
#include "W3DDevice/GameClient/W3DSmudge.h"
#include "Common/GameMemory.h"
#include "GameClient/View.h"
#include "GameClient/Display.h"
#include "WW3D2/texture.h"
#include "WW3D2/rinfo.h"
#include "WW3D2/camera.h"
#include "WW3D2/shader.h"
#include "WW3D2/sortingrenderer.h"
#include "WW3D2/ww3d.h"
#include "WW3D2/IRenderBackend.h"

#include <vector>


SmudgeManager *TheSmudgeManager=nullptr;

W3DSmudgeManager::W3DSmudgeManager() :
	m_backgroundTexture(nullptr),
	m_backBufferWidth(0),
	m_backBufferHeight(0)
{
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
}


#define SMUDGE_DRAW_SIZE	500	//draw at most 500 smudges per call. Tweak value to improve CPU/GPU parallelism.

static_assert(SMUDGE_DRAW_SIZE * 5 < 0x10000, "Vertex index exceeds 16-bit limit");

namespace
{

// Static vertex indices shared by every smudge batch: 4 triangles per smudge, each with 3 indices.
const unsigned short *Smudge_Indices()
{
	static unsigned short indices[SMUDGE_DRAW_SIZE*4*3];
	static bool built = false;
	if (!built)
	{
		unsigned short *ib = indices;
		//quad of 4 triangles:
		//	0-----3
		//  |\   /|
		//  |  4  |
		//	|/   \|
		//  1-----2
		unsigned short vbCount=0;
		for (Int i=0; i<SMUDGE_DRAW_SIZE; i++)
		{
			//Top
			ib[0]=vbCount;
			ib[1]=vbCount+4;
			ib[2]=vbCount+3;
			//Right
			ib[3]=vbCount+3;
			ib[4]=vbCount+4;
			ib[5]=vbCount+2;
			//Bottom
			ib[6]=vbCount+2;
			ib[7]=vbCount+4;
			ib[8]=vbCount+1;
			//Left
			ib[9]=vbCount+1;
			ib[10]=vbCount+4;
			ib[11]=vbCount+0;

			vbCount += 5;
			ib+=12;
		}
		built = true;
	}
	return indices;
}

} // namespace

void W3DSmudgeManager::ReAcquireResources()
{
	ReleaseResources();

	// Smudges refract a snapshot of the scene drawn so far, copied into an output-sized texture.
	IRenderBackend *backend = WW3D::Get_Render_Backend();
	Int width = 0, height = 0, bits = 0;
	bool windowed = false;
	if (!backend || !backend->Get_Output_Description(width, height, bits, windowed) || width <= 0 || height <= 0)
		return;

	m_backgroundTexture = WW3D::Create_Render_Texture(width, height, RenderBackendRenderDepth::Output);
	m_backBufferWidth = width;
	m_backBufferHeight = height;
}

Bool W3DSmudgeManager::testHardwareSupport()
{
	if (m_hardwareSupportStatus == SMUDGE_SUPPORT_UNKNOWN)
	{
		// The renderer copies its active target into m_backgroundTexture inside the scene
		// (Copy_Active_Target), so the effect needs only that texture to exist.
		m_hardwareSupportStatus = m_backgroundTexture ? SMUDGE_SUPPORT_YES : SMUDGE_SUPPORT_NO;
	}

	return (SMUDGE_SUPPORT_YES == m_hardwareSupportStatus);
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
	int targetWidth = 0, targetHeight = 0;
	if (!backend || !backend->Get_Render_Target_Size(targetWidth, targetHeight) || targetWidth <= 0 || targetHeight <= 0)
		return;

	// The snapshot must match the active target; the output can be resized between frames.
	if (m_backgroundTexture && (targetWidth != m_backBufferWidth || targetHeight != m_backBufferHeight ||
		!m_backgroundTexture->Get_Renderer_Texture().Is_Valid()))
		REF_PTR_RELEASE(m_backgroundTexture);
	if (!m_backgroundTexture)
	{
		m_backgroundTexture = WW3D::Create_Render_Texture(targetWidth, targetHeight, RenderBackendRenderDepth::Output);
		if (!m_backgroundTexture)
			return;
		m_backBufferWidth = targetWidth;
		m_backBufferHeight = targetHeight;
	}

	CameraClass &camera=rinfo.Camera;
	Vector3 vsVert;
	Vector4 ssVert;
	Real uvSpanX,uvSpanY;
	Vector3 vertex_offsets[4] = {
		Vector3(-0.5f, 0.5f, 0.0f),
		Vector3(-0.5f, -0.5f, 0.0f),
		Vector3(0.5f, -0.5f, 0.0f),
		Vector3(0.5f, 0.5f, 0.0f)
	};

	Matrix4x4 proj;
	Matrix3D view;

	camera.Get_View_Matrix(&view);
	camera.Get_Projection_Matrix(&proj);

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
		return;	//nothing to render.

	//Copy the scene drawn so far into the texture the smudges refract.
	const RenderBackendTextureHandle background = m_backgroundTexture->Get_Renderer_Texture();
	if (!backend->Copy_Active_Target(background))
		return;

	RenderBackendMaterialState material;
	if (!ShaderClass::_PresetAlphaShader.Get_Render_Backend_State(material))
		return;
	material.texture_combine = RenderBackendTextureCombine::Modulate;
	//Texture alpha is undefined, so alpha comes from the vertex diffuse alone.
	material.vertex_alpha = true;
	//A camera-facing quad is never seen from behind.
	material.cull = RenderBackendCullMode::None;
	//Need these states in case texture is non-power-of-2
	material.clamp_texture = true;
	material.sampler.address_u = material.sampler.address_v = RenderBackendTextureAddress::Clamp;
	material.sampler.min_filter = material.sampler.mag_filter = RenderBackendTextureFilter::Linear;
	material.sampler.mipmaps = false;

	// Smudge vertices are already in view space, so draw them through the projection alone.
	Matrix4x4 savedViewProjection;
	backend->Get_View_Projection(savedViewProjection);
	Matrix4x4 projection;
	camera.Get_Zero_To_One_Projection_Matrix(&projection);
	backend->Set_View_Projection(projection);

	//Corners are transparent; the center vertex carries the smudge opacity.
	const Real colorR = 0xff/255.0f, colorG = 0xee/255.0f, colorB = 0xdd/255.0f;
	const unsigned short *indices = Smudge_Indices();

	static std::vector<RenderBackendTexturedVertex> vertices;
	vertices.clear();
	vertices.reserve(SMUDGE_DRAW_SIZE*5);
	Int smudgesInRenderBatch=0;

	for (setIt=m_usedSmudgeSetList.begin(); setIt != m_usedSmudgeSetList.end(); ++setIt)
	{
		SmudgeDeque& smudgeList = (*setIt)->getUsedSmudgeList();

		for (SmudgeDeque::iterator smudgeIt = smudgeList.begin(); smudgeIt != smudgeList.end(); ++smudgeIt)
		{
			Smudge* smudge = *smudgeIt;
			if (!smudge->m_draw)
				continue;

			const Real centerAlpha = (Real)(((Int)(smudge->m_opacity * 255.0f)) & 0xff) / 255.0f;
			const Smudge::smudgeVertex *smVerts = smudge->m_verts;
			for (Int i=0; i<5; i++, smVerts++)
			{
				RenderBackendTexturedVertex v = {};
				v.x = smVerts->pos.X;
				v.y = smVerts->pos.Y;
				v.z = smVerts->pos.Z;
				v.r = colorR;
				v.g = colorG;
				v.b = colorB;
				v.a = (i == 4) ? centerAlpha : 0.0f;
				v.u = smVerts->uv.X;
				v.v = smVerts->uv.Y;
				vertices.push_back(v);
			}

			//Flush once the batch reaches the maximum number of smudges allowed per draw call.
			if (++smudgesInRenderBatch == SMUDGE_DRAW_SIZE)
			{
				backend->Draw_Indexed_Material_Triangles(vertices.data(), (unsigned int)vertices.size(),
					indices, smudgesInRenderBatch*12, background, material);
				vertices.clear();
				smudgesInRenderBatch = 0;
			}
		}
	}

	if (smudgesInRenderBatch)
		backend->Draw_Indexed_Material_Triangles(vertices.data(), (unsigned int)vertices.size(),
			indices, smudgesInRenderBatch*12, background, material);

	backend->Set_View_Projection(savedViewProjection);
}
