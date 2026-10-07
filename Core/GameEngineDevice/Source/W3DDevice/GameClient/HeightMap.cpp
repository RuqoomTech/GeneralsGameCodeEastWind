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

// FILE: Heightmap.cpp ////////////////////////////////////////////////
//-----------------------------------------------------------------------------
//
//                       Westwood Studios Pacific.
//
//                       Confidential Information
//                Copyright (C) 2001 - All Rights Reserved
//
//-----------------------------------------------------------------------------
//
// Project:   RTS3
//
// File name: Heightmap.cpp
//
// Created:   Mark W., John Ahlquist, April/May 2001
//
// Desc:      Draw the terrain and scorchmarks in a scene.
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//         Includes
//-----------------------------------------------------------------------------
#include "W3DDevice/GameClient/HeightMap.h"

#ifndef USE_FLAT_HEIGHT_MAP // Flat height map uses flattened textures. jba. [3/20/2003]

#include <stdlib.h>
#include <WW3D2/assetmgr.h>
#include <WW3D2/texture.h>
#include <WWMath/tri.h>
#include <WWMath/colmath.h>
#include <WW3D2/coltest.h>
#include <WW3D2/rinfo.h>
#include <WW3D2/camera.h>
#include "Common/GlobalData.h"
#include "Common/PerfTimer.h"

#include "GameClient/TerrainVisual.h"
#include "GameClient/View.h"
#include "GameClient/Water.h"

#include "GameLogic/AIPathfind.h"
#include "GameLogic/TerrainLogic.h"
#include "W3DDevice/GameClient/TerrainTex.h"
#include "W3DDevice/GameClient/W3DDynamicLight.h"
#include "W3DDevice/GameClient/W3DScene.h"
#include "W3DDevice/GameClient/W3DTerrainTracks.h"
#include "W3DDevice/GameClient/W3DBibBuffer.h"
#include "W3DDevice/GameClient/W3DTreeBuffer.h"
#include "W3DDevice/GameClient/W3DPropBuffer.h"
#include "W3DDevice/GameClient/W3DRoadBuffer.h"
#include "W3DDevice/GameClient/W3DBridgeBuffer.h"
#include "W3DDevice/GameClient/W3DWaypointBuffer.h"
#include "W3DDevice/GameClient/WorldHeightMap.h"
#include "W3DDevice/GameClient/W3DShaderManager.h"
#include "W3DDevice/GameClient/W3DShadow.h"
#include "W3DDevice/GameClient/W3DWater.h"
#include "W3DDevice/GameClient/W3DShroud.h"
#include "WW3D2/light.h"
#include "WW3D2/IRenderBackend.h"
#include "WW3D2/ww3d.h"
#include "WWMath/matrix4.h"
#include <vector>
#include <cstddef>
#include <cstdio>
#include "WW3D2/scene.h"
#include "W3DDevice/GameClient/W3DPoly.h"
#include "W3DDevice/GameClient/W3DCustomScene.h"

#include "Common/UnitTimings.h" //Contains the DO_UNIT_TIMINGS define jba.



HeightMapRenderObjClass *TheHeightMap = nullptr;
//-----------------------------------------------------------------------------
//         Private Data
//-----------------------------------------------------------------------------
#define SC_DETAIL_BLEND ( SHADE_CNST(ShaderClass::PASS_LEQUAL, ShaderClass::DEPTH_WRITE_ENABLE, ShaderClass::COLOR_WRITE_ENABLE, ShaderClass::SRCBLEND_ONE, \
	ShaderClass::DSTBLEND_ZERO, ShaderClass::FOG_DISABLE, ShaderClass::GRADIENT_MODULATE, ShaderClass::SECONDARY_GRADIENT_DISABLE, ShaderClass::TEXTURING_ENABLE, \
	ShaderClass::ALPHATEST_DISABLE, ShaderClass::CULL_MODE_ENABLE, ShaderClass::DETAILCOLOR_SCALE, ShaderClass::DETAILALPHA_DISABLE) )

static ShaderClass detailOpaqueShader(SC_DETAIL_BLEND);

#define DEFAULT_MAX_FRAME_EXTRABLEND_TILES		256	//default number of terrain tiles rendered per call (must fit in one VB)
#define DEFAULT_MAX_MAP_EXTRABLEND_TILES		2048	//default size of array allocated to hold all map extra blend tiles.
#define DEFAULT_MAX_BATCH_SHORELINE_TILES		512	//maximum number of terrain tiles rendered per call (must fit in one VB)
#define DEFAULT_MAX_MAP_SHORELINE_TILES		4096	//default size of array allocated to hold all map shoreline tiles.

#define ADJUST_FROM_INDEX_TO_REAL(k) ((k-m_map->getBorderSizeInline())*MAP_XY_FACTOR)
inline Int IABS(Int x) {	if (x>=0) return x; return -x;};

struct HeightMapRenderObjClass::GeometryState
{
    struct Tile {
        RenderBackendGeometryHandle geometry;
        bool dirty = true;
    };
    std::vector<TerrainLightingVertex> current;
    std::vector<Tile> tiles;
    IRenderBackend *backend = nullptr;

    void release(Tile &tile) {
        if (backend && backend == WW3D::Get_Render_Backend()) {
            backend->Release_Static_Geometry(tile.geometry);
        }
        tile.geometry = {};
    }
    ~GeometryState() { for (auto &tile : tiles) release(tile); }
};

//-----------------------------------------------------------------------------
//         Private Functions
//-----------------------------------------------------------------------------

//=============================================================================
// HeightMapRenderObjClass::freeIndexVertexBuffers
//=============================================================================
/** Frees the w3d resources used to draw the terrain. */
//=============================================================================
void HeightMapRenderObjClass::freeIndexVertexBuffers()
{
	delete m_geometry;
	m_geometry = nullptr;

	delete[] m_vertexBufferBackup;
	m_vertexBufferBackup = nullptr;

	m_numVertexBufferTiles = 0;
}

//=============================================================================
// HeightMapRenderObjClass::freeMapResources
//=============================================================================
/** Frees the w3d resources used to draw the terrain. */
//=============================================================================
Int HeightMapRenderObjClass::freeMapResources()
{
	BaseHeightMapRenderObjClass::freeMapResources();
	freeIndexVertexBuffers();

	return 0;
}

//=============================================================================

TerrainLightingVertex *HeightMapRenderObjClass::getVertexBufferTile(Int x, Int y)
{
	return m_geometry->current.data() + (y*m_numVBTilesX+x)*HEIGHTMAP_VERTEX_NUM;
}

void HeightMapRenderObjClass::markTileGeometryDirty(Int x, Int y)
{
    if (m_geometry && x>=0 && y>=0 && x<m_numVBTilesX && y<m_numVBTilesY)
        m_geometry->tiles[y*m_numVBTilesX+x].dirty = true;
}

//=============================================================================
VERTEX_FORMAT *HeightMapRenderObjClass::getVertexBufferBackup(Int x, Int y)
{
	return m_vertexBufferBackup + y*m_numVBTilesX*HEIGHTMAP_VERTEX_NUM + x*HEIGHTMAP_VERTEX_NUM;
}

//=============================================================================
// HeightMapRenderObjClass::doTheDynamicLight
//=============================================================================
/** Calculates the diffuse lighting as affected by dynamic lighting. */
//=============================================================================
UnsignedInt HeightMapRenderObjClass::doTheDynamicLight(VERTEX_FORMAT *vb, VERTEX_FORMAT *vbMirror, Vector3*light, Vector3*normal,  W3DDynamicLight *pLights[], Int numLights)
{
	Real shadeR, shadeG, shadeB;
	Int diffuse = vbMirror->diffuse;
#ifdef RTS_DEBUG
	//vbMirror->diffuse += 30;	// Shows which vertexes are geting touched by dynamic light. debug only.
#endif

	// (gth) avoiding the extra divides (compiler unfortunately didn't do this automatically...)
	const float oo255 = (1.0f/255.0f);
	shadeR = ((diffuse>>16)&0x00FF) * oo255;
	shadeG = ((diffuse>>8)&0x00FF) * oo255;
	shadeB = (diffuse&0x00FF) * oo255;

	Int alpha = (diffuse>>24)&0x00FF;
	Int k;
	for (k=0; k<numLights; k++) {
		W3DDynamicLight *pLight = pLights[k];
		if (!pLight->isEnabled()) {
			continue; // he is turned off.
		}
		Vector3 lightDirection(vbMirror->x, vbMirror->y, vbMirror->z);
		Real factor = 1.0f;
		switch(pLight->Get_Type()) {
		case LightClass::POINT:
		case LightClass::SPOT: {
				Vector3 lightLoc = pLight->Get_Position();
				lightDirection -= lightLoc;
				double range, midRange;
				pLight->Get_Far_Attenuation_Range(midRange, range);
				Real dist = lightDirection.Length();
				if (dist >= range) continue;
				if (midRange < 0.1) continue;
				factor = 1.0f - (dist - midRange) / (range - midRange);
				factor = WWMath::Clamp(factor,0.0f,1.0f);

				// (gth) normalize here since we have the length
				lightDirection /= dist;
			}
			break;
		case LightClass::DIRECTIONAL:
			pLight->Get_Spot_Direction(lightDirection);
			factor = 1.0;
			break;
		};

		// (gth) unneeded due to above normalization
		//lightDirection.Normalize();

		Vector3 lightRay(-lightDirection.X, -lightDirection.Y, -lightDirection.Z);
		Real shade = Vector3::Dot_Product(lightRay, *normal);
		shade *= factor;
		Vector3 diffuse;
		pLight->Get_Diffuse(&diffuse);
		Vector3 ambient;
		pLight->Get_Ambient(&ambient);
		if (shade > 1.0) shade = 1.0;
		if(shade < 0.0f) shade = 0.0f;
		shadeR += shade*diffuse.X;
		shadeG += shade*diffuse.Y;
		shadeB += shade*diffuse.Z;
		shadeR += factor*ambient.X;
		shadeG += factor*ambient.Y;
		shadeB += factor*ambient.Z;

	}
	if (shadeR > 1.0) shadeR = 1.0;
	if(shadeR < 0.0f) shadeR = 0.0f;
	if (shadeG > 1.0) shadeG = 1.0;
	if(shadeG < 0.0f) shadeG = 0.0f;
	if (shadeB > 1.0) shadeB = 1.0;
	if(shadeB < 0.0f) shadeB = 0.0f;
	shadeR*=255.0f;
	shadeG*=255.0f;
	shadeB*=255.0f;

//	(gth) faster float to int conversion, return the result so we can re-use it.
//	vb->diffuse=REAL_TO_INT(shadeB) | (REAL_TO_INT(shadeG) << 8) | (REAL_TO_INT(shadeR) << 16) | ((int)alpha << 24);
	UnsignedInt light_val = WWMath::Float_To_Int_Chop(shadeB) | (WWMath::Float_To_Int_Chop(shadeG) << 8) | (WWMath::Float_To_Int_Chop(shadeR) << 16) | ((int)alpha << 24);
	vb->diffuse = light_val;
	return light_val;

}

//=============================================================================
// HeightMapRenderObjClass::getXWithOrigin
//=============================================================================
/** Gets the x index that corresponds to the data.  For example, if the columns
are shifted by 3, index 3 is actually the first row of polygons, or 0.  Yes it
is confusing, but it makes sliding the map 10x faster.  */
//=============================================================================
Int HeightMapRenderObjClass::getXWithOrigin(Int x)
{
	const Int xMax = m_x-1;
	x -= m_originX;
	if (x < 0) x += xMax;
	if (x >= xMax) x -= xMax;
	if (x < 0) { DEBUG_CRASH(("X out of range.")); x = 0; }
	if (x >= xMax) { DEBUG_CRASH(("X out of range.")); x = xMax; }
	return x;
}

