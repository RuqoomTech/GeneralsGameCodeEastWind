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

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>
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
#include "WW3D2/statistics.h"
#include "WW3D2/shader.h"
#include "WW3D2/IRenderBackend.h"
#include "WW3D2/ww3d.h"
#include <vector>
#include <algorithm>
#include <map>
#include <tuple>
#include <cstdio>
#include <cmath>
#include <cstdint>
#include <exception>
#include <stdexcept>
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

// Retained sea-patch dimensions and authored bump tiling.
#define PATCH_SIZE 15		//number of vertices on patch edge.  Large patches may waste vertices off edge of screen.
#define PATCH_UV_TILES	42	//number of times the bump map texture is tiled across patch (must be integer!).
#define PATCH_SCALE (4.0f * MAP_XY_FACTOR)	//horizontal scale factor. Adjust this and size to get desired vertex density.
#define SEA_REFLECTION_SIZE 256		//dimensions of reflection texture

#define PATCH_WIDTH (PATCH_SIZE-1)	//internal defines
#define PATCH_UV_SCALE	((Real)PATCH_UV_TILES/(Real)PATCH_WIDTH)

// Deterministic water-grid physics retains its existing spacing and sizes.
#define WATER_MESH_X_VERTICES 128
#define WATER_MESH_Y_VERTICES 128
#define WATER_MESH_SPACING MAP_XY_FACTOR
#define DRAW_WATER_WAKES

