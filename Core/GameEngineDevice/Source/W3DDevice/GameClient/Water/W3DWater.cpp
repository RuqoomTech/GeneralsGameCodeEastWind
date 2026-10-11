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
#include "WW3D2/dx8wrapper.h"
#include "WW3D2/light.h"
#include "WW3D2/IRenderBackend.h"
#include "WW3D2/ww3d.h"
#include <vector>
#include <algorithm>
#include <map>
#include <tuple>
#include <cstdio>
#include <cmath>
#include <cstdint>
#include "WWLib/simplevec.h"
#include "WW3D2/mesh.h"
#include "WW3D2/matinfo.h"

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

#define PATCH_WIDTH (PATCH_SIZE-1)	//internal defines
#define PATCH_UV_SCALE	((Real)PATCH_UV_TILES/(Real)PATCH_WIDTH)

//3D Grid Mesh Water defines.
#define WATER_MESH_OPACITY		0.5f
#define WATER_MESH_X_VERTICES	128
#define WATER_MESH_Y_VERTICES	128
#define WATER_MESH_SPACING	MAP_XY_FACTOR	//same as terrain

#ifdef USE_MESH_NORMALS
#define WATER_MESH_FVF	DX8_FVF_XYZNDUV2
typedef VertexFormatXYZNDUV2 MaterMeshVertexFormat;
#else
#define WATER_MESH_FVF	DX8_FVF_XYZDUV2
typedef VertexFormatXYZDUV2 MaterMeshVertexFormat;
#endif

// Converts a FLOAT to a DWORD for use in SetRenderState() calls
static inline DWORD F2DW( FLOAT f ) { return *((DWORD*)&f); }

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

D3DMATRIX Build_Water_World_Matrix(const D3DMATRIX &patch_matrix, const D3DMATRIX &ww3d_matrix)
{
	return To_D3DMATRIX(To_Matrix4x4(ww3d_matrix) * To_Matrix4x4(patch_matrix));
}

void Build_Water_World_View_Projection_Constants(
	D3DMATRIX &shader_constants,
	const D3DMATRIX &patch_matrix,
	const D3DMATRIX &ww3d_matrix,
	const D3DMATRIX &view_matrix,
	const D3DMATRIX &projection_matrix)
{
	// Legacy D3DX built patch * WW3D * view * projection and then transposed
	// the result before uploading four vertex-shader constants. Conversion to
	// WWMath already transposes each D3DMATRIX, so the reversed order below
	// yields exactly that final transposed shader matrix.
	const Matrix4x4 shader_matrix =
		To_Matrix4x4(projection_matrix) *
		To_Matrix4x4(view_matrix) *
		To_Matrix4x4(ww3d_matrix) *
		To_Matrix4x4(patch_matrix);

	shader_constants = {};
	for (Int row = 0; row < 4; ++row) {
		for (Int column = 0; column < 4; ++column) {
			shader_constants.m[row][column] = shader_matrix[row][column];
		}
	}
}

} // namespace

WaterRenderObjClass *TheWaterRenderObj=nullptr; ///<global water rendering object

static UnsignedInt getRiverVertexDiffuse(W3DShroud *shroud, Real x, Real y, Real shadeR, Real shadeG, Real shadeB, UnsignedInt diffuse)
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

// Render-only ownership stays in the existing water object, outside snapshots.
struct WaterRenderObjClass::BackendWaterGeometry
{
    struct Patch { Vector3 points[4]{Vector3(0,0,0),Vector3(0,0,0),Vector3(0,0,0),Vector3(0,0,0)}; RenderBackendGeometryHandle geometry; };
    IRenderBackend *owner = nullptr;
    struct River {
        std::vector<ICoord3D> points;
        std::vector<RenderBackendTerrainVertex> vertices;
        std::vector<unsigned short> indices;
        std::vector<UnsignedInt> colors;
        RenderBackendGeometryHandle geometry;
        Int river_start = -1;
        bool dirty = true;
    };
    std::map<Int,River> rivers;
    std::map<std::tuple<Int,Int,Int>,Patch> patches;
    std::vector<RenderBackendTerrainVertex> grid_vertices;
    std::vector<unsigned short> grid_indices;
    RenderBackendGeometryHandle grid_geometry;
    bool grid_has_heights = false;
    int grid_x = 0, grid_y = 0;
    float grid_cell_size = 0;
    bool warned = false;

    ~BackendWaterGeometry()
    {
        if (owner && owner == WW3D::Get_Render_Backend()) {
            for (auto &entry : patches) owner->Release_Static_Geometry(entry.second.geometry);
            for (auto &entry : rivers) owner->Release_Static_Geometry(entry.second.geometry);
            owner->Release_Static_Geometry(grid_geometry);
        }
    }
    void failure(const char *reason)
    {
        if (!warned) {
            std::fprintf(stderr, "WaterRenderObjClass: %s\n", reason);
            warned = true;
        }
    }
};

