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

// FILE: W3DWater.cpp /////////////////////////////////////////////////////////////////////////////
// Created:   Mark Wilczynski, June 2001
// Desc:      Draw reflective water surface.  Also handles drawing of waves/ripples
//			  on the surface.
///////////////////////////////////////////////////////////////////////////////////////////////////

#define SCROLL_UV

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////

#include "W3DDevice/GameClient/W3DWater.h"
#include "W3DDevice/GameClient/HeightMap.h"
#include "W3DDevice/GameClient/W3DShroud.h"
#include "W3DDevice/GameClient/W3DWaterTracks.h"
#include "W3DDevice/GameClient/W3DAssetManager.h"
#include "WW3D2/texture.h"
#include "WW3D2/assetmgr.h"
#include "WW3D2/rinfo.h"
#include "WW3D2/camera.h"
#include "WW3D2/scene.h"
#include "WW3D2/ww3d.h"
#include "WW3D2/IRenderBackend.h"
#include "WW3D2/light.h"
#include "WW3D2/shader.h"
#include "WWLib/simplevec.h"
#include "WW3D2/mesh.h"
#include "WW3D2/matinfo.h"
#include <vector>

#include "Common/FramePacer.h"
#include "Common/GameState.h"
#include "Common/GlobalData.h"
#include "Common/PerfTimer.h"
#include "Common/Xfer.h"
#include "Common/GameLOD.h"

#include "GameClient/Color.h"
#include "GameClient/Water.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/PolygonTrigger.h"
#include "GameLogic/ScriptEngine.h"
#include "W3DDevice/GameClient/W3DShaderManager.h"
#include "W3DDevice/GameClient/W3DDisplay.h"
#include "W3DDevice/GameClient/W3DPoly.h"
#include "W3DDevice/GameClient/W3DScene.h"
#include "W3DDevice/GameClient/W3DCustomScene.h"



#define MIPMAP_BUMP_TEXTURE

// DEFINES ////////////////////////////////////////////////////////////////////////////////////////
#define SKYPLANE_SIZE	(384.0f*MAP_XY_FACTOR)
#define SKYPLANE_HEIGHT	(30.0f)

#define SKYBODY_TEXTURE	"TSMoonLarg.tga"
#define SKYBODY_SIZE	45.0f		//extent or radius of sky body

#define SKYBODY_X	150.0f	//location of skybody
#define SKYBODY_Y	550.0f	//location of skybody

/* in the bay
#define SKYBODY_X	120.0f			//location of skybody
#define SKYBODY_Y	75.0f			//location of skybody
*/

#define SKYBODY_HEIGHT	SKYPLANE_HEIGHT	//altitude of sky body (z-buffer disabled, so can equal sky height).

//GeForce3 water system defines
#define PATCH_SIZE 15		//number of vertices on patch edge.  Large patches may waste vertices off edge of screen.
#define PATCH_UV_TILES	42	//number of times the bump map texture is tiled across patch (must be integer!).
#define PATCH_SCALE (4.0f * MAP_XY_FACTOR)	//horizontal scale factor. Adjust this and size to get desired vertex density.
#define SEA_REFLECTION_SIZE 256		//dimensions of reflection texture

#define SEA_BUMP_SCALE		(0.06f)		//scales the du/dv offsets stored in bump map (~ amount to perturb)
#define BUMP_SIZE (50.f)
#define REFLECTION_FACTOR 0.1f

#define PATCH_WIDTH (PATCH_SIZE-1)	//internal defines
#define PATCH_UV_SCALE	((Real)PATCH_UV_TILES/(Real)PATCH_WIDTH)

//3D Grid Mesh Water defines.
#define WATER_MESH_OPACITY		0.5f
#define WATER_MESH_X_VERTICES	128
#define WATER_MESH_Y_VERTICES	128
#define WATER_MESH_SPACING	MAP_XY_FACTOR	//same as terrain

#define MIPMAP_BUMP_TEXTURE

// D3D12: water mesh vertex layout is CPU-side only. The legacy FVF aliases are
// retired; MaterMeshVertexFormat documents the two historical layouts.
struct WaterMeshCpuVertex
{
	float x, y, z;
	float nx, ny, nz;
	unsigned int diffuse;
	float u1, v1;
	float u2, v2;
};
typedef WaterMeshCpuVertex MaterMeshVertexFormat;

#define DRAW_WATER_WAKES
/// @todo: Fix clipping of objects that intersect the mirror surface
//#define CLIP_GEOMETRY_TO_PLANE	// this enables clipping of objects that intersect the mirror surfaces

// Some shader combinations that can be useful in rendering water:

// Modulate stage0 with stage1 texture.  Also modulate stage 0 with vertex color.
#define SC_DETAIL_BLEND ( SHADE_CNST(ShaderClass::PASS_LEQUAL, ShaderClass::DEPTH_WRITE_ENABLE, ShaderClass::COLOR_WRITE_ENABLE,\
	ShaderClass::SRCBLEND_SRC_ALPHA,ShaderClass::DSTBLEND_ONE_MINUS_SRC_ALPHA, ShaderClass::FOG_DISABLE, ShaderClass::GRADIENT_MODULATE, ShaderClass::SECONDARY_GRADIENT_DISABLE, \
	ShaderClass::TEXTURING_ENABLE, 	ShaderClass::ALPHATEST_DISABLE, ShaderClass::CULL_MODE_ENABLE, ShaderClass::DETAILCOLOR_DETAILBLEND, ShaderClass::DETAILALPHA_DISABLE) )

// Just a z-buffer fill, nothing is written to the color buffer.
#define SC_ZFILL_BLEND ( SHADE_CNST(ShaderClass::PASS_LEQUAL, ShaderClass::DEPTH_WRITE_ENABLE, ShaderClass::COLOR_WRITE_DISABLE, ShaderClass::SRCBLEND_ZERO, \
	ShaderClass::DSTBLEND_ONE, ShaderClass::FOG_DISABLE, ShaderClass::GRADIENT_MODULATE, ShaderClass::SECONDARY_GRADIENT_DISABLE, ShaderClass::TEXTURING_ENABLE, \
	ShaderClass::DETAILCOLOR_SCALE, ShaderClass::DETAILALPHA_DISABLE, ShaderClass::ALPHATEST_DISABLE, ShaderClass::CULL_MODE_ENABLE, \
	ShaderClass::DETAILCOLOR_SCALE, ShaderClass::DETAILALPHA_DISABLE) )

// No texturing, just vertex color with vertex alpha
#define SC_ZFILL_BLENDx ( SHADE_CNST(ShaderClass::PASS_LEQUAL, ShaderClass::DEPTH_WRITE_ENABLE, ShaderClass::COLOR_WRITE_ENABLE, \
	ShaderClass::SRCBLEND_ZERO, ShaderClass::DSTBLEND_SRC_COLOR, ShaderClass::FOG_DISABLE, ShaderClass::GRADIENT_MODULATE, ShaderClass::SECONDARY_GRADIENT_DISABLE, \
	ShaderClass::TEXTURING_DISABLE, ShaderClass::DETAILCOLOR_DISABLE, ShaderClass::DETAILALPHA_DISABLE, ShaderClass::ALPHATEST_DISABLE, ShaderClass::CULL_MODE_ENABLE, \
	ShaderClass::DETAILCOLOR_DISABLE, ShaderClass::DETAILALPHA_DISABLE) )

// Modulate blended with vertex alpha modulation
#define SC_ZFILL_MODULATE_TEX ( SHADE_CNST(ShaderClass::PASS_LEQUAL, ShaderClass::DEPTH_WRITE_ENABLE, ShaderClass::COLOR_WRITE_ENABLE,\
	ShaderClass::SRCBLEND_ZERO, ShaderClass::DSTBLEND_SRC_COLOR, ShaderClass::FOG_DISABLE, ShaderClass::GRADIENT_MODULATE, ShaderClass::SECONDARY_GRADIENT_DISABLE, \
	ShaderClass::TEXTURING_ENABLE, ShaderClass::ALPHATEST_DISABLE, ShaderClass::CULL_MODE_DISABLE, ShaderClass::DETAILCOLOR_DISABLE, ShaderClass::DETAILALPHA_DISABLE) )

// Alpha blended with vertex alpha modulation
#define SC_ZFILL_ALPHA_TEX ( SHADE_CNST(ShaderClass::PASS_LEQUAL, ShaderClass::DEPTH_WRITE_ENABLE, ShaderClass::COLOR_WRITE_ENABLE,\
	ShaderClass::SRCBLEND_SRC_ALPHA, ShaderClass::DSTBLEND_ONE_MINUS_SRC_ALPHA, ShaderClass::FOG_DISABLE, ShaderClass::GRADIENT_DISABLE, ShaderClass::SECONDARY_GRADIENT_DISABLE, \
	ShaderClass::TEXTURING_ENABLE, ShaderClass::ALPHATEST_DISABLE, ShaderClass::CULL_MODE_DISABLE, ShaderClass::DETAILCOLOR_DISABLE, ShaderClass::DETAILALPHA_DISABLE) )

// Alpha blended with vertex alpha modulation
#define SC_OPAQUE_TEXONLY ( SHADE_CNST(ShaderClass::PASS_LEQUAL, ShaderClass::DEPTH_WRITE_ENABLE, ShaderClass::COLOR_WRITE_ENABLE,\
	ShaderClass::SRCBLEND_ONE, ShaderClass::DSTBLEND_ZERO, ShaderClass::FOG_DISABLE, ShaderClass::GRADIENT_DISABLE, ShaderClass::SECONDARY_GRADIENT_DISABLE, \
	ShaderClass::TEXTURING_ENABLE, ShaderClass::ALPHATEST_DISABLE, ShaderClass::CULL_MODE_DISABLE, ShaderClass::DETAILCOLOR_DISABLE, ShaderClass::DETAILALPHA_DISABLE) )

// Alpha blended with vertex alpha modulation
#define SC_ZFILL_BLEND3 ( SHADE_CNST(ShaderClass::PASS_LEQUAL, ShaderClass::DEPTH_WRITE_ENABLE, ShaderClass::COLOR_WRITE_ENABLE,\
	ShaderClass::SRCBLEND_SRC_ALPHA, ShaderClass::DSTBLEND_ONE_MINUS_SRC_ALPHA, ShaderClass::FOG_DISABLE, ShaderClass::GRADIENT_MODULATE, ShaderClass::SECONDARY_GRADIENT_DISABLE, \
	ShaderClass::TEXTURING_ENABLE, ShaderClass::ALPHATEST_DISABLE, ShaderClass::CULL_MODE_DISABLE, ShaderClass::DETAILCOLOR_DISABLE, ShaderClass::DETAILALPHA_DISABLE) )

static ShaderClass zFillAlphaShader(SC_ZFILL_BLEND3);
static ShaderClass blendStagesShader(SC_DETAIL_BLEND);

#define NOISE_REPEAT_FACTOR ((float)(1.0f/(16.0f)))

namespace
{

// D3D12: renderer-neutral water helpers (renamed from the retired D3D matrix
// utilities). The noise texture transform and world/view/projection constant
// math below is preserved CPU-side for documentation; the backend material
// path has no texture-transform / shader-constant equivalent yet, so callers
// submit base geometry with the closest material (no fake perturb/mirror).
Matrix4x4 Build_Water_Noise_Texture_Transform(const Matrix4x4 &view_matrix, Real origin)
{
	Matrix4x4 inverse_view;
	float determinant = 0.0f;
	if (Matrix4x4::Inverse(&inverse_view, &determinant, &view_matrix) == nullptr) {
		Matrix4x4 identity(true);
		return identity;
	}

	Matrix4x4 scale(true);
	scale[0][0] = NOISE_REPEAT_FACTOR;
	scale[1][1] = NOISE_REPEAT_FACTOR;

	Matrix4x4 translation(true);
	translation[3][0] = origin;
	translation[3][1] = origin;

	// The legacy path calculated inverse(view) * scale * translation.
	return inverse_view * scale * translation;
}

Matrix4x4 Build_Water_World_Matrix(const Matrix4x4 &patch_matrix, const Matrix4x4 &ww3d_matrix)
{
	return ww3d_matrix * patch_matrix;
}

void Build_Water_World_View_Projection_Constants(
	Matrix4x4 &shader_constants,
	const Matrix4x4 &patch_matrix,
	const Matrix4x4 &ww3d_matrix,
	const Matrix4x4 &view_matrix,
	const Matrix4x4 &projection_matrix)
{
	// Legacy code built patch * WW3D * view * projection and uploaded four
	// vertex-shader constants. Preserved here as plain matrix math.
	shader_constants =
		patch_matrix *
		ww3d_matrix *
		view_matrix *
		projection_matrix;
}

} // namespace

WaterRenderObjClass *TheWaterRenderObj=nullptr; ///<global water rendering object

static Int getRiverVertexDiffuse(W3DShroud *shroud, Real x, Real y, Real shadeR, Real shadeG, Real shadeB, Int diffuse)
{
	if (!shroud)
		return diffuse;

	Int cellX = (Int)(x / shroud->getCellWidth());
	Int cellY = (Int)(y / shroud->getCellHeight());
	W3DShroudLevel level = shroud->getShroudLevel(cellX, cellY);
	Real shroudScale = (Real)level / 255.0f;
	return GameMakeColor(
		(Int)(shadeR * shroudScale),
		(Int)(shadeG * shroudScale),
		(Int)(shadeB * shroudScale),
		((diffuse >> 24) & 0xff) * shroudScale);
}

void doSkyBoxSet(Bool startDraw)
{
	if (TheWritableGlobalData)
		TheWritableGlobalData->m_drawSkyBox = startDraw;
}


#define DONUT_SIDES	90
#define INNER_RADIUS 200.0f
#define OUTER_RADIUS 250.0f
#define TEXTURE_REPEAT_COUNT 16
#define DONUT_HEIGHT	15.0f
//#define DO_FLAT_DONUT
#define AMP_SCALE	(30.0f/120.0f)
#define WAVE_FREQ	0.3f
#define AMP_SCALE2	(10.0f/120.0f)
#define NOISE_FREQ	(2.0f*PI/WAVE_FREQ)


static Bool wireframeForDebug = 0;

void WaterRenderObjClass::setupJbaWaterShader()
{
	// D3D12: river material intent is preserved (alpha vs additive from
	// TheWaterTransparency); texture/sampler setup uses Ensure_Renderer_Texture.
	// The legacy stage-1 sparkles / stage-2 camera-space noise transform /
	// stage-3 alpha-edge wrap and the ps.1.1 river pixel shader (c0
	// REFLECTION_FACTOR) have no backend equivalent yet. CPU scroll offsets
	// (m_riverVOrigin/m_riverXOffset/m_riverYOffset) and REFLECTION_FACTOR are
	// preserved CPU-side; river quads are submitted with the closest alpha
	// material and river alpha-edge wrap addressing (no fake perturb/mirror).
	// The inline .pso/.vso asm stays as archival reference in ReAcquireResources.
	if (m_riverTexture != nullptr) {
		m_riverTexture->Get_Filter().Set_Mag_Filter(TextureFilterClass::FILTER_TYPE_BEST);
		m_riverTexture->Get_Filter().Set_Min_Filter(TextureFilterClass::FILTER_TYPE_BEST);
		m_riverTexture->Get_Filter().Set_Mip_Mapping(TextureFilterClass::FILTER_TYPE_BEST);
		m_riverTexture->Ensure_Renderer_Texture();
	}
	if (m_riverAlphaEdge != nullptr) {
		// River alpha-edge wrap preserved via sampler (backend clamp_texture=false).
		m_riverAlphaEdge->Get_Filter().Set_U_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_REPEAT);
		m_riverAlphaEdge->Get_Filter().Set_V_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_REPEAT);
		m_riverAlphaEdge->Ensure_Renderer_Texture();
	}
	if (m_waterSparklesTexture != nullptr)
		m_waterSparklesTexture->Ensure_Renderer_Texture();
	if (m_waterNoiseTexture != nullptr)
		m_waterNoiseTexture->Ensure_Renderer_Texture();
	// GAP (documented): Build_Water_Noise_Texture_Transform CPU math above is
	// preserved but not uploaded (no backend texture-transform); the river
	// pixel-shader constant c0 == REFLECTION_FACTOR is preserved CPU-side.
	(void)REFLECTION_FACTOR;
}




//-------------------------------------------------------------------------------------------------
/** Destructor. Releases w3d assets. */
//-------------------------------------------------------------------------------------------------
WaterRenderObjClass::~WaterRenderObjClass()
{
	REF_PTR_RELEASE(m_meshVertexMaterialClass);
	REF_PTR_RELEASE(m_vertexMaterialClass);
	REF_PTR_RELEASE(m_meshLight);
	REF_PTR_RELEASE(m_alphaClippingTexture);
	REF_PTR_RELEASE (m_skyBox);

	REF_PTR_RELEASE (m_riverTexture);
	REF_PTR_RELEASE (m_whiteTexture);
	REF_PTR_RELEASE (m_waterNoiseTexture);
	REF_PTR_RELEASE (m_riverAlphaEdge);
	REF_PTR_RELEASE (m_waterSparklesTexture);

	Int i;

	for(i=0; i<TIME_OF_DAY_COUNT; i++)
	{	REF_PTR_RELEASE(m_settings[i].skyTexture);
		REF_PTR_RELEASE(m_settings[i].waterTexture);
	}

	i=NUM_BUMP_FRAMES;
	while (i--)
	{	// D3D12: bump frames are neutral RGBA8 uploads, not D3D textures.
	}
	m_bumpNeutralRGBA.clear();
	m_bumpNeutralWidth = 0;
	m_bumpNeutralHeight = 0;
	m_bumpNeutralTexture = RenderBackendTextureHandle();

	delete [] m_meshData;
	m_meshData = nullptr;
	m_meshDataSize = 0;

	//Release strings allocated inside global water settings.
	for  (i=0; i<TIME_OF_DAY_COUNT; i++)
	{	WaterSettings[i].m_skyTextureFile.clear();
		WaterSettings[i].m_waterTextureFile.clear();
	}
	deleteInstance((WaterTransparencySetting*)TheWaterTransparency.getNonOverloadedPointer());
	TheWaterTransparency = nullptr;
	ReleaseResources();

	delete m_waterTrackSystem;
}