//=============================================================================
// HeightMapRenderObjClass::getYWithOrigin
//=============================================================================
/** Gets the y index that corresponds to the data.  For example, if the rows
are shifted by 3, index 3 is actually the first row of polygons, or 0.  Yes it
is confusing, but it makes sliding the map 10x faster.  */
//=============================================================================
Int HeightMapRenderObjClass::getYWithOrigin(Int y)
{
	const Int yMax = m_y-1;
	y -= m_originY;
	if (y < 0) y += yMax;
	if (y >= yMax) y -= yMax;
	if (y < 0) { DEBUG_CRASH(("Y out of range.")); y = 0; }
	if (y >= yMax) { DEBUG_CRASH(("Y out of range.")); y = yMax; }
	return y;
}

//=============================================================================
// HeightMapRenderObjClass::updateVB
//=============================================================================
/** Update a rectangular block of the given Vertex Buffer.
data is expected to be an array same dimensions as current heightmap
mapped into this VB.
*/
//=============================================================================
Int HeightMapRenderObjClass::updateVB(TerrainLightingVertex *pVB, VERTEX_FORMAT *data, Int x0, Int y0, Int x1, Int y1, Int originX, Int originY, WorldHeightMap *pMap, RefRenderObjListIterator *pLightsIterator)
{
	Int i,j;
	Vector3 lightRay[MAX_GLOBAL_LIGHTS];
	Int xCoord, yCoord;
	Int vn0,un0,vp1,up1;
	Vector3 l2r,n2f,normalAtTexel;
	constexpr const Int vertsPerRow=(VERTEX_BUFFER_TILE_LENGTH)*4;	//vertices per row of VB
	constexpr const Int cellOffset = 1;

	REF_PTR_SET(m_map, pMap);	//update our heightmap pointer in case it changed since last call.
	if (m_geometry && pMap)
	{
#ifdef RTS_DEBUG
		assert(x0 >= originX && y0 >= originY && x1>x0 && y1>y0 && x1<=originX+VERTEX_BUFFER_TILE_LENGTH && y1<=originY+VERTEX_BUFFER_TILE_LENGTH);
#endif

		for (Int lightIndex=0; lightIndex < TheGlobalData->m_numGlobalLights; lightIndex++)
		{
			const Coord3D& lightPos = TheGlobalData->m_terrainLightPos[lightIndex];
			lightRay[lightIndex].Set(-lightPos.x, -lightPos.y, -lightPos.z);
		}

		VERTEX_FORMAT *vbHardware = pVB;
		VERTEX_FORMAT *vBase = data;
		// Note that we are building the vertex buffer data in the memory buffer, data.
		// At the bottom, we will copy the final vertex data for one cell into the
		// hardware vertex buffer.

		for (j=y0; j<y1; j++)
		{
			VERTEX_FORMAT *vb = vBase;
			vb += (j-originY)*vertsPerRow;	//skip to correct row in vertex buffer
			vb += (x0-originX)*4;		//skip to correct vertex in row.

			const Int mapY = getYWithOrigin(j);
			vn0 = mapY-cellOffset;
			if (vn0 < -pMap->getDrawOrgY())
				vn0=-pMap->getDrawOrgY();
			vp1 = getYWithOrigin(j+cellOffset)+cellOffset;
			if (vp1 >= pMap->getYExtent()-pMap->getDrawOrgY())
				vp1=pMap->getYExtent()-pMap->getDrawOrgY()-1;

			yCoord = mapY+pMap->getDrawOrgY();
			for (i=x0; i<x1; i++)
			{
				const Int mapX = getXWithOrigin(i);
				un0 = mapX-cellOffset;
				if (un0 < -pMap->getDrawOrgX())
					un0=-pMap->getDrawOrgX();
				up1 = getXWithOrigin(i+cellOffset)+cellOffset;
				if (up1 >= pMap->getXExtent()-pMap->getDrawOrgX())
					up1=pMap->getXExtent()-pMap->getDrawOrgX()-1;
				xCoord = mapX+pMap->getDrawOrgX();

				//update the 4 vertices in this block
				float U[4], V[4];
				UnsignedByte alpha[4];
				float UA[4], VA[4];
				Bool flipForBlend = false;			 // True if the blend needs the triangles flipped.

				pMap->getUVData(mapX, mapY, U, V);
				pMap->getAlphaUVData(mapX, mapY, UA, VA, alpha, &flipForBlend);

				//top-left sample
				l2r.Set(2*MAP_XY_FACTOR,0,MAP_HEIGHT_SCALE*(pMap->getDisplayHeight(mapX+cellOffset, mapY) - pMap->getDisplayHeight(un0, mapY)));
				n2f.Set(0,2*MAP_XY_FACTOR,MAP_HEIGHT_SCALE*(pMap->getDisplayHeight(mapX, (mapY+cellOffset)) - pMap->getDisplayHeight(mapX, vn0)));

#ifdef ALLOW_TEMPORARIES
				normalAtTexel= Normalize(Vector3::Cross_Product(l2r,n2f));
#else
				Vector3::Normalized_Cross_Product(l2r, n2f, &normalAtTexel);
#endif

				vb->x=xCoord;
				vb->y=yCoord;
				vb->z=  ((float)pMap->getDisplayHeight(mapX, mapY))*MAP_HEIGHT_SCALE;
				vb->x = ADJUST_FROM_INDEX_TO_REAL(vb->x);
				vb->y = ADJUST_FROM_INDEX_TO_REAL(vb->y);
				vb->u1=U[0];
				vb->v1=V[0];
				vb->u2=UA[0];
				vb->v2=VA[0];
				doTheLight(vb, lightRay, &normalAtTexel, pLightsIterator, alpha[0]);
				vb++;

				//top-right sample
				l2r.Set(2*MAP_XY_FACTOR,0,MAP_HEIGHT_SCALE*(pMap->getDisplayHeight(up1 , mapY ) - pMap->getDisplayHeight(mapX , mapY )));
				n2f.Set(0,2*MAP_XY_FACTOR,MAP_HEIGHT_SCALE*(pMap->getDisplayHeight(mapX+cellOffset , (mapY+cellOffset) ) - pMap->getDisplayHeight(mapX+cellOffset , vn0 )));

#ifdef ALLOW_TEMPORARIES
				normalAtTexel= Normalize(Vector3::Cross_Product(l2r,n2f));
#else
				Vector3::Normalized_Cross_Product(l2r, n2f, &normalAtTexel);
#endif

				vb->x=xCoord+cellOffset;
				vb->y=yCoord;
				vb->z=  ((float)pMap->getDisplayHeight(mapX+cellOffset, mapY))*MAP_HEIGHT_SCALE;
				vb->x = ADJUST_FROM_INDEX_TO_REAL(vb->x);
				vb->y = ADJUST_FROM_INDEX_TO_REAL(vb->y);
				vb->u1=U[1];
				vb->v1=V[1];
				vb->u2=UA[1];
				vb->v2=VA[1];
				doTheLight(vb, lightRay, &normalAtTexel, pLightsIterator, alpha[1]);
				vb++;

				//bottom-right sample
				l2r.Set(2*MAP_XY_FACTOR,0,MAP_HEIGHT_SCALE*(pMap->getDisplayHeight(up1 , (mapY+cellOffset) ) - pMap->getDisplayHeight(mapX , (mapY+cellOffset) )));
				n2f.Set(0,2*MAP_XY_FACTOR,MAP_HEIGHT_SCALE*(pMap->getDisplayHeight(mapX+cellOffset , vp1 ) - pMap->getDisplayHeight(mapX+cellOffset , mapY )));

#ifdef ALLOW_TEMPORARIES
				normalAtTexel= Normalize(Vector3::Cross_Product(l2r,n2f));
#else
				Vector3::Normalized_Cross_Product(l2r, n2f, &normalAtTexel);
#endif

				vb->x=xCoord+cellOffset;
				if (yCoord + 1 == pMap->getDrawOrgY() + m_y - 1) {
					vb->y=yCoord+1;
				} else {
					vb->y=yCoord+cellOffset;
				}
				vb->z=  ((float)pMap->getDisplayHeight(mapX+cellOffset, mapY+cellOffset))*MAP_HEIGHT_SCALE;
				vb->x = ADJUST_FROM_INDEX_TO_REAL(vb->x);
				vb->y = ADJUST_FROM_INDEX_TO_REAL(vb->y);
				vb->u1=U[2];
				vb->v1=V[2];
				vb->u2=UA[2];
				vb->v2=VA[2];
				doTheLight(vb, lightRay, &normalAtTexel, pLightsIterator, alpha[2]);
				vb++;

				//bottom-left sample
				l2r.Set(2*MAP_XY_FACTOR,0,MAP_HEIGHT_SCALE*(pMap->getDisplayHeight(mapX+cellOffset , (mapY+cellOffset) ) - pMap->getDisplayHeight(un0 , (mapY+cellOffset) )));
				n2f.Set(0,2*MAP_XY_FACTOR,MAP_HEIGHT_SCALE*(pMap->getDisplayHeight(mapX , vp1 ) - pMap->getDisplayHeight(mapX , mapY )));

#ifdef ALLOW_TEMPORARIES
				normalAtTexel= Normalize(Vector3::Cross_Product(l2r,n2f));
#else
				Vector3::Normalized_Cross_Product(l2r, n2f, &normalAtTexel);
#endif

				if (xCoord == pMap->getDrawOrgX()) {
					vb->x=xCoord;
					//if (vb->x < 0) vb->x = 0;
				} else {
					vb->x=xCoord;
				}
				if (yCoord + 1 == pMap->getDrawOrgY() + m_y - 1) {
					vb->y=yCoord+1;
				} else {
					vb->y=yCoord+cellOffset;
				}
				vb->z=  ((float)pMap->getDisplayHeight(mapX, mapY+cellOffset))*MAP_HEIGHT_SCALE;
				vb->x = ADJUST_FROM_INDEX_TO_REAL(vb->x);
				vb->y = ADJUST_FROM_INDEX_TO_REAL(vb->y);
				vb->u1=U[3];
				vb->v1=V[3];
				vb->u2=UA[3];
				vb->v2=VA[3];
				doTheLight(vb, lightRay, &normalAtTexel, pLightsIterator, alpha[3]);
				vb++;

				VERTEX_FORMAT *pCurVertices = vb-4;
#ifdef FLIP_TRIANGLES // jba - reduces "diamonding" in some cases, not others.  Better cliffs, though.
				VERTEX_FORMAT tmpVertex;
				if (flipForBlend) {
					tmpVertex = pCurVertices[0];
					pCurVertices[0] = pCurVertices[1];
					pCurVertices[1] = pCurVertices[2];
					pCurVertices[2] = pCurVertices[3];
					pCurVertices[3] = tmpVertex;
				}
#endif

				if (m_showImpassableAreas) {
					// Color impassable cells "red"
					DEBUG_ASSERTCRASH(PATHFIND_CELL_SIZE_F == MAP_XY_FACTOR, ("Pathfind must be terrain cell size, or this code needs reworking.  John A."));
					Real borderHiX = (pMap->getXExtent()-2*pMap->getBorderSizeInline())*MAP_XY_FACTOR;
					Real borderHiY = (pMap->getYExtent()-2*pMap->getBorderSizeInline())*MAP_XY_FACTOR;
					Bool border = pCurVertices[0].x == -MAP_XY_FACTOR || pCurVertices[0].y == -MAP_XY_FACTOR;
					Bool cliffMapped = pMap->isCliffMappedTexture(mapX, mapY);
					if (pCurVertices[0].x == borderHiX) {
						border = true;
					}
					if (pCurVertices[0].y == borderHiY) {
						border = true;
					}
					Bool isCliff = pMap->getCliffState(xCoord, yCoord) || showAsVisibleCliff(xCoord, yCoord);

					if ( isCliff || border || cliffMapped) {
						Int cellX, cellY;
						for (cellX=0; cellX<2; cellX++) {
							for (cellY=0; cellY<2; cellY++) {
								Int vertex = cellX+2*cellY;
								if (border) {
									Bool doBorder = false;
									if (pCurVertices[vertex].y >= 0 && pCurVertices[vertex].y <= borderHiY) {
										if (pCurVertices[vertex].x == 0 || pCurVertices[vertex].x == borderHiX) {
											doBorder = true;
										}
									}
									if (pCurVertices[vertex].x >= 0 && pCurVertices[vertex].x <= borderHiX) {
										if (pCurVertices[vertex].y == 0 || pCurVertices[vertex].y == borderHiY) {
											doBorder = true;
										}
									}
									if (doBorder) {
										pCurVertices[vertex].diffuse &= 0xFF0000ff; // blue with alpha.
									}
								} else if (isCliff) {
									pCurVertices[vertex].diffuse &= 0xFFFF0000; // red with alpha.
								}
								if (cliffMapped && vertex==0) {
									pCurVertices[vertex].diffuse &= 0xFF000000; // Black.
									pCurVertices[vertex].diffuse |= 0xff00; // Add green.
								}
							}
						}
					}
				}

				// Note - We have been building the vertex buffer in the memory location.
				// Now copy the set of vertices into the hardware buffer.
				// We don't copy the whole vertex buffer because we often update only
				// a couple of rows and its a lot faster to just copy the ones that change.
				const std::ptrdiff_t offset = pCurVertices - vBase;
				memcpy(vbHardware+offset, pCurVertices, 4*sizeof(VERTEX_FORMAT));
			}
		}
		m_geometry->tiles[(pVB-m_geometry->current.data())/HEIGHTMAP_VERTEX_NUM].dirty = true;
		return 0; //success.
	}
	return -1;
}