#define NOISE_REPEAT_FACTOR ((float)(1.0f/(16.0f)))

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
    bool grid_dirty = true;
    int grid_x = 0, grid_y = 0;
    float grid_cell_size = 0;
    bool warned = false;
    bool sea_assets_invalid = false;
    bool sea_ready = false;
    bool reflection_ready = false;
    bool reflection_initialized = false;
    bool sea_warned = false;
    bool reflection_failed = false;
    RenderBackendGeometryHandle sea_geometry;
    RenderBackendGeometryHandle sky_geometry[TIME_OF_DAY_COUNT];
    RenderBackendGeometryHandle moon_geometry;
    void seaFailure(const char *reason)
    {
        if (!sea_warned) std::fprintf(stderr, "WaterRenderObjClass sea: %s\n", reason);
        sea_warned = true;
        reflection_failed = true;
    }

    ~BackendWaterGeometry()
    {
        if (owner && owner == WW3D::Get_Render_Backend()) {
            for (auto &entry : patches) owner->Release_Static_Geometry(entry.second.geometry);
            for (auto &entry : rivers) owner->Release_Static_Geometry(entry.second.geometry);
            owner->Release_Static_Geometry(grid_geometry);
            owner->Release_Static_Geometry(sea_geometry);
            for (auto geometry : sky_geometry) owner->Release_Static_Geometry(geometry);
            owner->Release_Static_Geometry(moon_geometry);
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

WaterRenderObjClass::BackendWaterGeometry *WaterRenderObjClass::getBackendGeometry()
{
    auto *backend=WW3D::Get_Render_Backend();
    if (!backend || !backend->Is_Device_Ready()) return nullptr;
    if (!m_backendWaterGeometry || m_backendWaterGeometry->owner!=backend) {
        delete m_backendWaterGeometry;
        m_backendWaterGeometry=new BackendWaterGeometry;
        m_backendWaterGeometry->owner=backend;
    }
    return m_backendWaterGeometry;
}

bool WaterRenderObjClass::ensureSeaResources()
{
    auto *cache=getBackendGeometry();
    if (!cache) return false;
    if (cache->sea_assets_invalid) return false;
    if (cache->sea_ready && cache->owner->Is_Geometry_Valid(cache->sea_geometry) &&
        m_reflectionTexture && m_reflectionTexture->Get_Renderer_Texture().Is_Valid()) return true;
    try {
        for (unsigned frame=0;frame<NUM_BUMP_FRAMES;++frame) {
            if (!m_bumpTextures[frame]) {
                char name[32]; std::snprintf(name,sizeof(name),"caust%02u.tga",frame);
                auto source=RefCountPtr<TextureClass>::Create_No_Add_Ref(WW3DAssetManager::Get_Instance()->Get_Texture(name));
                const bool prepared=source && initBumpMap(m_bumpTextures[frame],source.Peek());
                // Signed frames own their data. Drop the temporary grayscale
                // asset cache reference as the original loader did.
                if (source) WW3DAssetManager::Get_Instance()->Release_Texture(source.Peek());
                if (!prepared) {
                    cache->sea_assets_invalid=true;
                    cache->seaFailure(name); cache->sea_ready=false; return false;
                }
            }
            if (!m_bumpTextures[frame]->Ensure_Renderer_Texture()) {
                cache->seaFailure("bump frame upload failed"); cache->sea_ready=false; return false;
            }
        }
        if (!m_reflectionTexture || !m_reflectionTexture->Get_Renderer_Texture().Is_Valid()) {
            REF_PTR_RELEASE(m_reflectionTexture);
            m_reflectionTexture=WW3D::Create_Render_Texture(SEA_REFLECTION_SIZE,SEA_REFLECTION_SIZE,RenderBackendRenderDepth::Owned);
            cache->reflection_ready=false;
            cache->reflection_initialized=false;
        }
        if (!m_reflectionTexture) { cache->seaFailure("owned-depth reflection target unavailable"); cache->sea_ready=false; return false; }
        if (!cache->owner->Is_Geometry_Valid(cache->sea_geometry)) {
            std::vector<RenderBackendTerrainVertex> vertices(PATCH_SIZE*PATCH_SIZE);
            std::vector<unsigned short> indices;
            indices.reserve((PATCH_SIZE-1)*(PATCH_SIZE-1)*6);
            for (unsigned y=0;y<PATCH_SIZE;++y) for (unsigned x=0;x<PATCH_SIZE;++x) {
                auto &v=vertices[y*PATCH_SIZE+x];
                v.x=static_cast<float>(x); v.y=static_cast<float>(y);
                v.r=v.g=v.b=v.a=1;
                v.u=x*PATCH_UV_SCALE; v.v=y*PATCH_UV_SCALE;
                if (x+1<PATCH_SIZE && y+1<PATCH_SIZE) {
                    const unsigned short a=y*PATCH_SIZE+x,b=a+1,c=a+PATCH_SIZE,d=c+1;
                    // Expand the original strip c,a,d,b using its a-d diagonal.
                    indices.insert(indices.end(),{c,a,d,d,a,b});
                }
            }
            cache->sea_geometry=cache->owner->Create_Static_Indexed_Terrain_Geometry(vertices.data(),
                static_cast<unsigned>(vertices.size()),indices.data(),static_cast<unsigned>(indices.size()));
        }
        cache->sea_ready=cache->owner->Is_Geometry_Valid(cache->sea_geometry);
        if (!cache->sea_ready) cache->seaFailure("retained sea geometry unavailable");
        return cache->sea_ready;
    } catch (...) {
        cache->sea_ready=false;
        cache->seaFailure("sea resource preparation failed");
        return false;
    }
}

//-------------------------------------------------------------------------------------------------
/** Destructor. Releases w3d assets. */
//-------------------------------------------------------------------------------------------------
WaterRenderObjClass::~WaterRenderObjClass()
{
	REF_PTR_RELEASE(m_skyBodyTexture);
	REF_PTR_RELEASE (m_skyBox);

	REF_PTR_RELEASE (m_riverTexture);
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
	{	REF_PTR_RELEASE(m_bumpTextures[i]);
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
	m_backendWaterGeometry=nullptr;
	m_gridGeometryDirty=TRUE;
	m_waterTrackSystem = nullptr;
	m_doWaterGrid = FALSE;
	m_skyBodyTexture=nullptr;
	m_useCloudLayer=true;
	m_waterType = WATER_TYPE_0_TRANSLUCENT;
	m_tod=TIME_OF_DAY_AFTERNOON;
	m_reflectionTexture=nullptr;
	m_skyBox=nullptr;

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
		m_bumpTextures[i]=nullptr;

	m_riverVOrigin=0;
	m_riverTexture=nullptr;
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
/** Prepares signed RG mip gradients from the authored grayscale bump source. */
//-------------------------------------------------------------------------------------------------
bool WaterRenderObjClass::initBumpMap(TextureClass *&texture, TextureClass *source)
{
    if (!source) return false;
    std::vector<RefCountPtr<SurfaceClass>> levels;
    for (unsigned level = 0; level < 15; ++level) {
        auto surface = RefCountPtr<SurfaceClass>::Create_No_Add_Ref(source->Get_Surface_Level(level));
        if (!surface) break;
        SurfaceClass::SurfaceDescription desc{};
        surface->Get_Description(desc);
        if (!desc.Width || !desc.Height || surface->Get_Bytes_Per_Pixel() != 4) return false;
        levels.push_back(surface);
        if (desc.Width == 1 && desc.Height == 1) break;
    }
    if (levels.empty() || source->Is_Missing_Texture()) return false;
    SurfaceClass::SurfaceDescription first{};
    levels[0]->Get_Description(first);
    auto prepared = RefCountPtr<TextureClass>::Create_No_Add_Ref(new TextureClass(first.Width, first.Height,
        WW3D_FORMAT_U8V8, static_cast<MipCountType>(levels.size()), TextureClass::POOL_MANAGED, false, false));
    for (unsigned level = 0; level < levels.size(); ++level) {
        SurfaceClass::SurfaceDescription desc{};
        levels[level]->Get_Description(desc);
        auto dest = RefCountPtr<SurfaceClass>::Create_No_Add_Ref(prepared->Get_Surface_Level(level));
        if (!dest) return false;
        int src_pitch = 0, dst_pitch = 0;
        const auto *src = static_cast<const SurfaceClass *>(levels[level].Peek())->Peek_CPU_Pixels(&src_pitch);
        auto *dst = static_cast<unsigned char *>(dest->Lock(&dst_pitch));
        if (!src || !dst || src_pitch < static_cast<int>(desc.Width * 4) ||
            dst_pitch < static_cast<int>(desc.Width * 2)) { if (dst) dest->Unlock(); return false; }
        auto height = [&](unsigned x, unsigned y) {
            return 256 - static_cast<int>(src[static_cast<std::size_t>(y) * src_pitch + x * 4]);
        };
        for (unsigned y = 0; y < desc.Height; ++y) {
            const unsigned up = y ? y - 1 : y, down = std::min(y + 1, desc.Height - 1);
            for (unsigned x = 0; x < desc.Width; ++x) {
                // Retain the interior native valley/gradient arithmetic. Both
                // horizontal edges clamp, replacing the old out-of-bounds reads.
                const int center = height(x, y), left = height(x ? x - 1 : x, y);
                const int right = height(std::min(x + 1, desc.Width - 1), y);
                int du = left - right;
                const int dv = height(x, up) - height(x, down);
                if (center < left && center < right) du = std::max(left - center, center - right);
                auto *pixel = dst + static_cast<std::size_t>(y) * dst_pitch + x * 2;
                pixel[0] = static_cast<unsigned char>(du);
                pixel[1] = static_cast<unsigned char>(dv);
            }
        }
        dest->Unlock();
    }
    prepared->Get_Filter().Set_Min_Filter(TextureFilterClass::FILTER_TYPE_BEST);
    prepared->Get_Filter().Set_Mag_Filter(TextureFilterClass::FILTER_TYPE_BEST);
    prepared->Get_Filter().Set_Mip_Mapping(TextureFilterClass::FILTER_TYPE_FAST);
    prepared->Add_Ref();
    REF_PTR_RELEASE(texture);
    texture = prepared.Peek();
    return true;
}

//-------------------------------------------------------------------------------------------------
/** Releases all w3d assets, to prepare for Reset device call. */
//-------------------------------------------------------------------------------------------------
void WaterRenderObjClass::ReleaseResources()
{
    delete m_backendWaterGeometry;
    m_backendWaterGeometry = nullptr;
    REF_PTR_RELEASE(m_reflectionTexture);
}

//-------------------------------------------------------------------------------------------------
/** (Re)allocates all W3D assets after a reset.. */
//-------------------------------------------------------------------------------------------------
void WaterRenderObjClass::ReAcquireResources()
{
    if (m_waterType == WATER_TYPE_2_PVSHADER) ensureSeaResources();
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

	// The real type-2 caller loads its signed bump frames through the shared
	// CPU texture owner and D3D12 upload seam, rather than native shader assets.
	m_skyBodyTexture = WW3DAssetManager::Get_Instance()->Get_Texture(SKYBODY_TEXTURE);

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

	m_waterNoiseTexture=WW3DAssetManager::Get_Instance()->Get_Texture("Noise0000.tga");
	m_riverAlphaEdge=WW3DAssetManager::Get_Instance()->Get_Texture("TWAlphaEdge.tga");
	m_waterSparklesTexture=WW3DAssetManager::Get_Instance()->Get_Texture("WaterSurfaceBubbles.tga");
#ifdef DRAW_WATER_WAKES
	m_waterTrackSystem = NEW WaterTracksRenderSystem;
	m_waterTrackSystem->init();
#endif

	return type == WATER_TYPE_2_PVSHADER && !ensureSeaResources() ? -1 : 0;
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
	{
		//water type has changed, must allocate necessary assets for new water.
		//contains the current deformed water surface z(height) values.  With 1 vertex invisible border
		//around surface to speed up normal calculations.
		m_meshDataSize = (m_gridCellsX+1+2)*(m_gridCellsY+1+2);
		m_meshData=NEW WaterMeshData[ m_meshDataSize ];
		memset(m_meshData,0,sizeof(WaterMeshData)*(m_gridCellsX+1+2)*(m_gridCellsY+1+2));
		reset();

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
}

//-------------------------------------------------------------------------------------------------
/**Copies GDF settings dealing with a particular time of day into our own
	* structures.  Also allocates any required W3D assets (textures). */
//-------------------------------------------------------------------------------------------------
void WaterRenderObjClass::loadSetting( Setting *setting, TimeOfDay timeOfDay )
{
	SurfaceClass::SurfaceDescription surfaceDesc{};

	// sanity
	DEBUG_ASSERTCRASH( setting, ("WaterRenderObjClass::loadSetting, null setting") );

	// textures
	setting->skyTexture = WW3DAssetManager::Get_Instance()->Get_Texture( WaterSettings[ timeOfDay ].m_skyTextureFile.str() );
	setting->waterTexture = WW3DAssetManager::Get_Instance()->Get_Texture( WaterSettings[ timeOfDay ].m_waterTextureFile.str() );

	// texelss per unit
	setting->skyTexelsPerUnit = WaterSettings[ timeOfDay ].m_skyTexelsPerUnit;
	if (setting->waterTexture) setting->waterTexture->Get_Level_Description( surfaceDesc, 0 );
	setting->skyTexelsPerUnit = surfaceDesc.Width ? setting->skyTexelsPerUnit / (Real)surfaceDesc.Width : 0.0f;

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
/** Render water's reflected scene before the main output consumes its texture. */
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
    if (Debug_Statistics::Extended_Stats.m_disableWater) return;
#endif
    if (!cam || !m_parentScene || !ensureSeaResources()) return;
    auto &cache = *m_backendWaterGeometry;
    cache.reflection_ready = false;
    cache.reflection_failed = false;
    auto *backend = cache.owner;
    const Matrix3D old_transform = cam->Get_Transform();
    Vector2 old_min, old_max;
    cam->Get_Viewport(old_min, old_max);
    const bool old_culling = ShaderClass::Is_Backface_Culling_Inverted();
    const unsigned old_mask = backend->Get_Pass_Color_Write_Mask();
    Matrix3D reflected = old_transform;
    // Reflect camera basis and translation across the unchanged horizontal plane.
    for (int column = 0; column < 3; ++column) reflected[2][column] = -old_transform[2][column];
    reflected[2][3] = 2.0f * m_level - old_transform[2][3];
    if (!WW3D::Set_Render_Texture(m_reflectionTexture, RenderBackendRenderDepth::Owned)) {
        cache.seaFailure("reflection target selection failed"); return;
    }
    bool rendering = false;
    bool cleanup_attempted = false;
    std::exception_ptr fatal_error;
    auto end_render = [&]() {
        // A throwing End_Render may already have closed/submitted the list.
        // Mark the attempt first so cleanup never submits it a second time.
        rendering = false;
        try {
            if (WW3D::End_Render(false) != WW3D_ERROR_OK)
                throw std::runtime_error("water reflection submission failed");
            cache.reflection_initialized = true;
        } catch (...) {
            fatal_error = std::current_exception();
            throw;
        }
    };
    auto restore = [&]() {
        cleanup_attempted = true;
        try {
            ShaderClass::Invert_Backface_Culling(old_culling);
            const bool mask_restored = backend->Set_Pass_Color_Write_Mask(old_mask);
            cam->Set_Transform(old_transform);
            cam->Set_Viewport(old_min, old_max);
            const bool output_restored = WW3D::Set_Render_Texture(nullptr);
            cam->Apply();
            if (!mask_restored || !output_restored)
                throw std::runtime_error("water reflection output restoration failed");
        } catch (...) {
            if (!fatal_error) fatal_error = std::current_exception();
            throw;
        }
    };
    try {
        // Native reflection targets were XRGB. Initialize A=1 once and keep
        // every reflected draw RGB-only, retaining subsequent depth-only clears.
        if (WW3D::Begin_Render(!cache.reflection_initialized, true, Vector3(0,0,0), 1.0f) != WW3D_ERROR_OK) {
            restore(); cache.seaFailure("reflection begin failed"); return;
        }
        rendering = true;
        backend->Set_Pass_Color_Write_Mask(7);
        cam->Set_Transform(reflected);
        cam->Set_Viewport(Vector2(0,0), Vector2(1,1));
        cam->Apply();
        ShaderClass::Invert_Backface_Culling(true);
        renderSky();
        if (m_tod == TIME_OF_DAY_NIGHT) renderSkyBody(&reflected);
        const auto result = WW3D::Render(m_parentScene, cam);
        end_render();
        restore();
        cache.reflection_ready = result == WW3D_ERROR_OK && !cache.reflection_failed;
        if (!cache.reflection_ready) cache.seaFailure("reflection scene rendering failed");
    } catch (...) {
        cache.reflection_ready = false;
        // Record failed cleanup, finish independent state restoration, then
        // propagate it. An ordinary render exception gets one End attempt.
        if (rendering) { try { end_render(); } catch (...) {} }
        if (!cleanup_attempted) { try { restore(); } catch (...) {} }
        cache.seaFailure("reflection rendering failed");
        if (fatal_error) std::rethrow_exception(fatal_error);
    }
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
	if (Debug_Statistics::Extended_Stats.m_disableWater) {
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
            // The unused framebuffer mirror implementation is retired; keep
            // this historical configuration value's actual surface caller.
            renderWater();
            break;

		default:
			break;
	}

	if (TheGlobalData && TheGlobalData->m_drawSkyBox && m_skyBox)
	{	//center skybox around camera
		Vector3 pos=rinfo.Camera.Get_Position();
		pos.Z = TheGlobalData->m_skyBoxPositionZ;
		m_skyBox->Set_Position(pos);
		m_skyBox->Render(rinfo);
	}

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
/** Draws retained sea patches through the backend's signed-bump reflection shader. */
//-------------------------------------------------------------------------------------------------
void WaterRenderObjClass::drawSea(RenderInfoClass &rinfo)
{
    AABoxClass sea_box;
    if (!getClippedWaterPlane(&rinfo.Camera, &sea_box) || !ensureSeaResources()) return;
    auto &cache = *m_backendWaterGeometry;
    if (!cache.reflection_ready) { cache.seaFailure("reflection has no rendered scene"); return; }
    const int frame = std::clamp(static_cast<int>(m_fBumpFrame), 0, NUM_BUMP_FRAMES - 1);
    auto *bump = m_bumpTextures[frame];
    if (!bump || !bump->Ensure_Renderer_Texture()) { cache.seaFailure("signed bump frame unavailable"); return; }
    RenderBackendMaterialState material;
    if (!ShaderClass::_PresetAlphaShader.Get_Render_Backend_State(material) ||
        !m_reflectionTexture->Get_Filter().Get_Render_Sampler(material.sampler, 1)) return;
    material.cull = RenderBackendCullMode::None;
    material.depth_write = false;
    material.sampler.address_u = material.sampler.address_v = RenderBackendTextureAddress::Clamp;
    material.sampler.min_filter = material.sampler.mag_filter = RenderBackendTextureFilter::Linear;
    material.sampler.mipmaps = false;
    material.sampler.max_anisotropy = 1;
    RenderBackendTerrainState terrain;
    terrain.project_world_coordinates = terrain.sea_water = terrain.use_constant_color = true;
    if (!bump->Get_Filter().Get_Render_Sampler(terrain.shroud_sampler, 0)) return;
    terrain.shroud_sampler.address_u = terrain.shroud_sampler.address_v = RenderBackendTextureAddress::Wrap;
    terrain.shroud_sampler.min_filter = terrain.shroud_sampler.mag_filter = RenderBackendTextureFilter::Linear;
    terrain.shroud_sampler.mip_filter = RenderBackendTextureFilter::Point;
    terrain.shroud_sampler.max_anisotropy = 1;
    terrain.shroud_texture = bump->Get_Renderer_Texture();
    const UnsignedInt diffuse = m_settings[m_tod].transparentWaterDiffuse;
    terrain.constant_color[0] = ((diffuse >> 16) & 255) / 255.f;
    terrain.constant_color[1] = ((diffuse >> 8) & 255) / 255.f;
    terrain.constant_color[2] = (diffuse & 255) / 255.f;
    terrain.constant_color[3] = ((diffuse >> 24) & 255) / 255.f;
    terrain.world_transform[0] = terrain.world_transform[5] = PATCH_SCALE;
    terrain.world_transform[11] = m_level;
    const float extent = PATCH_WIDTH * PATCH_SCALE;
    const int start_x = static_cast<int>((sea_box.Center.X - sea_box.Extent.X) / extent);
    const int start_y = static_cast<int>((sea_box.Center.Y - sea_box.Extent.Y) / extent);
    auto patches = [&](const RenderBackendMaterialState &state, RenderBackendTerrainState &projection,
        RenderBackendTextureHandle texture) {
        for (int y = start_y; y * extent < sea_box.Center.Y + sea_box.Extent.Y; ++y)
            for (int x = start_x; x * extent < sea_box.Center.X + sea_box.Extent.X; ++x) {
                projection.world_transform[3] = x * extent;
                projection.world_transform[7] = y * extent;
                if (!cache.owner->Draw_Static_Indexed_Terrain_Geometry(cache.sea_geometry, texture, state, projection)) return false;
            }
        return true;
    };
    if (!patches(material, terrain, m_reflectionTexture->Get_Renderer_Texture())) {
        cache.seaFailure("sea GPU submission failed"); return;
    }
    if (TheTerrainRenderObject && TheTerrainRenderObject->getShroud()) {
        W3DShroudMaterialPassClass pass;
        RenderBackendMaterialState shroud_material;
        RenderBackendTerrainState shroud_projection;
        TextureClass *texture = nullptr;
        if (!pass.Prepare_Terrain_Pass(shroud_material, shroud_projection, texture) ||
            !texture || !texture->Ensure_Renderer_Texture()) { cache.seaFailure("sea shroud unavailable"); return; }
        shroud_projection.world_transform[0] = shroud_projection.world_transform[5] = PATCH_SCALE;
        shroud_projection.world_transform[11] = m_level;
        if (!patches(shroud_material, shroud_projection, texture->Get_Renderer_Texture()))
            cache.seaFailure("sea projected shroud submission failed");
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
    Setting *setting = &m_settings[m_tod];
    auto *cache = getBackendGeometry();
    if (!cache || !setting->skyTexture || setting->skyTexture->Is_Missing_Texture() ||
        !std::isfinite(setting->skyTexelsPerUnit) ||
        !setting->skyTexture->Ensure_Renderer_Texture()) {
        if (cache) cache->seaFailure("sky texture or texel density unavailable"); return;
    }
    const UnsignedInt now = timeGetTime();
    const UnsignedInt elapsed = now - static_cast<UnsignedInt>(m_LastUpdateTime);
    m_LastUpdateTime = static_cast<Int>(now);
    m_uOffset += elapsed * setting->uScrollPerMs * setting->skyTexelsPerUnit;
    m_vOffset += elapsed * setting->vScrollPerMs * setting->skyTexelsPerUnit;
    m_uOffset -= static_cast<Real>(static_cast<Int>(m_uOffset));
    m_vOffset -= static_cast<Real>(static_cast<Int>(m_vOffset));
    auto &geometry = cache->sky_geometry[m_tod];
    if (!cache->owner->Is_Geometry_Valid(geometry)) {
        RenderBackendTerrainVertex vertices[4]{};
        const float x[4]{-SKYPLANE_SIZE, SKYPLANE_SIZE, SKYPLANE_SIZE, -SKYPLANE_SIZE};
        const float y[4]{SKYPLANE_SIZE, SKYPLANE_SIZE, -SKYPLANE_SIZE, -SKYPLANE_SIZE};
        const UnsignedInt colors[4]{setting->vertex01Diffuse, setting->vertex11Diffuse,
            setting->vertex10Diffuse, setting->vertex00Diffuse};
        for (unsigned i = 0; i < 4; ++i) {
            vertices[i].x=x[i]; vertices[i].y=y[i]; vertices[i].z=SKYPLANE_HEIGHT;
            vertices[i].r=((colors[i]>>16)&255)/255.f;
            vertices[i].g=((colors[i]>>8)&255)/255.f;
            vertices[i].b=(colors[i]&255)/255.f;
            vertices[i].a=((colors[i]>>24)&255)/255.f;
        }
        const unsigned short indices[6]{3,0,2,2,0,1};
        geometry=cache->owner->Create_Static_Indexed_Terrain_Geometry(vertices,4,indices,6);
    }
    RenderBackendMaterialState material;
    if (!ShaderClass::_PresetOpaqueShader.Get_Render_Backend_State(material) ||
        !setting->skyTexture->Get_Filter().Get_Render_Sampler(material.sampler,0)) {
        cache->seaFailure("sky material unavailable"); return;
    }
    material.cull=RenderBackendCullMode::None;
    material.depth_test=RenderBackendDepthTest::Always;
    material.depth_write=false;
    RenderBackendTerrainState terrain;
    terrain.project_world_coordinates=terrain.project_base_to_cloud=true;
    terrain.cloud_noise_projection[0]=setting->skyTexelsPerUnit;
    terrain.cloud_noise_projection[1]=m_uOffset+SKYPLANE_SIZE*setting->skyTexelsPerUnit;
    terrain.cloud_noise_projection[2]=m_vOffset+SKYPLANE_SIZE*setting->skyTexelsPerUnit;
    if (!cache->owner->Draw_Static_Indexed_Terrain_Geometry(geometry,setting->skyTexture->Get_Renderer_Texture(),material,terrain))
        cache->seaFailure("sky GPU submission failed");
}

//-------------------------------------------------------------------------------------------------
/** Renders (draws) the sky body.  Used for moon and sun.  We rotate the image
	* so that it always faces the camera.  This removes perspective and helps hide that
	* it's a flat image. */
//-------------------------------------------------------------------------------------------------
///	@todo: Add code to render properly sorted sun sky body.
void WaterRenderObjClass::renderSkyBody(Matrix3D *mat)
{
    auto *cache=getBackendGeometry();
    if (!cache || !mat || !m_skyBodyTexture || m_skyBodyTexture->Is_Missing_Texture() ||
        !m_skyBodyTexture->Ensure_Renderer_Texture()) {
        if (cache) cache->seaFailure("moon texture unavailable"); return;
    }
    if (!cache->owner->Is_Geometry_Valid(cache->moon_geometry)) {
        RenderBackendTerrainVertex vertices[4]{};
        const float x[4]{-SKYBODY_SIZE,SKYBODY_SIZE,SKYBODY_SIZE,-SKYBODY_SIZE};
        const float y[4]{SKYBODY_SIZE,SKYBODY_SIZE,-SKYBODY_SIZE,-SKYBODY_SIZE};
        // The original moon uses UV2. Map those complete authored coordinates
        // to this caller's base binding, rather than using uninitialized UV1.
        const float u[4]{0,1,1,0}, v[4]{1,1,0,0};
        for (unsigned i=0;i<4;++i) {
            vertices[i].x=x[i]; vertices[i].y=y[i];
            vertices[i].r=vertices[i].g=vertices[i].b=vertices[i].a=1;
            vertices[i].u=u[i]; vertices[i].v=v[i];
        }
        const unsigned short indices[6]{3,0,2,2,0,1};
        cache->moon_geometry=cache->owner->Create_Static_Indexed_Terrain_Geometry(vertices,4,indices,6);
    }
    Vector3 camera;
    mat->Get_Translation(&camera);
    Vector3 view=camera-Vector3(SKYBODY_X,SKYBODY_Y,SKYBODY_HEIGHT);
    if (view.Length2()<=1e-12f) return;
    view.Normalize();
    const Vector3 up(0,0,-1);
    Vector3 axis;
    Vector3::Cross_Product(up,view,&axis);
    const float angle=std::acos(std::clamp(Vector3::Dot_Product(up,view),-1.f,1.f));
    if (axis.Length2()>1e-12f) axis.Normalize(); else axis.Set(1,0,0);
    Matrix3D world(1);
    world.Set(axis,angle);
    world.Adjust_Translation(Vector3(SKYBODY_X,SKYBODY_Y,SKYBODY_HEIGHT));
    RenderBackendMaterialState material;
    if (!ShaderClass::_PresetAlphaShader.Get_Render_Backend_State(material) ||
        !m_skyBodyTexture->Get_Filter().Get_Render_Sampler(material.sampler,0)) {
        cache->seaFailure("moon material unavailable"); return;
    }
    material.cull=RenderBackendCullMode::None;
    material.depth_test=RenderBackendDepthTest::Always;
    material.depth_write=false;
    RenderBackendTerrainState terrain;
    terrain.project_world_coordinates=true;
    for (unsigned row=0;row<3;++row) for (unsigned col=0;col<4;++col)
        terrain.world_transform[row*4+col]=world[row][col];
    if (!cache->owner->Draw_Static_Indexed_Terrain_Geometry(cache->moon_geometry,m_skyBodyTexture->Get_Renderer_Texture(),material,terrain))
        cache->seaFailure("moon GPU submission failed");
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
        if (cache.grid_x != mx || cache.grid_y != my) {
            // Different row topology requires different index data.
            backend->Release_Static_Geometry(cache.grid_geometry);
            cache.grid_geometry = {};
        }
        cache.grid_has_heights = false;
        cache.grid_dirty = true;
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
        cache.grid_dirty = true;
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
    if (!backend->Is_Geometry_Valid(cache.grid_geometry)) {
        cache.grid_geometry = backend->Create_Static_Indexed_Terrain_Geometry(cache.grid_vertices.data(),
            static_cast<unsigned>(cache.grid_vertices.size()), cache.grid_indices.data(),
            static_cast<unsigned>(cache.grid_indices.size()));
        if (!backend->Is_Geometry_Valid(cache.grid_geometry)) { cache.failure("water grid GPU geometry unavailable"); return; }
        cache.grid_dirty = false;
    } else if (cache.grid_dirty) {
        // Height changes copy only vertices on the active command list. The
        // retained index buffer and handle survive motion without fence waits.
        if (!backend->Update_Static_Indexed_Terrain_Geometry(cache.grid_geometry,cache.grid_vertices.data(),
            static_cast<unsigned>(cache.grid_vertices.size()))) { cache.failure("water grid GPU height update failed"); return; }
        cache.grid_dirty = false;
    }
    const bool submitted = backend->Draw_Static_Indexed_Terrain_Geometry(cache.grid_geometry,texture,material,terrain);
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
	{
		//resolution has changed
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
    bool sameXY = river.river_start == riverStart && river.points.size() == static_cast<std::size_t>(count);
    bool same = sameXY;
    for (Int i=0; sameXY && i<count; ++i) {
        const auto &point = *pTrig->getPoint(i), &old = river.points[i];
        sameXY = point.x == old.x && point.y == old.y;
        same = same && sameXY && point.z == old.z;
    }
    if (!same && sameXY) {
        // Scripted water-height changes leave lengths, UV progress and indices
        // intact. Update cached Z and the same GPU vertex buffer asynchronously.
        for (Int i=0; i<count; ++i) river.points[i].z = pTrig->getPoint(i)->z;
        Int inner = riverStart, outer = inner+1;
        for (std::size_t i=0; i<river.vertices.size()/2; ++i) {
            river.vertices[i*2].z = static_cast<Real>(river.points[outer].z);
            river.vertices[i*2+1].z = static_cast<Real>(river.points[inner].z);
            if (++outer == count) outer = 0;
            if (--inner < 0) inner = count-1;
        }
        river.dirty = true;
    } else if (!same) {
        // RiverStart and XY changes preserve the index pattern when the paired
        // vertex count is unchanged, so they also use the queued vertex update.
        if (river.vertices.size() != static_cast<std::size_t>((count/2)*2)) {
            backend->Release_Static_Geometry(river.geometry);
            river.geometry = {};
        }
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
		{
			//special case where we disable lighting
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
    if (!getBackendGeometry()) return false;
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
		{
			//special case where we disable lighting
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