//-------------------------------------------------------------------------------------------------
/** Constructor. Just nulls out some variables. */
//-------------------------------------------------------------------------------------------------
WaterRenderObjClass::WaterRenderObjClass()
{
	memset( &m_settings, 0, sizeof( m_settings ) );
	m_dx=0;
	m_dy=0;
	m_quadIndices.clear();
	m_waterTrackSystem = nullptr;
	m_doWaterGrid = FALSE;
	m_meshVertexMaterialClass=nullptr;
	m_meshLight=nullptr;
	m_vertexMaterialClass=nullptr;
	m_alphaClippingTexture=nullptr;
	m_useCloudLayer=true;
	m_waterType = WATER_TYPE_0_TRANSLUCENT;
	m_tod=TIME_OF_DAY_AFTERNOON;
	m_pReflectionTexture=nullptr;
	m_skyBox=nullptr;
	m_patchVertices.clear();
	m_patchIndices.clear();
	m_bumpNeutralRGBA.clear();
	m_bumpNeutralWidth=0;
	m_bumpNeutralHeight=0;
	m_bumpNeutralTexture=RenderBackendTextureHandle();

	m_wavePixelShader=0;
	m_waveVertexShader=0;
	m_meshData=nullptr;
	m_meshDataSize = 0;
	m_meshInMotion = FALSE;
	m_gridOrigin=Vector2(0,0);
	m_gridDirectionX=Vector2(1.0f,0.0f);
	m_gridDirectionY=Vector2(1.0f,0.0f);

	m_gridCellSize=WATER_MESH_SPACING;
	m_gridCellsX=WATER_MESH_X_VERTICES;
	m_gridCellsY=WATER_MESH_Y_VERTICES;
	m_gridWidth = m_gridCellsX * m_gridCellSize;
	m_gridHeight = m_gridCellsY * m_gridCellSize;

	m_riverVOrigin=0;
	m_riverTexture=nullptr;
	m_whiteTexture=nullptr;
	m_waterNoiseTexture=nullptr;
	m_riverAlphaEdge=nullptr;
	m_waterPixelShader=0;		///<retired pixel-shader handle (reference only, always 0).
	m_riverWaterPixelShader=0;		///<retired pixel-shader handle (reference only, always 0).
	m_trapezoidWaterPixelShader=0;		///<retired pixel-shader handle (reference only, always 0).
	m_waterSparklesTexture=nullptr;
	m_riverXOffset=0;
	m_riverYOffset=0;
}

//-------------------------------------------------------------------------------------------------
/** WW3D method that returns object bounding sphere used in frustum culling*/
//-------------------------------------------------------------------------------------------------
void WaterRenderObjClass::Get_Obj_Space_Bounding_Sphere(SphereClass & sphere) const
{
	//Since this object is more of a system (containing lots of water pieces),
	//let's disable culling by making bounds huge.  Let each piece do it's own cull.
	Vector3	ObjSpaceCenter(0,0,0);
//	Vector3	ObjSpaceRadius(m_dx,m_dy,0);
	Vector3	ObjSpaceRadius(50000,50000,0);

	sphere.Init(ObjSpaceCenter,ObjSpaceRadius.Length());
}

//-------------------------------------------------------------------------------------------------
/** WW3D method that returns object bounding box used in collision detection*/
//-------------------------------------------------------------------------------------------------
void WaterRenderObjClass::Get_Obj_Space_Bounding_Box(AABoxClass & box) const
{
	//Since this object is more of a system (containing lots of water pieces),
	//let's disable culling by making bounds huge.  Let each piece do it's own cull.

	Vector3	ObjSpaceCenter(0,0,0);
	Vector3	ObjSpaceExtents(50000,50000,0.001f*m_dy);	//since mirror is a plane, it has no thickness. Set to m_dy/1000.

	box.Init(ObjSpaceCenter,ObjSpaceExtents);
}

//-------------------------------------------------------------------------------------------------
/** returns the class id, so the scene can tell what kind of render object it has. */
//-------------------------------------------------------------------------------------------------
Int WaterRenderObjClass::Class_ID() const
{
	return RenderObjClass::CLASSID_UNKNOWN;
}

//-------------------------------------------------------------------------------------------------
/** Not used, but required virtual method. */
//-------------------------------------------------------------------------------------------------
RenderObjClass *	 WaterRenderObjClass::Clone() const
{
	assert(false);
	return nullptr;
}

//-------------------------------------------------------------------------------------------------
/** Uploads a neutral RGBA8 bump image (D3D12 replaces the retired U8V8 format).
	*   The legacy grayscale-to-gradient (du/dv) math is retired with the D3D
	*   lock path; bump perturb has no backend equivalent yet (documented gap). */
//-------------------------------------------------------------------------------------------------
HRESULT WaterRenderObjClass::initBumpMap(TextureClass *pBumpSource)
{
	(void)pBumpSource;
	// RGBA8 neutral normal (128,128,255 == flat). Uploaded once and reused.
	const unsigned int neutralSize = 64;
	m_bumpNeutralWidth = neutralSize;
	m_bumpNeutralHeight = neutralSize;
	m_bumpNeutralRGBA.assign(static_cast<size_t>(neutralSize) * neutralSize * 4, 0);
	for (size_t px = 0; px < static_cast<size_t>(neutralSize) * neutralSize; ++px) {
		m_bumpNeutralRGBA[px * 4 + 0] = 128;
		m_bumpNeutralRGBA[px * 4 + 1] = 128;
		m_bumpNeutralRGBA[px * 4 + 2] = 255;
		m_bumpNeutralRGBA[px * 4 + 3] = 255;
	}
	if (IRenderBackend *bumpBackend = WW3D::Get_Render_Backend()) {
		RenderBackendTextureMipLevel bumpLevel;
		bumpLevel.width = neutralSize;
		bumpLevel.height = neutralSize;
		bumpLevel.row_pitch = neutralSize * 4;
		bumpLevel.pixels = m_bumpNeutralRGBA.data();
		m_bumpNeutralTexture = bumpBackend->Create_Static_RGBA8_Texture(&bumpLevel, 1);
	}
	return S_OK;
}

//-------------------------------------------------------------------------------------------------
// Retired D3D8 bump-gradient reference (U8V8 lock path). Kept out of the build;
// the active path is the neutral RGBA8 upload above.
//-------------------------------------------------------------------------------------------------
#if 0
//-------------------------------------------------------------------------------------------------
/** Copies raw bits from pBumpSrc (a regular grayscale texture) into a D3D
	*   bump-map format. */
static void WaterBumpMap_D3D8_Reference_Note() {}

	for (Int level=0; level < numLevels; level++)
	{
		surf=pBumpSource->Get_Surface_Level(level);
		surf->Get_Description(d3dsd);
		pSrc=(unsigned char *)surf->Lock((int *)&dwSrcPitch);

		pTex[0]->LockRect( level, &d3dlr, nullptr, 0 );
		DWORD dwDstPitch = (DWORD)d3dlr.Pitch;
		BYTE* pDst       = (BYTE*)d3dlr.pBits;

		for( DWORD y=0; y<d3dsd.Height; y++ )
		{
			BYTE* pDstT  = pDst;
			BYTE* pSrcB0 = (BYTE*)pSrc;
			BYTE* pSrcB1 = ( pSrcB0 + dwSrcPitch );
			BYTE* pSrcB2 = ( pSrcB0 - dwSrcPitch );

			if( y == d3dsd.Height-1 )  // Don't go past the last line
				pSrcB1 = pSrcB0;
			if( y == 0 )               // Don't go before first line
				pSrcB2 = pSrcB0;

			for( DWORD x=0; x<d3dsd.Width; x++ )
			{
				LONG v00 = 256-*(pSrcB0+0); // Get the current pixel
				LONG v01 = 256-*(pSrcB0+4); // and the pixel to the right
				LONG vM1 = 256-*(pSrcB0-4); // and the pixel to the left
				LONG v10 = 256-*(pSrcB1+0); // and the pixel one line below.
				LONG v1M = 256-*(pSrcB2+0); // and the pixel one line above.

				LONG iDu = (vM1-v01); // The delta-u bump value
				LONG iDv = (v1M-v10); // The delta-v bump value

				if( (v00 < vM1) && (v00 < v01) )  // If we are at valley
				{
					iDu = vM1-v00;                 // Choose greater of 1st order diffs
					if( iDu < v00-v01 )
						iDu = v00-v01;
				}

				// The luminance bump value (land masses are less shiny)
				WORD uL = ( v00>1 ) ? 63 : 127;

				switch( D3DFMT_V8U8)//m_BumpMapFormat )
				{
					case D3DFMT_V8U8:
						*pDstT++ = (BYTE)iDu;
						*pDstT++ = (BYTE)iDv;
						break;

					case D3DFMT_L6V5U5:
						*(WORD*)pDstT  = (WORD)( ( (iDu>>3) & 0x1f ) <<  0 );
						*(WORD*)pDstT |= (WORD)( ( (iDv>>3) & 0x1f ) <<  5 );
						*(WORD*)pDstT |= (WORD)( ( ( uL>>2) & 0x3f ) << 10 );
						pDstT += 2;
						break;

					case D3DFMT_X8L8V8U8:
						*pDstT++ = (BYTE)iDu;
						*pDstT++ = (BYTE)iDv;
						*pDstT++ = (BYTE)uL;
						*pDstT++ = (BYTE)0L;
						break;
				}

				// Move one pixel to the left (src is 32-bpp)
				pSrcB0+=4;   pSrcB1+=4;   pSrcB2+=4;
			}

			// Move to the next line
			pSrc += dwSrcPitch;    pDst += dwDstPitch;
		}

		pTex[0]->UnlockRect(level);
		surf->Unlock();
		REF_PTR_RELEASE (surf);
	}

#if 0 // retired non-mip bump path (was #else, now stripped with the block above)
	surf=pBumpSource->Get_Surface_Level();
	surf->Get_Description(d3dsd);
	pSrc=(unsigned char *)surf->Lock((int *)&dwSrcPitch);

    // Create the bumpmap's surface and texture objects
	m_pBumpTexture[i]=DX8Wrapper::_Create_DX8_Texture(d3dsd.Width,d3dsd.Height,WW3D_FORMAT_U8V8,TextureClass::MIP_LEVELS_1,D3DPOOL_MANAGED,false);

    // Fill the bits of the new texture surface with bits from
    // a private format.

    m_pBumpTexture[i]->LockRect( 0, &d3dlr, 0, 0 );
    DWORD dwDstPitch = (DWORD)d3dlr.Pitch;
    BYTE* pDst       = (BYTE*)d3dlr.pBits;

    for( DWORD y=0; y<d3dsd.Height; y++ )
    {
        BYTE* pDstT  = pDst;
        BYTE* pSrcB0 = (BYTE*)pSrc;
        BYTE* pSrcB1 = ( pSrcB0 + dwSrcPitch );
        BYTE* pSrcB2 = ( pSrcB0 - dwSrcPitch );

        if( y == d3dsd.Height-1 )  // Don't go past the last line
            pSrcB1 = pSrcB0;
        if( y == 0 )               // Don't go before first line
            pSrcB2 = pSrcB0;

        for( DWORD x=0; x<d3dsd.Width; x++ )
        {
            LONG v00 = 256-*(pSrcB0+0); // Get the current pixel
            LONG v01 = 256-*(pSrcB0+4); // and the pixel to the right
            LONG vM1 = 256-*(pSrcB0-4); // and the pixel to the left
            LONG v10 = 256-*(pSrcB1+0); // and the pixel one line below.
            LONG v1M = 256-*(pSrcB2+0); // and the pixel one line above.

            LONG iDu = (vM1-v01); // The delta-u bump value
            LONG iDv = (v1M-v10); // The delta-v bump value

            if( (v00 < vM1) && (v00 < v01) )  // If we are at valley
            {
                iDu = vM1-v00;                 // Choose greater of 1st order diffs
                if( iDu < v00-v01 )
                    iDu = v00-v01;
            }

            // The luminance bump value (land masses are less shiny)
            WORD uL = ( v00>1 ) ? 63 : 127;

            switch( D3DFMT_V8U8)//m_BumpMapFormat )
            {
                case D3DFMT_V8U8:
                    *pDstT++ = (BYTE)iDu;
                    *pDstT++ = (BYTE)iDv;
                    break;

                case D3DFMT_L6V5U5:
                    *(WORD*)pDstT  = (WORD)( ( (iDu>>3) & 0x1f ) <<  0 );
                    *(WORD*)pDstT |= (WORD)( ( (iDv>>3) & 0x1f ) <<  5 );
                    *(WORD*)pDstT |= (WORD)( ( ( uL>>2) & 0x3f ) << 10 );
                    pDstT += 2;
                    break;

                case D3DFMT_X8L8V8U8:
                    *pDstT++ = (BYTE)iDu;
                    *pDstT++ = (BYTE)iDv;
                    *pDstT++ = (BYTE)uL;
                    *pDstT++ = (BYTE)0L;
                    break;
            }

            // Move one pixel to the left (src is 32-bpp)
            pSrcB0+=4;   pSrcB1+=4;   pSrcB2+=4;
        }

        // Move to the next line
        pSrc += dwSrcPitch;    pDst += dwDstPitch;
    }

    m_pBumpTexture[i]->UnlockRect(0);
    surf->Unlock();
#endif // retired non-mip bump path
#endif // retired D3D8 bump-gradient reference (U8V8 lock path)

//-------------------------------------------------------------------------------------------------
/** Create and fill a CPU patch grid with water surface vertices (D3D12) */
//-------------------------------------------------------------------------------------------------
HRESULT WaterRenderObjClass::generateVertexBuffer( Int sizeX, Int sizeY, Int vertexSize, Bool doStatic)
{
	(void)vertexSize;
	m_numVertices=sizeX*sizeY;

	Setting *setting=&m_settings[m_tod];

	HRESULT hr = S_OK;

	m_patchVertices.clear();
	m_patchVertices.reserve(static_cast<size_t>(m_numVertices));

	if (!doStatic)
		return S_OK;	//only reserve the grid, other code will fill it.

	// Fill the CPU patch grid (position + water diffuse/alpha + bump UVs).
	for (Int z=0; z<sizeY; z++)
	{
		for (Int x=0; x<sizeX; x++)
		{
			RenderBackendTexturedVertex v;
			v.x=(float)x;
			v.y=m_level;
			v.z=(float)z;
			const UnsignedInt c = setting->transparentWaterDiffuse;	//vertex alpha/color
			v.a = ((c >> 24) & 255) / 255.0f;
			v.r = ((c >> 16) & 255) / 255.0f;
			v.g = ((c >> 8) & 255) / 255.0f;
			v.b = (c & 255) / 255.0f;
			v.u=(float)x*PATCH_UV_SCALE;
			v.v=(float)z*PATCH_UV_SCALE;
			v.q = 1.0f;
			m_patchVertices.push_back(v);
		}
	}

	return S_OK;
}

//-------------------------------------------------------------------------------------------------
/** Create and fill a CPU index list with water surface triangle indices (D3D12) */
//-------------------------------------------------------------------------------------------------
HRESULT WaterRenderObjClass::generateIndexBuffer(Int sizeX, Int sizeY)
{
	// D3D12: triangle list (no degenerate strip links; zero-area triangles are
	// simply not generated). Grid topology matches the legacy strip layout.
	m_patchIndices.clear();
	m_patchIndices.reserve(static_cast<size_t>(sizeY - 1) * (sizeX - 1) * 6);
	for (Int z = 0; z < sizeY - 1; ++z) {
		for (Int x = 0; x < sizeX - 1; ++x) {
			const UnsignedShort a = static_cast<UnsignedShort>(z * sizeX + x);
			const UnsignedShort b = static_cast<UnsignedShort>(z * sizeX + x + 1);
			const UnsignedShort c = static_cast<UnsignedShort>((z + 1) * sizeX + x);
			const UnsignedShort d = static_cast<UnsignedShort>((z + 1) * sizeX + x + 1);
			m_patchIndices.push_back(a);
			m_patchIndices.push_back(c);
			m_patchIndices.push_back(b);
			m_patchIndices.push_back(b);
			m_patchIndices.push_back(c);
			m_patchIndices.push_back(d);
		}
	}
	m_numIndices = static_cast<Int>(m_patchIndices.size());

	return S_OK;
}