//=============================================================================
// HeightMapRenderObjClass::updateVBForLight
//=============================================================================
/** Update the dynamic lighting values only in a rectangular block of the given Vertex Buffer.
The vertex locations and texture coords are unchanged.
*/
Int HeightMapRenderObjClass::updateVBForLight(TerrainLightingVertex *pVB, VERTEX_FORMAT *data, Int x0, Int y0, Int x1, Int y1, Int originX, Int originY, W3DDynamicLight *pLights[], Int numLights)
{


	Int i,j,k;
	Int vn0,un0,vp1,up1;
	Vector3 l2r,n2f,normalAtTexel;
	constexpr const Int	vertsPerRow=(VERTEX_BUFFER_TILE_LENGTH)*4;	//vertices per row of VB

	if (m_geometry && m_map)
	{
#ifdef RTS_DEBUG
		assert(x0 >= originX && y0 >= originY && x1>x0 && y1>y0 && x1<=originX+VERTEX_BUFFER_TILE_LENGTH && y1<=originY+VERTEX_BUFFER_TILE_LENGTH);
#endif

		VERTEX_FORMAT *vBase = pVB;
		VERTEX_FORMAT *vb;

		for (j=y0; j<y1; j++)
		{
			const Int mapY = getYWithOrigin(j);
			const Int yCoord = mapY+m_map->getDrawOrgY()-m_map->getBorderSizeInline();
			Bool intersect = false;
			for (k=0; k<numLights; k++) {
				if (pLights[k]->m_minY <= yCoord+1 &&
					pLights[k]->m_maxY >= yCoord) {
					intersect = true;
				}
				if (pLights[k]->m_prevMinY <= yCoord+1 &&
					pLights[k]->m_prevMaxY >= yCoord) {
					intersect = true;
				}
			}
			if (!intersect) {
				continue;
			}
			vn0 = mapY-1;
			if (vn0 < -m_map->getDrawOrgY())
				vn0=-m_map->getDrawOrgY();
			vp1 = getYWithOrigin(j+1)+1;
			if (vp1 >= m_map->getYExtent()-m_map->getDrawOrgY())
				vp1=m_map->getYExtent()-m_map->getDrawOrgY()-1;

			for (i=x0; i<x1; i++)
			{
				const Int mapX = getXWithOrigin(i);
				const Int xCoord = mapX+m_map->getDrawOrgX()-m_map->getBorderSizeInline();
				Bool intersect = false;
				for (k=0; k<numLights; k++) {
					if (pLights[k]->m_minX <= xCoord+1 &&
						pLights[k]->m_maxX >= xCoord &&
						pLights[k]->m_minY <= yCoord+1 &&
						pLights[k]->m_maxY >= yCoord) {
						intersect = true;
					}
					if (pLights[k]->m_prevMinX <= xCoord+1 &&
						pLights[k]->m_prevMaxX >= xCoord &&
						pLights[k]->m_prevMinY <= yCoord+1 &&
						pLights[k]->m_prevMaxY >= yCoord) {
						intersect = true;
					}
				}
				if (!intersect) {
					continue;
				}
				// vb is the current CPU vertex; the backup retains baseline lighting.
				Int offset = (j-originY)*vertsPerRow+4*(i-originX);
				vb = vBase + offset;	//skip to correct row in vertex buffer
				// vbMirror is the pointer to the vertex in our memory based copy.
				// The important point is that we can read out of our copy to get the original
				// diffuse color, and xyz location.  It is VERY SLOW to read out of the
				// hardware vertex buffer, possibly worse... jba.
				VERTEX_FORMAT *vbMirror = data + offset;
				un0 = mapX-1;
				if (un0 < -m_map->getDrawOrgX())
					un0=-m_map->getDrawOrgX();
				up1 = getXWithOrigin(i+1)+1;
				if (up1 >= m_map->getXExtent()-m_map->getDrawOrgX())
					up1=m_map->getXExtent()-m_map->getDrawOrgX()-1;

				Vector3 lightRay(0,0,0);

				//top-left sample
				l2r.Set(2*MAP_XY_FACTOR,0,MAP_HEIGHT_SCALE*(m_map->getDisplayHeight(mapX+1, mapY) - m_map->getDisplayHeight(un0, mapY)));
				n2f.Set(0,2*MAP_XY_FACTOR,MAP_HEIGHT_SCALE*(m_map->getDisplayHeight(mapX, (mapY+1)) - m_map->getDisplayHeight(mapX, vn0)));

#ifdef ALLOW_TEMPORARIES
				normalAtTexel= Normalize(Vector3::Cross_Product(l2r,n2f));
#else
				Vector3::Normalized_Cross_Product(l2r, n2f, &normalAtTexel);
#endif

				doTheDynamicLight(vb, vbMirror, &lightRay, &normalAtTexel, pLights, numLights);
				vb++;	vbMirror++;

				//top-right sample
				l2r.Set(2*MAP_XY_FACTOR,0,MAP_HEIGHT_SCALE*(m_map->getDisplayHeight(up1 , mapY ) - m_map->getDisplayHeight(mapX , mapY )));
				n2f.Set(0,2*MAP_XY_FACTOR,MAP_HEIGHT_SCALE*(m_map->getDisplayHeight(mapX+1 , (mapY+1) ) - m_map->getDisplayHeight(mapX+1 , vn0 )));

#ifdef ALLOW_TEMPORARIES
				normalAtTexel= Normalize(Vector3::Cross_Product(l2r,n2f));
#else
				Vector3::Normalized_Cross_Product(l2r, n2f, &normalAtTexel);
#endif

				doTheDynamicLight(vb, vbMirror, &lightRay, &normalAtTexel, pLights, numLights);
				vb++;	vbMirror++;

				//bottom-right sample
				l2r.Set(2*MAP_XY_FACTOR,0,MAP_HEIGHT_SCALE*(m_map->getDisplayHeight(up1 , (mapY+1) ) - m_map->getDisplayHeight(mapX , (mapY+1) )));
				n2f.Set(0,2*MAP_XY_FACTOR,MAP_HEIGHT_SCALE*(m_map->getDisplayHeight(mapX+1 , vp1 ) - m_map->getDisplayHeight(mapX+1 , mapY )));

#ifdef ALLOW_TEMPORARIES
				normalAtTexel= Normalize(Vector3::Cross_Product(l2r,n2f));
#else
				Vector3::Normalized_Cross_Product(l2r, n2f, &normalAtTexel);
#endif

				doTheDynamicLight(vb, vbMirror, &lightRay, &normalAtTexel, pLights, numLights);
				vb++;	vbMirror++;

				//bottom-left sample
				l2r.Set(2*MAP_XY_FACTOR,0,MAP_HEIGHT_SCALE*(m_map->getDisplayHeight(mapX+1 , (mapY+1) ) - m_map->getDisplayHeight(un0 , (mapY+1) )));
				n2f.Set(0,2*MAP_XY_FACTOR,MAP_HEIGHT_SCALE*(m_map->getDisplayHeight(mapX , vp1 ) - m_map->getDisplayHeight(mapX , mapY )));

#ifdef ALLOW_TEMPORARIES
				normalAtTexel= Normalize(Vector3::Cross_Product(l2r,n2f));
#else
				Vector3::Normalized_Cross_Product(l2r, n2f, &normalAtTexel);
#endif

				doTheDynamicLight(vb, vbMirror, &lightRay, &normalAtTexel, pLights, numLights);
				vb++;	vbMirror++;
			}
		}
		m_geometry->tiles[(pVB-m_geometry->current.data())/HEIGHTMAP_VERTEX_NUM].dirty = true;
		return 0; //success.
	}
	return -1;
}


//=============================================================================
// HeightMapRenderObjClass::doPartialUpdate
//=============================================================================
/** Updates a partial block of vertices from [x0,y0 to x1,y1]
The coordinates in partialRange are map cell coordinates, relative to the entire map.
The vertex coordinates and texture coordinates, as well as static lighting are updated.
*/
void HeightMapRenderObjClass::doPartialUpdate(const IRegion2D &partialRange, WorldHeightMap *htMap, RefRenderObjListIterator *pLightsIterator)
{
	// Adjust range into the current drawn map range.
	Int minX = partialRange.lo.x - htMap->getDrawOrgX();
	Int maxX = partialRange.hi.x - htMap->getDrawOrgX();
	Int minY = partialRange.lo.y - htMap->getDrawOrgY();
	Int maxY = partialRange.hi.y - htMap->getDrawOrgY();
	if (minX<0) minX = 0;
	if (minY<0) minY = 0;
	if (maxX > m_x-1) maxX = m_x-1;
	if (maxY > m_y-1) maxY = m_y-1;
	if (maxX < minX) return;
	if (maxY < minY) return;
	if (m_originX == 0 && m_originY == 0) {
		// simple case.
		updateBlock(minX, minY, maxX, maxY,
								htMap, pLightsIterator);
	}
	else
	{
		minY = minY+m_originY;
		maxY = maxY+m_originY;

		if (minY> m_y-1) {
			minY -= m_y-1;
 			maxY -= m_y-1;
		}
		if (maxY > m_y-1) {
			maxY -= m_y-1;
			updateBlock(0, minY, m_x-1, m_y-1, htMap, pLightsIterator);
			updateBlock(0, 0, m_x-1, maxY, htMap, pLightsIterator);
		} else {
			updateBlock(0, minY, m_x-1, maxY, htMap, pLightsIterator);
		}
	}

	if (!m_extraBlendTilePositions)
	{	//Need to allocate memory
		m_extraBlendTilePositions = NEW Int[DEFAULT_MAX_MAP_EXTRABLEND_TILES];
		m_extraBlendTilePositionsSize = DEFAULT_MAX_MAP_EXTRABLEND_TILES;
	}

	//Find list of all extra blend tiles used on map.  These are tiles with 3 materials/textures
	//over the same tile and require an extra render pass.

	Int i, j;
	//First remove any existing extra blend tiles within this partial region
	for (j=0; j<m_numExtraBlendTiles; j++)
	{	Int x = m_extraBlendTilePositions[j] & 0xffff;
		Int y = m_extraBlendTilePositions[j] >> 16;
		if (x >= partialRange.lo.x && x < partialRange.hi.x &&
			y >= partialRange.lo.y && y < partialRange.hi.y)
		{	//this tile is inside region being updated so remove it by shifting tile array
			memcpy(m_extraBlendTilePositions+j,m_extraBlendTilePositions+j+1,(m_numExtraBlendTiles-1-j)*sizeof(Int));
			m_numExtraBlendTiles--;
			j--;	//need to look at index j again because this tile was removed
		}
	}

	for (j=partialRange.lo.y; j<partialRange.hi.y; j++)
		for (i=partialRange.lo.x; i<partialRange.hi.x; i++)
		{
			if (j<0 || i<0) continue;
			Real U[4],V[4];
			UnsignedByte alpha[4];
			Bool flipState,cliffState;
			if (htMap->getExtraAlphaUVData(i,j,U,V,alpha,&flipState, &cliffState))
			{	if (m_numExtraBlendTiles >= m_extraBlendTilePositionsSize)
				{	//no more room to store extra blend tiles so enlarge the buffer.
					Int *tempPositions=NEW Int[m_extraBlendTilePositionsSize+512];
					memcpy(tempPositions, m_extraBlendTilePositions, m_extraBlendTilePositionsSize*sizeof(Int));
					delete [] m_extraBlendTilePositions;
					//enlarge by more tiles to reduce memory trashing
					m_extraBlendTilePositions = tempPositions;
					m_extraBlendTilePositionsSize += 512;
				}
				//Pack x and y position into single integer since maps are limited in size
				m_extraBlendTilePositions[m_numExtraBlendTiles]=i | (j <<16);
				m_numExtraBlendTiles++;
			}
		}
	updateShorelineTiles(partialRange.lo.x,partialRange.lo.y,partialRange.hi.x,partialRange.hi.y,htMap);

	updateViewImpassableAreas(TRUE, minX, maxX, minY, maxY);
}