//-------------------------------------------------------------------------------------------------
/** Destructor. Releases w3d assets. */
//-------------------------------------------------------------------------------------------------
WaterRenderObjClass::~WaterRenderObjClass()
{
	REF_PTR_RELEASE(m_vertexMaterialClass);
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
	{	SAFE_RELEASE( m_pBumpTexture[i]);
		SAFE_RELEASE( m_pBumpTexture2[i]);
	}

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
	m_indexBuffer=nullptr;
	m_backendWaterGeometry=nullptr;
	m_gridGeometryDirty=TRUE;
	m_waterTrackSystem = nullptr;
	m_doWaterGrid = FALSE;
	m_vertexMaterialClass=nullptr;
	m_alphaClippingTexture=nullptr;
	m_useCloudLayer=true;
	m_waterType = WATER_TYPE_0_TRANSLUCENT;
	m_tod=TIME_OF_DAY_AFTERNOON;
	m_pReflectionTexture=nullptr;
	m_skyBox=nullptr;
	m_vertexBufferD3D=nullptr;
	m_indexBufferD3D=nullptr;
	m_vertexBufferD3DOffset=0;

	m_dwWavePixelShader=0;
	m_dwWaveVertexShader=0;
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

	Int i=NUM_BUMP_FRAMES;
	while (i--)
		m_pBumpTexture[i]=nullptr;

	m_riverVOrigin=0;
	m_riverTexture=nullptr;
	m_whiteTexture=nullptr;
	m_waterNoiseTexture=nullptr;
	m_riverAlphaEdge=nullptr;
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
/** Copies raw bits from pBumpSrc (a regular grayscale texture) into a D3D
	*   bump-map format. */
//-------------------------------------------------------------------------------------------------
HRESULT WaterRenderObjClass::initBumpMap(LPDIRECT3DTEXTURE8 *pTex, TextureClass *pBumpSource)
{
    SurfaceClass::SurfaceDescription    d3dsd;
	SurfaceClass * surf;
    D3DLOCKED_RECT     d3dlr;
	DWORD dwSrcPitch;
	BYTE* pSrc;
	Int numLevels;

#ifdef MIPMAP_BUMP_TEXTURE

	pBumpSource->Get_Level_Description(d3dsd);

	if (Get_Bytes_Per_Pixel(d3dsd.Format) != 4)
	{
		// LORENZEN WAS BUGGED BY THIS,
		//		DEBUG_CRASH(("WaterRenderObjClass::Invalid BumpMap format - Was it compressed?") );
		return S_OK;
	}

	if (pBumpSource->Peek_D3D_Texture())
	{
		numLevels=pBumpSource->Peek_D3D_Texture()->GetLevelCount();
	}
	else
		return S_OK;

	pTex[0]=DX8Wrapper::_Create_DX8_Texture(d3dsd.Width,d3dsd.Height,WW3D_FORMAT_U8V8,MIP_LEVELS_ALL,D3DPOOL_MANAGED,false);

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

#else
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
#endif

    return S_OK;
}

//-------------------------------------------------------------------------------------------------
/** Create and fill a D3D vertex buffer with water surface vertices */
//-------------------------------------------------------------------------------------------------
HRESULT WaterRenderObjClass::generateVertexBuffer( Int sizeX, Int sizeY, Int vertexSize, Bool doStatic)
{
	m_numVertices=sizeX*sizeY;
	//Assuming dynamic vertex buffer, allocate maximum multiple of required size to allow rendering from
	//different parts of the buffer. 5-15-03: Disabled this since we use DISCARD mode instead to avoid Nvidia Runtime bug. -MW
	//m_numVertices=(65536 / (sizeX*sizeY))*sizeX*sizeY;

	SEA_PATCH_VERTEX* pVertices;

	Setting *setting=&m_settings[m_tod];

	HRESULT hr;

	//default setting for a dynamic vertex buffer
	D3DPOOL pool = D3DPOOL_DEFAULT;
	DWORD usage = D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC;
	DWORD fvf = WATER_MESH_FVF;

	if (doStatic)
	{	//change settings for a static vertex buffer
		pool = D3DPOOL_MANAGED;
		usage = D3DUSAGE_WRITEONLY;
		fvf=0;// DX8 Docs confusing on this. Say no FVF for vertex shaders. Else DX8_FVF_XYZDUV1;
		m_numVertices=sizeX*sizeY;
	}

	if (m_vertexBufferD3D == nullptr)
	{	// Create vertex buffer

		if (FAILED(hr=m_pDev->CreateVertexBuffer
		(
			m_numVertices*vertexSize,
			usage,
			fvf,
			pool,
			&m_vertexBufferD3D
		)))
			return hr;
	}

	m_vertexBufferD3DOffset=0;

	if (!doStatic)
		return S_OK;	//only create the buffer, other code will fill it.

	// load results into buffer
	if (FAILED(hr=m_vertexBufferD3D->Lock
	(
		0,
		m_numVertices*sizeof(SEA_PATCH_VERTEX),
		(BYTE**)&pVertices,
		0//D3DLOCK_DISCARD
	)))
		return hr;

	Int x,z;
	for (z=0; z<sizeY; z++)
	{
		for (x=0; x<sizeX; x++)
		{
			pVertices->x=(float)x;
			pVertices->y=m_level;
			pVertices->z=(float)z;

			pVertices->tu=(float)x*PATCH_UV_SCALE;
			pVertices->tv=(float)z*PATCH_UV_SCALE;
			pVertices->c=setting->transparentWaterDiffuse;	//vertex alpha/color
			pVertices++;
		}
	}

	if (FAILED(hr=m_vertexBufferD3D->Unlock())) return hr;

	return S_OK;
}

//-------------------------------------------------------------------------------------------------
/** Create and fill a D3D index buffer with water surface strip indices */
//-------------------------------------------------------------------------------------------------
HRESULT WaterRenderObjClass::generateIndexBuffer(Int sizeX, Int sizeY)
{
	HRESULT hr;

	//Will need SizeY-1 strips, each of length SizeX*2 (2 indices per strip segment).
	//Will also need 2 extra indices to connect each strip to next one (except last strip)
	//Total index buffer size = (SizeY-1)*(SizeX*2+2) - 2 (drop the extra 2 indices from last strip)

	m_numIndices=(sizeY-1)*(sizeX*2+2) - 2;

	//old way

	// Create index buffer
	WORD* pIndices;

	if (FAILED(hr=m_pDev->CreateIndexBuffer
	(
		(m_numIndices+2)*sizeof(WORD),
		D3DUSAGE_WRITEONLY,
		D3DFMT_INDEX16,
		D3DPOOL_MANAGED,
		&m_indexBufferD3D
	)))
		return hr;

	if (FAILED(hr=m_indexBufferD3D->Lock
	(
		0,
		m_numIndices*sizeof(WORD),
		(BYTE**)&pIndices,
		0
	)))
		return hr;

	Int i,j,k;

	for (i=0,j=0,k=0; i<m_numIndices; j++)
	{
		for (;k<(sizeX*(j+1)); k++,i+=2)
		{
			pIndices[i]=(UnsignedShort) k+sizeX;
			pIndices[i+1]=(UnsignedShort) k;
		}
		//Generate 4 degenerate triangle to connect current strip to next strip/row of map
		//To do this, we just repeat the last index of first strip and first index of new strip.
		//Any triangles with repeated vertices will be skipped during rendering.
		if (i<m_numIndices) //check if there is at least 1 more strip to go
		{
			pIndices[i]=k-1;
			pIndices[i+1]=k+sizeX;
			i+=2;
		}
	}

	/*Old way
	Int step=1;
	Int psize=(size-1)/step;

	m_numIndices=psize*((psize+1)*2)+(psize*2)-2;


	Int x,z,s_toggle=1;
	for (z=step; z<size; z+=step)
	{
		if (s_toggle)
		{
			for (x=0; x<(size-step); x+=step)
			{
				*pIndices++=(WORD)((z-0)*size+(x));
				*pIndices++=(WORD)((z-step)*size+(x));
			}
				*pIndices++=(WORD)((z-0)*size+(size-1));
			*pIndices++=(WORD)((z-step)*size+(size-1));
			// insert additional degenerate to start next row
			*pIndices++=pIndices[-2];
			*pIndices++=pIndices[-1];
		}
		else
		{
			*pIndices++=(WORD)((z-step)*size+(size-1));
			*pIndices++=(WORD)((z-0)*size+(size-1));
			for (x=size-1; x>0; x-=step)
			{
				*pIndices++=(WORD)((z-step)*size+(x-step));
				*pIndices++=(WORD)((z-0)*size+(x-step));
			}
			// insert additional degenerate to start next row
			*pIndices++=pIndices[-1];
			*pIndices++=pIndices[-1];
		}

		s_toggle=!s_toggle;
	}
*/
	if (FAILED(hr=m_indexBufferD3D->Unlock())) return hr;

	return S_OK;
}

//-------------------------------------------------------------------------------------------------
/** Releases all w3d assets, to prepare for Reset device call. */
//-------------------------------------------------------------------------------------------------
void WaterRenderObjClass::ReleaseResources()
{
	delete m_backendWaterGeometry;
	m_backendWaterGeometry = nullptr;

	REF_PTR_RELEASE(m_indexBuffer);

	REF_PTR_RELEASE(m_pReflectionTexture);
	SAFE_RELEASE(m_vertexBufferD3D);
	SAFE_RELEASE(m_indexBufferD3D);

	if (m_dwWavePixelShader)
		m_pDev->DeletePixelShader(m_dwWavePixelShader);

	if (m_dwWaveVertexShader)
		m_pDev->DeleteVertexShader(m_dwWaveVertexShader);




	m_dwWavePixelShader=0;
	m_dwWaveVertexShader=0;
}

//-------------------------------------------------------------------------------------------------
/** (Re)allocates all W3D assets after a reset.. */
//-------------------------------------------------------------------------------------------------
void WaterRenderObjClass::ReAcquireResources()
{
	HRESULT hr;

	m_indexBuffer=NEW_REF(DX8IndexBufferClass,(6));
	// Fill up the IB
	{
		DX8IndexBufferClass::WriteLockClass lockIdxBuffer(m_indexBuffer);
		UnsignedShort *ib=lockIdxBuffer.Get_Index_Array();
		//quad of 2 triangles:
		//	3-----2
		//  |    /|
		//  |  /  |
		//	|/    |
		//  0-----1
		ib[0]=3;
		ib[1]=0;
		ib[2]=2;
		ib[3]=2;
		ib[4]=0;
		ib[5]=1;
	}

	m_pDev=DX8Wrapper::_Get_D3D_Device8();

	//We're using the same grid for either 3D Water Mesh or Pixel/Vertex shader.  Just
	//allocate the right size depending on usage
	// Grid water now uploads only CPU-owned physics heights through the backend.
	if (!m_meshData && m_waterType == WATER_TYPE_2_PVSHADER)
	{	//pixel/vertex shader based water assets.
		if (FAILED(hr=generateIndexBuffer(PATCH_SIZE,PATCH_SIZE)))
			return;

		if (FAILED(hr=generateVertexBuffer(PATCH_SIZE,PATCH_SIZE,sizeof(SEA_PATCH_VERTEX),true)))
			return;

		//shader decleration
		DWORD Declaration[]=
		{
			(D3DVSD_STREAM(0)),
			(D3DVSD_REG(0, D3DVSDT_FLOAT3)), // Position
			(D3DVSD_REG(1, D3DVSDT_D3DCOLOR)), // Diffuse
			(D3DVSD_REG(2, D3DVSDT_FLOAT2)), // Bump map texture
			(D3DVSD_END())
		};

		hr = W3DShaderManager::LoadAndCreateD3DShader("shaders\\wave.pso", &Declaration[0], 0, false, &m_dwWavePixelShader);
		if (FAILED(hr))
			return;

		hr = W3DShaderManager::LoadAndCreateD3DShader("shaders\\wave.vso", &Declaration[0], 0, true, &m_dwWaveVertexShader);
		if (FAILED(hr))
			return;

		// Create reflection texture
		m_pReflectionTexture = DX8Wrapper::Create_Render_Target (SEA_REFLECTION_SIZE, SEA_REFLECTION_SIZE);
	}


	//W3D Invalidate textures after losing the device and since we peek at the textures directly, it won't
	//know to reinit them for us.  Do it here manually:
	if (m_riverTexture && !m_riverTexture->Is_Initialized())
		m_riverTexture->Init();
	if (m_waterNoiseTexture && !m_waterNoiseTexture->Is_Initialized())
		m_waterNoiseTexture->Init();
	if (m_riverAlphaEdge && !m_riverAlphaEdge->Is_Initialized())
		m_riverAlphaEdge->Init();
	if (m_waterSparklesTexture && !m_waterSparklesTexture->Is_Initialized())
		m_waterSparklesTexture->Init();
	if (m_whiteTexture && !m_whiteTexture->Is_Initialized())
	{	m_whiteTexture->Init();
		SurfaceClass *surface=m_whiteTexture->Get_Surface_Level();
		int pitch;
		void *pBits = surface->Lock(&pitch);
		const unsigned int bytesPerPixel = surface->Get_Bytes_Per_Pixel();
		surface->Draw_Pixel(0, 0, 0xffffffff, bytesPerPixel, pBits, pitch);
		surface->Unlock();
		REF_PTR_RELEASE(surface);
	}
}

void WaterRenderObjClass::load()
{
	delete m_backendWaterGeometry;
	m_backendWaterGeometry = nullptr;
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
	if (type == WATER_TYPE_2_PVSHADER || (W3DShaderManager::getChipset() >= DC_GENERIC_PIXEL_SHADER_1_1))
	{	//geforce3 specific water requires some extra D3D assets
		m_pDev=DX8Wrapper::_Get_D3D_Device8();
		//save previous thumbnail mode
		bool thumbnails_enabled = WW3D::Get_Thumbnail_Enabled();
		WW3D::Set_Thumbnail_Enabled(false);

		//load bump map textures off disk
		TextureClass *pBumpSource;	//temporary textures in a format W3D understands
		TextureClass *pBumpSource2;	//temporary textures in a format W3D understands
		Int i;
		i=NUM_BUMP_FRAMES;
		while (i--)
		{
			char bump_name[128];

			sprintf(bump_name,"caust%.2d.tga",i);
			pBumpSource=WW3DAssetManager::Get_Instance()->Get_Texture(bump_name);
			sprintf(bump_name,"caustS%.2d.tga",i);
			pBumpSource2=WW3DAssetManager::Get_Instance()->Get_Texture(bump_name);
			initBumpMap(m_pBumpTexture+i, pBumpSource);
			initBumpMap(m_pBumpTexture2+i, pBumpSource2);
			WW3DAssetManager::Get_Instance()->Release_Texture(pBumpSource);
			WW3DAssetManager::Get_Instance()->Release_Texture(pBumpSource2);
			REF_PTR_RELEASE(pBumpSource);
			REF_PTR_RELEASE(pBumpSource2);
		}
		//restore previous thumpnail mode
		WW3D::Set_Thumbnail_Enabled(thumbnails_enabled);
	}
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
	m_whiteTexture=MSGNEW("TextureClass") TextureClass(1,1,WW3D_FORMAT_A4R4G4B4,MIP_LEVELS_1);
	SurfaceClass *surface=m_whiteTexture->Get_Surface_Level();
	int pitch;
	void *pBits = surface->Lock(&pitch);
	const unsigned int bytesPerPixel = surface->Get_Bytes_Per_Pixel();
	surface->Draw_Pixel(0, 0, 0xffffffff, bytesPerPixel, pBits, pitch);
	surface->Unlock();
	REF_PTR_RELEASE(surface);

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
	m_gridGeometryDirty=TRUE;

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
		SAFE_RELEASE(m_vertexBufferD3D);
		SAFE_RELEASE(m_indexBufferD3D);


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
            m_gridGeometryDirty=TRUE;
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


	DX8Wrapper::Set_Render_Target_With_Z((TextureClass*)m_pReflectionTexture);

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
	DX8Wrapper::Set_Render_Target((IDirect3DSurface8 *)nullptr);
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
	//Force render state apply so that the null texture gets applied to D3D, thus releasing shroud reference count.
	DX8Wrapper::Apply_Render_State_Changes();
	DX8Wrapper::Invalidate_Cached_Render_States();

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

	D3DMATRIX matProj, matView, matWW3D;

	//create a transform which will flip the y and z coordinates to fit our system
	memset(&matWW3D,0,sizeof(D3DMATRIX));
	matWW3D._11=1.0f;
	matWW3D._32=1.0f;
	matWW3D._23=1.0f;
	matWW3D._44=1.0f;

	DX8Wrapper::Set_Transform(D3DTS_WORLD,Transform);	//position the water surface
	DX8Wrapper::Set_Texture(0,nullptr);	//we'll be setting our own textures, so reset W3D
	DX8Wrapper::Set_Texture(1,nullptr);	//we'll be setting our own textures, so reset W3D


	DX8Wrapper::Apply_Render_State_Changes();	//force update of view and projection matrices

	Vector3 camTran;

	rinfo.Camera.Get_Transform().Get_Translation(&camTran);

	DX8Wrapper::_Get_DX8_Transform(D3DTS_VIEW, matView);
	DX8Wrapper::_Get_DX8_Transform(D3DTS_PROJECTION, matProj);

	//default setup from Kenny's demo
	m_pDev->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
	m_pDev->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
	m_pDev->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_MODULATE);
	m_pDev->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );
	m_pDev->SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );

	m_pDev->SetTextureStageState( 1, D3DTSS_COLORARG1, D3DTA_TEXTURE );
	m_pDev->SetTextureStageState( 1, D3DTSS_COLORARG2, D3DTA_CURRENT );
	m_pDev->SetTextureStageState( 1, D3DTSS_COLOROP,   D3DTOP_MODULATE);
	m_pDev->SetTextureStageState( 1, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );
	m_pDev->SetTextureStageState( 1, D3DTSS_TEXCOORDINDEX, 1 );

	m_pDev->SetTextureStageState( 2, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	m_pDev->SetTextureStageState( 2, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_PASSTHRU|2);

	m_pDev->SetTextureStageState( 3, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	m_pDev->SetTextureStageState( 3, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_PASSTHRU|3);

//	m_pDev->SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
//	m_pDev->SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
//	m_pDev->SetTextureStageState( 0, D3DTSS_MIPFILTER, D3DTEXF_POINT );

//	m_pDev->SetTextureStageState( 1, D3DTSS_MINFILTER, D3DTEXF_POINT );
//	m_pDev->SetTextureStageState( 1, D3DTSS_MAGFILTER, D3DTEXF_POINT );
//	m_pDev->SetTextureStageState( 1, D3DTSS_MIPFILTER, D3DTEXF_NONE );
	//end of default setup

	m_pDev->SetTextureStageState(0, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP);
	m_pDev->SetTextureStageState(0, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP);
	m_pDev->SetRenderState( D3DRS_WRAP0, D3DWRAP_U | D3DWRAP_V);

	m_pDev->SetTextureStageState(1, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
	m_pDev->SetTextureStageState(1, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);

	m_pDev->SetTexture( 0, m_pBumpTexture[(Int)m_fBumpFrame]);
#ifdef MIPMAP_BUMP_TEXTURE
	m_pDev->SetTextureStageState( 0, D3DTSS_MIPFILTER, D3DTEXF_POINT );
	m_pDev->SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
	m_pDev->SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
#endif
	m_pDev->SetTextureStageState( 1, D3DTSS_BUMPENVMAT00, F2DW(m_fBumpScale) );
	m_pDev->SetTextureStageState( 1, D3DTSS_BUMPENVMAT01, F2DW(0.0f) );
	m_pDev->SetTextureStageState( 1, D3DTSS_BUMPENVMAT10, F2DW(0.0f) );
	m_pDev->SetTextureStageState( 1, D3DTSS_BUMPENVMAT11, F2DW(m_fBumpScale) );
	m_pDev->SetTextureStageState( 1, D3DTSS_BUMPENVLSCALE, F2DW(1.0f) );
	m_pDev->SetTextureStageState( 1, D3DTSS_BUMPENVLOFFSET, F2DW(0.0f) );

	m_pDev->SetTextureStageState( 2, D3DTSS_COLOROP,   D3DTOP_DISABLE );
	m_pDev->SetTextureStageState( 2, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );

	m_pDev->SetRenderState(D3DRS_ZWRITEENABLE , FALSE);

	D3DMATRIX mat = {};

	mat._11 = 0.5f; mat._12 = -0.5f; mat._13 = 0.5f;   mat._14=0.5f;
	mat._21 = 0.5f; mat._22 = 0.5f; mat._23 = 0.0f;   mat._24=0.0f;
	mat._31 = 0.0f; mat._32 = 0.0f; mat._33 = 0.0f;   mat._34=1.0f;
	mat._41 = 0.0f; mat._42 = 0.0f; mat._43 = 0.0f;   mat._44=1.0f;

	m_pDev->SetVertexShaderConstant(CV_TEXPROJ_0, &mat, 4);

	// Setup constants
	const Vector4 zeroConstant(0.0f, 0.0f, 0.0f, 0.0f);
	const Vector4 oneConstant(1.0f, 1.0f, 1.0f, 1.0f);
	m_pDev->SetVertexShaderConstant(CV_ZERO, &zeroConstant.X, 1);
	m_pDev->SetVertexShaderConstant(CV_ONE, &oneConstant.X, 1);

	m_pDev->SetVertexShader(m_dwWaveVertexShader);
	m_pDev->SetPixelShader(m_dwWavePixelShader);

//	Make reflection brighter to compensate for darker coloring on sea floor
//	m_pDev->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_ONE );
//	m_pDev->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_SRCCOLOR );

	m_pDev->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
	m_pDev->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );

	m_pDev->SetRenderState(D3DRS_ALPHABLENDENABLE , TRUE);
	m_pDev->SetTexture( 1, m_pReflectionTexture->Peek_D3D_Texture());

//	m_pDev->SetRenderState(D3DRS_FILLMODE,D3DFILL_WIREFRAME);//LORENZEN

	Int patchX,patchY,startX,startY;

	D3DMATRIX patchMatrix = {};
	patchMatrix._11=PATCH_SCALE;
	patchMatrix._22=1.0f;
	patchMatrix._33=PATCH_SCALE;
	patchMatrix._44=1.0f;

	m_pDev->SetStreamSource(0,m_vertexBufferD3D,sizeof(WaterRenderObjClass::SEA_PATCH_VERTEX));
	m_pDev->SetIndices(m_indexBufferD3D,0);

	for (startY=patchY=(seaBox.Center.Y-seaBox.Extent.Y)/(PATCH_WIDTH*PATCH_SCALE); (patchY*PATCH_WIDTH*PATCH_SCALE)<(seaBox.Center.Y+seaBox.Extent.Y); patchY++)
	{
		for (startX=patchX=(seaBox.Center.X-seaBox.Extent.X)/(PATCH_WIDTH*PATCH_SCALE); (patchX*PATCH_WIDTH*PATCH_SCALE)<(seaBox.Center.X+seaBox.Extent.X); patchX++)
		{
			D3DMATRIX matWorldViewProj = {};
			patchMatrix._41=(float)(patchX*PATCH_WIDTH*PATCH_SCALE );
			patchMatrix._43=(float)(patchY*PATCH_WIDTH*PATCH_SCALE );
			Build_Water_World_View_Projection_Constants(
				matWorldViewProj, patchMatrix, matWW3D, matView, matProj);
			m_pDev->SetVertexShaderConstant(CV_WORLDVIEWPROJ_0, &matWorldViewProj, 4);	//pass transform matrix into shader

			m_pDev->DrawIndexedPrimitive(D3DPT_TRIANGLESTRIP,0,m_numVertices,0,m_numIndices);
		}
	}
//	m_pDev->SetRenderState(D3DRS_FILLMODE,D3DFILL_SOLID);
	m_pDev->SetRenderState(D3DRS_ALPHABLENDENABLE , FALSE);
	m_pDev->SetTexture( 0, nullptr);	//release reference to bump texture
	m_pDev->SetTexture( 1, nullptr);	//release reference to reflection texture
	m_pDev->SetTexture( 2, nullptr);	//release reference to reflection texture

	m_pDev->SetTextureStageState( 0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	m_pDev->SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_PASSTHRU|0);
	m_pDev->SetTextureStageState( 1, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	m_pDev->SetTextureStageState( 1, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_PASSTHRU|1);
	m_pDev->SetRenderState(D3DRS_ZWRITEENABLE , TRUE);

	m_pDev->SetTextureStageState(1, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP);
	m_pDev->SetTextureStageState(1, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP);

	m_pDev->SetRenderState( D3DRS_WRAP0, 0);	//turn off texture wrapping

	m_pDev->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_DISABLE );
	m_pDev->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );
	m_pDev->SetTextureStageState( 1, D3DTSS_COLOROP,   D3DTOP_DISABLE );
	m_pDev->SetTextureStageState( 1, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );
	m_pDev->SetTextureStageState( 2, D3DTSS_COLOROP,   D3DTOP_DISABLE );
	m_pDev->SetTextureStageState( 2, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );

	//Restore old transforms
	DX8Wrapper::_Set_DX8_Transform(D3DTS_VIEW, matView);
	DX8Wrapper::_Set_DX8_Transform(D3DTS_PROJECTION, matProj);

	m_pDev->SetPixelShader(0);	//turn off pixel shader
	m_pDev->SetVertexShader(DX8_FVF_XYZDUV1);	//turn off custom vertex shader

	DX8Wrapper::Invalidate_Cached_Render_States();

	if (TheTerrainRenderObject->getShroud())
	{
		//do second pass to apply the shroud on water plane
		W3DShaderManager::setTexture(0,TheTerrainRenderObject->getShroud()->getShroudTexture());
		W3DShaderManager::setShader(W3DShaderManager::ST_SHROUD_TEXTURE, 0);
		m_pDev->SetStreamSource(0,m_vertexBufferD3D,sizeof(WaterRenderObjClass::SEA_PATCH_VERTEX));
		m_pDev->SetIndices(m_indexBufferD3D,0);
		for (startY=patchY=(seaBox.Center.Y-seaBox.Extent.Y)/(PATCH_WIDTH*PATCH_SCALE); (patchY*PATCH_WIDTH*PATCH_SCALE)<(seaBox.Center.Y+seaBox.Extent.Y); patchY++)
		{
			for (startX=patchX=(seaBox.Center.X-seaBox.Extent.X)/(PATCH_WIDTH*PATCH_SCALE); (patchX*PATCH_WIDTH*PATCH_SCALE)<(seaBox.Center.X+seaBox.Extent.X); patchX++)
			{
				patchMatrix._41=(float)(patchX*PATCH_WIDTH*PATCH_SCALE);
				patchMatrix._43=(float)(patchY*PATCH_WIDTH*PATCH_SCALE);

				const D3DMATRIX matTemp = Build_Water_World_Matrix(patchMatrix, matWW3D);

				DX8Wrapper::_Set_DX8_Transform(D3DTS_WORLD, matTemp);

				m_pDev->DrawIndexedPrimitive(D3DPT_TRIANGLESTRIP,0,m_numVertices,0,m_numIndices);
			}
		}
		W3DShaderManager::resetShader(W3DShaderManager::ST_SHROUD_TEXTURE);
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
							drawTrapezoidWater(points,pTrig->getID(),k,r);
							points[0].Z += (FEATHER_THICKNESS/TheGlobalData->m_featherWater);
						}
					}

					else
						drawTrapezoidWater(points,pTrig->getID(),k,0);


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


	VertexMaterialClass *vmat=VertexMaterialClass::Get_Preset(VertexMaterialClass::PRELIT_DIFFUSE);
	DX8Wrapper::Set_Material(vmat);
	REF_PTR_RELEASE(vmat);

	ShaderClass m_shader2=ShaderClass::_PresetOpaqueShader;
	m_shader2.Set_Cull_Mode(ShaderClass::CULL_MODE_DISABLE);
	m_shader2.Set_Depth_Compare(ShaderClass::PASS_ALWAYS);	//no need to check against z-buffer, sky always rendered first.
	m_shader2.Set_Depth_Mask(ShaderClass::DEPTH_WRITE_DISABLE);	//sky is always behind everything so no need to update z-buffer

	DX8Wrapper::Set_Shader(m_shader2);

	DX8Wrapper::Set_Texture(0,setting->skyTexture);

	//draw an infinite sky plane
	DynamicVBAccessClass vb_access(BUFFER_TYPE_DYNAMIC_DX8,dynamic_fvf_type,4);
	{
		DynamicVBAccessClass::WriteLockClass lock(&vb_access);
		VertexFormatXYZNDUV2* verts=lock.Get_Formatted_Vertex_Array();
		if(verts)
		{
			verts[0].x=-SKYPLANE_SIZE;
			verts[0].y=SKYPLANE_SIZE;
			verts[0].z=SKYPLANE_HEIGHT;
			verts[0].u1=m_uOffset;
			verts[0].v1=fv;
			verts[0].diffuse=setting->vertex01Diffuse;

			verts[1].x=SKYPLANE_SIZE;
			verts[1].y=SKYPLANE_SIZE;
			verts[1].z=SKYPLANE_HEIGHT;
			verts[1].u1=fu;
			verts[1].v1=fv;
			verts[1].diffuse=setting->vertex11Diffuse;

			verts[2].x=SKYPLANE_SIZE;
			verts[2].y=-SKYPLANE_SIZE;
			verts[2].z=SKYPLANE_HEIGHT;
			verts[2].u1=fu;
			verts[2].v1=m_vOffset;
			verts[2].diffuse=setting->vertex10Diffuse;

			verts[3].x=-SKYPLANE_SIZE;
			verts[3].y=-SKYPLANE_SIZE;
			verts[3].z=SKYPLANE_HEIGHT;
			verts[3].u1=m_uOffset;
			verts[3].v1=m_vOffset;
			verts[3].diffuse=setting->vertex00Diffuse;
		}
	}

	DX8Wrapper::Set_Index_Buffer(m_indexBuffer,0);
	DX8Wrapper::Set_Vertex_Buffer(vb_access);

	Matrix3D tm(1);
	tm.Set_Translation(Vector3(0,0,0));
	DX8Wrapper::Set_Transform(D3DTS_WORLD,tm);

	DX8Wrapper::Draw_Triangles(	0,2, 0,	4);	//draw a quad, 2 triangles, 4 verts
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


	DX8Wrapper::Set_Transform(D3DTS_WORLD,tm);


	VertexMaterialClass *vmat=VertexMaterialClass::Get_Preset(VertexMaterialClass::PRELIT_DIFFUSE);
	DX8Wrapper::Set_Material(vmat);
	REF_PTR_RELEASE(vmat);

	ShaderClass m_shader2=ShaderClass::_PresetAlphaShader;
	m_shader2.Set_Cull_Mode(ShaderClass::CULL_MODE_DISABLE);
	m_shader2.Set_Depth_Compare(ShaderClass::PASS_ALWAYS);	//no need to check against z-buffer, sky always rendered first.
	m_shader2.Set_Depth_Mask(ShaderClass::DEPTH_WRITE_DISABLE);	//sky is always behind everything so no need to update z-buffer

	DX8Wrapper::Set_Shader(m_shader2);