//-------------------------------------------------------------------------------------------------
/** Releases all renderer assets, to prepare for Reset device call. */
//-------------------------------------------------------------------------------------------------
void WaterRenderObjClass::ReleaseResources()
{

	m_quadIndices.clear();

	REF_PTR_RELEASE(m_pReflectionTexture);
	m_patchVertices.clear();
	m_patchIndices.clear();

	if (m_waterTrackSystem)
		m_waterTrackSystem->ReleaseResources();

	// D3D12: wave/river pixel/vertex shader blobs are archival reference only.
	// No D3D handles exist; reset the reference handles.
	m_wavePixelShader=0;
	m_waveVertexShader=0;
	m_waterPixelShader = 0;
	m_trapezoidWaterPixelShader=0;
	m_riverWaterPixelShader=0;
}

//-------------------------------------------------------------------------------------------------
/** (Re)allocates all W3D assets after a reset.. */
//-------------------------------------------------------------------------------------------------
void WaterRenderObjClass::ReAcquireResources()
{
	HRESULT hr = S_OK;

	// D3D12: quad indices live on the CPU (replaces DX8IndexBufferClass).
	//quad of 2 triangles:
	//	3-----2
	//  |    /|
	//  |  /  |
	//	|/    |
	//  0-----1
	m_quadIndices.clear();
	{
		const UnsignedShort quad[6] = { 3, 0, 2, 2, 0, 1 };
		m_quadIndices.assign(quad, quad + 6);
	}

	//We're using the same grid for either 3D Water Mesh or Pixel/Vertex shader.  Just
	//allocate the right size depending on usage
	if (m_meshData)
	{
		//Create new grid data
		if (FAILED(generateIndexBuffer(m_gridCellsX+1,m_gridCellsY+1)))
			return;
		if (FAILED(generateVertexBuffer(m_gridCellsX+1,m_gridCellsY+1,sizeof(MaterMeshVertexFormat),false)))
			return;
	}
	else
	if (m_waterType == WATER_TYPE_2_PVSHADER)
	{	//pixel/vertex shader based water assets.
		if (FAILED(hr=generateIndexBuffer(PATCH_SIZE,PATCH_SIZE)))
			return;

		if (FAILED(hr=generateVertexBuffer(PATCH_SIZE,PATCH_SIZE,sizeof(SEA_PATCH_VERTEX),true)))
			return;

		// D3D12: shaders\\wave.pso / wave.vso are archival reference only.
		// The active path uses material state; no D3D handles are created.
		m_wavePixelShader = 0;
		m_waveVertexShader = 0;

		// Create 256px reflection RT via the backend (not D3D8 surfaces).
		REF_PTR_RELEASE(m_pReflectionTexture);
		m_pReflectionTexture = WW3D::Create_Render_Texture(SEA_REFLECTION_SIZE, SEA_REFLECTION_SIZE);
	}

	if (m_waterTrackSystem)
		m_waterTrackSystem->ReAcquireResources();

#if !defined(RTS_EVOLUTION_X64)
	// The inline ps.1.1 assembler is part of the archival DX8 renderer. Evolution
	// intentionally does not depend on D3DX8; these shaders will be translated
	// when the real water caller crosses the renderer-neutral D3D12 seam.
	if (W3DShaderManager::getChipset() >= DC_GENERIC_PIXEL_SHADER_1_1)
	{
		ID3DXBuffer *compiledShader;
		const char *shader =
			"ps.1.1\n \
			tex t0 \n\
			tex t1	\n\
			tex t2	\n\
			tex t3\n\
			mul r0.rgb, v0, t0 ; blend vertex color into t0. \n\
			mov r0.a, t0 ; keep vertex alpha from fading the base water. \n\
			mul r1, t1, t2 ; mul\n\
			add r1.rgb, r1, t3\n\
			mul r1.rgb, r1, v0.a\n\
			+mul r0.a, r0, t3\n\
			add r0.rgb, r0, r1\n";
		hr = D3DXAssembleShader( shader, strlen(shader), 0, nullptr, &compiledShader, nullptr);
		if (hr==0) {
			hr = 	DX8Wrapper::_Get_D3D_Device8()->CreatePixelShader((DWORD*)compiledShader->GetBufferPointer(), &m_riverWaterPixelShader);
			compiledShader->Release();
		}
		shader =
			"ps.1.1\n \
			tex t0 \n\
			tex t1	\n\
			texbem t2, t1 ; use t1 as env map adjustment on t2.\n\
			mul r0,v0,t0 ; blend vertex color into t0. \n\
			mul r1.rgb,t2,c0 ; reduce t2 (environment mapped reflection) by constant\n\
			add r0.rgb, r0, r1";
		hr = D3DXAssembleShader( shader, strlen(shader), 0, nullptr, &compiledShader, nullptr);
		if (hr==0) {
			hr = 	DX8Wrapper::_Get_D3D_Device8()->CreatePixelShader((DWORD*)compiledShader->GetBufferPointer(), &m_waterPixelShader);
			compiledShader->Release();
		}
		shader =
			"ps.1.1\n \
			tex t0 ;get water texture\n\
			tex t1 ;get white highlights on black background\n\
			tex t2 ;get white highlights with more tiling\n\
			tex t3	; get black shroud \n\
			mul r0,v0,t0 ; blend vertex color and alpha into base texture. \n\
			mad r0.rgb, t1, t2, r0	; blend sparkles and noise \n\
			mul r0.rgb, r0, t3 ; blend in black shroud \n\
			;\n";
		hr = D3DXAssembleShader( shader, strlen(shader), 0, nullptr, &compiledShader, nullptr);
		if (hr==0) {
			hr = 	DX8Wrapper::_Get_D3D_Device8()->CreatePixelShader((DWORD*)compiledShader->GetBufferPointer(), &m_trapezoidWaterPixelShader);
			compiledShader->Release();
		}
	}
#endif

	// Renderer textures are backend-owned; re-ensure file-backed textures here.
	// (Legacy code peeked at D3D textures directly and re-inited manually.)
	if (m_riverTexture)
		m_riverTexture->Ensure_Renderer_Texture();
	if (m_waterNoiseTexture)
		m_waterNoiseTexture->Ensure_Renderer_Texture();
	if (m_riverAlphaEdge)
		m_riverAlphaEdge->Ensure_Renderer_Texture();
	if (m_waterSparklesTexture)
		m_waterSparklesTexture->Ensure_Renderer_Texture();
	// m_whiteTexture is a procedural 1x1 white placeholder; the backend treats
	// a missing/untextured stage as white via the material path (documented).
}

void WaterRenderObjClass::load()
{
	if (m_waterTrackSystem)
		m_waterTrackSystem->loadTracks();
}

//-------------------------------------------------------------------------------------------------
/** Initializes water with dimensions and parent scene.
	* During rendering, we will render a water surface of given dimensions
	* and reflect the parent scene in its surface.  For now, waters are
	* forced to be rectangles. */
//-------------------------------------------------------------------------------------------------
Int WaterRenderObjClass::init(Real waterLevel, Real dx, Real dy, SceneClass *parentScene, WaterType type)
{

	m_fBumpFrame=0;
	m_fBumpScale=SEA_BUMP_SCALE;

	m_dx=dx;
	m_dy=dy;
	m_level=waterLevel;

	m_LastUpdateTime=timeGetTime();
	m_uScrollPerMs=0.001f;
	m_vScrollPerMs=0.001f;
	m_uOffset=0;
	m_vOffset=0;

	m_parentScene=parentScene;
	m_waterType = type;

	/// Hack for now
	//m_waterType = WATER_TYPE_0_TRANSLUCENT;

	///@todo: calculate a real normal/distance for arbitrary planes.
	m_planeNormal=Vector3(0,0,1);		//water plane normal
	m_planeDistance=m_level;	//water plane distance(always at zero for now)

	m_meshLight=NEW_REF(LightClass,(LightClass::DIRECTIONAL));
	m_meshLight->Set_Ambient(Vector3(0.1f,0.1f,0.1f));
	m_meshLight->Set_Diffuse(Vector3(1.0f,1.0f,1.0f));
	m_meshLight->Set_Specular(Vector3(1.0f,1.0f,1.0f));
	m_meshLight->Set_Position(Vector3(1000,1000,1000));
	//testLight->Set_Spot_Direction(Vector3(TheGlobalData->m_terrainLightX,TheGlobalData->m_terrainLightY,TheGlobalData->m_terrainLightZ));
	m_meshLight->Set_Spot_Direction(Vector3(-0.57f,-0.57f,-0.57f));

	//Setup material for 3D Mesh water.
	m_meshVertexMaterialClass=NEW_REF(VertexMaterialClass,());
	m_meshVertexMaterialClass->Set_Shininess(20.0);
	m_meshVertexMaterialClass->Set_Ambient(1.0f,1.0f,1.0f);
	m_meshVertexMaterialClass->Set_Diffuse(1.0f,1.0f,1.0f);
	m_meshVertexMaterialClass->Set_Specular(0.5,0.5,0.5);
	m_meshVertexMaterialClass->Set_Opacity(WATER_MESH_OPACITY);
	m_meshVertexMaterialClass->Set_Lighting(true);

	//
	// assign the data from the WaterSettings[] global to the data for this
	// render object (we at present only have one water plane)
	//
	loadSetting( &m_settings[ TIME_OF_DAY_MORNING ], TIME_OF_DAY_MORNING );
	loadSetting( &m_settings[ TIME_OF_DAY_AFTERNOON ], TIME_OF_DAY_AFTERNOON );
	loadSetting( &m_settings[ TIME_OF_DAY_EVENING ], TIME_OF_DAY_EVENING );
	loadSetting( &m_settings[ TIME_OF_DAY_NIGHT ], TIME_OF_DAY_NIGHT );

	Set_Sort_Level(2);	//force water to be drawn after all other non translucent objects in scene.
	Set_Force_Visible(TRUE);	//water is always visible since it's a composite object made of multiple planes all over the map.

	ReAcquireResources();
#if 0	//MD does not support the old bump-mapped water at all so no point loading textures. -MW 8-11-03
	// D3D12: retired U8V8 bump-frame uploads. The active path uses the neutral
	// RGBA8 upload from initBumpMap (see above). Block kept as reference.
#endif

	//Setup material for regular water
	m_vertexMaterialClass=VertexMaterialClass::Get_Preset(VertexMaterialClass::PRELIT_DIFFUSE);



	m_shaderClass = zFillAlphaShader;//ShaderClass::_PresetAlphaShader;ShaderClass::_PresetOpaqueShader;//detailOpaqueShader;
	m_shaderClass.Set_Cull_Mode(ShaderClass::CULL_MODE_DISABLE);	//water should be visible from both sides

	//Assets used for all types of water
	m_alphaClippingTexture=WW3DAssetManager::Get_Instance()->Get_Texture(SKYBODY_TEXTURE);

#ifdef CLIP_GEOMETRY_TO_PLANE
	m_alphaClippingTexture=WW3DAssetManager::Get_Instance()->Get_Texture("alphaclip.tga");
#endif

	m_skyBox = ((W3DAssetManager*)W3DAssetManager::Get_Instance())->Create_Render_Obj( "new_skybox", TheGlobalData->m_skyBoxScale, 0);

	//Enable clamping on all textures used by the skybox (to reduce corner seams).
	if (m_skyBox && m_skyBox->Class_ID() == RenderObjClass::CLASSID_MESH)
	{
		MeshClass *mesh=(MeshClass*) m_skyBox;
		MaterialInfoClass	*material = mesh->Get_Material_Info();

		for (Int i=0; i<material->Texture_Count(); i++)
		{
			if (material->Peek_Texture(i))
			{
				material->Peek_Texture(i)->Get_Filter().Set_U_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_CLAMP);
				material->Peek_Texture(i)->Get_Filter().Set_V_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_CLAMP);
			}
		}

		REF_PTR_RELEASE(material);
	}

	m_riverTexture=WW3DAssetManager::Get_Instance()->Get_Texture(TheWaterTransparency->m_standingWaterTexture.str());

	//For some reason setting a null texture does not result in 0xffffffff for pixel shaders so using explicit "white" texture.
	// D3D12: procedural 1x1 white placeholder. The backend treats an invalid
	// texture handle as white via the material path; no surface fill needed.
	m_whiteTexture=MSGNEW("TextureClass") TextureClass(1,1,WW3D_FORMAT_A4R4G4B4,MIP_LEVELS_1);

	m_waterNoiseTexture=WW3DAssetManager::Get_Instance()->Get_Texture("Noise0000.tga");
	m_riverAlphaEdge=WW3DAssetManager::Get_Instance()->Get_Texture("TWAlphaEdge.tga");
	m_waterSparklesTexture=WW3DAssetManager::Get_Instance()->Get_Texture("WaterSurfaceBubbles.tga");
#ifdef DRAW_WATER_WAKES
	m_waterTrackSystem = NEW WaterTracksRenderSystem;
	m_waterTrackSystem->init();
#endif

	return 0;
}

void WaterRenderObjClass::updateMapOverrides()
{
	if (m_riverTexture && TheWaterTransparency->m_standingWaterTexture.compareNoCase(m_riverTexture->Get_Texture_Name()) != 0)
	{
		REF_PTR_RELEASE(m_riverTexture);
		m_riverTexture = WW3DAssetManager::Get_Instance()->Get_Texture(TheWaterTransparency->m_standingWaterTexture.str());
	}
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void WaterRenderObjClass::reset()
{

	// for vertex animated water mesh reset the values
	if( m_meshData)
	{
		Int i, j;
		WaterMeshData *pData;
		Int	mx = m_gridCellsX + 1;
		Int my = m_gridCellsY + 1;

		// go through each mesh point and adjust the height according to the velocity
		for( j = 0, pData = m_meshData; j < (my + 2); j++ )
		{

			for( i = 0; i < (mx + 2); i++ )
			{

				// areset grid values for this cell
				pData->velocity = 0.0f;
				pData->height = 0.0f;
				pData->preferredHeight = 0.0f;
				pData->status = WaterRenderObjClass::AT_REST;

				// on to the next one
				pData++;

			}

		}

		// mesh data is no longer in motion
		m_meshInMotion = FALSE;

	}

	if (m_waterTrackSystem)
		m_waterTrackSystem->reset();
}

void WaterRenderObjClass::enableWaterGrid(Bool state)
{
	m_doWaterGrid = state;

	m_drawingRiver = false;
	m_disableRiver = false;

	if (state && m_meshData == nullptr)
	{	//water type has changed, must allocate necessary assets for new water.
		//contains the current deformed water surface z(height) values.  With 1 vertex invisible border
		//around surface to speed up normal calculations.
		m_meshDataSize = (m_gridCellsX+1+2)*(m_gridCellsY+1+2);
		m_meshData=NEW WaterMeshData[ m_meshDataSize ];
		memset(m_meshData,0,sizeof(WaterMeshData)*(m_gridCellsX+1+2)*(m_gridCellsY+1+2));
		reset();

		//Release existing grid data
		m_patchVertices.clear();
		m_patchIndices.clear();

		//Create new grid data
		if (FAILED(generateIndexBuffer(m_gridCellsX+1,m_gridCellsY+1)))
			return;
		if (FAILED(generateVertexBuffer(m_gridCellsX+1,m_gridCellsY+1,sizeof(MaterMeshVertexFormat),false)))
			return;
	}
}

// ------------------------------------------------------------------------------------------------
/** Update phase for water if we need it. */
// ------------------------------------------------------------------------------------------------
void WaterRenderObjClass::update()
{
	// TheSuperHackers @tweak The water movement time step is now decoupled from the render update.
	const Real timeScale = TheFramePacer->getActualLogicTimeScaleOverFpsRatio();

	{
		constexpr const Real MagicOffset = 0.0125f * 33 / 5000; ///< the work of top Munkees; do not question it

		m_riverVOrigin += 0.002f * timeScale;
		m_riverXOffset += (Real)(MagicOffset * timeScale);
		m_riverYOffset += (Real)(2 * MagicOffset * timeScale);

		// This moves offsets towards zero when smaller -1.0 or larger 1.0
		m_riverXOffset -= (Int)m_riverXOffset;
		m_riverYOffset -= (Int)m_riverYOffset;

		m_fBumpFrame += timeScale;
		if (m_fBumpFrame >= NUM_BUMP_FRAMES)
			m_fBumpFrame = 0.0f;

		// for vertex animated water we need to update the vector field
		if( m_doWaterGrid && m_meshInMotion == TRUE )
		{
			const Real PREFERRED_HEIGHT_FUDGE = 1.0f;		///< this is close enough to at rest
			const Real AT_REST_VELOCITY_FUDGE = 1.0f;		///< when we're close enough to at rest height and velocity we will stop
			const Real WATER_DAMPENING = 0.93f;					///< use with up force of 15.0
			Int i, j;
			Int	mx = m_gridCellsX+1;
			Int my = m_gridCellsY+1;
			WaterMeshData *pData;

			//
			// we will mark the mesh as clean now ... if any of the fields are still in motion
			// they will continue to mark the mesh as dirty so processing continues next frame
			//
			m_meshInMotion = FALSE;

			// go through each mesh point and adjust the height according to the velocity
			for( j = 0, pData = m_meshData; j < (my + 2); j++ )
			{

				for( i = 0; i < (mx + 2); i++ )
				{

					// only pay attention to mesh points that are in motion
					if( BitIsSet( pData->status, WaterRenderObjClass::IN_MOTION ) )
					{

						// DAMPENING to slow the changes down
						pData->velocity *= WATER_DAMPENING;

						// if the height here is below our preferred height, we want to add upward force to counteract it
						if( pData->height < pData->preferredHeight )
							pData->velocity -= TheGlobalData->m_gravity * 3.0f;
						else
							pData->velocity += TheGlobalData->m_gravity * 3.0f;

						// adjust the height at this grid location according to the current velocity
						pData->height = pData->height + pData->velocity;

						//
						// if we are close enough to our preferred height and our velocity is small enough
						// this will be our resting location
						//
						if( fabs( pData->height - pData->preferredHeight ) < PREFERRED_HEIGHT_FUDGE &&
								fabs( pData->velocity ) < AT_REST_VELOCITY_FUDGE )
						{

							BitClear( pData->status, WaterRenderObjClass::IN_MOTION );
							pData->height = pData->preferredHeight;
							pData->velocity = 0.0f;

						}
						else
						{

							// there is still motion in the mesh, we need to process next frame
							m_meshInMotion = TRUE;

						}

					}

					// on to the next one
					pData++;

				}

			}

		}

	}

}


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void WaterRenderObjClass::replaceSkyboxTexture(const AsciiString& oldTexName, const AsciiString& newTextName)
{
	W3DAssetManager* assetManager = ((W3DAssetManager*)W3DAssetManager::Get_Instance());

	assetManager->replacePrototypeTexture(m_skyBox, oldTexName.str(), newTextName.str());

	//Enable clamping on all textures used by the skybox (to reduce corner seams).
	if (m_skyBox && m_skyBox->Class_ID() == RenderObjClass::CLASSID_MESH)
	{
		MeshClass *mesh=(MeshClass*) m_skyBox;
		MaterialInfoClass	*material = mesh->Get_Material_Info();

		for (Int i=0; i<material->Texture_Count(); i++)
		{
			if (material->Peek_Texture(i))
			{
				material->Peek_Texture(i)->Get_Filter().Set_U_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_CLAMP);
				material->Peek_Texture(i)->Get_Filter().Set_V_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_CLAMP);
			}
		}
	}

}