//=============================================================================
// HeightMapRenderObjClass::updateBlock
//=============================================================================
/** Updates a block of vertices from [x0,y0 to x1,y1]
The vertex coordinates and texture coordinates, as well as static lighting are updated.
*/
Int HeightMapRenderObjClass::updateBlock(Int x0, Int y0, Int x1, Int y1,  WorldHeightMap *pMap, RefRenderObjListIterator *pLightsIterator)
{
#ifdef RTS_DEBUG
	DEBUG_ASSERTCRASH(x0>=0,  ("HeightMapRenderObjClass::UpdateBlock parameters extend beyond left edge."));
	DEBUG_ASSERTCRASH(y0>=0,  ("HeightMapRenderObjClass::UpdateBlock parameters extend beyond bottom edge."));
	DEBUG_ASSERTCRASH(x1<m_x, ("HeightMapRenderObjClass::UpdateBlock parameters extend beyond right edge."));
	DEBUG_ASSERTCRASH(y1<m_y, ("HeightMapRenderObjClass::UpdateBlock parameters extend beyond top edge."));
	DEBUG_ASSERTCRASH(x0<=x1, ("HeightMapRenderObjClass::UpdateBlock parameters have inside-out rectangle (on X)."));
	DEBUG_ASSERTCRASH(y0<=y1, ("HeightMapRenderObjClass::UpdateBlock parameters have inside-out rectangle (on Y)."));
#endif
	Invalidate_Cached_Bounding_Volumes();
	if (pMap && m_treeBuffer != nullptr) {
		REF_PTR_SET(m_stageZeroTexture, pMap->getTerrainTexture());
		REF_PTR_SET(m_stageOneTexture, pMap->getTerrainTexture());
	}

	Int i,j;
	Int originX,originY;
	//step through each vertex buffer that needs updating
	for (j=0; j<m_numVBTilesY; j++)
	{
		originY=j*VERTEX_BUFFER_TILE_LENGTH;	//location of this VB on the large full-size heightmap
		Int yMin, yMax;
		yMin = originY;
		if (y0>yMin) yMin = y0;
		yMax = originY+VERTEX_BUFFER_TILE_LENGTH;
		if (y1<yMax) yMax = y1;
		if (yMin >= yMax) {
			continue;
		}
		for (i=0; i<m_numVBTilesX; i++)
		{
			originX=i*VERTEX_BUFFER_TILE_LENGTH;	//location of this VB on the large full-size heightmap
			Int xMin, xMax;
			xMin = originX;
			if (xMin<x0) xMin = x0;
			xMax = originX+VERTEX_BUFFER_TILE_LENGTH;
			if (xMax>x1) xMax = x1;
			if (xMin >= xMax) {
				continue;
			}
			TerrainLightingVertex *pVB = getVertexBufferTile(i, j);
			VERTEX_FORMAT *pData = getVertexBufferBackup(i, j);
			updateVB(pVB, pData, xMin, yMin, xMax, yMax, originX, originY, pMap, pLightsIterator);
		}
	}

	return 0;
}


//-----------------------------------------------------------------------------
//         Public Functions
//-----------------------------------------------------------------------------

//=============================================================================
// HeightMapRenderObjClass::~HeightMapRenderObjClass
//=============================================================================
/** Destructor. Releases w3d assets. */
//=============================================================================
HeightMapRenderObjClass::~HeightMapRenderObjClass()
{
	freeMapResources();

	delete [] m_extraBlendTilePositions;
	m_extraBlendTilePositions = nullptr;
}

//=============================================================================
// HeightMapRenderObjClass::HeightMapRenderObjClass
//=============================================================================
/** Constructor. Mostly nulls out the member variables. */
//=============================================================================
HeightMapRenderObjClass::HeightMapRenderObjClass():
m_extraBlendTilePositions(nullptr),
m_numExtraBlendTiles(0),
m_numVisibleExtraBlendTiles(0),
m_extraBlendTilePositionsSize(0),
m_geometry(nullptr),
m_vertexBufferBackup(nullptr),
m_originX(0),
m_originY(0),
m_desiredDrawWidth(WorldHeightMap::NORMAL_DRAW_WIDTH),
m_desiredDrawHeight(WorldHeightMap::NORMAL_DRAW_HEIGHT),
m_oversizeDrawWidth(0),
m_oversizeDrawHeight(0),
m_numVBTilesX(0),
m_numVBTilesY(0),
m_numVertexBufferTiles(0),
m_numBlockColumnsInLastVB(0),
m_numBlockRowsInLastVB(0)
{
	TheHeightMap = this;
}


//=============================================================================
// HeightMapRenderObjClass::adjustTerrainLOD
//=============================================================================
/** Adjust the terrain Level Of Detail.  If adj > 0 , increases LOD 1 step, if
adj < 0 decreases it one step, if adj==0, then just sets up for the current LOD */
//=============================================================================
void HeightMapRenderObjClass::adjustTerrainLOD(Int adj)
{
	BaseHeightMapRenderObjClass::adjustTerrainLOD(adj);

	return;

#if 0
	if (adj>0 && TheGlobalData->m_terrainLOD<TERRAIN_LOD_MAX) TheWritableGlobalData->m_terrainLOD=(TerrainLOD)(TheGlobalData->m_terrainLOD+1);
	if (adj<0 && TheGlobalData->m_terrainLOD>TERRAIN_LOD_MIN) TheWritableGlobalData->m_terrainLOD=(TerrainLOD)(TheGlobalData->m_terrainLOD-1);

	switch (TheGlobalData->m_terrainLOD) {
		case	TERRAIN_LOD_MIN: TheWritableGlobalData->m_useCloudMap = false;
									TheWritableGlobalData->m_useLightMap = false ;
									TheWritableGlobalData->m_useWaterPlane = false;
									break;
		case TERRAIN_LOD_NO_CLOUDS: TheWritableGlobalData->m_useCloudMap = false;
									TheWritableGlobalData->m_useLightMap = false;
									TheWritableGlobalData->m_useWaterPlane = false;
									break;
		default:
		case TERRAIN_LOD_NO_WATER: TheWritableGlobalData->m_useCloudMap = true;
									TheWritableGlobalData->m_useLightMap = true;
									TheWritableGlobalData->m_useWaterPlane = false;
									break;
		case TERRAIN_LOD_MAX: TheWritableGlobalData->m_useCloudMap = true;
									TheWritableGlobalData->m_useLightMap = true;
									TheWritableGlobalData->m_useWaterPlane = true;
									break;
	}
	if (m_map==nullptr) return;
	m_map->setDrawOrg(m_map->getDrawOrgX(), m_map->getDrawOrgX());
	if (m_shroud)
		m_shroud->reset();	//need reset here since initHeightData will load new shroud.
	this->initHeightData(m_map->getDrawWidth(),
											m_map->getDrawHeight(), m_map, nullptr);
	staticLightingChanged();
	if (TheTacticalView) {
		TheTacticalView->forceRedraw();
	}
#endif
}

//=============================================================================
// HeightMapRenderObjClass::ReleaseResources
//=============================================================================
/** Releases all w3d assets, to prepare for Reset device call. */
//=============================================================================
void HeightMapRenderObjClass::ReleaseResources()
{
	BaseHeightMapRenderObjClass::ReleaseResources();
}

//=============================================================================
// HeightMapRenderObjClass::ReAcquireResources
//=============================================================================
/** Reallocates all W3D assets after a reset.. */
//=============================================================================
void HeightMapRenderObjClass::ReAcquireResources()
{
	BaseHeightMapRenderObjClass::ReAcquireResources();
}

//=============================================================================
// HeightMapRenderObjClass::reset
//=============================================================================
/** Updates the macro noise/lightmap texture (pass 3) */
//=============================================================================
void HeightMapRenderObjClass::reset()
{
	BaseHeightMapRenderObjClass::reset();
	m_oversizeDrawWidth = 0;
	m_oversizeDrawHeight = 0;
}

//=============================================================================
// HeightMapRenderObjClass::oversizeTerrain
//=============================================================================
/** Sets the terrain oversize amount. */
//=============================================================================
void HeightMapRenderObjClass::oversizeTerrain(Int tilesToOversize)
{
	if (tilesToOversize>0)
	{
		m_oversizeDrawWidth = WorldHeightMap::NORMAL_DRAW_WIDTH + VERTEX_BUFFER_TILE_LENGTH * tilesToOversize;
		m_oversizeDrawHeight = WorldHeightMap::NORMAL_DRAW_HEIGHT + VERTEX_BUFFER_TILE_LENGTH * tilesToOversize;
		setTerrainDrawSize(0, 0);
	}
	else
	{
		m_oversizeDrawWidth = 0;
		m_oversizeDrawHeight = 0;
		setTerrainDrawSize(m_desiredDrawWidth, m_desiredDrawHeight);
	}
}

void HeightMapRenderObjClass::setTerrainDrawSize(Int width, Int height)
{
	if (m_map == nullptr)
		return;

	if (width > 0)
		m_desiredDrawWidth = width;

	if (height > 0)
		m_desiredDrawHeight = height;

	width = std::max(m_oversizeDrawWidth, m_desiredDrawWidth);
	height = std::max(m_oversizeDrawHeight, m_desiredDrawHeight);

	width = std::min(width, m_map->getXExtent());
	height = std::min(height, m_map->getYExtent());

	if (width == m_map->getDrawWidth() && height == m_map->getDrawHeight())
		return;

	Int dx = width-m_map->getDrawWidth();
	Int dy = height-m_map->getDrawHeight();
	m_map->setDrawWidth(width);
	m_map->setDrawHeight(height);
	dx /= 2;
	dy /= 2;
	Int newOrgX = m_map->getDrawOrgX()-dx;
	Int newOrgY = m_map->getDrawOrgY()-dy;
	m_map->setDrawOrg(newOrgX,newOrgY);
	m_originX = 0;
	m_originY = 0;
	if (m_shroud)
		m_shroud->reset();
	//delete m_shroud;
	//m_shroud = nullptr;
	initHeightData(m_map->getDrawWidth(), m_map->getDrawHeight(), m_map, nullptr, FALSE);
	scheduleFullUpdate();
}