//	DX8Wrapper::Set_Shader(ShaderClass::/*_PresetAdditiveShader*//*_PresetOpaqueShader*/_PresetAlphaShader);
//	DX8Wrapper::Set_Texture(0,setting->skyBodyTexture);

	DX8Wrapper::Set_Texture(0,m_alphaClippingTexture);

	//draw an infinite sky plane
	DynamicVBAccessClass vb_access(BUFFER_TYPE_DYNAMIC_DX8,dynamic_fvf_type,4);
	{
		DynamicVBAccessClass::WriteLockClass lock(&vb_access);
		VertexFormatXYZNDUV2* verts=lock.Get_Formatted_Vertex_Array();
		if(verts)
		{
			verts[0].x=-SKYBODY_SIZE;
			verts[0].y=SKYBODY_SIZE;
			verts[0].z=0;
			verts[0].u2=0;
			verts[0].v2=1;
			verts[0].diffuse=0xffffffff;

			verts[1].x=SKYBODY_SIZE;
			verts[1].y=SKYBODY_SIZE;
			verts[1].z=0;
			verts[1].u2=1;
			verts[1].v2=1;
			verts[1].diffuse=0xffffffff;

			verts[2].x=SKYBODY_SIZE;
			verts[2].y=-SKYBODY_SIZE;
			verts[2].z=0;
			verts[2].u2=1;
			verts[2].v2=0;
			verts[2].diffuse=0xffffffff;

			verts[3].x=-SKYBODY_SIZE;
			verts[3].y=-SKYBODY_SIZE;
			verts[3].z=0;
			verts[3].u2=0;
			verts[3].v2=0;
			verts[3].diffuse=0xffffffff;
		}
	}

	DX8Wrapper::Set_Index_Buffer(m_indexBuffer,0);
	DX8Wrapper::Set_Vertex_Buffer(vb_access);

	DX8Wrapper::Draw_Triangles(	0,2, 0,	4);	//draw a quad, 2 triangles, 4 verts
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
    if (!m_doWaterGrid || !m_meshData) return;
    RenderBackendMaterialState material;
    RenderBackendTerrainState terrain;
    RenderBackendTextureHandle texture;
    if (!prepareWaterMaterial(material, terrain, texture,false)) return;
    auto *backend = WW3D::Get_Render_Backend();
    auto &cache = *m_backendWaterGeometry;
    const int mx = m_gridCellsX + 1, my = m_gridCellsY + 1;
    if (mx < 2 || my < 2 || static_cast<unsigned long long>(mx) * my > 65535 ||
        m_gridCellSize <= 0 || m_meshDataSize < static_cast<unsigned long long>(mx+2)*(my+2)) {
        cache.failure("invalid water grid dimensions"); return;
    }
    if (cache.grid_x != mx || cache.grid_y != my || cache.grid_cell_size != m_gridCellSize) {
        backend->Release_Static_Geometry(cache.grid_geometry);
        cache.grid_geometry = {};
        cache.grid_has_heights = false;
        cache.grid_vertices.resize(static_cast<std::size_t>(mx)*my);
        cache.grid_indices.clear();
        // The old strip [next-row,current-row,...] alternated these windings.
        // Connector degenerates are omitted, retaining each initialized triangle.
        for (int y=0; y<my-1; ++y) for (int x=0; x<mx-1; ++x) {
            const unsigned short a=y*mx+x, b=(y+1)*mx+x, c=a+1, d=b+1;
            cache.grid_indices.insert(cache.grid_indices.end(), {b,a,d, d,a,c});
        }
        for (int y=0; y<my; ++y) for (int x=0; x<mx; ++x)
            cache.grid_vertices[y*mx+x] = {x*m_gridCellSize,y*m_gridCellSize,0,
                1,1,1,1,0,0,0,0,0,0,0,0};
        cache.grid_x=mx; cache.grid_y=my; cache.grid_cell_size=m_gridCellSize;
    }
    // Heights retain their existing CPU simulation/Xfer owner. Rendering UVs,
    // world transforms, noise and shroud projection are evaluated in HLSL.
    if (m_gridGeometryDirty || !cache.grid_has_heights) {
        for (int y=0; y<my; ++y) for (int x=0; x<mx; ++x)
            cache.grid_vertices[y*mx+x].z = m_meshData[(y+1)*(mx+2)+x+1].height;
        backend->Release_Static_Geometry(cache.grid_geometry);
        cache.grid_geometry = {};
        cache.grid_has_heights = true;
        m_gridGeometryDirty = FALSE;
    }
    const unsigned int diffuse = m_settings[m_tod].waterDiffuse;
    terrain.use_constant_color = true;
    terrain.constant_color[0] = ((diffuse>>16)&255)/255.f;
    terrain.constant_color[1] = ((diffuse>>8)&255)/255.f;
    terrain.constant_color[2] = (diffuse&255)/255.f;
    terrain.constant_color[3] = ((static_cast<int>((diffuse>>24)&255)-32)&255)/255.f;
    terrain.flat_water_animation = 3;
    terrain.flat_water_uv_scale = m_settings[m_tod].waterRepeatCount/128.f/10.f*.2f;
    terrain.flat_water_phase_scale = PI/(8*MAP_XY_FACTOR);
    for (int row=0; row<3; ++row) for (int column=0; column<4; ++column)
        terrain.world_transform[row*4+column] = Transform[row][column];
    bool submitted = false;
    if (m_meshInMotion) {
        // Changing simulation heights use the existing per-frame upload path;
        // don't allocate/wait on persistent buffers on every physics update.
        submitted = backend->Draw_Indexed_Terrain_Triangles(cache.grid_vertices.data(),
            static_cast<unsigned>(cache.grid_vertices.size()), cache.grid_indices.data(),
            static_cast<unsigned>(cache.grid_indices.size()), texture, material, terrain);
    } else {
        if (!backend->Is_Geometry_Valid(cache.grid_geometry))
            cache.grid_geometry = backend->Create_Static_Indexed_Terrain_Geometry(cache.grid_vertices.data(),
                static_cast<unsigned>(cache.grid_vertices.size()), cache.grid_indices.data(),
                static_cast<unsigned>(cache.grid_indices.size()));
        submitted = backend->Draw_Static_Indexed_Terrain_Geometry(cache.grid_geometry,texture,material,terrain);
    }
    if (!submitted) cache.failure("D3D12 water grid submission failed");
    else cache.warned = false;
}