//-------------------------------------------------------------------------------------------------
/** Adjusts various water/sky rendering settings that depend on time of day. */
//-------------------------------------------------------------------------------------------------
void WaterRenderObjClass::setTimeOfDay(TimeOfDay tod)
{
	m_tod=tod;
	if (m_waterType == WATER_TYPE_2_PVSHADER)
		generateVertexBuffer(PATCH_SIZE,PATCH_SIZE,sizeof(SEA_PATCH_VERTEX),true);	//update the water mesh with new lighting/alpha
}

//-------------------------------------------------------------------------------------------------
/**Copies GDF settings dealing with a particular time of day into our own
	* structures.  Also allocates any required W3D assets (textures). */
//-------------------------------------------------------------------------------------------------
void WaterRenderObjClass::loadSetting( Setting *setting, TimeOfDay timeOfDay )
{
	SurfaceClass::SurfaceDescription surfaceDesc;

	// sanity
	DEBUG_ASSERTCRASH( setting, ("WaterRenderObjClass::loadSetting, null setting") );

	// textures
	setting->skyTexture = WW3DAssetManager::Get_Instance()->Get_Texture( WaterSettings[ timeOfDay ].m_skyTextureFile.str() );
	setting->waterTexture = WW3DAssetManager::Get_Instance()->Get_Texture( WaterSettings[ timeOfDay ].m_waterTextureFile.str() );

	// texelss per unit
	setting->skyTexelsPerUnit = WaterSettings[ timeOfDay ].m_skyTexelsPerUnit;
	setting->waterTexture->Get_Level_Description( surfaceDesc, 0 );
	setting->skyTexelsPerUnit /= (Real)surfaceDesc.Width;

	// water repeat
	setting->waterRepeatCount = WaterSettings[ timeOfDay ].m_waterRepeatCount;

	// U and V scroll per ms
	setting->uScrollPerMs = WaterSettings[ timeOfDay ].m_uScrollPerMs;
	setting->vScrollPerMs = WaterSettings[ timeOfDay ].m_vScrollPerMs;

	//
	// vertex colors
	//
	// bottom left
	setting->vertex00Diffuse = (WaterSettings[ timeOfDay ].m_vertex00Diffuse.red << 16) |
														 (WaterSettings[ timeOfDay ].m_vertex00Diffuse.green << 8) |
														  WaterSettings[ timeOfDay ].m_vertex00Diffuse.blue;
	// top left
	setting->vertex01Diffuse = (WaterSettings[ timeOfDay ].m_vertex01Diffuse.red << 16) |
														 (WaterSettings[ timeOfDay ].m_vertex01Diffuse.green << 8) |
														  WaterSettings[ timeOfDay ].m_vertex01Diffuse.blue;
	// bottom right
	setting->vertex10Diffuse = (WaterSettings[ timeOfDay ].m_vertex10Diffuse.red << 16) |
														 (WaterSettings[ timeOfDay ].m_vertex10Diffuse.green << 8) |
														  WaterSettings[ timeOfDay ].m_vertex10Diffuse.blue;
	// top right
	setting->vertex11Diffuse = (WaterSettings[ timeOfDay ].m_vertex11Diffuse.red << 16) |
														 (WaterSettings[ timeOfDay ].m_vertex11Diffuse.green << 8) |
														  WaterSettings[ timeOfDay ].m_vertex11Diffuse.blue;

	// diffuse water color
	setting->waterDiffuse = (WaterSettings[ timeOfDay ].m_waterDiffuseColor.alpha << 24) |
												  (WaterSettings[ timeOfDay ].m_waterDiffuseColor.red		<< 16) |
													(WaterSettings[ timeOfDay ].m_waterDiffuseColor.green << 8) |
												   WaterSettings[ timeOfDay ].m_waterDiffuseColor.blue;

	// transparent water color
	setting->transparentWaterDiffuse = (WaterSettings[ timeOfDay ].m_transparentWaterDiffuse.alpha << 24) |
																		 (WaterSettings[ timeOfDay ].m_transparentWaterDiffuse.red	 << 16) |
																		 (WaterSettings[ timeOfDay ].m_transparentWaterDiffuse.green << 8) |
																		  WaterSettings[ timeOfDay ].m_transparentWaterDiffuse.blue;

}

//-------------------------------------------------------------------------------------------------
/** Our water may use effects that require run-time rendered textures.  These
	*	textures need to be updated before we start rendering to the main screen
	* render target because D3D doesn't multiple render targets. */
//-------------------------------------------------------------------------------------------------
void WaterRenderObjClass::updateRenderTargetTextures(CameraClass *cam)
{
	if (m_waterType == WATER_TYPE_2_PVSHADER && getClippedWaterPlane(cam, nullptr) &&
		TheTerrainRenderObject && TheTerrainRenderObject->getMap())
		renderMirror(cam);	//generate texture containing reflected scene
}

//-------------------------------------------------------------------------------------------------
/** Renders the reflected scene into an offscreen texture. */
//-------------------------------------------------------------------------------------------------
void WaterRenderObjClass::renderMirror(CameraClass *cam)
{
#ifdef EXTENDED_STATS
	if (DX8Wrapper::stats.m_disableWater) {
		return;
	}
#endif
	Matrix3D	OldCameraMatrix=cam->Get_Transform();
	Matrix4x4	FullMatrix4(cam->Get_Transform());	//copy 3x4 matrix into a 4x4
	Vector3		WaterNormal(0,0,1);	//normal of plane used for reflection
	Vector4		WaterPlane(WaterNormal.X,WaterNormal.Y,WaterNormal.Z,m_level);
	Vector3		rRight,rUp,rN,rPos;	//orientation and translation vectors of camera

	Matrix4x4	FullMatrix(FullMatrix4.Transpose());	//swap rows/columns

	//reflect camera right vector
	Real axis_distance=Vector3::Dot_Product((Vector3&)FullMatrix[0],WaterNormal);
	rRight = (Vector3&)FullMatrix[0] - (2.0f*axis_distance*WaterNormal);

	//reflect camera up vector
	axis_distance=Vector3::Dot_Product((Vector3&)FullMatrix[1],WaterNormal);
	rUp = (Vector3&)FullMatrix[1] - (2.0f*axis_distance*WaterNormal);

	//reflect camera n vector
	axis_distance=Vector3::Dot_Product((Vector3&)FullMatrix[2],WaterNormal);
	rN = (Vector3&)FullMatrix[2] - (2.0f*axis_distance*WaterNormal);

	//reflect camera position
	axis_distance=Vector3::Dot_Product((Vector3&)FullMatrix[3],WaterNormal);	//distance cam to origin
	axis_distance -= WaterPlane.W;	// subtract mirror plane distance to get distance camera to plane
	rPos = (Vector3&)FullMatrix[3] - (2.0f*axis_distance*WaterNormal);

	//generate a new camera matrix from reflected vectors
	Matrix3D reflectedTransform(rRight,rUp,rN,rPos);


	// D3D12: 256px reflection RT via Set_Render_Texture (not D3D8 surfaces).
	WW3D::Set_Render_Texture(m_pReflectionTexture);

	// Clear the backbuffer
	WW3D::Begin_Render(false,true,Vector3(0.0f,0.0f,0.0f));	//clearing only z-buffer since background always filled with clouds

	cam->Set_Transform( reflectedTransform );

	//Force reflected image to be drawn into full texture size - not a viewport inside texture.
	Vector2 vMin,vMax,vOldMax,vOldMin;
 	cam->Get_Viewport(vOldMin,vOldMax);
 	vMax.X=vMax.Y=1.0f;
	vMin.X=vMin.Y=0.0f;
 	cam->Set_Viewport(vMin,vMax);

	cam->Apply();	//force an update of all the camera dependent parameters like frustum clip planes

	//flip the winding order of polygons to draw the reflected back sides.
	ShaderClass::Invert_Backface_Culling(true);

	// Render the scene
	renderSky();
	if (m_tod == TIME_OF_DAY_NIGHT)
		renderSkyBody(&reflectedTransform);

	WW3D::Render(m_parentScene,cam);

	cam->Set_Transform(OldCameraMatrix);	//restore original non-reflected matrix
 	cam->Set_Viewport(vOldMin,vOldMax);

	cam->Apply();	//force an update of all the camera dependent parameters like frustum clip planes

	ShaderClass::Invert_Backface_Culling(false);

	WW3D::End_Render(false);

	// Change the rendertarget back to the main backbuffer
	WW3D::Set_Render_Texture(nullptr);
}

//-------------------------------------------------------------------------------------------------
/** Renders (draws) the water.
	*	Algorithm:
	*	Draw reflected scene.
	*	Draw reflected sky layer(s) and bodies.
	*	Clear Zbuffer
	*	Fill Zbuffer by drawing water surface (allows proper sorting into regular scene).
	*	Draw non-reflected scene (done in regular app render loop).
	*
	*	This algorithm doesn't apply to translucent water, which is rendered into a
	*   texture and rendered at end of scene. */
//-------------------------------------------------------------------------------------------------
//DECLARE_PERF_TIMER(Water)
void WaterRenderObjClass::Render(RenderInfoClass & rinfo)
{
	//USE_PERF_TIMER(Water)
	if (TheTerrainRenderObject && !TheTerrainRenderObject->getMap())
		return;	//no map has been loaded yet.

	if (((RTS3DScene *)rinfo.Camera.Get_User_Data())->getCustomPassMode() == SCENE_PASS_ALPHA_MASK ||
		((SceneClass *)rinfo.Camera.Get_User_Data())->Get_Extra_Pass_Polygon_Mode() == SceneClass::EXTRA_PASS_CLEAR_LINE)
		return;	//water is not drawn in wireframe or custom scene passes

#ifdef EXTENDED_STATS
	if (DX8Wrapper::stats.m_disableWater) {
		return;
	}
#endif
	if (ShaderClass::Is_Backface_Culling_Inverted())
		return;	//the water object will not reflect in itself, so don't do anything if rendering a mirror.

	//this water type needs to rendered after the rest of scene, so buffer it up for later

	// If static sort lists are enabled and this mesh has a sort level, put it on the list instead
	// of rendering it.
	unsigned int sort_level = (unsigned int)Get_Sort_Level();

	if (WW3D::Are_Static_Sort_Lists_Enabled() && sort_level != SORT_LEVEL_NONE)
	{
		WW3D::Add_To_Static_Sort_List(this, sort_level);
		return;
	}

	switch(m_waterType)
	{
		case WATER_TYPE_0_TRANSLUCENT:
		case WATER_TYPE_3_GRIDMESH:
			//Draw the water surface as a bunch of alpha blended tiles covering areas where water is visible
			renderWater();
			if (!m_drawingRiver || m_disableRiver) {
				renderWaterMesh();	//Draw water surface as 3D deforming mesh if it's enabled on this map.
			}
			break;

		case WATER_TYPE_2_PVSHADER:
			//Pixel/Vertex Shader based water which uses an off-screen rendered reflection texture
			drawSea(rinfo);	//draw water surface
			break;

		case WATER_TYPE_1_FB_REFLECTION:
			{
				//Normal frame buffer reflection water type. Non translucent.  Legacy code we're not using anymore.
				Matrix3D	OldCameraMatrix=rinfo.Camera.Get_Transform();
				Matrix4x4	FullMatrix4(rinfo.Camera.Get_Transform());	//copy 3x4 matrix into a 4x4
				Vector3		WaterNormal(0,0,1);	//normal of plane used for reflection
				Vector4		WaterPlane(WaterNormal.X,WaterNormal.Y,WaterNormal.Z,m_level);	//assume distance to origin 0
				Vector3		rRight,rUp,rN,rPos;	//orientation and translation vectors of camera

				Matrix4x4	FullMatrix(FullMatrix4.Transpose());	//swap rows/columns

				//reflect camera right vector
				Real axis_distance=Vector3::Dot_Product((Vector3&)FullMatrix[0],WaterNormal);
				rRight = (Vector3&)FullMatrix[0] - (2.0f*axis_distance*WaterNormal);

				//reflect camera up vector
				axis_distance=Vector3::Dot_Product((Vector3&)FullMatrix[1],WaterNormal);
				rUp = (Vector3&)FullMatrix[1] - (2.0f*axis_distance*WaterNormal);

				//reflect camera n vector
				axis_distance=Vector3::Dot_Product((Vector3&)FullMatrix[2],WaterNormal);
				rN = (Vector3&)FullMatrix[2] - (2.0f*axis_distance*WaterNormal);

				//reflect camera position
				axis_distance=Vector3::Dot_Product((Vector3&)FullMatrix[3],WaterNormal);	//distance cam to origin
				axis_distance -= WaterPlane.W;	// subtract mirror plane distance to get distance camera to plane
				rPos = (Vector3&)FullMatrix[3] - (2.0f*axis_distance*WaterNormal);

				//generate a new camera matrix from reflected vectors
				Matrix3D reflectedTransform(rRight,rUp,rN,rPos);

				//flip the winding order of polygons to draw the reflected back sides.
				ShaderClass::Invert_Backface_Culling(true);

			#ifdef CLIP_GEOMETRY_TO_PLANE
			  // Set a clip plane, so that only objects above the water are reflected
				WaterPlane.W *= -1.0f;	//flip sign of plane distance for D3D use.

			//	DX8Wrapper::Set_DX8_Clip_Plane( 0, &WaterPlane.X );
			//	DX8Wrapper::Set_DX8_Render_State(D3DRS_CLIPPLANEENABLE, D3DCLIPPLANE0 );	//turn on first clip plane

				// Alternate Clipping Method using alpha testing hack!
				/**************************************************************************************/

				//get current view matrix
				D3DMATRIX curView;
				DX8Wrapper::_Get_DX8_Transform(D3DTS_VIEW, curView);

				//get inverse of view matrix(= view to world matrix)
				Matrix4x4 inv;
				Real det = 0.0f;
				const Matrix4x4 neutralView = To_Matrix4x4(curView);
				Matrix4x4::Inverse(&inv, &det, &neutralView);

				//create clipping matrix by inserting our plane equation into the 1st column
				D3DMATRIX clipMatrix = {};
				clipMatrix.m[0][0]=WaterNormal.X;
				clipMatrix.m[1][0]=WaterNormal.Y;
				clipMatrix.m[2][0]=WaterNormal.Z;
				clipMatrix.m[3][0]=WaterPlane.W+0.5f;
				clipMatrix.m[1][1]=1.0f;
				clipMatrix.m[2][2]=1.0f;
				clipMatrix.m[3][3]=1.0f;
				const D3DMATRIX invClip = To_D3DMATRIX(To_Matrix4x4(clipMatrix) * inv);

				// Change texture wrapping mode to 'clamp' for texture stage 1
				DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
				DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);

				// Use CameraSpace vertices as input to matrix and use texture wrap mode from stage 1
				DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_CAMERASPACEPOSITION|1);
				// Two output coordinates are used.
				DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);

				// Set texture generation matrix for stage 1
				DX8Wrapper::_Set_DX8_Transform(D3DTS_TEXTURE1, invClip);

				// Disable bilinear filtering
				DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MINFILTER, D3DTEXF_POINT);
				DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_MAGFILTER, D3DTEXF_POINT);

				// Pass stage 0 texture data untouched(by modulating with white)
				DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_COLORARG1, D3DTA_TEXTURE );	//stage 1 texture
				DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_COLORARG2, D3DTA_CURRENT );	//previous stage texture
				DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_COLOROP,   D3DTOP_MODULATE );	//module with white => does nothing

				DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );	//stage 1 texture
				DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_ALPHAARG2, D3DTA_CURRENT );	//previous stage texture
				DX8Wrapper::Set_DX8_Texture_Stage_State( 1, D3DTSS_ALPHAOP,   D3DTOP_MODULATE );	//modulate with clipping texture

				DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHAREF,0x00);
				DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHAFUNC,D3DCMP_NOTEQUAL);	//pass pixels who's alpha is not zero
				DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHATESTENABLE, true);	//test pixels if transparent(clipped) before rendering.

				// Set clipping texture
				m_alphaClippingTexture->Set_U_Addr_Mode(TextureClass::TEXTURE_ADDRESS_CLAMP);
				m_alphaClippingTexture->Set_V_Addr_Mode(TextureClass::TEXTURE_ADDRESS_CLAMP);
				m_alphaClippingTexture->Set_Min_Filter(TextureClass::FILTER_TYPE_NONE);
				m_alphaClippingTexture->Set_Mag_Filter(TextureClass::FILTER_TYPE_NONE);
				m_alphaClippingTexture->Set_Mip_Mapping(TextureClass::FILTER_TYPE_NONE);

				DX8Wrapper::Set_Texture(0,m_alphaClippingTexture);

				//TODO: Will have to make sure that the shader system is not resetting my stage 1 setup
				//while rendering the scene

				/*************************************************************************************/
			#endif

			#if 0	// No longer do simple rendering.
				if (TheGlobalData->m_useWaterPlane)
				{
					//@todo : Would it be better to create a new camera or change the transform of the
					//existing one?
					rinfo.Camera.Set_Transform( reflectedTransform );
					rinfo.Camera.Apply();	//force an update of all the camera dependent parameters like frustum clip planes

					if(m_useCloudLayer)
					{
						if (TheGlobalData && TheGlobalData->m_drawEntireTerrain)
							m_skyBox->Render(rinfo);
						else
						{
							renderSky();
							if (m_tod == TIME_OF_DAY_NIGHT)
								renderSkyBody(&reflectedTransform);
						}
					}

					WW3D::Render(m_parentScene,&rinfo.Camera);

					rinfo.Camera.Set_Transform(OldCameraMatrix);	//restore original non-reflected matrix
					rinfo.Camera.Apply();	//force an update of all the camera dependent parameters like frustum clip planes

					//clear the z-buffer to remove changes made by objects inside mirror
					DX8Wrapper::Clear(false,true,Vector3(0.1f,0.1f,0.1f));
				}
			#endif

			#ifdef CLIP_GEOMETRY_TO_PLANE
				//restore default culling mode
			//	DX8Wrapper::Set_DX8_Render_State(D3DRS_CLIPPLANEENABLE, 0 );	//turn off first clip plane

				//disable texture coordinate generation
				DX8Wrapper::Set_DX8_Texture_Stage_State(1, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
				DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHATESTENABLE, false);	//disable alpha testing
			#endif

				ShaderClass::Invert_Backface_Culling(false);	//return culling back to normal

				ShaderClass::Invalidate();	//reset shading system so it forces full state set.

				renderWater();
			}
			break;

		default:
			break;
	}

	if (TheGlobalData && TheGlobalData->m_drawSkyBox)
	{	//center skybox around camera
		Vector3 pos=rinfo.Camera.Get_Position();
		pos.Z = TheGlobalData->m_skyBoxPositionZ;
		m_skyBox->Set_Position(pos);
		m_skyBox->Render(rinfo);
	}

	//Clean up after any pixel shaders.
	//Force backend state refresh so released textures drop their references.
	if (IRenderBackend *cleanupBackend = WW3D::Get_Render_Backend())
		cleanupBackend->Invalidate_Cached_Render_States();

	if (m_waterTrackSystem)
		m_waterTrackSystem->flush(rinfo);