//=============================================================================
// HeightMapRenderObjClass::initHeightData
//=============================================================================
/** Allocate a heightmap of x by y vertices and fill with initial height values.
Also allocates all rendering resources such as vertex buffers, index buffers,
shaders, and materials.*/
//=============================================================================
Int HeightMapRenderObjClass::initHeightData(Int x, Int y, WorldHeightMap *pMap, RefRenderObjListIterator *pLightsIterator, Bool updateExtraPassTiles)
{
	BaseHeightMapRenderObjClass::initHeightData(x, y, pMap, pLightsIterator, updateExtraPassTiles);
	Int i,j;
//	Int	vertsPerRow=x*2-2;
//	Int	vertsPerColumn=y*2-2;

	HeightSampleType *data = nullptr;
	if (pMap) {
		data = pMap->getDataPtr();
	}

	if (updateExtraPassTiles)
	{
		m_numExtraBlendTiles = 0;
		//Do some preprocessing on map to extract useful data
		if (pMap)
		{
			Int m_mapDX=pMap->getXExtent();
			Int m_mapDY=pMap->getYExtent();
			if (!m_extraBlendTilePositions)
			{	//Need to allocate memory
				m_extraBlendTilePositions = NEW Int[DEFAULT_MAX_MAP_EXTRABLEND_TILES];
				m_extraBlendTilePositionsSize = DEFAULT_MAX_MAP_EXTRABLEND_TILES;
			}

			//Find list of all extra blend tiles used on map.  These are tiles with 3 materials/textures
			//over the same tile and require an extra render pass.
			for (j=0; j<(m_mapDY-1); j++)
				for (i=0; i<(m_mapDX-1); i++)
				{
					Real U[4],V[4];
					UnsignedByte alpha[4];
					Bool flipState,cliffState;
					if (pMap->getExtraAlphaUVData(i,j,U,V,alpha,&flipState, &cliffState))
					{	if (m_numExtraBlendTiles >= m_extraBlendTilePositionsSize)
						{	//no more room to store extra blend tiles so enlarge the buffer.
							Int *tempPositions=NEW Int[m_extraBlendTilePositionsSize+512];
							memcpy(tempPositions, m_extraBlendTilePositions, m_extraBlendTilePositionsSize*sizeof(Int));
							delete [] m_extraBlendTilePositions;
							//enlarge by more tiles to reduce memory trashing
							m_extraBlendTilePositions = tempPositions;
							m_extraBlendTilePositionsSize += 512;
						}
						//Pack x and y position into single integer since maps are limited in size
						m_extraBlendTilePositions[m_numExtraBlendTiles]=i | (j <<16);
						m_numExtraBlendTiles++;
					}
				}
		}
	}

	m_originX = 0;
	m_originY = 0;
	scheduleFullUpdate();

	// If the size changed, we need to allocate.
	Bool needToAllocate = (x != m_x || y != m_y);
	// If the textures aren't allocated (usually because of a hardware reset) need to allocate.
	if (m_stageOneTexture == nullptr || !m_geometry) {
		needToAllocate = true;
	}
	if (data && needToAllocate && m_treeBuffer != nullptr)
	{	//requested heightmap different from old one.
		freeIndexVertexBuffers();
		//Get number of vertex buffers needed to hold current map
		//First round dimensions to next multiple of VERTEX_BUFFER_TILE_LENGTH since that's our block size
		m_numVBTilesX=1;
		for (i=VERTEX_BUFFER_TILE_LENGTH+1; i<x;)
		{	i+=VERTEX_BUFFER_TILE_LENGTH;
			m_numVBTilesX++;
		}
		m_numVBTilesY=1;
		for (j=VERTEX_BUFFER_TILE_LENGTH+1; j<y;)
		{	j+=VERTEX_BUFFER_TILE_LENGTH;
			m_numVBTilesY++;
		}

		m_numBlockColumnsInLastVB=(x-1)%VERTEX_BUFFER_TILE_LENGTH;	//right border within last VB
		m_numBlockRowsInLastVB=(y-1)%VERTEX_BUFFER_TILE_LENGTH;	//bottom border within last VB

		m_numVertexBufferTiles=m_numVBTilesX*m_numVBTilesY;
		m_x=x;
		m_y=y;

        m_geometry = NEW GeometryState;
        m_geometry->current.resize(m_numVertexBufferTiles * HEIGHTMAP_VERTEX_NUM);
        m_geometry->tiles.resize(m_numVertexBufferTiles);
        m_vertexBufferBackup = NEW VERTEX_FORMAT[m_numVertexBufferTiles * HEIGHTMAP_VERTEX_NUM]{};

		//go with a preset material for now.
	}

	updateBlock(0,0,x-1,y-1,pMap,pLightsIterator);

	return 0;
}


//=============================================================================
// HeightMapRenderObjClass::On_Frame_Update
//=============================================================================
// Updates the diffuse color values in the vertices as affected by the dynamic
// lights.
// TheSuperHackers @bugfix xezon 15/12/2025 Now draws the dynamic lights
// properly on the entirety of the drawable map region.
//=============================================================================
void HeightMapRenderObjClass::On_Frame_Update()
{
	BaseHeightMapRenderObjClass::On_Frame_Update();
	Int i,j,k;
	Int originX,originY;
	if (Scene==nullptr) return;
	RTS3DScene *pMyScene = (RTS3DScene *)Scene;


	RefRenderObjListIterator pDynamicLightsIterator(pMyScene->getDynamicLights());
	if (m_map == nullptr) {
		return;
	}

#ifdef DO_UNIT_TIMINGS
#pragma MESSAGE("*** WARNING *** DOING DO_UNIT_TIMINGS!!!!")
	return;
#endif


	Int numDynaLights=0;
	W3DDynamicLight *enabledLights[MAX_ENABLED_DYNAMIC_LIGHTS];

	const Int xCoordMin = m_map->getDrawOrgX() - m_map->getBorderSizeInline();
	const Int yCoordMin = m_map->getDrawOrgY() - m_map->getBorderSizeInline();
	const Int xCoordMax = xCoordMin + m_map->getDrawWidth();
	const Int yCoordMax = yCoordMin + m_map->getDrawHeight();

	for (pDynamicLightsIterator.First(); !pDynamicLightsIterator.Is_Done(); pDynamicLightsIterator.Next())
	{
		W3DDynamicLight *pLight = (W3DDynamicLight*)pDynamicLightsIterator.Peek_Obj();
		pLight->m_processMe = false;
		if (pLight->m_enabled || pLight->m_priorEnable) {
			Real range = pLight->Get_Attenuation_Range();
			if (pLight->m_priorEnable) {
				pLight->m_prevMinX = pLight->m_minX;
				pLight->m_prevMinY = pLight->m_minY;
				pLight->m_prevMaxX = pLight->m_maxX;
				pLight->m_prevMaxY = pLight->m_maxY;
			}
			Vector3	pos = pLight->Get_Position();
			pLight->m_minX = (pos.X-range)/MAP_XY_FACTOR;
			pLight->m_maxX = (pos.X+range)/MAP_XY_FACTOR+1.0f;
			pLight->m_minY = (pos.Y-range)/MAP_XY_FACTOR;
			pLight->m_maxY = (pos.Y+range)/MAP_XY_FACTOR+1.0f;
			if (!pLight->m_priorEnable) {
				pLight->m_prevMinX = pLight->m_minX;
				pLight->m_prevMinY = pLight->m_minY;
				pLight->m_prevMaxX = pLight->m_maxX;
				pLight->m_prevMaxY = pLight->m_maxY;
			}

			if (pLight->m_minX < xCoordMax &&
					pLight->m_minY < yCoordMax &&
					pLight->m_maxX > xCoordMin &&
					pLight->m_maxY > yCoordMin) {
				pLight->m_processMe = TRUE;
			} else if (pLight->m_prevMinX < xCoordMax &&
					pLight->m_prevMinY < yCoordMax &&
					pLight->m_prevMaxX > xCoordMin &&
					pLight->m_prevMaxY > yCoordMin) {
				pLight->m_processMe = TRUE;
			} else {
				pLight->m_processMe = false;
			}
			if (pLight->m_processMe) {
				enabledLights[numDynaLights] = pLight;
				numDynaLights++;
				if (numDynaLights == MAX_ENABLED_DYNAMIC_LIGHTS) {
					break;
				}
			}
		}
		pLight->m_priorEnable = pLight->m_enabled;
	}
	if (numDynaLights > 0) {
		//step through each vertex buffer that needs updating
		for (j=0; j<m_numVBTilesY; j++)
		{
			originY=j*VERTEX_BUFFER_TILE_LENGTH;	//location of this VB on the large full-size heightmap
			Int yMin, yMax;
			yMin = originY;
			yMax = originY+VERTEX_BUFFER_TILE_LENGTH;
			Bool intersect = false;
			Int yCoordMin = getYWithOrigin(yMin)+m_map->getDrawOrgY()-m_map->getBorderSizeInline();
			Int yCoordMax = getYWithOrigin(yMax-1)+m_map->getDrawOrgY()+1-m_map->getBorderSizeInline();
			if (yCoordMax>yCoordMin) {
				// no wrap occurred.
				for (k=0; k<numDynaLights; k++) {
					if (enabledLights[k]->m_minY < yCoordMax &&
						enabledLights[k]->m_maxY > yCoordMin) {
						intersect = true;
						break;
					}
					if (enabledLights[k]->m_prevMinY < yCoordMax &&
						enabledLights[k]->m_prevMaxY > yCoordMin) {
						intersect = true;
						break;
					}
				}
			} else {
				// wrap occurred, so we are outside of this range.
				int tmp=yCoordMin;
				yCoordMin = yCoordMax;
				yCoordMax = tmp;
				for (k=0; k<numDynaLights; k++) {
					if (enabledLights[k]->m_minY <=  yCoordMin ||
						enabledLights[k]->m_maxY >= yCoordMax) {
						intersect = true;
						break;
					}
					if (enabledLights[k]->m_prevMinY <=  yCoordMin ||
						enabledLights[k]->m_prevMaxY >= yCoordMax) {
						intersect = true;
						break;
					}
				}
			}
			if (!intersect) {
				continue;
			}

			for (i=0; i<m_numVBTilesX; i++)
			{
				originX=i*VERTEX_BUFFER_TILE_LENGTH;	//location of this VB on the large full-size heightmap
				Int xMin, xMax;
				xMin = originX;
				xMax = originX+VERTEX_BUFFER_TILE_LENGTH;

				Bool intersect = false;
				Int xCoordMin = getXWithOrigin(xMin)+m_map->getDrawOrgX()-m_map->getBorderSizeInline();
				Int xCoordMax = getXWithOrigin(xMax-1)+m_map->getDrawOrgX()+1-m_map->getBorderSizeInline();
				if (xCoordMax>xCoordMin) {
					// no wrap occurred.
					for (k=0; k<numDynaLights; k++) {
						if (enabledLights[k]->m_minX < xCoordMax &&
							enabledLights[k]->m_maxX > xCoordMin) {
							intersect = true;
							break;
						}
						if (enabledLights[k]->m_prevMinX < xCoordMax &&
							enabledLights[k]->m_prevMaxX > xCoordMin) {
							intersect = true;
							break;
						}
					}
				} else {
					// wrap occurred, so we are outside of this range.
					int tmp=xCoordMin;
					xCoordMin = xCoordMax;
					xCoordMax = tmp;
					for (k=0; k<numDynaLights; k++) {
						if (enabledLights[k]->m_minX <=  xCoordMin ||
							enabledLights[k]->m_maxX >= xCoordMax) {
							intersect = true;
							break;
						}
						if (enabledLights[k]->m_prevMinX <=  xCoordMin ||
							enabledLights[k]->m_prevMaxX >= xCoordMax) {
							intersect = true;
							break;
						}
					}
				}
				if (!intersect) {
					continue;
				}
				TerrainLightingVertex *pVB = getVertexBufferTile(i, j);
				VERTEX_FORMAT *pData = getVertexBufferBackup(i, j);
				updateVBForLight(pVB, pData, xMin, yMin, xMax, yMax, originX,originY, enabledLights, numDynaLights);
			}
		}
	}
}