inline void WaterRenderObjClass::setGridVertexHeight(Int x, Int y, Real value)
{
	m_gridGeometryDirty=TRUE;
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
	m_gridGeometryDirty=TRUE;
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
    if (m_disableRiver) return;
    m_drawingRiver = true;
    if (!pTrig) return;
    RenderBackendMaterialState material;
    RenderBackendTerrainState terrain;
    RenderBackendTextureHandle texture;
    if (!prepareWaterMaterial(material,terrain,texture,true)) return;
    auto *backend = WW3D::Get_Render_Backend();
    auto &cache = *m_backendWaterGeometry;
    const Int count = pTrig->getNumPoints(), riverStart = pTrig->getRiverStart();
    if (count < 4 || count > 65535 || riverStart < 0 || riverStart >= count-1) {
        cache.failure("invalid river polygon"); return;
    }
    auto &river = cache.rivers[pTrig->getID()];
    bool same = river.river_start == riverStart && river.points.size() == static_cast<std::size_t>(count);
    for (Int i=0; same && i<count; ++i) {
        const auto &point = *pTrig->getPoint(i), &old = river.points[i];
        same = point.x == old.x && point.y == old.y && point.z == old.z;
    }
    if (!same) {
        backend->Release_Static_Geometry(river.geometry);
        river.geometry = {};
        river.points.clear(); river.vertices.clear(); river.indices.clear();
        river.colors.clear(); river.river_start = -1;
        Real totalLen=0, endLen=0;
        for (Int i=0; i<count-1; ++i) {
            const auto &a = *pTrig->getPoint(i), &b = *pTrig->getPoint(i+1);
            const Real dx = static_cast<Real>(static_cast<std::int64_t>(a.x)-b.x);
            const Real dy = static_cast<Real>(static_cast<std::int64_t>(a.y)-b.y);
            const Real length = sqrt(dx*dx+dy*dy);
            totalLen += length;
            if (i == riverStart) endLen = length;
        }
        const Int rectangleCount = count/2-1;
        const Real vScale = ((totalLen/2)-endLen)/endLen/rectangleCount;
        if (!(endLen > 0) || !std::isfinite(vScale)) {
            cache.failure("degenerate river width"); return;
        }
        for (Int i=0; i<count; ++i) river.points.push_back(*pTrig->getPoint(i));
        river.river_start = riverStart;
        river.vertices.reserve((rectangleCount+1)*2);
        river.indices.reserve(rectangleCount*6);
        Int inner = riverStart, outer = inner+1;
        for (Int i=0; i<=rectangleCount; ++i) {
            const auto &innerPt = river.points[outer], &outerPt = river.points[inner];
            // Positions and longitudinal progress are invariant under scrolling.
            // VSTerrain applies the original wobble to both independent UV sets.
            river.vertices.push_back({static_cast<Real>(innerPt.x),static_cast<Real>(innerPt.y),static_cast<Real>(innerPt.z),
                1,1,1,1, .5f,vScale*i, 1,vScale*i, 0,0,0,0});
            river.vertices.push_back({static_cast<Real>(outerPt.x),static_cast<Real>(outerPt.y),static_cast<Real>(outerPt.z),
                1,1,1,1, 0,vScale*i, 0,vScale*i, 0,0,0,0});
            if (++outer == count) outer = 0;
            if (--inner < 0) inner = count-1;
            if (i < rectangleCount) {
                const unsigned short a=static_cast<unsigned short>(i*2);
                river.indices.insert(river.indices.end(), {a,static_cast<unsigned short>(a+1),static_cast<unsigned short>(a+3),
                    a,static_cast<unsigned short>(a+3),static_cast<unsigned short>(a+2)});
            }
        }
        river.colors.resize(river.vertices.size());
        river.dirty = true;
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

	UnsignedInt diffuse=static_cast<UnsignedInt>(REAL_TO_INT(shadeB)) | (static_cast<UnsignedInt>(REAL_TO_INT(shadeG)) << 8) | (static_cast<UnsignedInt>(REAL_TO_INT(shadeR)) << 16);

	//Keep diffuse from lighting calculations but substitute custom alpha
	diffuse |= m_settings[m_tod].waterDiffuse & 0xff000000;	//copy alpha/opacity from ini setting


    // Keep the existing source-cell lookup and packed-byte quantization. Smooth
    // pixel shroud sampling would change river edge brightness and opacity.
    W3DShroud *shroud = TheTerrainRenderObject ? TheTerrainRenderObject->getShroud() : nullptr;
    for (std::size_t i=0; i<river.vertices.size(); ++i) {
        auto &vertex = river.vertices[i];
        const UnsignedInt color = getRiverVertexDiffuse(shroud,vertex.x,vertex.y,shadeR,shadeG,shadeB,diffuse);
        if (!same || river.colors[i] != color) {
            river.colors[i] = color;
            vertex.r = ((color>>16)&255)/255.f; vertex.g = ((color>>8)&255)/255.f;
            vertex.b = (color&255)/255.f; vertex.a = ((color>>24)&255)/255.f;
            river.dirty = true;
        }
    }
    if (!backend->Is_Geometry_Valid(river.geometry)) {
        river.geometry = backend->Create_Static_Indexed_Terrain_Geometry(river.vertices.data(),
            static_cast<unsigned int>(river.vertices.size()),river.indices.data(),static_cast<unsigned int>(river.indices.size()));
        if (!backend->Is_Geometry_Valid(river.geometry)) { cache.failure("river GPU geometry unavailable"); return; }
        river.dirty = false;
    } else if (river.dirty) {
        // Same queue ordering and frame upload lifetime; no resource recreation
        // or CPU fence wait when quantized lighting/shroud colors change.
        if (!backend->Update_Static_Indexed_Terrain_Geometry(river.geometry,river.vertices.data(),
            static_cast<unsigned int>(river.vertices.size()))) { cache.failure("river GPU color update failed"); return; }
        river.dirty = false;
    }
    material.cull = RenderBackendCullMode::None;
    material.wireframe = wireframeForDebug;
    material.source_blend = RenderBackendBlendFactor::SourceAlpha;
    material.destination_blend = TheWaterTransparency->m_additiveBlend ?
        RenderBackendBlendFactor::One : RenderBackendBlendFactor::InverseSourceAlpha;
    if (!backend->Draw_Static_Indexed_Terrain_Geometry(river.geometry,texture,material,terrain))
        cache.failure("river GPU draw rejected");
    else cache.warned = false;
}

bool WaterRenderObjClass::prepareWaterMaterial(RenderBackendMaterialState &material,
    RenderBackendTerrainState &terrain, RenderBackendTextureHandle &texture, bool river)
{
    auto *backend = WW3D::Get_Render_Backend();
    if (!backend || !backend->Is_Device_Ready()) return false;
    if (!m_backendWaterGeometry || m_backendWaterGeometry->owner != backend) {
        delete m_backendWaterGeometry;
        m_backendWaterGeometry = new BackendWaterGeometry;
        m_backendWaterGeometry->owner = backend;
    }
    auto fail = [&]() { m_backendWaterGeometry->failure("water texture/material unavailable"); return false; };
    if (!TheWaterTransparency || !m_riverTexture || !m_waterSparklesTexture || !m_waterNoiseTexture) return fail();
    const ShaderClass &shader = TheWaterTransparency->m_additiveBlend ?
        ShaderClass::_PresetAdditiveShader : ShaderClass::_PresetAlphaShader;
    if (!shader.Get_Render_Backend_State(material)) return fail();
    material.depth_test = RenderBackendDepthTest::LessEqual;
    material.depth_write = false;
    auto &filter = m_riverTexture->Get_Filter();
    filter.Set_Mag_Filter(TextureFilterClass::FILTER_TYPE_BEST);
    filter.Set_Min_Filter(TextureFilterClass::FILTER_TYPE_BEST);
    filter.Set_Mip_Mapping(TextureFilterClass::FILTER_TYPE_BEST);
    if (!m_riverTexture->Ensure_Renderer_Texture() || !filter.Get_Render_Sampler(material.sampler,0) ||
        !m_waterSparklesTexture->Ensure_Renderer_Texture() ||
        !m_waterSparklesTexture->Get_Filter().Get_Render_Sampler(terrain.cloud_sampler,1) ||
        !m_waterNoiseTexture->Ensure_Renderer_Texture() ||
        !m_waterNoiseTexture->Get_Filter().Get_Render_Sampler(terrain.noise_sampler,2)) return fail();
    material.sampler.min_filter = material.sampler.mag_filter = RenderBackendTextureFilter::Linear;
    terrain.cloud_sampler.min_filter = terrain.cloud_sampler.mag_filter = RenderBackendTextureFilter::Linear;
    terrain.noise_sampler.min_filter = terrain.noise_sampler.mag_filter = RenderBackendTextureFilter::Linear;
    material.sampler.max_anisotropy = terrain.cloud_sampler.max_anisotropy = terrain.noise_sampler.max_anisotropy = 1;
    terrain.cloud_sampler.address_u = terrain.cloud_sampler.address_v = RenderBackendTextureAddress::Wrap;
    terrain.noise_sampler.address_u = terrain.noise_sampler.address_v = RenderBackendTextureAddress::Wrap;
    texture = m_riverTexture->Get_Renderer_Texture();
    terrain.cloud_texture = m_waterSparklesTexture->Get_Renderer_Texture();
    terrain.noise_texture = m_waterNoiseTexture->Get_Renderer_Texture();
    terrain.project_world_coordinates = true;
    terrain.flat_water = !river;
    terrain.river_water = river;
    terrain.cloud_noise_projection[0] = NOISE_REPEAT_FACTOR;
    terrain.cloud_noise_projection[1] = terrain.cloud_noise_projection[2] = m_riverVOrigin;
    if (river) {
        if (!m_riverAlphaEdge || !m_riverAlphaEdge->Ensure_Renderer_Texture() ||
            !m_riverAlphaEdge->Get_Filter().Get_Render_Sampler(terrain.shroud_sampler,3)) return fail();
        terrain.shroud_texture = m_riverAlphaEdge->Get_Renderer_Texture();
        terrain.shroud_sampler.min_filter = terrain.shroud_sampler.mag_filter = RenderBackendTextureFilter::Linear;
        terrain.shroud_sampler.max_anisotropy = 1;
        terrain.shroud_sampler.address_u = terrain.shroud_sampler.address_v = RenderBackendTextureAddress::Wrap;
    } else if (TheTerrainRenderObject && TheTerrainRenderObject->getShroud()) {
        W3DShroudMaterialPassClass pass;
        RenderBackendMaterialState shroud_material;
        RenderBackendTerrainState shroud_projection;
        TextureClass *shroud_texture = nullptr;
        if (!pass.Prepare_Terrain_Pass(shroud_material, shroud_projection, shroud_texture) ||
            !shroud_texture->Ensure_Renderer_Texture()) return fail();
        terrain.shroud_texture = shroud_texture->Get_Renderer_Texture();
        terrain.shroud_sampler = shroud_material.sampler;
        for (int i=0; i<4; ++i) terrain.shroud_projection[i] = shroud_projection.shroud_projection[i];
    }
    return true;
}

//-------------------------------------------------------------------------------------------------
//Draw a 4 sided flat water area.
//-------------------------------------------------------------------------------------------------
void WaterRenderObjClass::drawTrapezoidWater(Vector3 points[4], Int triggerID, Int sector, Int layer)
{
    RenderBackendMaterialState material;
    RenderBackendTerrainState terrain;
    RenderBackendTextureHandle texture;
    if (!prepareWaterMaterial(material,terrain,texture,false)) return;
    auto *backend = WW3D::Get_Render_Backend();
    auto &cache = *m_backendWaterGeometry;
    // Stable local trigger/sector/layer identity bounds the cache when water
    // heights move. No native pointer is serialized or hashed as an asset key.
    auto *patch = &cache.patches[std::make_tuple(triggerID,sector,layer)];
    bool same = true;
    for (int i=0; i<4; ++i) same = same && patch->points[i] == points[i];
    if (!same) {
        backend->Release_Static_Geometry(patch->geometry);
        patch->geometry = {};
        for (int i=0; i<4; ++i) patch->points[i] = points[i];
    }
    if (!backend->Is_Geometry_Valid(patch->geometry)) {
        const Vector3 origin=points[0], uVec1=points[1]-origin, vVec1=points[3]-origin;
        const Vector3 uVec2=points[2]-points[3], vVec2=points[2]-points[1];
        const int uCount=std::min(50,std::max(1,static_cast<int>((uVec1.Length()+uVec2.Length())/(8*MAP_XY_FACTOR))))+1;
        const int vCount=std::min(50,std::max(1,static_cast<int>((vVec1.Length()+vVec2.Length())/(8*MAP_XY_FACTOR))))+1;
        std::vector<RenderBackendTerrainVertex> vertices;
        std::vector<unsigned short> indices;
        vertices.reserve(uCount*vCount); indices.reserve((uCount-1)*(vCount-1)*6);
        const Real constE=1.f/(vCount-1), constF=1.f/(uCount-1);
        for (int y=0; y<vCount; ++y) for (int x=0; x<uCount; ++x) {
            const Real dv=y*constE, du=x*constF;
            Vector3 position=origin;
            position += uVec1*du; position += vVec1*dv;
            position += (dv)*(du)*(vVec2-vVec1);
            vertices.push_back({position.X,position.Y,position.Z,1,1,1,1,0,0,0,0,0,0,0,0});
        }
        for (int y=0; y<vCount-1; ++y) for (int x=0; x<uCount-1; ++x) {
            const unsigned short a=y*uCount+x, b=(y+1)*uCount+x, c=a+1, d=b+1;
            indices.insert(indices.end(), {a,d,b, a,c,d});
        }
        patch->geometry=backend->Create_Static_Indexed_Terrain_Geometry(vertices.data(),
            static_cast<unsigned>(vertices.size()),indices.data(),static_cast<unsigned>(indices.size()));
        if (!patch->geometry.Is_Valid()) { cache.failure("flat-water geometry upload failed"); return; }
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

    const unsigned int packed = static_cast<unsigned int>(diffuse);
    terrain.use_constant_color = true;
    terrain.constant_color[0] = ((packed>>16)&255)/255.f;
    terrain.constant_color[1] = ((packed>>8)&255)/255.f;
    terrain.constant_color[2] = (packed&255)/255.f;
    terrain.constant_color[3] = ((packed>>24)&255)/255.f;
    if (TheGlobalData->m_featherWater) {
        const int alphaByLayers[]{0,255,200,140,110,80};
        const int layers=TheGlobalData->m_featherWater;
        terrain.constant_color[3] = layers>=1 && layers<=5 ? alphaByLayers[layers]/255.f : 0.f;
    }
    terrain.flat_water_animation = TheGlobalData->m_featherWater ? 2 : 1;
    terrain.flat_water_uv_scale = 1.f/150.f;
    terrain.flat_water_phase_scale = PI/(4*MAP_XY_FACTOR);
    material.cull = RenderBackendCullMode::None;
    if (TheGlobalData->m_showSoftWaterEdge && TheWaterTransparency->m_transparentWaterDepth != 0) {
        // The D3D12 output is RGBA8; destination alpha is shoreline coverage.
        material.source_blend = RenderBackendBlendFactor::DestinationAlpha;
        if (!TheWaterTransparency->m_additiveBlend)
            material.destination_blend = RenderBackendBlendFactor::InverseDestinationAlpha;
    }
    if (!backend->Draw_Static_Indexed_Terrain_Geometry(patch->geometry,texture,material,terrain))
        cache.failure("D3D12 flat-water submission failed");
    else cache.warned = false;
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
	if (xfer->getXferMode() == XFER_LOAD) m_gridGeometryDirty=TRUE;

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