//	renderWaterMesh();
//	renderWaterWave();
}

//-------------------------------------------------------------------------------------------------
/** Clips the water plane to the current camera frustum and returns a bounding
	* box enclosing the clipped plane.  Returns false if water plane is not visible. */
//-------------------------------------------------------------------------------------------------
Bool WaterRenderObjClass::getClippedWaterPlane(CameraClass *cam, AABoxClass *box)
{
	const FrustumClass & frustum = cam->Get_Frustum();

	ClipPolyClass	ClippedPoly0;
	ClipPolyClass	ClippedPoly1;

	///@todo: generate proper sized polygon
	ClippedPoly0.Reset();
	ClippedPoly0.Add_Vertex(Vector3(0,0,m_level));
	ClippedPoly0.Add_Vertex(Vector3(0,m_dy,m_level));
	ClippedPoly0.Add_Vertex(Vector3(m_dx,m_dy,m_level));
	ClippedPoly0.Add_Vertex(Vector3(m_dx,0,m_level));

	//clip against all 6 frustum planes
	ClippedPoly0.Clip(frustum.Planes[0],ClippedPoly1);
	ClippedPoly1.Clip(frustum.Planes[1],ClippedPoly0);
	ClippedPoly0.Clip(frustum.Planes[2],ClippedPoly1);
	ClippedPoly1.Clip(frustum.Planes[3],ClippedPoly0);
	ClippedPoly0.Clip(frustum.Planes[4],ClippedPoly1);
	ClippedPoly1.Clip(frustum.Planes[5],ClippedPoly0);

	Int final_vcount = ClippedPoly0.Verts.Count();

	//make sure the polygon is visible
	if (final_vcount >= 3)
	{
		//find axis aligned bounding box around visible polygon
		if (box)
  			box->Init(&(ClippedPoly0.Verts[0]),final_vcount);
		return TRUE;
	}

	return FALSE;	//water plane is not visible
}

//-------------------------------------------------------------------------------------------------
/** Draws the water surface using a custom D3D vertex/pixel shader and a
	* reflection texture.  Only tested to work on GeForce3. */
//-------------------------------------------------------------------------------------------------
void WaterRenderObjClass::drawSea(RenderInfoClass & rinfo)
{
	AABoxClass	seaBox;

	if (!getClippedWaterPlane(&rinfo.Camera,&seaBox))
		return;	//the sea is not visible

	// D3D12: bump-envmap / reflection-perturb / wave vertex-shader path has no
	// backend equivalent. CPU clip + patch math below is preserved; base patch
	// geometry is submitted with the closest alpha material (no fake
	// perturb/mirror). The inline wave .pso/.vso asm stays as archival
	// reference in ReAcquireResources.
	Matrix4x4 matWW3D(true);
	matWW3D[1][1] = 0.0f;
	matWW3D[2][2] = 0.0f;

	//create a transform which will flip the y and z coordinates to fit our system
	matWW3D[0][0]=1.0f;
	matWW3D[2][1]=1.0f;
	matWW3D[1][2]=1.0f;
	matWW3D[3][3]=1.0f;

	IRenderBackend *seaBackend = WW3D::Get_Render_Backend();
	if (seaBackend == nullptr)
		return;
	RenderBackendMaterialState seaMaterial;
	if (!zFillAlphaShader.Get_Render_Backend_State(seaMaterial))
		return;
	seaMaterial.cull = RenderBackendCullMode::None;
	const RenderBackendTextureHandle seaTexture; // untextured: no fake mirror.

	Vector3 camTran;

	rinfo.Camera.Get_Transform().Get_Translation(&camTran);
	(void)camTran;
	(void)matWW3D;

	// D3D12: fixed-function stage states retired; seaMaterial above carries
	// the closest alpha blend state (SRCALPHA/INVSRCALPHA, no depth write
	// change). Bump address wrap + bump-scale live CPU-side (m_fBumpScale).

	// D3D12: bump binding + BUMPENV matrix + texture-project constants retired.
	// m_fBumpFrame/m_fBumpScale advance CPU-side in update(); the neutral RGBA8
	// bump upload (initBumpMap) is bound by future material work, not faked here.

	// D3D12: stage disable / depth-write / texture-projector constants retired.
	// The wave vertex/pixel shaders (CV_TEXPROJ_0/CV_ZERO/CV_ONE) stay archival.

	Int patchX,patchY,startX,startY;

	// Patch placement preserved CPU-side (PATCH_SCALE grid over the clipped box).
	const float patchOriginX0 = (seaBox.Center.X-seaBox.Extent.X)/(PATCH_WIDTH*PATCH_SCALE);
	const float patchOriginY0 = (seaBox.Center.Y-seaBox.Extent.Y)/(PATCH_WIDTH*PATCH_SCALE);
	(void)patchOriginX0; (void)patchOriginY0;

	for (startY=patchY=(seaBox.Center.Y-seaBox.Extent.Y)/(PATCH_WIDTH*PATCH_SCALE); (patchY*PATCH_WIDTH*PATCH_SCALE)<(seaBox.Center.Y+seaBox.Extent.Y); patchY++)
	{
		for (startX=patchX=(seaBox.Center.X-seaBox.Extent.X)/(PATCH_WIDTH*PATCH_SCALE); (patchX*PATCH_WIDTH*PATCH_SCALE)<(seaBox.Center.X+seaBox.Extent.X); patchX++)
		{
			// CPU WVP math preserved (no backend shader-constant equivalent).
			Matrix4x4 patchMatrix(true);
			patchMatrix[0][0]=PATCH_SCALE;
			patchMatrix[2][2]=PATCH_SCALE;
			patchMatrix[3][0]=(float)(patchX*PATCH_WIDTH*PATCH_SCALE);
			patchMatrix[3][2]=(float)(patchY*PATCH_WIDTH*PATCH_SCALE);
			Matrix4x4 matWorldViewProj;
			Matrix4x4 matView(true), matProj(true);
			Build_Water_World_View_Projection_Constants(
				matWorldViewProj, patchMatrix, matWW3D, matView, matProj);
			(void)matWorldViewProj;

			if (m_patchVertices.empty() || m_patchIndices.empty())
				continue;
			std::vector<RenderBackendTexturedVertex> seaVerts;
			seaVerts.reserve(m_patchVertices.size());
			const float patchOffsetX = (float)(patchX*PATCH_WIDTH*PATCH_SCALE);
			const float patchOffsetZ = (float)(patchY*PATCH_WIDTH*PATCH_SCALE);
			for (size_t pv = 0; pv < m_patchVertices.size(); ++pv) {
				RenderBackendTexturedVertex sv = m_patchVertices[pv];
				// Patch grid stores (x, level, z); translate to world placement.
				sv.x = m_patchVertices[pv].x * PATCH_SCALE + patchOffsetX;
				sv.z = m_patchVertices[pv].z * PATCH_SCALE + patchOffsetZ;
				seaVerts.push_back(sv);
			}
			seaBackend->Draw_Indexed_Material_Triangles(seaVerts.data(),
				static_cast<unsigned int>(seaVerts.size()),
				m_patchIndices.data(), static_cast<unsigned int>(m_patchIndices.size()),
				seaTexture, seaMaterial);
		}
	}
	seaBackend->Invalidate_Cached_Render_States();

	if (TheTerrainRenderObject != nullptr && TheTerrainRenderObject->getShroud() != nullptr)
	{
		// GAP (documented): the shroud second pass (ST_SHROUD_TEXTURE stage-0
		// projection over the same patches) has no backend UV-projection
		// equivalent yet. Base water above is submitted unmodulated rather than
		// double-drawn; CPU shroud levels remain authoritative (see
		// getRiverVertexDiffuse and W3DShroud).
	}

}


#define FEATHER_LAYER_COUNT (5.0f)
#define FEATHER_THICKNESS   (4.0f)

//-------------------------------------------------------------------------------------------------
/** Renders (draws) the water surface.*/
//-------------------------------------------------------------------------------------------------
void WaterRenderObjClass::renderWater()
{
	for (PolygonTrigger *pTrig=PolygonTrigger::getFirstPolygonTrigger(); pTrig; pTrig = pTrig->getNext()) {
		if (pTrig->isWaterArea()) {
			if (pTrig->getNumPoints()>2) {
				if (pTrig->isRiver()) {
					drawRiverWater(pTrig);
					continue;
				}
				Int k;
				for (k=1; k<pTrig->getNumPoints()-1; k=k+2) {
					ICoord3D pt3 = *pTrig->getPoint(0);
					ICoord3D pt2 = *pTrig->getPoint(k);
					ICoord3D pt1 = *pTrig->getPoint(k+1);
					ICoord3D pt0 = *pTrig->getPoint(k+1);
					if (k+2<pTrig->getNumPoints()) {
						pt0 = *pTrig->getPoint(k+2);
					}
					Vector3 points[4];
					points[0].Set(pt0.x, pt0.y, pt0.z);
					points[1].Set(pt1.x, pt1.y, pt1.z);
					points[2].Set(pt2.x, pt2.y, pt2.z);
					points[3].Set(pt3.x, pt3.y, pt3.z);

					if ( TheGlobalData->m_featherWater )
					{
						for (int r = 0; r < TheGlobalData->m_featherWater; ++r)
						{
							drawTrapezoidWater(points);
							points[0].Z += (FEATHER_THICKNESS/TheGlobalData->m_featherWater);
						}
					}

					else
						drawTrapezoidWater(points);


				}
			}
		}
	}

}

//-------------------------------------------------------------------------------------------------
/** Renders (draws) the sky plane.  Will apply current time-of-day settings including
	* some simple UV scrolling animation. */
//-------------------------------------------------------------------------------------------------
void WaterRenderObjClass::renderSky()
{
	Int timeNow,timeDiff;
	Real fu,fv;

	Setting *setting=&m_settings[m_tod];

	timeNow=timeGetTime();

	timeDiff=timeNow-m_LastUpdateTime;
	m_LastUpdateTime=timeNow;

	m_uOffset += timeDiff * setting->uScrollPerMs * setting->skyTexelsPerUnit;
	m_vOffset += timeDiff * setting->vScrollPerMs * setting->skyTexelsPerUnit;

	//clamp uv coordinate into 0,1 range
	m_uOffset = m_uOffset - (Real)((Int) m_uOffset);
	m_vOffset = m_vOffset - (Real)((Int) m_vOffset);

	fu= m_uOffset + (SKYPLANE_SIZE * 2) * setting->skyTexelsPerUnit;
	fv= m_vOffset + (SKYPLANE_SIZE * 2) * setting->skyTexelsPerUnit;


	// D3D12: sky plane quad via Draw_Indexed_Material_Triangles. Scroll,
	// per-vertex diffuse, and opaque/no-depth-write intent preserved via
	// Get_Render_Backend_State (PASS_ALWAYS, cull disabled).
	IRenderBackend *skyBackend = WW3D::Get_Render_Backend();
	if (skyBackend == nullptr)
		return;
	ShaderClass skyShader=ShaderClass::_PresetOpaqueShader;
	skyShader.Set_Cull_Mode(ShaderClass::CULL_MODE_DISABLE);
	skyShader.Set_Depth_Compare(ShaderClass::PASS_ALWAYS);	//no need to check against z-buffer, sky always rendered first.
	skyShader.Set_Depth_Mask(ShaderClass::DEPTH_WRITE_DISABLE);	//sky is always behind everything so no need to update z-buffer
	RenderBackendMaterialState skyMaterial;
	if (!skyShader.Get_Render_Backend_State(skyMaterial))
		return;
	RenderBackendTextureHandle skyHandle;
	if (setting->skyTexture != nullptr) {
		const TextureFilterClass &skyFilter = setting->skyTexture->Get_Filter();
		if (!skyFilter.Get_Render_Sampler(skyMaterial.sampler))
			return;
		skyMaterial.clamp_texture = skyFilter.Get_U_Addr_Mode() == TextureFilterClass::TEXTURE_ADDRESS_CLAMP;
		if (!setting->skyTexture->Ensure_Renderer_Texture())
			return;
		skyHandle = setting->skyTexture->Get_Renderer_Texture();
	}

	//draw an infinite sky plane
	struct SkyCpuVertex { float x, y, z; unsigned int diffuse; float u, v; };
	SkyCpuVertex skyVerts[4];
	{
		skyVerts[0].x=-SKYPLANE_SIZE;
		skyVerts[0].y=SKYPLANE_SIZE;
		skyVerts[0].z=SKYPLANE_HEIGHT;
		skyVerts[0].u=m_uOffset;
		skyVerts[0].v=fv;
		skyVerts[0].diffuse=setting->vertex01Diffuse;

		skyVerts[1].x=SKYPLANE_SIZE;
		skyVerts[1].y=SKYPLANE_SIZE;
		skyVerts[1].z=SKYPLANE_HEIGHT;
		skyVerts[1].u=fu;
		skyVerts[1].v=fv;
		skyVerts[1].diffuse=setting->vertex11Diffuse;

		skyVerts[2].x=SKYPLANE_SIZE;
		skyVerts[2].y=-SKYPLANE_SIZE;
		skyVerts[2].z=SKYPLANE_HEIGHT;
		skyVerts[2].u=fu;
		skyVerts[2].v=m_vOffset;
		skyVerts[2].diffuse=setting->vertex10Diffuse;

		skyVerts[3].x=-SKYPLANE_SIZE;
		skyVerts[3].y=-SKYPLANE_SIZE;
		skyVerts[3].z=SKYPLANE_HEIGHT;
		skyVerts[3].u=m_uOffset;
		skyVerts[3].v=m_vOffset;
		skyVerts[3].diffuse=setting->vertex00Diffuse;
	}

	RenderBackendTexturedVertex skyBackendVerts[4];
	for (int skyVert = 0; skyVert < 4; ++skyVert) {
		skyBackendVerts[skyVert].x = skyVerts[skyVert].x;
		skyBackendVerts[skyVert].y = skyVerts[skyVert].y;
		skyBackendVerts[skyVert].z = skyVerts[skyVert].z;
		const UnsignedInt skyDiffuse = skyVerts[skyVert].diffuse;
		skyBackendVerts[skyVert].a = ((skyDiffuse >> 24) & 255) / 255.0f;
		skyBackendVerts[skyVert].r = ((skyDiffuse >> 16) & 255) / 255.0f;
		skyBackendVerts[skyVert].g = ((skyDiffuse >> 8) & 255) / 255.0f;
		skyBackendVerts[skyVert].b = (skyDiffuse & 255) / 255.0f;
		skyBackendVerts[skyVert].u = skyVerts[skyVert].u;
		skyBackendVerts[skyVert].v = skyVerts[skyVert].v;
		skyBackendVerts[skyVert].q = 1.0f;
	}

	skyBackend->Draw_Indexed_Material_Triangles(skyBackendVerts, 4,
		m_quadIndices.data(), static_cast<unsigned int>(m_quadIndices.size()),
		skyHandle, skyMaterial);	//draw a quad, 2 triangles, 4 verts
}