//=============================================================================
// HeightMapRenderObjClass::staticLightingChanged
//=============================================================================
/** Notification that all lighting needs to be recalculated. */
//=============================================================================
void HeightMapRenderObjClass::staticLightingChanged()
{
	BaseHeightMapRenderObjClass::staticLightingChanged();
}

#define CENTER_LIMIT 2
#define BIG_JUMP 16
#define WIDE_STEP 32

static Int visMinX, visMinY, visMaxX, visMaxY;
static Bool check(const FrustumClass & frustum, WorldHeightMap *pMap, Int x, Int y)
{
	if (x<0 || y<0) return(false);
	if (x>= pMap->getXExtent() || y>= pMap->getYExtent()) return(false);
	if (x >= visMinX && y >= visMinY && x <=visMaxX && y <= visMaxY) {
		return(true);
	}
	Int height = pMap->getHeight(x, y);
	Vector3 loc((x-pMap->getBorderSizeInline())*MAP_XY_FACTOR, (y-pMap->getBorderSizeInline())*MAP_XY_FACTOR, height*MAP_HEIGHT_SCALE);
	if (CollisionMath::Overlap_Test(frustum,loc) == CollisionMath::INSIDE) {
		if (x<visMinX) visMinX=x;
		if (x>visMaxX) visMaxX=x;
		if (y<visMinY) visMinY=y;
		if (y>visMaxY) visMaxY=y;
		return(true);
	}
	return(false);
}

static void calcVis(const FrustumClass & frustum, WorldHeightMap *pMap, Int minX, Int minY, Int maxX, Int maxY, Int limit)
{
	if (maxX-minX<2) return;
	if (maxY-minY<2) return;
	if (minX >=visMinX && minY >= visMinY && maxX <=visMaxX && maxY <= visMaxY) {
		return;
	}
	Int midX = (minX+maxX)/2;
	Int midY = (minY+maxY)/2;
	Bool recurse1 = maxX-minX>=limit;
	Bool recurse2 = recurse1;
	Bool recurse3 = recurse1;
	Bool recurse4 = recurse1;
	/* boxes are:

			1     2


			3			4 */

	if (check(frustum, pMap, midX, maxY)) {
		recurse1=true;
		recurse2=true;
	}
	if (check(frustum, pMap, midX, minY)) {
		recurse3=true;
		recurse4=true;
	}
	if (check(frustum, pMap, midX, midY)) {
		recurse1=true;
		recurse2=true;
		recurse3=true;
		recurse4=true;
	}
	if (check(frustum, pMap, minX, midY)) {
		recurse1=true;
		recurse3=true;
	}
	if (check(frustum, pMap, maxX, midY)) {
		recurse2=true;
		recurse4=true;
	}
	if (recurse1) {
		calcVis(frustum, pMap, minX, midY, midX, maxY, limit);
	}
	if (recurse2) {
		calcVis(frustum, pMap, midX, midY, maxX, maxY, limit);
	}
	if (recurse3) {
		calcVis(frustum, pMap, minX, minY, midX, midY, limit);
	}
	if (recurse4) {
		calcVis(frustum, pMap, midX, minY, maxX, midY, limit);
	}
}





//=============================================================================
// HeightMapRenderObjClass::updateCenter
//=============================================================================
/** Updates the positioning of the drawn portion of the height map in the
heightmap.  As the view slides around, this determines what is the actually
rendered portion of the terrain. Only a small section is rendered at any time.
*/
//=============================================================================
void HeightMapRenderObjClass::updateCenter(CameraClass *camera, const Vector3 *cameraPivot, RefRenderObjListIterator *pLightsIterator)
{
	if (m_map==nullptr) {
		return;
	}
	if (m_updating) {
		return;
	}
	if (m_geometry ==nullptr)
		return;		//did not initialize resources yet.

	BaseHeightMapRenderObjClass::updateCenter(camera, cameraPivot, pLightsIterator);

	m_updating = true;

	if (m_x >= m_map->getXExtent() && m_y >= m_map->getYExtent())
	{
		if (m_needFullUpdate)
		{
			m_needFullUpdate = false;
			updateBlock(0, 0, m_x-1, m_y-1, m_map, pLightsIterator);
		}

		m_updating = false;
		return; // no need to center.
	}

	const Real cameraPitch = asin(fabs(camera->Get_Forward_Dir().Z));
	Int newOrgX;
	Int newOrgY;

	if (cameraPitch > ViewDefaultLowPitchRadians)
	{
		// TheSuperHackers @info This is the original code to determine the center position for the visible terrain area.
		// It is relatively expensive and breaks when the frustum planes can no longer intersect with the terrain at low camera
		// pitch or when the camera is too far from the terrain, but it is very accurate when the camera is close to the
		// terrain. For now, we prefer to keep this code for the original camera pitch and above.

		// determine the ray corresponding to the camera and distance to projection plane
		const Matrix3D& camera_matrix = camera->Get_Transform();
		Vector3 camera_location  = camera->Get_Position();
		Vector3 rayLocation;
		Vector3 rayDirection;
		Vector3 rayDirectionPt;
		// the projected ray has the same origin as the camera
		rayLocation = camera_location;
		// determine the location of the screen coordinate in camera-model space
		const ViewportClass &viewport = camera->Get_Viewport();
		Int i, j, minHt;

		Real intersectionZ;
		minHt = m_map->getMaxHeightValue();
		for (j=0; j<m_y; j+=4) {
			for (i=0; i<m_x; i+=4) {
				Short cur = m_map->getDisplayHeight(i,j);
				if (cur<minHt) minHt = cur;
			}
		}
		intersectionZ = (float)minHt;
	//	float aspect = camera->Get_Aspect_Ratio();

		Vector2 min,max;
		camera->Get_View_Plane(min,max);
		float xscale = (max.X - min.X);
		float yscale = (max.Y - min.Y);

		float zmod = -1.0; // Scene->vpd; // Note: view plane distance is now always 1.0 from the camera
		float minX = 200000;
		float maxX = -minX;
		float minY = 200000;
		float maxY = -minY;
		for (i=0; i<2; i++) {
			for (j=0; j<2; j++) {
				float xmod = (-i + 0.5 + viewport.Min.X) * zmod * xscale;// / aspect;
				float ymod = (j - 0.5 - viewport.Min.Y) * zmod * yscale;// * aspect;

				// transform the screen coordinates by the camera's matrix into world coordinates.
				float x = zmod * camera_matrix[0][2] + xmod * camera_matrix[0][0] + ymod * camera_matrix[0][1];
				float y = zmod * camera_matrix[1][2] + xmod * camera_matrix[1][0] + ymod * camera_matrix[1][1];
				float z = zmod * camera_matrix[2][2] + xmod * camera_matrix[2][0] + ymod * camera_matrix[2][1];

				rayDirection.Set(x,y,z);
				rayDirection.Normalize();
				rayDirectionPt = rayLocation+rayDirection;

				x = Vector3::Find_X_At_Z(intersectionZ, rayLocation, rayDirectionPt);
				y = Vector3::Find_Y_At_Z(intersectionZ, rayLocation, rayDirectionPt);
				if (x<minX) minX = x;
				if (x>maxX) maxX = x;
				if (y<minY) minY = y;
				if (y>maxY) maxY = y;
			}
		}

		// convert back to cell indexes.
		minX /= MAP_XY_FACTOR;
		maxX /= MAP_XY_FACTOR;
		minY /= MAP_XY_FACTOR;
		maxY /= MAP_XY_FACTOR;

		minX += m_map->getBorderSizeInline();
		maxX += m_map->getBorderSizeInline();
		minY += m_map->getBorderSizeInline();
		maxY += m_map->getBorderSizeInline();

		visMinX = m_map->getXExtent();
		visMinY = m_map->getYExtent();
		visMaxX = 0;
		visMaxY = 0;

		///< @todo find out why values go out of range
		if (minX<0) minX=0;
		if (minY<0) minY=0;
		if (maxX > visMinX) maxX = visMinX;
		if (maxY > visMinY) maxY = visMinY;

		const FrustumClass & frustum = camera->Get_Frustum();
		Int limit = (maxX-minX)/2;
		if (limit > WIDE_STEP/2) {
			limit=WIDE_STEP/2;
		}
		calcVis(frustum, m_map, minX-WIDE_STEP/2, minY-WIDE_STEP/2, maxX+WIDE_STEP/2, maxY+WIDE_STEP/2, limit);

		newOrgX = (visMaxX+visMinX)/2 - m_x/2;
		newOrgY = (visMaxY+visMinY)/2 - m_y/2;
	}
	else
	{
		// TheSuperHackers @fix Very fast approximation. Works well for all camera pitches, but is less accurate
		// than the original implementation. Using this method for higher camera pitch would require to increase
		// the normal draw width by at least one tile length.
		const Real visibleTerrainEdgeLen = (m_x+m_y)/2 * MAP_XY_FACTOR;
		const Real magicEdgeLenScale = 0.25f * visibleTerrainEdgeLen;
		Vector3 viewDir = camera->Get_Forward_Dir();
		Vector2 shiftPivot;
		shiftPivot.X = viewDir.X * magicEdgeLenScale;
		shiftPivot.Y = viewDir.Y * magicEdgeLenScale;

		newOrgX = WWMath::Round((cameraPivot->X + shiftPivot.X)/MAP_XY_FACTOR) - m_x/2 + m_map->getBorderSizeInline();
		newOrgY = WWMath::Round((cameraPivot->Y + shiftPivot.Y)/MAP_XY_FACTOR) - m_y/2 + m_map->getBorderSizeInline();
	}

	WorldHeightMap::DrawArea newDrawArea = m_map->createDrawArea(newOrgX, newOrgY);

	if (m_needFullUpdate)
	{
		m_needFullUpdate = false;
		m_map->setDrawArea(newDrawArea);
		updateBlock(0, 0, m_x-1, m_y-1, m_map, pLightsIterator);
		m_updating = false;
		return;
	}
	else
	{
		constexpr const Int cellOffset = 1;
		const Int deltaX = newDrawArea.originX - m_map->getDrawOrgX();
		const Int deltaY = newDrawArea.originY - m_map->getDrawOrgY();

		if (IABS(deltaX) > m_x/2 || IABS(deltaY)>m_x/2) {
			if (m_map->setDrawArea(newDrawArea)) {
				m_originY = 0;
				m_originX = 0;
				updateBlock(0, 0, m_x-1, m_y-1, m_map, pLightsIterator);
			}
			m_updating = false;
			return;
		}

		if (abs(deltaY) > CENTER_LIMIT) {
			if (m_map->setDrawOrg(m_map->getDrawOrgX(), newOrgY)) {
				Int minY = 0;
				Int maxY = 0;
				m_originY += deltaY;
				if (m_originY >= m_y-1) m_originY -= m_y-1;
				if (deltaY<0) {
					minY = m_originY;
					maxY = m_originY-deltaY;
				} else {
					minY = m_originY - deltaY;
					maxY = m_originY;
				}
				minY-=cellOffset;
				if (m_originY < 0) m_originY += m_y-1;
				if (minY<0) {
					minY += m_y-1;
					if (minY<0) minY = 0;
					updateBlock(0, minY, m_x-1, m_y-1, m_map, pLightsIterator);
					updateBlock(0, 0, m_x-1, maxY, m_map, pLightsIterator);
				} else {
					updateBlock(0, minY, m_x-1, maxY, m_map, pLightsIterator);
				}
			}
			// It is much more efficient to update a couple of columns one frame, and then
			// a couple of rows.  So if we aren't "jumping" to a new view, and have done X
			// recently, return.
			if (abs(deltaX) < BIG_JUMP && !m_doXNextTime) {
				m_updating = false;
				m_doXNextTime = true;
				return;	// Only do the y this frame.  Do x next frame.  jba.
			}
		}
		if (abs(deltaX) > CENTER_LIMIT) {
			m_doXNextTime = false;
			newOrgX = m_map->getDrawOrgX() + deltaX;
			if (m_map->setDrawOrg(newOrgX, m_map->getDrawOrgY())) {
				Int minX = 0;
				Int maxX = 0;
				m_originX += deltaX;
				if (m_originX >= m_x-1) m_originX -= m_x-1;
				if (deltaX<0) {
					minX = m_originX;
					maxX = m_originX-deltaX;
				} else {
					minX = m_originX - deltaX;
					maxX = m_originX;
				}
				minX-=cellOffset;
				maxX+=cellOffset;
				if (m_originX < 0) m_originX += m_x-1;
				if (minX<0) {
					minX += m_x-1;
					if (minX<0) minX = 0;
					updateBlock(minX,0,m_x-1, m_y-1, m_map, pLightsIterator);
					updateBlock(0,0,maxX, m_y-1, m_map, pLightsIterator);
				} else {
					updateBlock(minX,0,maxX, m_y-1, m_map, pLightsIterator);
				}
			}
		}
	}
	m_updating = false;
}