//-------------------------------------------------------------------------------------------------
/** Renders (draws) the sky body.  Used for moon and sun.  We rotate the image
	* so that it always faces the camera.  This removes perspective and helps hide that
	* it's a flat image. */
//-------------------------------------------------------------------------------------------------
///	@todo: Add code to render properly sorted sun sky body.
void WaterRenderObjClass::renderSkyBody(Matrix3D *mat)
{
	Vector3 cPos;

	Vector3 pView,pRight,pUp,pPos(SKYBODY_X,SKYBODY_Y,SKYBODY_HEIGHT);

	mat->Get_Translation(&cPos);

	pView=cPos-pPos;	//billboard to camera
	pView.Normalize();	//particle view direction

	Vector3 WorldUp(0,0,-1);	///@todo: hacked so only works for reflections across xy plane

#ifdef ALLOW_TEMPORARIES
	Vector3 rotAxis=Vector3::Cross_Product(WorldUp,pView);	//get axis of rotation.
	rotAxis.Normalize();
#else
	Vector3 rotAxis;
	Vector3::Normalized_Cross_Product(WorldUp, pView, &rotAxis);
#endif

	Real angle=Vector3::Dot_Product(WorldUp,pView);

	angle = acos(angle);


	Matrix3D tm(1);
	tm.Set(rotAxis,angle);
	tm.Adjust_Translation(Vector3(SKYBODY_X,SKYBODY_Y,SKYBODY_HEIGHT));


	// D3D12: billboard math above preserved; quad corners are transformed
	// CPU-side by tm (the legacy WORLD transform) and submitted via
	// Draw_Indexed_Material_Triangles with the alpha/no-depth-write material.
	IRenderBackend *bodyBackend = WW3D::Get_Render_Backend();
	if (bodyBackend == nullptr)
		return;
	ShaderClass bodyShader=ShaderClass::_PresetAlphaShader;
	bodyShader.Set_Cull_Mode(ShaderClass::CULL_MODE_DISABLE);
	bodyShader.Set_Depth_Compare(ShaderClass::PASS_ALWAYS);	//no need to check against z-buffer, sky always rendered first.
	bodyShader.Set_Depth_Mask(ShaderClass::DEPTH_WRITE_DISABLE);	//sky is always behind everything so no need to update z-buffer
	RenderBackendMaterialState bodyMaterial;
	if (!bodyShader.Get_Render_Backend_State(bodyMaterial))
		return;
	RenderBackendTextureHandle bodyHandle;
	if (m_alphaClippingTexture != nullptr) {
		const TextureFilterClass &bodyFilter = m_alphaClippingTexture->Get_Filter();
		if (!bodyFilter.Get_Render_Sampler(bodyMaterial.sampler))
			return;
		bodyMaterial.clamp_texture = bodyFilter.Get_U_Addr_Mode() == TextureFilterClass::TEXTURE_ADDRESS_CLAMP;
		if (!m_alphaClippingTexture->Ensure_Renderer_Texture())
			return;
		bodyHandle = m_alphaClippingTexture->Get_Renderer_Texture();
	}

	//draw an infinite sky plane
	struct SkyBodyCpuVertex { float x, y, z; float u, v; };
	const SkyBodyCpuVertex bodyLocal[4] = {
		{ -SKYBODY_SIZE, SKYBODY_SIZE, 0, 0, 1 },
		{ SKYBODY_SIZE, SKYBODY_SIZE, 0, 1, 1 },
		{ SKYBODY_SIZE, -SKYBODY_SIZE, 0, 1, 0 },
		{ -SKYBODY_SIZE, -SKYBODY_SIZE, 0, 0, 0 },
	};

	RenderBackendTexturedVertex bodyVerts[4];
	for (int bodyVert = 0; bodyVert < 4; ++bodyVert) {
		Vector3 local(bodyLocal[bodyVert].x, bodyLocal[bodyVert].y, bodyLocal[bodyVert].z);
		Vector3 world;
		Matrix3D::Transform_Vector(tm, local, &world);
		bodyVerts[bodyVert].x = world.X;
		bodyVerts[bodyVert].y = world.Y;
		bodyVerts[bodyVert].z = world.Z;
		bodyVerts[bodyVert].r = 1.0f;
		bodyVerts[bodyVert].g = 1.0f;
		bodyVerts[bodyVert].b = 1.0f;
		bodyVerts[bodyVert].a = 1.0f;
		bodyVerts[bodyVert].u = bodyLocal[bodyVert].u;
		bodyVerts[bodyVert].v = bodyLocal[bodyVert].v;
		bodyVerts[bodyVert].q = 1.0f;
	}

	bodyBackend->Draw_Indexed_Material_Triangles(bodyVerts, 4,
		m_quadIndices.data(), static_cast<unsigned int>(m_quadIndices.size()),
		bodyHandle, bodyMaterial);	//draw a quad, 2 triangles, 4 verts
}

//Defines for procedural water animation.
#define WATER_FREQ	(2.0*3.2831/4.0)	//2pi (full cycle) cover 4 units
#define WATER_AMP	(1.0f)
#define	WATER_OFFSET (0.1f)

//-------------------------------------------------------------------------------------------------
/** Renders (draws) the water surface mesh geometry.
	*	This is a work-in-progress!  Do not use this code! */
//-------------------------------------------------------------------------------------------------
void WaterRenderObjClass::renderWaterMesh()
{

	if (!m_doWaterGrid)
		return;	//the water grid is disabled.

	// D3D12: CPU grid batch rebuilt every frame (no DISCARD hack).

	Setting *setting=&m_settings[m_tod];

	WaterMeshData *pData;
	Int	mx=m_gridCellsX+1;
	Int my=m_gridCellsY+1;
	Int i,j;

	Real cellSizeX=m_gridCellSize;
	Real cellSizeY=m_gridCellSize;
//	Real	uScale2=5.0f*setting->waterRepeatCount/(128.0f)*cellSizeX/10.0f;
//	Real	vScale2=5.0f*setting->waterRepeatCount/(128.0f)*cellSizeY/10.0f;

	//Old waterRepeatCount settings in INI were based on 128x128 water grid of cellsize=10
	//Scale values to correct size.
	Real	uScale=setting->waterRepeatCount/(128.0f)*cellSizeX/10.0f*0.2f;
	Real	vScale=setting->waterRepeatCount/(128.0f)*cellSizeY/10.0f*0.2f;

	Vector3	nx(cellSizeX*2.0f,0,0);
	Vector3 ny(0,cellSizeY*2.0f,0);
	Vector3 C;

#ifdef DO_WATER_SIMULATION		//Debug code used to create a dummy water animation
	//
	// Mark: If you re-enable this water simulation, you might want to consider moving
	// this code to the update() method of the water render object (Colin)
	//

	static Real PhasePerFrameX=0.1f;
	static Real PhasePerFrameY=0.1f;

	//update the mesh heights for this frame (update buffer is 2 samples wider/taller due to border)
	for (j=0,pData=m_meshData; j<(my+2); j++)
	{
		for (i=0; i<(mx+2); i++)
		{
			//*pData = WATER_AMP * sin(WATER_FREQ*(0.7f*i + 0.7f*j) - PhasePerFrame);

			pData->height=WATER_OFFSET+WATER_AMP*(sin((float)i*WATER_FREQ*0.4+PhasePerFrameX*0.5)+sin((float)i*WATER_FREQ*0.6+PhasePerFrameX*0.2)+sin((float)j*WATER_FREQ+PhasePerFrameX)+sin((float)j*WATER_FREQ*0.7+PhasePerFrameX*0.3));
//			*pData=WATER_OFFSET+WATER_AMP*(sin((float)i*WATER_FREQ*0.4+PhasePerFrameX*0.5)+sin((float)i*WATER_FREQ*0.6+PhasePerFrameX*0.2)+sin((float)j*WATER_FREQ+PhasePerFrameX)+sin((float)j*WATER_FREQ*0.7+PhasePerFrameX*0.3));
			pData++;
		}
	}

	PhasePerFrameX -= 0.08f;
	PhasePerFrameY -= 0.1f;
#endif

	// D3D12: grid verts are built on the CPU (position + water diffuse/alpha +
	// scrolled u1/v1; u2/v2 bump coords have no single-texture material
	// equivalent and are preserved CPU-side only, not faked). Normals feed the
	// legacy lighting comment below; the backend shades via vertex color.
	struct WaterGridCpuVertex { float x, y, z; unsigned int diffuse; float u, v; };
	std::vector<WaterGridCpuVertex> gridCpu;
	gridCpu.reserve(static_cast<size_t>(mx) * my);
	Int diffuse;
	diffuse = setting->waterDiffuse&0x00ffffff;
	Int alpha = (setting->waterDiffuse & 0xff000000)>>24;
	// Reduce alpha for wave mesh
	alpha -= 0x20;
	diffuse |= alpha<<24;

	//I pulled some of these constants out of the loops for speed:
	Real uvCosScale=0.02*cos(3*m_riverVOrigin);
	Real sinOffset=25*m_riverVOrigin;
	Real originScale=m_riverVOrigin/vScale;
	Real bumpSizeDiv=cellSizeY/BUMP_SIZE;
	Real bumpSizeDiv2=0.3f*cellSizeY/BUMP_SIZE;

	//Data has a 1 vertex padding all around it so we don't need to special-case edges.  Improves performance
	for (j=0,pData=m_meshData+mx+2+1; j<my; j++,pData+=2)	//skip 2 horizontal border samples after each row
	{
		Real y=(float)j*cellSizeY;
		Real v1Offset=m_riverVOrigin+(float)j*vScale + uvCosScale*WWMath::Fast_Sin(sinOffset+y*PI/(8*MAP_XY_FACTOR));
		Real v2Offset=((float)j+originScale)*bumpSizeDiv + (float)j*bumpSizeDiv2;

		for (i=0; i<mx; i++)
		{
			//compute normal by looking at 4 vertex neightbors
#ifdef USE_MESH_NORMALS
			nx.Z=(pData+1)->height - (pData-1)->height;
			ny.Z=(pData+mx+2)->height - (pData-mx-2)->height;
			Vector3::Cross_Product(nx,ny,&C);
			C.Normalize();
			// D3D12: normal kept CPU-side (legacy lighting reference); the
			// backend shades this grid via vertex color (PRELIT-style).
			(void)C;
#endif
			Real x = (float)i*cellSizeX;
			WaterGridCpuVertex gv;
			gv.x=	x;
			gv.y=	y;
			gv.z=  pData->height;

			gv.diffuse = static_cast<unsigned int>(diffuse);
#ifdef SCROLL_UV
			gv.u=(float)i*uScale;
			gv.v=v1Offset;
#else
			gv.u=(float)i*uScale;
			gv.v=(float)j*vScale;
#endif
			// u2/v2 bump coords preserved CPU-side only (see note above).
			gridCpu.push_back(gv);
			pData++;
		}
	}

	// D3D12: world placement via the object transform (legacy WORLD transform),
	// submitted as world-space indexed triangles with the water material.
	const Matrix3D &meshWorldTm = Get_Transform();	//position the water surface

	ShaderClass::CullModeType oldCullMode=m_shaderClass.Get_Cull_Mode();

	ShaderClass::DepthMaskType oldDepthMask=m_shaderClass.Get_Depth_Mask();
	m_shaderClass.Set_Depth_Mask(ShaderClass::DEPTH_WRITE_DISABLE);	//disable writing to z-buffer to prevent particle clipping.

	m_shaderClass.Set_Cull_Mode(ShaderClass::CULL_MODE_ENABLE);	//water should be visible from both sides

	RenderBackendMaterialState meshMaterial;
	const Bool meshMaterialValid = m_shaderClass.Get_Render_Backend_State(meshMaterial);
	setupFlatWaterShader();
	if (meshMaterialValid) {
		RenderBackendTextureHandle meshTexture;
		if (setting->waterTexture != nullptr) {
			const TextureFilterClass &meshFilter = setting->waterTexture->Get_Filter();
			if (meshFilter.Get_Render_Sampler(meshMaterial.sampler))
				meshMaterial.clamp_texture = false;
			if (setting->waterTexture->Ensure_Renderer_Texture())
				meshTexture = setting->waterTexture->Get_Renderer_Texture();
		}
		if (IRenderBackend *meshBackend = WW3D::Get_Render_Backend()) {
			std::vector<RenderBackendTexturedVertex> meshVerts;
			meshVerts.reserve(gridCpu.size());
			for (size_t gvIdx = 0; gvIdx < gridCpu.size(); ++gvIdx) {
				Vector3 local(gridCpu[gvIdx].x, gridCpu[gvIdx].y, gridCpu[gvIdx].z);
				Vector3 world;
				Matrix3D::Transform_Vector(meshWorldTm, local, &world);
				RenderBackendTexturedVertex mv;
				mv.x = world.X; mv.y = world.Y; mv.z = world.Z;
				const UnsignedInt mvDiffuse = gridCpu[gvIdx].diffuse;
				mv.a = ((mvDiffuse >> 24) & 255) / 255.0f;
				mv.r = ((mvDiffuse >> 16) & 255) / 255.0f;
				mv.g = ((mvDiffuse >> 8) & 255) / 255.0f;
				mv.b = (mvDiffuse & 255) / 255.0f;
				mv.u = gridCpu[gvIdx].u; mv.v = gridCpu[gvIdx].v; mv.q = 1.0f;
				meshVerts.push_back(mv);
			}
			std::vector<unsigned short> meshIndices;
			meshIndices.reserve(static_cast<size_t>(mx - 1) * (my - 1) * 6);
			for (Int meshZ = 0; meshZ < my - 1; ++meshZ) {
				for (Int meshX = 0; meshX < mx - 1; ++meshX) {
					const unsigned short meshA = static_cast<unsigned short>(meshZ * mx + meshX);
					const unsigned short meshB = static_cast<unsigned short>(meshZ * mx + meshX + 1);
					const unsigned short meshC = static_cast<unsigned short>((meshZ + 1) * mx + meshX);
					const unsigned short meshD = static_cast<unsigned short>((meshZ + 1) * mx + meshX + 1);
					meshIndices.push_back(meshA);
					meshIndices.push_back(meshC);
					meshIndices.push_back(meshB);
					meshIndices.push_back(meshB);
					meshIndices.push_back(meshC);
					meshIndices.push_back(meshD);
				}
			}
			if (!meshVerts.empty() && !meshIndices.empty())
				meshBackend->Draw_Indexed_Material_Triangles(meshVerts.data(),
					static_cast<unsigned int>(meshVerts.size()),
					meshIndices.data(), static_cast<unsigned int>(meshIndices.size()),
					meshTexture, meshMaterial);
			Debug_Statistics::Record_Rendered_Polys_And_Vertices(
				static_cast<Int>(meshIndices.size()),mx*my,ShaderClass::_PresetOpaqueShader);
			// GAP: shroud stage-1 modulate over the mesh has no backend
			// UV-projection equivalent yet; mesh submitted unmodulated.
			meshBackend->Invalidate_Cached_Render_States();
		}
	}

	m_shaderClass.Set_Cull_Mode(oldCullMode);	//water should be visible from both sides

	// restore shader to old mask
	m_shaderClass.Set_Depth_Mask(oldDepthMask);

}

inline void WaterRenderObjClass::setGridVertexHeight(Int x, Int y, Real value)
{
	DEBUG_ASSERTCRASH( x < (m_gridCellsX+1) && y < (m_gridCellsY+1), ("Invalid Water Mesh Coordinates") );

	if (m_meshData)
	{
		m_meshData[(y+1)*(m_gridCellsX+1+2)+x+1].height = value;
	}
}

void WaterRenderObjClass::setGridHeightClamps(Real minz, Real maxz)
{
	m_minGridHeight = minz;
	m_maxGridHeight = maxz;
}

void WaterRenderObjClass::addVelocity( Real worldX, Real worldY,
																			 Real zVelocity, Real preferredHeight )
{

	if( m_doWaterGrid)
	{
		Real gx,gy;
		Real minX,maxX,minY,maxY;
		Int x,y;
		WaterMeshData *meshPoint;
		m_disableRiver = true;

		//check if center falls within grid bounds
		if (worldToGridSpace(worldX, worldY, gx, gy))
		{

			//find extents of influence
			minX = floorf(gx - m_gridChangeMaxRange);
			if (minX < 0 )
				minX = 0;	//clamp extent to fall within box
			maxX = ceilf(gx + m_gridChangeMaxRange);
			if (maxX > m_gridCellsX)
				maxX = m_gridCellsX;	//clamp extent to fall within box

			minY = floorf(gy - m_gridChangeMaxRange);
			if (minY < 0 )
				minY = 0;	//clamp extent to fall within box
			maxY = ceilf(gy + m_gridChangeMaxRange);
			if (maxY > m_gridCellsY)
				maxY = m_gridCellsY;	//clamp extent to fall within box

			for (y=minY; y<=maxY; y++)
			{
				for (x=minX; x<=maxX; x++)
				{

					// get the mesh point that we're concerned with
					meshPoint = &m_meshData[ (y + 1) * (m_gridCellsX + 1 + 2) + x + 1 ];

					// we now have a new preferred height
					meshPoint->preferredHeight = preferredHeight;

					//
					// set the velocity of this point based on the distance from the center of the
					// "core" point for this call
					//
					meshPoint->velocity = meshPoint->velocity + zVelocity;

					// this point is now "in motion"
					BitSet( meshPoint->status, WaterRenderObjClass::IN_MOTION );

				}
			}

			//
			// the mesh data is now dirty, we need to pass through the velocity field
			// during an update phase to update the positions
			//
			m_meshInMotion = TRUE;

		}

	}

}

void WaterRenderObjClass::changeGridHeight(Real wx, Real wy, Real delta)
{
	Real gx,gy;
	Real *oldData;
	Real newData;
	Real distance;
	Real minX,maxX,minY,maxY;
	Int x,y;

	//check if center falls within grid bounds
	if (worldToGridSpace(wx, wy, gx, gy))
	{	//find extents of influence
		minX = floorf(gx - m_gridChangeMaxRange);
		if (minX < 0 )
			minX = 0;	//clamp extent to fall within box
		maxX = ceilf(gx + m_gridChangeMaxRange);
		if (maxX > m_gridCellsX)
			maxX = m_gridCellsX;	//clamp extent to fall within box

		minY = floorf(gy - m_gridChangeMaxRange);
		if (minY < 0 )
			minY = 0;	//clamp extent to fall within box
		maxY = ceilf(gy + m_gridChangeMaxRange);
		if (maxY > m_gridCellsY)
			maxY = m_gridCellsY;	//clamp extent to fall within box

		for (y=minY; y<=maxY; y++)
		{
			for (x=minX; x<=maxX; x++)
			{	oldData = &m_meshData[(y+1)*(m_gridCellsX+1+2)+x+1].height;
				distance = (gx - (Real)x)*(gx - (Real)x) + (gy - (Real)y)*(gy - (Real)y);
				distance = sqrt(distance);
				newData = *oldData + 1.0f/(m_gridChangeAtt0+m_gridChangeAtt1*distance+distance*distance*m_gridChangeAtt2)*delta;
				//Clamp to min/max values
				if (newData < m_minGridHeight)
					newData = m_minGridHeight;
				if (newData > m_maxGridHeight)
					newData = m_maxGridHeight;
				*oldData = newData;
			}
		}
	}
}

void WaterRenderObjClass::setGridChangeAttenuationFactors(Real a, Real b, Real c, Real range)
{
	m_gridChangeAtt0 = a;
	m_gridChangeAtt1 = b;
	m_gridChangeAtt2 = c;
	m_gridChangeMaxRange = range/m_gridCellSize;	//convert range to grid space
}

void WaterRenderObjClass::setGridTransform(Real angle, Real x, Real y, Real z)
{
	m_gridDirectionX = Vector2(1.0f,0.0f);

	m_gridOrigin.X = x;
	m_gridOrigin.Y = y;

	Matrix3D xform(1);
	xform.Rotate_Z(angle);

	m_gridDirectionX.X = xform.Get_X_Vector().X;
	m_gridDirectionX.Y = xform.Get_X_Vector().Y;

	m_gridDirectionY.X = xform.Get_Y_Vector().X;
	m_gridDirectionY.Y = xform.Get_Y_Vector().Y;

	xform.Set_Translation(Vector3(x,y,z));
	Set_Transform(xform);
}

void WaterRenderObjClass::setGridTransform(const Matrix3D *transform )
{

	if( transform )
		Set_Transform( *transform );

}

void WaterRenderObjClass::getGridTransform(Matrix3D *transform )
{

	if( transform )
		*transform = Get_Transform();

}

void WaterRenderObjClass::setGridResolution(Real gridCellsX, Real gridCellsY, Real cellSize)
{
	m_gridCellSize=cellSize;

	if (m_gridCellsX != gridCellsX || m_gridCellsY != gridCellsY)
	{	//resolution has changed
		m_gridCellsX=gridCellsX;
		m_gridCellsY=gridCellsY;

		if (m_meshData)
		{

			delete [] m_meshData;//free previously allocated grid and allocate new size
			m_meshData = nullptr;	 // must set to null so that we properly re-allocate
			m_meshDataSize = 0;

			Bool enable = m_doWaterGrid;
			enableWaterGrid(true);	// allocates buffers.
			m_doWaterGrid = enable;

		}
	}
}

void WaterRenderObjClass::getGridResolution( Real *gridCellsX, Real *gridCellsY, Real *cellSize )
{

	if( gridCellsX )
		*gridCellsX = m_gridCellsX;
	if( gridCellsY )
		*gridCellsY = m_gridCellsY;
	if( cellSize )
		*cellSize = m_gridCellSize;

}

static Real wobble(Real baseV, Real offset, Bool wobble)
{
	if (!wobble) return 0;
	offset = sin(2*PI*baseV - 3*offset);
	return offset/22;
}

/**Utility function used to query water heights in a manner that works in both RTS and WB.*/
Real WaterRenderObjClass::getWaterHeight(Real x, Real y)
{
	const WaterHandle *waterHandle = nullptr;
	Real waterZ = 0.0f;
	ICoord3D iLoc;

	iLoc.x = REAL_TO_INT_FLOOR( x + 0.5f );
	iLoc.y = REAL_TO_INT_FLOOR( y + 0.5f );
	iLoc.z = 0;

	for( PolygonTrigger *pTrig = PolygonTrigger::getFirstPolygonTrigger(); pTrig; pTrig = pTrig->getNext() )
	{

		if( !pTrig->isWaterArea() )
			continue;

		// See if point is in a water area
		if( pTrig->pointInTrigger( iLoc ) )
		{

			if( pTrig->getPoint( 0 )->z >= waterZ )
			{

				waterZ = pTrig->getPoint( 0 )->z;
				waterHandle = pTrig->getWaterHandle();

			}

		}

	}

	if (waterHandle)
		return waterHandle->m_polygon->getPoint( 0 )->z;
	return INVALID_WATER_HEIGHT;	//point not underwater
}

//-------------------------------------------------------------------------------------------------
//Draw a many sided river polygon.
//-------------------------------------------------------------------------------------------------
void WaterRenderObjClass::drawRiverWater(PolygonTrigger *pTrig)
{
	// D3D12: backend state refresh (legacy note about rivers needing a full
	// state reset preserved via the renderer-neutral invalidate).
	if (IRenderBackend *riverBackend = WW3D::Get_Render_Backend())
		riverBackend->Invalidate_Cached_Render_States();

	Int rectangleCount = pTrig->getNumPoints()/2;
	rectangleCount--;

	Real bumpFactor = 5;
	static Bool doWobble = true;

	if (m_disableRiver) return;
	m_drawingRiver = true;

	//allocate 2 triangles per side with 3 indices per triangle
	// D3D12: CPU index list (same topology as the retired dynamic IB).
	std::vector<unsigned short> riverIndices;
	riverIndices.reserve(static_cast<size_t>(rectangleCount + 1) * 2 * 3);
	{
		for (Int i=0; i<rectangleCount; i++)
		{
			//triangle 1
			riverIndices.push_back(static_cast<unsigned short>(i*2));
			riverIndices.push_back(static_cast<unsigned short>(i*2+1));
			riverIndices.push_back(static_cast<unsigned short>(i*2+3));

			//triangle 2
			riverIndices.push_back(static_cast<unsigned short>(i*2));
			riverIndices.push_back(static_cast<unsigned short>(i*2+3));
			riverIndices.push_back(static_cast<unsigned short>(i*2+2));
		}
	}


	Real shadeR=TheWaterTransparency->m_standingWaterColor.red;
	Real shadeG=TheWaterTransparency->m_standingWaterColor.green;
	Real shadeB=TheWaterTransparency->m_standingWaterColor.blue;

	//If the water color is not overridden, use legacy lighting code.
	if ( shadeR==1.0f && shadeG==1.0f && shadeB==1.0f)
	{
		shadeR = TheGlobalData->m_terrainAmbient[0].red;
		shadeG = TheGlobalData->m_terrainAmbient[0].green;
		shadeB = TheGlobalData->m_terrainAmbient[0].blue;

		//Add in diffuse lighting from each terrain light
		for (Int lightIndex=0; lightIndex < TheGlobalData->m_numGlobalLights; lightIndex++)
		{
			if (-TheGlobalData->m_terrainLightPos[lightIndex].z > 0)
			{	shadeR += -TheGlobalData->m_terrainLightPos[lightIndex].z * TheGlobalData->m_terrainDiffuse[lightIndex].red;
				shadeG += -TheGlobalData->m_terrainLightPos[lightIndex].z * TheGlobalData->m_terrainDiffuse[lightIndex].green;
				shadeB += -TheGlobalData->m_terrainLightPos[lightIndex].z * TheGlobalData->m_terrainDiffuse[lightIndex].blue;
			}
		}

		//Get water material colors
		Real waterShadeR = (m_settings[m_tod].waterDiffuse & 0xff) / 255.0f;
		Real waterShadeG = ((m_settings[m_tod].waterDiffuse >> 8) & 0xff) / 255.0f;
		Real waterShadeB = ((m_settings[m_tod].waterDiffuse >> 16) & 0xff) / 255.0f;

		shadeR=shadeR*waterShadeR*255.0f;
		shadeG=shadeG*waterShadeG*255.0f;
		shadeB=shadeB*waterShadeB*255.0f;
	}
	else
	{
		shadeR=shadeR*255.0f;
		shadeG=shadeG*255.0f;
		shadeB=shadeB*255.0f;

		if (shadeR == 0 && shadeG == 0 && shadeB == 0)
		{	//special case where we disable lighting
			shadeR=255;
			shadeG=255;
			shadeB=255;
		}
	}

	Int diffuse=REAL_TO_INT(shadeB) | (REAL_TO_INT(shadeG) << 8) | (REAL_TO_INT(shadeR) << 16);

	//Keep diffuse from lighting calculations but substitute custom alpha
	diffuse |= m_settings[m_tod].waterDiffuse & 0xff000000;	//copy alpha/opacity from ini setting

	Int innerNdx = pTrig->getRiverStart();
	Int outerNdx = innerNdx+1;

	Real endLen=0;
	Real totalLen=0;
	Int i;
	for (i=0; i<pTrig->getNumPoints()-1; i++) {
		ICoord3D innerPt = *pTrig->getPoint(i);
		ICoord3D outerPt = *pTrig->getPoint(i+1);
		Real dx = innerPt.x-outerPt.x;
		Real dy = innerPt.y-outerPt.y;
		Real curLen = sqrt(dx*dx+dy*dy);
		totalLen += curLen;
		if ( i==innerNdx) {
			endLen = curLen;
		}
	}
	bumpFactor = endLen/BUMP_SIZE;

	Real lengthOfRiver = (totalLen/2)-endLen;
	Real repeatCount = lengthOfRiver / (endLen);

	Real vScale=(Real)repeatCount/(Real)rectangleCount;

#define HEIGHT_TO_USE (0.5f)
	if (innerNdx >= pTrig->getNumPoints()-1) return;
	//allocate 2 vertices per side
	// D3D12: CPU vertex list submitted via Draw_Indexed_Material_Triangles.
	// Primary river UVs (u1/v1) are preserved; second UV set (u2/v2 sparkles
	// coords) and normal padding (nx/ny/nz) have no single-texture material
	// equivalent and are preserved CPU-side only (documented, not faked).
	std::vector<RenderBackendTexturedVertex> riverVerts;
	riverVerts.resize(static_cast<size_t>(rectangleCount + 1) * 2);
	{
		RenderBackendTexturedVertex *vb = riverVerts.data();

		Real constA=3*m_riverVOrigin;

		// TheSuperHackers @bugfix afc-afc0 14/04/2026 Apply shroud per-vertex to avoid double-darkening
		// at river borders.
		W3DShroud *shroud = TheTerrainRenderObject ? TheTerrainRenderObject->getShroud() : nullptr;

		for (i=0; i<(pTrig->getNumPoints()/2); i++)
		{
			Real x,y;
			ICoord3D innerPt = *pTrig->getPoint(outerNdx);
			ICoord3D outerPt = *pTrig->getPoint(innerNdx);
			outerNdx++;
			innerNdx--;
			if (innerNdx<0) {
				innerNdx = pTrig->getNumPoints()-1;
			}
			if (outerNdx >= pTrig->getNumPoints()) {
				outerNdx = 0;
			}
			x=innerPt.x;
			y=innerPt.y;

			vb->x=x;
			vb->y=y;

			vb->z=innerPt.z;

			{
				const UnsignedInt riverDiffuseInner = static_cast<UnsignedInt>(
					getRiverVertexDiffuse(shroud, x, y, shadeR, shadeG, shadeB, diffuse));
				vb->a = ((riverDiffuseInner >> 24) & 255) / 255.0f;
				vb->r = ((riverDiffuseInner >> 16) & 255) / 255.0f;
				vb->g = ((riverDiffuseInner >> 8) & 255) / 255.0f;
				vb->b = (riverDiffuseInner & 255) / 255.0f;
			}

			Real wobbleConst=-m_riverVOrigin+vScale*(Real)i + WWMath::Fast_Sin(2*PI*(vScale*(Real)i) - constA)/22.0f;
  			//old slower version
			//vb->v=-m_riverVOrigin+vScale*(Real)i + wobble(vScale*i, m_riverVOrigin, doWobble);
			vb->v=wobbleConst;
			vb->u=HEIGHT_TO_USE ;
			//old slower version
			//vb->v = -m_riverVOrigin+vScale*(Real)i + wobble(vScale*i, m_riverVOrigin, doWobble);
			// second UV set (sparkles) preserved CPU-side only; see note above.
			vb++;

			x=outerPt.x;
			y=outerPt.y;

			vb->x=x;
			vb->y=y;
			vb->z=outerPt.z;

			{
				const UnsignedInt riverDiffuseOuter = static_cast<UnsignedInt>(
					getRiverVertexDiffuse(shroud, x, y, shadeR, shadeG, shadeB, diffuse));
				vb->a = ((riverDiffuseOuter >> 24) & 255) / 255.0f;
				vb->r = ((riverDiffuseOuter >> 16) & 255) / 255.0f;
				vb->g = ((riverDiffuseOuter >> 8) & 255) / 255.0f;
				vb->b = (riverDiffuseOuter & 255) / 255.0f;
			}
  			//old slower version
			//vb->v=-m_riverVOrigin+vScale*(Real)i + wobble(vScale*i, m_riverVOrigin, doWobble);
			vb->v=wobbleConst;
			vb->u=0;
			//old slower version
  			//vb->v = -m_riverVOrigin+vScale*(Real)i + wobble(vScale*i, m_riverVOrigin, doWobble);
			// second UV set (sparkles) preserved CPU-side only; see note above.
			vb++;

		}
	}

	Matrix3D tm(1);

	// D3D12: river quads are world-space already (identity WORLD transform);
	// submit via Draw_Indexed_Material_Triangles with the alpha/additive
	// material. River alpha-edge wrap preserved via repeat sampler. The ps.1.1
	// river pixel shader (c0 REFLECTION_FACTOR) and wireframe debug fill stay
	// archival (wireframeForDebug is always false).
	(void)tm;
	setupJbaWaterShader();

	IRenderBackend *riverSubmitBackend = WW3D::Get_Render_Backend();
	if (riverSubmitBackend != nullptr && !riverVerts.empty() && !riverIndices.empty()) {
		ShaderClass riverShader = TheWaterTransparency->m_additiveBlend
			? ShaderClass::_PresetAdditiveShader : ShaderClass::_PresetAlphaShader;
		RenderBackendMaterialState riverMaterial;
		if (riverShader.Get_Render_Backend_State(riverMaterial)) {
			riverMaterial.cull = RenderBackendCullMode::None;
			RenderBackendTextureHandle riverHandle;
			if (m_riverTexture != nullptr) {
				const TextureFilterClass &riverFilter = m_riverTexture->Get_Filter();
				if (riverFilter.Get_Render_Sampler(riverMaterial.sampler))
					riverMaterial.clamp_texture = false; // alpha-edge wrap
				if (m_riverTexture->Ensure_Renderer_Texture())
					riverHandle = m_riverTexture->Get_Renderer_Texture();
			}
			riverSubmitBackend->Draw_Indexed_Material_Triangles(riverVerts.data(),
				static_cast<unsigned int>(riverVerts.size()),
				riverIndices.data(), static_cast<unsigned int>(riverIndices.size()),
				riverHandle, riverMaterial);
			riverSubmitBackend->Invalidate_Cached_Render_States();
		}
	}


}