//=============================================================================
// HeightMapRenderObjClass::Render
//=============================================================================
/** Renders (draws) the terrain. */
//=============================================================================
//DECLARE_PERF_TIMER(Terrain_Render)

bool HeightMapRenderObjClass::drawTerrainGeometry(CameraClass &camera,
    const RenderBackendTerrainState &terrain, const RenderBackendMaterialState &material,
    TextureClass *baseTexture)
{
    IRenderBackend *backend = WW3D::Get_Render_Backend();
    if (Is_Hidden()) return true;
    if (!backend || !backend->Is_Device_Ready() || !m_geometry) return false;
    RenderBackendTextureHandle base;
    if (baseTexture) {
        if (!baseTexture->Ensure_Renderer_Texture()) {
            std::fprintf(stderr,"Foreground terrain texture upload failed.\n"); return false;
        }
        base = baseTexture->Get_Renderer_Texture();
    }
    if (m_geometry->backend != backend) {
        for (auto &tile : m_geometry->tiles) m_geometry->release(tile);
        m_geometry->backend = backend;
    }
    Matrix4x4 saved;
    backend->Get_View_Projection(saved);
    struct Restore { IRenderBackend *backend; const Matrix4x4 &saved;
        ~Restore() { backend->Set_View_Projection(saved); } } restore={backend,saved};
    camera.Apply();
    for (Int y=0; y<m_numVBTilesY; ++y) for (Int x=0; x<m_numVBTilesX; ++x) {
        auto &tile = m_geometry->tiles[y*m_numVBTilesX+x];
        if (tile.dirty) { m_geometry->release(tile); tile.dirty = false; }
        auto &handle = tile.geometry;
        if (!backend->Is_Geometry_Valid(handle)) {
            const Int columns = std::min(VERTEX_BUFFER_TILE_LENGTH,m_x-1-x*VERTEX_BUFFER_TILE_LENGTH);
            const Int rows = std::min(VERTEX_BUFFER_TILE_LENGTH,m_y-1-y*VERTEX_BUFFER_TILE_LENGTH);
            if (columns<=0 || rows<=0) continue;
            std::vector<RenderBackendTerrainVertex> vertices(HEIGHTMAP_VERTEX_NUM);
            const TerrainLightingVertex *source = getVertexBufferTile(x,y);
            std::vector<unsigned short> indices;
            indices.reserve(columns*rows*6);
            for (Int row=0; row<rows; ++row) for (Int column=0; column<columns; ++column) {
                const unsigned short first = (row*VERTEX_BUFFER_TILE_LENGTH+column)*4;
                for (unsigned n=0; n<4; ++n) {
                    const auto &v = source[first+n];
                    auto &out = vertices[first+n];
                    out.x=v.x; out.y=v.y; out.z=v.z;
                    out.r=((v.diffuse>>16)&255)/255.f;
                    out.g=((v.diffuse>>8)&255)/255.f;
                    out.b=(v.diffuse&255)/255.f;
                    out.a=((v.diffuse>>24)&255)/255.f;
                    out.u=v.u1; out.v=v.v1; out.u2=v.u2; out.v2=v.v2;
                }
                indices.insert(indices.end(),{first,static_cast<unsigned short>(first+2),static_cast<unsigned short>(first+3),
                    first,static_cast<unsigned short>(first+1),static_cast<unsigned short>(first+2)});
            }
            handle = backend->Create_Static_Indexed_Terrain_Geometry(vertices.data(),vertices.size(),indices.data(),indices.size());
            if (!handle.Is_Valid()) { std::fprintf(stderr,"Foreground terrain geometry upload failed.\n"); return false; }
        }
        if (!backend->Draw_Static_Indexed_Terrain_Geometry(handle,base,material,terrain)) {
            std::fprintf(stderr,"Foreground terrain draw failed.\n"); return false;
        }
    }
    return true;
}

bool HeightMapRenderObjClass::renderTerrainPass(CameraClass *camera, MaterialPassClass *pass)
{
    if (!camera || !pass) return false;
    RenderBackendMaterialState material;
    RenderBackendTerrainState terrain;
    TextureClass *texture = nullptr;
    bool ready = false;
    if (auto *shroud = dynamic_cast<W3DShroudMaterialPassClass *>(pass))
        ready = shroud->Prepare_Terrain_Pass(material,terrain,texture);
    else if (auto *mask = dynamic_cast<W3DMaskMaterialPassClass *>(pass))
        ready = mask->Prepare_Terrain_Pass(material,terrain,texture);
    if (!ready) {
        std::fprintf(stderr,"Unsupported additional foreground terrain pass.\n"); return false;
    }
    // The historical additional pass uses identity world coordinates, independently
    // of the base terrain Transform; derived pass projection remains authoritative.
    return drawTerrainGeometry(*camera,terrain,material,texture);
}

void HeightMapRenderObjClass::Render(RenderInfoClass &rinfo)
{
    if (!m_map || !m_geometry) return;
    const Bool doCloud = useCloud();
    if (doCloud) W3DShaderManager::updateCloud();
    if (m_treeBuffer) m_treeBuffer->setIsTerrain();
#ifdef DO_UNIT_TIMINGS
#pragma MESSAGE("*** WARNING *** DOING DO_UNIT_TIMINGS!!!!")
    return;
#endif
    auto *scene = static_cast<RTS3DScene *>(rinfo.Camera.Get_User_Data());
    const bool custom = scene && (scene->getCustomPassMode()==SCENE_PASS_ALPHA_MASK ||
        scene->Get_Extra_Pass_Polygon_Mode()==SceneClass::EXTRA_PASS_CLEAR_LINE);
    if (custom && WW3D::Is_Texturing_Enabled() && rinfo.Additional_Pass_Count()) {
        renderTerrainPass(&rinfo.Camera,rinfo.Peek_Additional_Pass(0));
        return;
    }
    RenderBackendTerrainState terrain;
    terrain.project_world_coordinates=true;
    for (unsigned row=0; row<3; ++row) for (unsigned column=0; column<4; ++column)
        terrain.world_transform[row*4+column]=Transform[row][column];
    RenderBackendMaterialState material;
    ShaderClass shader = m_disableTextures ? ShaderClass::_PresetOpaque2DShader : m_shaderClass;
    shader.Set_Post_Detail_Color_Func(ShaderClass::DETAILCOLOR_DISABLE);
    shader.Set_Post_Detail_Alpha_Func(ShaderClass::DETAILALPHA_DISABLE);
    if (!shader.Get_Render_Backend_State(material)) {
        std::fprintf(stderr,"Foreground terrain material translation failed.\n"); return;
    }
    material.color_write_mask=7;
    if (custom && !WW3D::Is_Texturing_Enabled()) {
        if (!ShaderClass::_PresetOpaqueSolidShader.Get_Render_Backend_State(material)) return;
        material.color_write_mask=7;
        material.wireframe=true;
        terrain = RenderBackendTerrainState{};
        terrain.project_world_coordinates=true;
        terrain.use_constant_color=true;
        terrain.constant_color[0]=terrain.constant_color[1]=terrain.constant_color[2]=128.f/255.f;
        terrain.constant_color[3]=1.f;
        drawTerrainGeometry(rinfo.Camera,terrain,material,nullptr);
        return;
    }
    TextureClass *base = m_disableTextures ? nullptr : m_stageZeroTexture;
    if (base) {
        if (!base->Ensure_Renderer_Texture() || !base->Get_Filter().Get_Render_Sampler(material.sampler)) {
            std::fprintf(stderr,"Foreground terrain atlas upload failed.\n"); return;
        }
        material.sampler.min_filter=material.sampler.mag_filter=
            TheGlobalData->m_bilinearTerrainTex || TheGlobalData->m_trilinearTerrainTex ?
            RenderBackendTextureFilter::Linear : RenderBackendTextureFilter::Point;
        material.sampler.mip_filter=TheGlobalData->m_trilinearTerrainTex ?
            RenderBackendTextureFilter::Linear : RenderBackendTextureFilter::Point;
        material.sampler.mipmaps=true;
        material.sampler.max_anisotropy=1;
        material.sampler.address_u=material.sampler.address_v=RenderBackendTextureAddress::Clamp;
        terrain.blend_secondary_by_vertex_alpha=true;
        terrain.shroud_texture=base->Get_Renderer_Texture();
        if (!base->Get_Filter().Get_Render_Sampler(terrain.shroud_sampler,1)) return;
        terrain.shroud_sampler.min_filter=terrain.shroud_sampler.mag_filter=material.sampler.min_filter;
        terrain.shroud_sampler.mipmaps=true;
        terrain.shroud_sampler.max_anisotropy=1;
        terrain.shroud_sampler.address_u=terrain.shroud_sampler.address_v=RenderBackendTextureAddress::Clamp;
        terrain.shroud_sampler.mip_filter=RenderBackendTextureFilter::Linear;
        if (!ShaderClass::Is_Backface_Culling_Inverted()) {
            W3DShaderManager::getTerrainNoiseProjection(terrain.cloud_noise_projection[0],
                terrain.cloud_noise_projection[1],terrain.cloud_noise_projection[2]);
            auto layer=[](TextureClass *texture, RenderBackendTextureHandle &handle,
                RenderBackendSamplerState &sampler, unsigned stage) {
                if (!texture || !texture->Ensure_Renderer_Texture() ||
                    !texture->Get_Filter().Get_Render_Sampler(sampler,stage)) return false;
                handle=texture->Get_Renderer_Texture();
                sampler.min_filter=sampler.mag_filter=RenderBackendTextureFilter::Linear;
                sampler.max_anisotropy=1;
                sampler.address_u=sampler.address_v=RenderBackendTextureAddress::Wrap;
                return handle.Is_Valid();
            };
            if (doCloud && !layer(m_stageTwoTexture,terrain.cloud_texture,terrain.cloud_sampler,2)) return;
            if (TheGlobalData->m_useLightMap) {
                if (!layer(m_stageThreeTexture,terrain.noise_texture,terrain.noise_sampler,3)) return;
                terrain.noise_sampler.min_filter=RenderBackendTextureFilter::Point;
            }
        }
    }
    if (!drawTerrainGeometry(rinfo.Camera,terrain,material,base)) return;
    if (!custom) {
        renderShoreLines(&rinfo.Camera);
        if (TheGlobalData->m_use3WayTerrainBlends) renderExtraBlendTiles(&rinfo.Camera);
        const Int yCoordMin=m_map->getDrawOrgY(), yCoordMax=m_y+yCoordMin-1;
        const Int xCoordMin=m_map->getDrawOrgX(), xCoordMax=m_x+xCoordMin-1;
#ifdef DO_ROADS
        if (!ShaderClass::Is_Backface_Culling_Inverted() && Scene) {
            RTS3DScene *terrainScene=static_cast<RTS3DScene *>(Scene);
            RefRenderObjListIterator lights(terrainScene->getDynamicLights());
            m_roadBuffer->drawRoads(&rinfo.Camera,doCloud?m_stageTwoTexture:nullptr,
                TheGlobalData->m_useLightMap?m_stageThreeTexture:nullptr,m_disableTextures,
                xCoordMin-m_map->getBorderSizeInline(),xCoordMax-m_map->getBorderSizeInline(),
                yCoordMin-m_map->getBorderSizeInline(),yCoordMax-m_map->getBorderSizeInline(),&lights,Transform);
        }
#endif
        if (m_propBuffer) m_propBuffer->drawProps(rinfo);
        drawScorches(rinfo.Camera,Transform);
        m_bridgeBuffer->drawBridges(&rinfo.Camera,m_disableTextures,doCloud?m_stageTwoTexture:nullptr,Transform);
        if (TheTerrainTracksRenderObjClassSystem) TheTerrainTracksRenderObjClassSystem->flush();
        if (m_shroud && rinfo.Additional_Pass_Count())
            renderTerrainPass(&rinfo.Camera,rinfo.Peek_Additional_Pass(0));
    } else m_bridgeBuffer->drawBridges(&rinfo.Camera,m_disableTextures,m_stageTwoTexture,Transform);
    if (m_waypointBuffer) m_waypointBuffer->drawWaypoints(rinfo);
    m_bibBuffer->renderBibs(&rinfo.Camera,Transform);
}