void WaterRenderObjClass::setupFlatWaterShader()
{

	// D3D12: flat-water material intent preserved (alpha vs additive);
	// textures ensured via the backend. The stage-3 shroud projection, the
	// camera-space noise transform, and the ps.1.1 trapezoid pixel shader (c0
	// REFLECTION_FACTOR) have no backend equivalent yet. CPU scroll offsets and
	// REFLECTION_FACTOR are preserved CPU-side; quads below are submitted with
	// the closest alpha material (no fake perturb/mirror).
	if (m_riverTexture != nullptr) {
		m_riverTexture->Get_Filter().Set_Mag_Filter(TextureFilterClass::FILTER_TYPE_BEST);
		m_riverTexture->Get_Filter().Set_Min_Filter(TextureFilterClass::FILTER_TYPE_BEST);
		m_riverTexture->Get_Filter().Set_Mip_Mapping(TextureFilterClass::FILTER_TYPE_BEST);
		m_riverTexture->Ensure_Renderer_Texture();
	}
	if (m_waterSparklesTexture != nullptr)
		m_waterSparklesTexture->Ensure_Renderer_Texture();
	if (m_waterNoiseTexture != nullptr)
		m_waterNoiseTexture->Ensure_Renderer_Texture();
	(void)REFLECTION_FACTOR;
}

//-------------------------------------------------------------------------------------------------
//Draw a 4 sided flat water area.
//-------------------------------------------------------------------------------------------------
void WaterRenderObjClass::drawTrapezoidWater(Vector3 points[4])
{
	Vector3 origin(points[0]);
	Vector3 uVec1(points[1]);
	Vector3 vVec1(points[3]);
	Vector3 uVec2(points[2]);
	Vector3 vVec2(points[2]);
	uVec2 -= vVec1;
	vVec2	-= uVec1;
	uVec1 -= origin;
	vVec1 -= origin;
	Int uCount = (uVec1.Length()+uVec2.Length()) / (8*MAP_XY_FACTOR);
	if (uCount<1) uCount = 1;
	Int vCount = (vVec1.Length()+vVec2.Length()) / (8*MAP_XY_FACTOR);
	if (vCount<1) vCount = 1;

	if (uCount>50) uCount = 50;
	if (vCount>50) vCount = 50;

	static Bool doWobble = true;

	Int rectangleCount = uCount*vCount;

	uCount++;
	vCount++;

	Int i, j;
	//allocate 2 triangles per side with 3 indices per triangle
	// D3D12: CPU index list (same grid topology as the retired dynamic IB).
	std::vector<unsigned short> trapIndices;
	trapIndices.reserve(static_cast<size_t>(rectangleCount) * 6);
	{
		for (j=0; j<vCount-1; j++)
		{	for (i=0; i<uCount-1; i++)
			{
				//triangle 1
				trapIndices.push_back(static_cast<unsigned short>((j)*uCount + i));
				trapIndices.push_back(static_cast<unsigned short>((j+1)*uCount + i+1));
				trapIndices.push_back(static_cast<unsigned short>((j+1)*uCount + i));

				//triangle 2
				trapIndices.push_back(static_cast<unsigned short>((j)*uCount + i));
				trapIndices.push_back(static_cast<unsigned short>((j)*uCount + i+1));
				trapIndices.push_back(static_cast<unsigned short>((j+1)*uCount + i+1));
			}
		}
	}

	Real	waterFactor=150;
	Real shadeR=TheWaterTransparency->m_standingWaterColor.red;
	Real shadeG=TheWaterTransparency->m_standingWaterColor.green;
	Real shadeB=TheWaterTransparency->m_standingWaterColor.blue;

	//If the water color is not overridden, use legacy lighting code.
	if ( shadeR==1.0f && shadeG==1.0f && shadeB==1.0f)
	{
		shadeR = TheGlobalData->m_terrainAmbient[0].red;
		shadeG = TheGlobalData->m_terrainAmbient[0].green;
		shadeB = TheGlobalData->m_terrainAmbient[0].blue;

		//Add in diffuse lighting from each terrain light
		for (Int lightIndex=0; lightIndex < TheGlobalData->m_numGlobalLights; lightIndex++)
		{
			if (-TheGlobalData->m_terrainLightPos[lightIndex].z > 0)
			{	shadeR += -TheGlobalData->m_terrainLightPos[lightIndex].z * TheGlobalData->m_terrainDiffuse[lightIndex].red;
				shadeG += -TheGlobalData->m_terrainLightPos[lightIndex].z * TheGlobalData->m_terrainDiffuse[lightIndex].green;
				shadeB += -TheGlobalData->m_terrainLightPos[lightIndex].z * TheGlobalData->m_terrainDiffuse[lightIndex].blue;
			}
		}

		//Get water material colors
		Real waterShadeR = (m_settings[m_tod].waterDiffuse & 0xff) / 255.0f;
		Real waterShadeG = ((m_settings[m_tod].waterDiffuse >> 8) & 0xff) / 255.0f;
		Real waterShadeB = ((m_settings[m_tod].waterDiffuse >> 16) & 0xff) / 255.0f;

		shadeR=shadeR*waterShadeR*255.0f;
		shadeG=shadeG*waterShadeG*255.0f;
		shadeB=shadeB*waterShadeB*255.0f;
	}
	else
	{
		shadeR=shadeR*255.0f;
		shadeG=shadeG*255.0f;
		shadeB=shadeB*255.0f;

		if (shadeR == 0 && shadeG == 0 && shadeB == 0)
		{	//special case where we disable lighting
			shadeR=255;
			shadeG=255;
			shadeB=255;
		}
	}

	Int diffuse=REAL_TO_INT(shadeB) | (REAL_TO_INT(shadeG) << 8) | (REAL_TO_INT(shadeR) << 16);

	//Keep diffuse from lighting calculations but substitute custom alpha
	diffuse |= m_settings[m_tod].waterDiffuse & 0xff000000;	//copy alpha/opacity from ini setting

	// D3D12: CPU vertex list (primary u1/v1 UVs preserved; u2/v2 second UV
	// set and nx/ny/nz padding have no single-texture material equivalent).
	std::vector<RenderBackendTexturedVertex> trapVerts;
	trapVerts.resize(static_cast<size_t>(uCount) * vCount);

//#define WAVY_WATER
//#define FEATHER_LAYER_COUNT (3) //LORENZEN
//#define FEATHER_LAYER_THICKNESS (2.5f)
//#define FEATHER_WATER

//#ifdef WAVY_WATER // the NEW WATER a'la LORENZEN
	if ( TheGlobalData->m_featherWater )
	{

		RenderBackendTexturedVertex* vb=trapVerts.data();

		Real phase = 0;
		Real mapCoeff = PI/(4*MAP_XY_FACTOR);
		Real wave = 0;
		Real amplitude = 0.5f;

		//The first (high order) byte is the Alpha value for this patch
		// It needs to be set proportional to the number of feather layers
		// this comes from TheGlobalData->m_featherWater, which is a count of layers


		Int Alpha = 0;
		if ( TheGlobalData->m_featherWater == 5) Alpha = 80;
		if ( TheGlobalData->m_featherWater == 4) Alpha = 110;
		if ( TheGlobalData->m_featherWater == 3) Alpha = 140;
		if ( TheGlobalData->m_featherWater == 2) Alpha = 200;
		if ( TheGlobalData->m_featherWater == 1) Alpha = 255;

		//Keep diffuse from lighting calculations but substitute custom alpha
		Int customDiffuse = (diffuse & 0x00ffffff) | (Alpha<< 24);//(0x80 << 16)|(0x90 << 8)|0xa0;

		for (j=0; j<vCount; j++)
		{
			Real dv = j;
			dv /= (vCount-1);
			for (i=0; i<uCount; i++)
			{
				Real du = i;
				du /= (uCount-1);
				Vector3 vertex = origin;
				vertex += uVec1*du;
				vertex += vVec1*dv;
				vertex += (dv)*(du)*(vVec2-vVec1);

				vb->x=vertex.X;
				vb->y=vertex.Y;

				// common to all the waving effects
				phase = 25 * m_riverVOrigin + vertex.X * mapCoeff;
				wave = (sin(phase) - 1.0f) * amplitude;

				vb->z = (vertex.Z + wave);
				{
					const UnsignedInt featherDiffuse = static_cast<UnsignedInt>(customDiffuse);
					vb->a = ((featherDiffuse >> 24) & 255) / 255.0f;
					vb->r = ((featherDiffuse >> 16) & 255) / 255.0f;
					vb->g = ((featherDiffuse >> 8) & 255) / 255.0f;
					vb->b = (featherDiffuse & 255) / 255.0f;
				}
				vb->u = (vertex.X/waterFactor) + 0.02*cos(11*m_riverVOrigin)*wave;
				vb->v = (vertex.Y/waterFactor) + 0.02*cos(5*m_riverVOrigin)*wave;
				vb->q = 1.0f;
				// u2/v2 bump coords + nx/ny/nz padding: CPU-side only (see note).
				vb++;
			}
		}
	}
//#else // STILL THE OLD FLAT WATER
	else

	{
		RenderBackendTexturedVertex* vb=trapVerts.data();

		//Pulling some constants out of the inner loops to improve performance -MW
		Real constA=0.02*cos(11*m_riverVOrigin);
		Real constB=0.02*cos(5*m_riverVOrigin);
		Real constC=25*m_riverVOrigin;
		Real ooWaterFactor = 1.0f/waterFactor;
		const Real constD=PI/(4*MAP_XY_FACTOR);
		Real constE=1.0f/(Real)(vCount-1);
		Real constF=1.0f/(Real)(uCount-1);

		for (j=0; j<vCount; j++)
		{
			Real dv = (Real)j * constE;

			for (i=0; i<uCount; i++)
			{
				Real du = (Real)i * constF;
				Vector3 vertex = origin;
				vertex += uVec1*du;
				vertex += vVec1*dv;
				vertex += (dv)*(du)*(vVec2-vVec1);

				vb->x=vertex.X;
				vb->y=vertex.Y;
				vb->z=vertex.Z;

				{
					const UnsignedInt flatDiffuse = static_cast<UnsignedInt>(diffuse);
					vb->a = ((flatDiffuse >> 24) & 255) / 255.0f;
					vb->r = ((flatDiffuse >> 16) & 255) / 255.0f;
					vb->g = ((flatDiffuse >> 8) & 255) / 255.0f;
					vb->b = (flatDiffuse & 255) / 255.0f;
				}
				//Old slower version
  				//vb->u=(vertex.X/waterFactor) + 0.02*cos(11*m_riverVOrigin)*sin(25*m_riverVOrigin+vertex.X*PI/(4*MAP_XY_FACTOR));
  				//vb->v=(vertex.Y/waterFactor) + 0.02*cos(5*m_riverVOrigin)*sin(25*m_riverVOrigin+vertex.Y*PI/(4*MAP_XY_FACTOR));
				vb->u=vertex.X*ooWaterFactor + constA*WWMath::Fast_Sin(constC+vertex.X*constD);
				vb->v=vertex.Y*ooWaterFactor + constB*WWMath::Fast_Sin(constC+vertex.Y*constD);
				vb->q = 1.0f;
				// u2/v2 bump coords + nx/ny/nz padding: CPU-side only (see note).
				vb++;
			}
		}
	}

//#endif // OLD VS NEW WATER



	Matrix3D tm(1);

	// D3D12: trapezoid quads are world-space already (identity WORLD
	// transform); submit via Draw_Indexed_Material_Triangles. Destination-alpha
	// shoreline feathering (DESTALPHA/INVDESTALPHA) has no backend equivalent
	// yet; normal alpha blending is used and the intent documented here.
	(void)tm;
	setupFlatWaterShader();// lorenzen sez use the alpha shader

	{
		IRenderBackend *trapBackend = WW3D::Get_Render_Backend();
		if (trapBackend != nullptr && !trapVerts.empty() && !trapIndices.empty()) {
			ShaderClass trapShader = TheWaterTransparency->m_additiveBlend
				? ShaderClass::_PresetAdditiveShader : ShaderClass::_PresetAlphaShader;
			RenderBackendMaterialState trapMaterial;
			if (trapShader.Get_Render_Backend_State(trapMaterial)) {
				trapMaterial.cull = RenderBackendCullMode::None;
				RenderBackendTextureHandle trapHandle;
				if (m_riverTexture != nullptr) {
					const TextureFilterClass &trapFilter = m_riverTexture->Get_Filter();
					if (trapFilter.Get_Render_Sampler(trapMaterial.sampler))
						trapMaterial.clamp_texture = false;
					if (m_riverTexture->Ensure_Renderer_Texture())
						trapHandle = m_riverTexture->Get_Renderer_Texture();
				}
				trapBackend->Draw_Indexed_Material_Triangles(trapVerts.data(),
					static_cast<unsigned int>(trapVerts.size()),
					trapIndices.data(), static_cast<unsigned int>(trapIndices.size()),
					trapHandle, trapMaterial);
				trapBackend->Invalidate_Cached_Render_States();
			}
		}
		// GAP: shroud second pass (ST_SHROUD_TEXTURE stage-0 re-draw with
		// LESSEQUAL) has no backend UV-projection equivalent yet; water above
		// is submitted unmodulated rather than double-drawn.
	}
}



//-------------------------------------------------------------------------------------------------
//debug version where moon rotates with the camera	(always upright on screen)
//-------------------------------------------------------------------------------------------------
#if 0
void WaterRenderObjClass::renderSkyBody(Matrix3D *mat)
{
	Vector3 vRight,vUp,V0,V1,V2,V3;

	mat->Get_X_Vector(&vRight);
	mat->Get_Y_Vector(&vUp);

	//calculate offsets from quad center to each of the 4 corners
	//	0-----1
	//  |    /|
	//  |  /  |
	//	|/    |
	//  3-----2
	V0=-vRight+vUp;
	V2=vRight+vUp;
	V2=vRight-vUp;
	V3=-vRight-vUp;

	VertexMaterialClass *vmat=VertexMaterialClass::Get_Preset(VertexMaterialClass::PRELIT_DIFFUSE);
	DX8Wrapper::Set_Material(vmat);
	REF_PTR_RELEASE(vmat);
	DX8Wrapper::Set_Shader(ShaderClass::/*_PresetAdditiveShader*//*_PresetOpaqueShader*/_PresetAlphaShader);
//	DX8Wrapper::Set_Texture(0,setting->skyBodyTexture);

	DX8Wrapper::Set_Texture(0,m_alphaClippingTexture);

	//draw an infinite sky plane
	DynamicVBAccessClass vb_access(BUFFER_TYPE_DYNAMIC_DX8,4);
	{
		DynamicVBAccessClass::WriteLockClass lock(&vb_access);
		VertexFormatXYZNDUV2* verts=lock.Get_Formatted_Vertex_Array();
		if(verts)
		{
			verts[0].x=SKYBODY_SIZE*V0.X;
			verts[0].y=SKYBODY_SIZE*V0.Y;
			verts[0].z=SKYBODY_SIZE*V0.Z;
			verts[0].u2=0;
			verts[0].v2=1;
			verts[0].diffuse=0xffffffff;

			verts[1].x=SKYBODY_SIZE*V1.X;
			verts[1].y=SKYBODY_SIZE*V1.Y;
			verts[1].z=SKYBODY_SIZE*V1.Z;
			verts[1].u2=1;
			verts[1].v2=1;
			verts[1].diffuse=0xffffffff;

			verts[2].x=SKYBODY_SIZE*V2.X;
			verts[2].y=SKYBODY_SIZE*V2.Y;
			verts[2].z=SKYBODY_SIZE*V2.Z;
			verts[2].u2=1;
			verts[2].v2=0;
			verts[2].diffuse=0xffffffff;

			verts[3].x=SKYBODY_SIZE*V3.X;
			verts[3].y=SKYBODY_SIZE*V3.Y;
			verts[3].z=SKYBODY_SIZE*V3.Z;
			verts[3].u2=0;
			verts[3].v2=0;
			verts[3].diffuse=0xffffffff;
		}
	}

	DX8Wrapper::Set_Index_Buffer(m_indexBuffer,0);
	DX8Wrapper::Set_Vertex_Buffer(vb_access);

	Matrix3D tm(1);
	//set position of skybody in world
//	tm.Set_Translation(Vector3(40,0,0));
	DX8Wrapper::Set_Transform(D3DTS_WORLD,tm);

	DX8Wrapper::Draw_Triangles(	0,2, 0,	4);	//draw a quad, 2 triangles, 4 verts
}
#endif

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void WaterRenderObjClass::crc( Xfer *xfer )
{

}

// ------------------------------------------------------------------------------------------------
/** Xfer
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void WaterRenderObjClass::xfer( Xfer *xfer )
{

	// version
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// grid cells x
	Int cellsX = m_gridCellsX;
	xfer->xferInt( &cellsX );
	if( cellsX != m_gridCellsX )
	{

		DEBUG_CRASH(( "WaterRenderObjClass::xfer - cells X mismatch" ));
		throw SC_INVALID_DATA;

	}

	// grid cells Y
	Int cellsY = m_gridCellsY;
	xfer->xferInt( &cellsY );
	if( cellsY != m_gridCellsY )
	{

		DEBUG_CRASH(( "WaterRenderObjClass::xfer - cells Y mismatch" ));
		throw SC_INVALID_DATA;

	}

	// xfer each of the mesh data points
	for( UnsignedInt i = 0; i < m_meshDataSize; ++i )
	{

		// height
		xfer->xferReal( &m_meshData[ i ].height );

		// velocity
		xfer->xferReal( &m_meshData[ i ].velocity );

		// status
		xfer->xferUnsignedByte( &m_meshData[ i ].status );

		// preferred height
		xfer->xferUnsignedByte( &m_meshData[ i ].preferredHeight );

	}

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void WaterRenderObjClass::loadPostProcess()
{

}