//=============================================================================
// HeightMapRenderObjClass::renderExtraBlendTiles
//=============================================================================
/** Renders an additional terrain pass including only those tiles which have more than 2 textures
blended together.  Used primarily for corner cases where 3 different textures meet.*/
void HeightMapRenderObjClass::renderExtraBlendTiles(CameraClass *camera)
{
    m_numVisibleExtraBlendTiles = 0;
    if (!m_numExtraBlendTiles || !m_map || !camera) return;
    IRenderBackend *backend = WW3D::Get_Render_Backend();
    if (!backend || !backend->Is_Device_Ready()) return;
    Int vertexCount = 0;
    Int indexCount = 0;
    const Int xExtent = m_map->getXExtent();
    const Int border = m_map->getBorderSizeInline();
    static Int maxBlendTiles = DEFAULT_MAX_FRAME_EXTRABLEND_TILES;
    if (maxBlendTiles > 10000) maxBlendTiles = 10000;
    std::vector<RenderBackendTerrainVertex> vertices;
    std::vector<UnsignedShort> indices;
    vertices.reserve(maxBlendTiles*4);
    indices.reserve(maxBlendTiles*6);
    RenderBackendTerrainVertex *vb = nullptr;
    UnsignedShort *ib = nullptr;
    const bool debug = TheGlobalData->m_use3WayTerrainBlends == 2;
    auto setColor = [debug](RenderBackendTerrainVertex &vertex, UnsignedInt diffuse, UnsignedByte alpha) {
        const Vector4 color = Unpack_ARGB_Color((static_cast<UnsignedInt>(alpha)<<24) | (diffuse & 0x00ffffffu));
        vertex.r = debug ? 1.f : color.X;
        vertex.g = debug ? 1.f : color.Y;
        vertex.b = debug ? 1.f : color.Z;
        vertex.a = debug ? 1.f : color.W;
    };
	const UnsignedByte* data = m_map->getDataPtr();

	//Loop over visible terrain and extract all the tiles that need extra blend
	Int drawEdgeY=m_map->getDrawOrgY()+m_map->getDrawHeight()-1;
	Int drawEdgeX=m_map->getDrawOrgX()+m_map->getDrawWidth()-1;
	if (drawEdgeX > (m_map->getXExtent()-1))
		drawEdgeX = m_map->getXExtent()-1;
	if (drawEdgeY > (m_map->getYExtent()-1))
		drawEdgeY = m_map->getYExtent()-1;
	Int drawStartX=m_map->getDrawOrgX();
	Int drawStartY=m_map->getDrawOrgY();

	for (Int j=0; j<m_numExtraBlendTiles; j++)
	{
		if (vertexCount >= (maxBlendTiles*4))
			break;	//no room in vertex buffer

		Real U[4],V[4];
		UnsignedByte alpha[4];
		Bool flipState,cliffState;
		Int x = m_extraBlendTilePositions[j] & 0xffff;
		Int y = m_extraBlendTilePositions[j] >> 16;

		if (x >= drawStartX && x < drawEdgeX &&
			y >= drawStartY && y < drawEdgeY &&
			m_map->getExtraAlphaUVData(x,y,U,V,alpha,&flipState, &cliffState))
		{	//this tile is inside visible region and has 3rd blend layer.
            vertices.resize(vertexCount+4);
            indices.resize(indexCount+6);
            vb = vertices.data()+vertexCount;
            ib = indices.data()+indexCount;

			Int idx = x+y*xExtent;

			Real p0=data[idx]*MAP_HEIGHT_SCALE;
			Real p1=data[idx+1]*MAP_HEIGHT_SCALE;
			Real p2=data[idx + 1 + xExtent]*MAP_HEIGHT_SCALE;
			Real p3=data[idx + xExtent]*MAP_HEIGHT_SCALE;
			if (cliffState && abs(p0-p2) > abs(p1-p3))	//cliffs sometimes force a flip
				flipState = TRUE;

			vb->x=(x-border)*MAP_XY_FACTOR;
			vb->y=(y-border)*MAP_XY_FACTOR;
			vb->z=p0;
			setColor(*vb,getStaticDiffuse(x,y),alpha[0]);
			vb->u=U[0];
			vb->v=V[0];
			vb->u2=0;
			vb->v2=0;
			vb++;

			vb->x=(x+1-border)*MAP_XY_FACTOR;
			vb->y=(y-border)*MAP_XY_FACTOR;
			vb->z=p1;
			setColor(*vb,getStaticDiffuse(x+1,y),alpha[1]);
			vb->u=U[1];
			vb->v=V[1];
			vb->u2=0;
			vb->v2=0;
			vb++;

			vb->x=(x+1-border)*MAP_XY_FACTOR;
			vb->y=(y+1-border)*MAP_XY_FACTOR;
			vb->z=p2;
			setColor(*vb,getStaticDiffuse(x+1,y+1),alpha[2]);
			vb->u=U[2];
			vb->v=V[2];
			vb->u2=0;
			vb->v2=0;
			vb++;

			vb->x=(x-border)*MAP_XY_FACTOR;
			vb->y=(y+1-border)*MAP_XY_FACTOR;
			vb->z=p3;
			setColor(*vb,getStaticDiffuse(x,y+1),alpha[3]);
			vb->u=U[3];
			vb->v=V[3];
			vb->u2=0;
			vb->v2=0;
			vb++;

			if (flipState)
			{
				ib[0]=1+vertexCount;
				ib[1]=3+vertexCount;
				ib[2]=0+vertexCount;
				ib[3]=1+vertexCount;
				ib[4]=2+vertexCount;
				ib[5]=3+vertexCount;
			}
			else
			{
				ib[0]=0+vertexCount;
				ib[1]=2+vertexCount;
				ib[2]=3+vertexCount;
				ib[3]=0+vertexCount;
				ib[4]=1+vertexCount;
				ib[5]=2+vertexCount;
			}
			ib += 6;
			vertexCount +=4;
			indexCount +=6;
		}
	}
    if (!vertexCount) return;
    if (vertexCount == maxBlendTiles*4) maxBlendTiles += 16;
    if (Is_Hidden()) return;
    ShaderClass shader = ShaderClass::_PresetOpaqueShader;
    shader.Set_Depth_Mask(ShaderClass::DEPTH_WRITE_DISABLE);
    if (debug) {
        shader.Set_Primary_Gradient(ShaderClass::GRADIENT_DISABLE);
        shader.Set_Texturing(ShaderClass::TEXTURING_DISABLE);
    } else {
        shader.Set_Src_Blend_Func(ShaderClass::SRCBLEND_SRC_ALPHA);
        shader.Set_Dst_Blend_Func(ShaderClass::DSTBLEND_ONE_MINUS_SRC_ALPHA);
    }
    RenderBackendMaterialState material;
    if (!shader.Get_Render_Backend_State(material)) {
        std::fprintf(stderr,"Extra-blend terrain material translation failed.\n"); return;
    }
    material.color_write_mask = 7; // Water owns the shoreline destination alpha.
    RenderBackendTerrainState terrain;
    terrain.project_world_coordinates = true;
    for (unsigned row=0;row<3;++row)
        for (unsigned column=0;column<4;++column)
            terrain.world_transform[row*4+column] = Transform[row][column];
    W3DShaderManager::getTerrainNoiseProjection(terrain.cloud_noise_projection[0],
        terrain.cloud_noise_projection[1],terrain.cloud_noise_projection[2]);
    RenderBackendTextureHandle base;
    if (!debug) {
        // Both authored atlas coordinate sets sample the same retained terrain texture.
        if (!m_stageZeroTexture || !m_stageZeroTexture->Ensure_Renderer_Texture() ||
            !m_stageZeroTexture->Get_Filter().Get_Render_Sampler(material.sampler)) {
            std::fprintf(stderr,"Extra-blend terrain tile texture upload failed.\n"); return;
        }
        base = m_stageZeroTexture->Get_Renderer_Texture();
        const auto mipFilter = TheGlobalData->m_trilinearTerrainTex ?
            RenderBackendTextureFilter::Linear : RenderBackendTextureFilter::Point;
        const bool combinedLayers = useCloud() && TheGlobalData->m_useLightMap;
        const bool linear = TheGlobalData->m_bilinearTerrainTex || TheGlobalData->m_trilinearTerrainTex;
        material.sampler.min_filter = material.sampler.mag_filter = linear ?
            RenderBackendTextureFilter::Linear : RenderBackendTextureFilter::Point;
        material.sampler.mip_filter = mipFilter;
        material.sampler.mipmaps = true;
        material.sampler.max_anisotropy = 1;
        material.sampler.address_u = material.sampler.address_v = RenderBackendTextureAddress::Clamp;
        auto layer = [mipFilter](TextureClass *texture, RenderBackendTextureHandle &handle,
                                RenderBackendSamplerState &sampler, unsigned stage,
                                RenderBackendTextureFilter filter, bool overrideMip) {
            if (!texture || !texture->Ensure_Renderer_Texture() ||
                !texture->Get_Filter().Get_Render_Sampler(sampler,stage)) return false;
            handle = texture->Get_Renderer_Texture();
            sampler.min_filter = filter;
            sampler.mag_filter = RenderBackendTextureFilter::Linear;
            if (overrideMip) {
                sampler.mip_filter = mipFilter;
                sampler.mipmaps = true;
            }
            sampler.max_anisotropy = 1;
            sampler.address_u = sampler.address_v = RenderBackendTextureAddress::Wrap;
            return handle.Is_Valid();
        };
        if (useCloud() && !layer(m_stageTwoTexture,terrain.cloud_texture,terrain.cloud_sampler,1,RenderBackendTextureFilter::Linear,true)) {
            std::fprintf(stderr,"Extra-blend terrain cloud upload failed.\n"); return;
        }
        if (TheGlobalData->m_useLightMap && !layer(m_stageThreeTexture,terrain.noise_texture,terrain.noise_sampler,2,RenderBackendTextureFilter::Point,!combinedLayers)) {
            std::fprintf(stderr,"Extra-blend terrain noise upload failed.\n"); return;
        }
    }
    // Dual noise used roadnoise2.nvp; single-noise fixed stages lit first.
    terrain.diffuse_after_layers = terrain.cloud_texture.Is_Valid() && terrain.noise_texture.Is_Valid();
    Matrix4x4 savedProjection;
    backend->Get_View_Projection(savedProjection);
    struct RestoreProjection {
        IRenderBackend *backend;
        const Matrix4x4 &projection;
        ~RestoreProjection() { backend->Set_View_Projection(projection); }
    } restore={backend,savedProjection};
    camera->Apply();
    if (!backend->Draw_Indexed_Terrain_Triangles(vertices.data(),vertexCount,indices.data(),indexCount,base,material,terrain)) {
        std::fprintf(stderr,"Extra-blend terrain draw failed.\n"); return;
    }
    m_numVisibleExtraBlendTiles = indexCount/6;
}

#endif
