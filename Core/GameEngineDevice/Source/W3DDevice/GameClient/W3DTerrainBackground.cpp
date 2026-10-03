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

// FILE: W3DTerrainBackground.cpp ////////////////////////////////////////////////
//-----------------------------------------------------------------------------
//
//                       EA Pacific.
//
//                       Confidential Information
//                Copyright (C) 2003 - All Rights Reserved
//
//-----------------------------------------------------------------------------
//
// Project:   RTS3
//
// File name: W3DTerrainBackground.cpp
//
// Created:   John Ahlquist, March 2003
//
// Desc:      Draw buffer to handle backup terrain at lower res.
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//         Includes
//-----------------------------------------------------------------------------

#include "W3DDevice/GameClient/W3DTerrainBackground.h"

#include <WW3D2/assetmgr.h>
#include <WW3D2/texture.h>
#include "Common/GlobalData.h"
#include "GameClient/View.h"
#include "W3DDevice/GameClient/TerrainTex.h"
#include "W3DDevice/GameClient/HeightMap.h"
#include "WW3D2/ww3d.h"
#include "WW3D2/IRenderBackend.h"
#include "WW3D2/camera.h"
#include <vector>

// D3D12 migration notes (W3DTerrainBackground):
// - CPU tessellation/UV/static-lighting gen is unchanged.
// - Buffers are CPU vectors; drawVisiblePolys() converts to
//   RenderBackendTexturedVertex and submits via Draw_Indexed_Material_Triangles.
// - 4X/2X/1X selection is preserved but submitted as a single-texture material
//   draw. Dual-UV/second-stage detail is not submitted (no multitexture PSO).
// - Cloud/noise-only passes call drawVisiblePolys(disableTextures=TRUE); those
//   are skipped here to avoid re-drawing base geometry (documented gap).


//-----------------------------------------------------------------------------
//         Private Data
//-----------------------------------------------------------------------------
// A W3D shader that does alpha, texturing, tests zbuffer, doesn't update zbuffer.
#define SC_DETAIL ( SHADE_CNST(ShaderClass::PASS_LEQUAL, ShaderClass::DEPTH_WRITE_ENABLE, ShaderClass::COLOR_WRITE_ENABLE, ShaderClass::SRCBLEND_ONE, \
	ShaderClass::DSTBLEND_ZERO, ShaderClass::FOG_DISABLE, ShaderClass::GRADIENT_MODULATE, ShaderClass::SECONDARY_GRADIENT_DISABLE, ShaderClass::TEXTURING_ENABLE, \
	ShaderClass::ALPHATEST_DISABLE, ShaderClass::CULL_MODE_DISABLE, \
	ShaderClass::DETAILCOLOR_DISABLE, ShaderClass::DETAILALPHA_DISABLE) )

static ShaderClass detailShader(SC_DETAIL);

const Int PIXELS_PER_GRID = 8; // default tex resolution allocated for each tile. jba. [3/24/2003]

//-----------------------------------------------------------------------------
//         Private Functions
//-----------------------------------------------------------------------------


//=============================================================================
// W3DTerrainBackground::loadTerrainInVertexAndIndexBuffers
//=============================================================================
/** Loads the terrain into the vertex buffer for drawing. */
//=============================================================================
void W3DTerrainBackground::setFlip(WorldHeightMap *htMap)
{
	if (m_map==nullptr) return;
	if (htMap) {
		REF_PTR_SET(m_map, htMap);
	}
	if (!m_initialized) {
		return;
	}

	setFlipRecursive(0, 0, m_width);


}


const Int STEP=4;
//=============================================================================
// W3DTerrainBackground::doPartialUpdate
//=============================================================================
/** Updates a partial block of vertices from [x0,y0 to x1,y1]
The coordinates in partialRange are map cell coordinates, relative to the entire map.
The vertex coordinates and texture coordinates, as well as static lighting are updated.
*/
void W3DTerrainBackground::doPartialUpdate(const IRegion2D &partialRange, WorldHeightMap *htMap, Bool doTextures )
{
	if (m_map==nullptr) return;
	if (htMap) {
		REF_PTR_SET(m_map, htMap);
	}

	if (!m_initialized) {
		return;
	}
	doTesselatedUpdate(partialRange, htMap, doTextures);
}

//=============================================================================
// W3DTerrainBackground::fillVBRecursive
//=============================================================================
/** Fills in vertex & index buffers.
*/
Bool W3DTerrainBackground::advanceLeft(ICoord2D &left, Int xOffset, Int yOffset, Int width)
{
	while (left.y < yOffset+width) {
		left.y++;
		if (m_map->getFlipState(left.x+m_xOrigin, left.y+m_yOrigin)) {
			return true;
		}
	}
	while (left.x < xOffset+width-1) {
		left.x++;
		if (m_map->getFlipState(left.x+m_xOrigin, left.y+m_yOrigin)) {
			return true;
		}
	}
	return false;
}

//=============================================================================
// W3DTerrainBackground::fillVBRecursive
//=============================================================================
/** Fills in vertex & index buffers.
*/
Bool W3DTerrainBackground::advanceRight(ICoord2D &right, Int xOffset, Int yOffset, Int width)
{
	while (right.x < xOffset+width) {
		right.x++;
		if (m_map->getFlipState(right.x+m_xOrigin, right.y+m_yOrigin)) {
			return true;
		}
	}
	while (right.y < yOffset+width-1) {
		right.y++;
		if (m_map->getFlipState(right.x+m_xOrigin, right.y+m_yOrigin)) {
			return true;
		}
	}
	return false;
}

//=============================================================================
// W3DTerrainBackground::fillVBRecursive
//=============================================================================
/** Fills in vertex & index buffers.
*/
void W3DTerrainBackground::fillVBRecursive(UnsignedShort *ib, Int xOffset, Int yOffset,
																					 Int width, UnsignedShort *ndx, Int &curIndex)
{

	Int bottomLeftNdx	= ndx[xOffset+yOffset*(m_width+1)];
	Int topRightNdx	= ndx[xOffset+width + (yOffset+width)*(m_width+1)];

	Int limitX = m_map->getXExtent()-1;
	Int limitY = m_map->getYExtent()-1;
	Int i, j;
	Bool match = true;
	Int minX = m_xOrigin+xOffset;
	Int minY = m_yOrigin+yOffset;
	Int cornerHeight = m_map->getHeight(minX, minY);

	for (i=0; i<=width; i++) {
		for (j=0; j<=width; j++) {
			Int k = minX+i;
			k = k<limitX?k:limitX;
			Int l = minY+j;
			l = l<limitY?l:limitY;
			if (cornerHeight!=m_map->getHeight(k, l)) {
				match = false;
				break;
			}
		}
	}
	if (width==1) {
		match = true;
	}

	if (match) {


		UnsignedShort prevNdxLeft;
		UnsignedShort prevNdxRight;
		ICoord2D left;
		left.x = xOffset;
		left.y = yOffset;
		ICoord2D right;
		right.x = xOffset;
		right.y = yOffset;
		advanceLeft(left, xOffset, yOffset, width);
		advanceRight(right, xOffset, yOffset, width);

		if (ib) {
			ib[curIndex] = bottomLeftNdx;
		}
		curIndex++;

		prevNdxRight = ndx[right.x+right.y*(m_width+1)];
		if (ib) {
			ib[curIndex] = prevNdxRight;
		}
		curIndex++;

		prevNdxLeft = ndx[left.x+left.y*(m_width+1)];
		if (ib) {
			ib[curIndex] = prevNdxLeft;
		}
		curIndex++;
		Bool didLeft = true;
		Bool didRight = true;
		while (didLeft || didRight) {
			didLeft = advanceLeft(left, xOffset, yOffset, width);
			if (didLeft) {

				if (ib) {
					ib[curIndex] = prevNdxLeft;
				}
				curIndex++;

				if (ib) {
					ib[curIndex] = prevNdxRight;
				}
				curIndex++;

				prevNdxLeft = ndx[left.x+left.y*(m_width+1)];
				if (ib) {
					ib[curIndex] = prevNdxLeft;
				}
				curIndex++;
			}
			didRight = advanceRight(right, xOffset, yOffset, width);
			if (didRight) {

				if (ib) {
					ib[curIndex] = prevNdxLeft;
				}
				curIndex++;

				if (ib) {
					ib[curIndex] = prevNdxRight;
				}
				curIndex++;

				prevNdxRight = ndx[right.x+right.y*(m_width+1)];
				if (ib) {
					ib[curIndex] = prevNdxRight;
				}
				curIndex++;
			}

		}

		if (ib) {
			ib[curIndex] = prevNdxLeft;
		}

		curIndex++;

		if (ib) {
			ib[curIndex] = prevNdxRight;
		}
		curIndex++;

		if (ib) {
			ib[curIndex] = topRightNdx;
		}
		curIndex++;

		return;
	}
	Int halfWidth = width/2;

	fillVBRecursive(ib, xOffset, yOffset, halfWidth, ndx, curIndex);
	fillVBRecursive(ib, xOffset, yOffset+halfWidth, halfWidth, ndx, curIndex);
	fillVBRecursive(ib, xOffset+halfWidth, yOffset, halfWidth, ndx, curIndex);
	fillVBRecursive(ib, xOffset+halfWidth, yOffset+halfWidth, halfWidth, ndx, curIndex);

}

//=============================================================================
// W3DTerrainBackground::fillVBRecursive
//=============================================================================
/** Fills in vertex & index buffers.
*/
void W3DTerrainBackground::setFlipRecursive(Int xOffset, Int yOffset, Int width)
{

	Int limitX = m_map->getXExtent()-1;
	Int limitY = m_map->getYExtent()-1;
	Int i, j;
	Bool match = true;
	Int minX = m_xOrigin+xOffset;
	Int minY = m_yOrigin+yOffset;
	Int cornerHeight = m_map->getHeight(minX, minY);

	for (i=0; i<=width; i++) {
		for (j=0; j<=width; j++) {
			Int k = minX+i;
			k = k<limitX?k:limitX;
			Int l = minY+j;
			l = l<limitY?l:limitY;
			if (cornerHeight!=m_map->getHeight(k, l)) {
				match = false;
				break;
			}
		}
	}
	if (width==1) {
		match = true;
	}

	if (match) {
		m_map->setFlipState(minX, minY, true);
		m_map->setFlipState(minX+width, minY, true);
		m_map->setFlipState(minX+width, minY+width, true);
		m_map->setFlipState(minX, minY+width, true);
		return;
	}
	Int halfWidth = width/2;


	setFlipRecursive(xOffset, yOffset, halfWidth);
	setFlipRecursive(xOffset, yOffset+halfWidth, halfWidth);
	setFlipRecursive(xOffset+halfWidth, yOffset, halfWidth);
	setFlipRecursive(xOffset+halfWidth, yOffset+halfWidth, halfWidth);

}

//=============================================================================
// W3DTerrainBackground::doTesselatedUpdate
//=============================================================================
/** Updates a partial block of vertices from [x0,y0 to x1,y1]
The coordinates in partialRange are map cell coordinates, relative to the entire map.
The vertex coordinates and texture coordinates, as well as static lighting are updated.
*/
void W3DTerrainBackground::doTesselatedUpdate(const IRegion2D &partialRange, WorldHeightMap *htMap, Bool doTextures )
{
	if (m_map==nullptr) return;
	if (htMap) {
		REF_PTR_SET(m_map, htMap);
	}
	if (!m_initialized) {
		return;
	}
	Int minX = m_xOrigin;
	Int minY = m_yOrigin;
	Int maxX = m_xOrigin + m_width;
	Int maxY = m_yOrigin + m_width;
	Int limitX = m_map->getXExtent()-1;
	Int limitY = m_map->getYExtent()-1;

	if (partialRange.lo.x > maxX) return;
	if (partialRange.lo.y > maxY) return;
	if (partialRange.hi.x < minX) return;
	if (partialRange.hi.y < minY) return;

	setFlip(htMap);

	Int count = (m_width+1)*(m_width+1);

	UnsignedShort *ndx = new UnsignedShort[count];

	Int requiredVertex = 0;
	Int i, j;
	for (j=minY; j<=maxY; j++) {
		for (i=minX; i<=maxX; i++) {
			Int ndxNdx = i-minX + (m_width+1)*(j-minY);
			DEBUG_ASSERTCRASH(ndxNdx<count, ("Bad ndxNdx"));
			ndx[ndxNdx] = 0;
			if (m_map->getFlipState(i, j)) {
				requiredVertex++;
			}
		}
	}

	// D3D12: CPU staging vectors replace DX8 buffers. Tessellation/UV math unchanged.
	m_terrainVertices.clear();
	m_terrainVertices.reserve((size_t)requiredVertex + 4);

	m_curNumTerrainVertices = 0;
	// Add to the CPU vertex staging.
	for (j=minY; j<=maxY; j++) {
		for (i=minX; i<=maxX; i++) {
			if (m_map->getFlipState(i, j)) {
				VertexFormatXYZDUV2 v;
				v.diffuse = (0<<24)|TheTerrainRenderObject->getStaticDiffuse(i,j);
				Vector3 pos;
				Int k = i<limitX?i:limitX;
				Int l = j<limitY?j:limitY;
				pos.Z = ((float)m_map->getHeight(k,l)*MAP_HEIGHT_SCALE);
				pos.X = (i)*MAP_XY_FACTOR - m_map->getBorderSizeInline()*MAP_XY_FACTOR;
				pos.Y = (j)*MAP_XY_FACTOR - m_map->getBorderSizeInline()*MAP_XY_FACTOR;
				v.u1 = (float)(i-minX)/(float)(m_width);
				v.v1 = 1.0f - (float)(j-minY)/(float)(m_width);
				v.u2 = v.u1;
				v.v2 = v.v1;
				v.x = pos.X;
				v.y = pos.Y;
				v.z = pos.Z;
				m_terrainVertices.push_back(v);
				Int ndxNdx = i-minX + (m_width+1)*(j-minY);
				DEBUG_ASSERTCRASH(ndxNdx<count, ("Bad ndxNdx"));
				ndx[ndxNdx] = (UnsignedShort)m_curNumTerrainVertices;
				m_curNumTerrainVertices++;
			}
		}
	}

	Int requiredIndex = 0;

	fillVBRecursive(nullptr, 0, 0, m_width, ndx, requiredIndex);

	m_terrainIndices.resize((size_t)requiredIndex + 4);

	m_curNumTerrainIndices = 0;

	fillVBRecursive(m_terrainIndices.data(), 0, 0, m_width, ndx, m_curNumTerrainIndices);
	m_terrainIndices.resize((size_t)m_curNumTerrainIndices);
	delete[] ndx;
	ndx = nullptr;

	MinMaxAABoxClass bounds;
	bounds.Init_Empty();

	for (j=minY; j<=maxY; j+=1) {
		for (i=minX; i<=maxX; i+=1) {
			Vector3 pos;
			Int k = i<limitX?i:limitX;
			Int l = j<limitY?j:limitY;
			pos.Z = ((float)m_map->getHeight(k,l)*MAP_HEIGHT_SCALE);
			pos.X = (i)*MAP_XY_FACTOR - m_map->getBorderSizeInline()*MAP_XY_FACTOR;
			pos.Y = (j)*MAP_XY_FACTOR - m_map->getBorderSizeInline()*MAP_XY_FACTOR;
			bounds.Add_Point(pos);
		}
	}
	m_bounds.Init(bounds);

	if (m_terrainTexture == nullptr || doTextures) {
		REF_PTR_RELEASE(m_terrainTexture);
		REF_PTR_RELEASE(m_terrainTexture2X);
		REF_PTR_RELEASE(m_terrainTexture4X);
		m_terrainTexture = m_map->getFlatTexture(m_xOrigin, m_yOrigin, m_width, PIXELS_PER_GRID);
		//	DEBUG ONLY. jba. m_terrainTexture =  (TerrainTextureClass *)NEW_REF(TextureClass, ("TBBib.tga"));
		m_terrainTexture->Get_Filter().Set_U_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_CLAMP);
		m_terrainTexture->Get_Filter().Set_V_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_CLAMP);
	}

}

//-----------------------------------------------------------------------------
//         Public Functions
//-----------------------------------------------------------------------------

//=============================================================================
// W3DTerrainBackground::~W3DTerrainBackground
//=============================================================================
/** Destructor. Releases w3d assets. */
//=============================================================================
W3DTerrainBackground::~W3DTerrainBackground()
{
	freeTerrainBuffers();
	REF_PTR_RELEASE(m_terrainTexture);
	REF_PTR_RELEASE(m_terrainTexture2X);
	REF_PTR_RELEASE(m_terrainTexture4X);
}

//=============================================================================
// W3DTerrainBackground::W3DTerrainBackground
//=============================================================================
/** Constructor. Sets m_initialized to true if it finds the w3d models it needs
for the bibs. */
//=============================================================================
W3DTerrainBackground::W3DTerrainBackground():
m_initialized(FALSE),
m_terrainTexture(nullptr),
m_terrainTexture2X(nullptr),
m_terrainTexture4X(nullptr),
m_cullStatus(CULL_STATUS_UNKNOWN),
m_texMultiplier(TEX1X),
m_curNumTerrainVertices(0),
m_curNumTerrainIndices(0),
m_xOrigin(0),
m_yOrigin(0),
m_width(0),
m_map(nullptr),
m_anythingChanged(false)
{
}

//=============================================================================
// W3DTerrainBackground::freeTerrainBuffers
//=============================================================================
/** Frees the index and vertex buffers. */
//=============================================================================
void W3DTerrainBackground::freeTerrainBuffers()
{
	m_terrainVertices.clear();
	m_terrainIndices.clear();
	m_curNumTerrainVertices=0;
	m_curNumTerrainIndices=0;
	m_initialized = false;
	REF_PTR_RELEASE(m_map);
	REF_PTR_RELEASE(m_map);
}

//=============================================================================
// W3DTerrainBackground::allocateTerrainBuffers
//=============================================================================
/** Allocates the index and vertex buffers. */
//=============================================================================
void W3DTerrainBackground::allocateTerrainBuffers(WorldHeightMap *htMap, Int xOrigin, Int yOrigin, Int width)
{
	if (htMap==nullptr) return;
	freeTerrainBuffers(); // in case already allocated. jba [3/24/2003]
	m_curNumTerrainVertices=0;
	m_curNumTerrainIndices=0;
	m_xOrigin = xOrigin;
	m_yOrigin = yOrigin;
	m_width = width;
	m_initialized = true;
	REF_PTR_SET(m_map, htMap);
}


//=============================================================================
// W3DTerrainBackground::updateCenter
//=============================================================================
/** Updates the culling status. */
//=============================================================================
void W3DTerrainBackground::updateCenter(CameraClass *camera)
{
	if (camera->Cull_Box(m_bounds)) {
		m_cullStatus = CULL_STATUS_INVISIBLE;
	}	else {
		m_cullStatus = CULL_STATUS_VISIBLE;
	}

	if (m_cullStatus==CULL_STATUS_INVISIBLE) {
		REF_PTR_RELEASE(m_terrainTexture2X);
		REF_PTR_RELEASE(m_terrainTexture4X);
		m_texMultiplier = TEX1X;
		return;
	}
	Vector3 cameraPos = camera->Get_Position();
	const Real mipDistance = 310;
	const Real mipSlop = 40;
	const Real mip4xDistanceSqr = sqr(mipDistance+mipSlop);
	const Real mip2xDistanceSqr = sqr(2*mipDistance+mipSlop);
	const Real mipLODDistanceSqr = sqr(4*mipDistance+mipSlop);
	Real minDistSqr = 2*mip2xDistanceSqr;
	Int i, j, k;
	for (i=-1; i<2; i++) {
		for (j=-1; j<2; j++) {
			for (k=-1; k<2; k++) {
				Vector3 corner = m_bounds.Center;
				corner.X += m_bounds.Extent.X * i;
				corner.Y += m_bounds.Extent.Y * j;
				corner.Z += m_bounds.Extent.Z * k;
				Real distSqr = (cameraPos-corner).Length2();
				if (distSqr<minDistSqr) minDistSqr = distSqr;
			}
		}
	}
	m_texMultiplier = TEX1X;
	if (minDistSqr<mip4xDistanceSqr) {
		m_texMultiplier = TEX4X;
	} else if (minDistSqr<mip2xDistanceSqr) {
		m_texMultiplier = TEX2X;
	} else {
		REF_PTR_RELEASE(m_terrainTexture4X);
		REF_PTR_RELEASE(m_terrainTexture2X);
		Int LOD = 0;
		if (minDistSqr>mipLODDistanceSqr) {
			LOD = 1;
		}
		m_terrainTexture->setLOD(LOD);
	}

}

//=============================================================================
// W3DTerrainBackground::updateCenter
//=============================================================================
/** Updates the culling status. */
//=============================================================================
void W3DTerrainBackground::updateTexture()
{
	if (m_cullStatus==CULL_STATUS_INVISIBLE) {
		REF_PTR_RELEASE(m_terrainTexture2X);
		REF_PTR_RELEASE(m_terrainTexture4X);
		return;
	}

	if (m_texMultiplier == TEX4X) {
		REF_PTR_RELEASE(m_terrainTexture2X);
		if (m_terrainTexture4X == nullptr) {
			m_terrainTexture4X = m_map->getFlatTexture(m_xOrigin, m_yOrigin, m_width, 4*PIXELS_PER_GRID);
			m_terrainTexture4X->Get_Filter().Set_U_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_CLAMP);
			m_terrainTexture4X->Get_Filter().Set_V_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_CLAMP);
		}
	} else if (m_texMultiplier == TEX2X) {
		REF_PTR_RELEASE(m_terrainTexture4X);
		if (m_terrainTexture2X == nullptr) {
			m_terrainTexture2X = m_map->getFlatTexture(m_xOrigin, m_yOrigin, m_width, 2*PIXELS_PER_GRID);
			m_terrainTexture2X->Get_Filter().Set_U_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_CLAMP);
			m_terrainTexture2X->Get_Filter().Set_V_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_CLAMP);
		}
	} else {
		REF_PTR_RELEASE(m_terrainTexture4X);
		REF_PTR_RELEASE(m_terrainTexture2X);
	}

}

//=============================================================================
// W3DTerrainBackground::renderTerrain
//=============================================================================
//=============================================================================
void W3DTerrainBackground::drawVisiblePolys(RenderInfoClass & rinfo, Bool disableTextures)
{
	if (m_curNumTerrainIndices == 0) {
		return;
	}
	if (m_cullStatus==CULL_STATUS_INVISIBLE) {
		return;
	}
	if (m_terrainVertices.empty() || m_terrainIndices.empty()) {
		return;
	}
	// Cloud/noise-only passes (pass>0 in FlatHeightMap) call with disableTextures=TRUE.
	// Those second stages have no backend PSO; skip to avoid re-drawing base geometry.
	if (disableTextures) {
		return;
	}
	// Preserve 4X/2X/1X selection, submitted as a single-texture material draw.
	TerrainTextureClass *selected = m_terrainTexture;
	if (m_terrainTexture4X) {
		selected = m_terrainTexture4X;
	} else if (m_terrainTexture2X) {
		selected = m_terrainTexture2X;
	}
	IRenderBackend *backend = WW3D::Get_Render_Backend();
	if (backend == nullptr) {
		return;
	}
	RenderBackendMaterialState state;
	if (!detailShader.Get_Render_Backend_State(state)) {
		return;
	}
	// depth_test LessEqual + depth_write true come from detailShader (SC_DETAIL).
	RenderBackendTextureHandle handle;
	if (selected != nullptr) {
		RenderBackendMaterialState batch = state;
		if (!selected->Get_Filter().Get_Render_Sampler(batch.sampler)) {
			return;
		}
		batch.clamp_texture = false;
		if (!selected->Ensure_Renderer_Texture()) {
			return;
		}
		handle = selected->Get_Renderer_Texture();
		state = batch;
	}
	// Convert CPU verts (diffuse ARGB + base UV) to backend verts; second UV (u2/v2)
	// is preserved in gen but not submitted (single-texture PSO).
	std::vector<RenderBackendTexturedVertex> vertices;
	vertices.reserve(m_terrainVertices.size());
	for (size_t i = 0; i < m_terrainVertices.size(); ++i) {
		const VertexFormatXYZDUV2 &src = m_terrainVertices[i];
		RenderBackendTexturedVertex dst;
		dst.x = src.x; dst.y = src.y; dst.z = src.z;
		dst.r = ((src.diffuse >> 16) & 255) / 255.0f;
		dst.g = ((src.diffuse >> 8) & 255) / 255.0f;
		dst.b = (src.diffuse & 255) / 255.0f;
		dst.a = ((src.diffuse >> 24) & 255) / 255.0f;
		dst.u = src.u1; dst.v = src.v1; dst.q = 1.0f;
		vertices.push_back(dst);
	}
	// 65535 split (meshrenderer flush pattern): remap indices per chunk.
	const unsigned short *srcIndices = m_terrainIndices.data();
	const size_t indexCount = m_terrainIndices.size();
	size_t cursor = 0;
	std::vector<unsigned short> chunkIndices;
	chunkIndices.reserve(60000);
	std::vector<RenderBackendTexturedVertex> chunkVerts;
	chunkVerts.reserve(65535);
	std::vector<int> remap;
	while (cursor < indexCount) {
		chunkVerts.clear();
		chunkIndices.clear();
		remap.assign(vertices.size(), -1);
		size_t chunkStart = cursor;
		// Grow chunk until vertex or index budget would overflow.
		while (cursor < indexCount) {
			// Peek next triangle (indices are triangle list).
			size_t triEnd = cursor + 3;
			if (triEnd > indexCount) {
				triEnd = indexCount;
			}
			size_t needed = 0;
			for (size_t k = cursor; k < triEnd; ++k) {
				if (srcIndices[k] < vertices.size() && remap[srcIndices[k]] < 0) {
					needed++;
				}
			}
			if (chunkVerts.size() + needed > 65535 || chunkIndices.size() + (triEnd - cursor) > 60000) {
				if (chunkIndices.empty()) {
					// Single triangle exceeds budget (should not happen); force it.
				} else {
					break;
				}
			}
			for (size_t k = cursor; k < triEnd; ++k) {
				unsigned short src = srcIndices[k];
				if (src >= vertices.size()) {
					break;
				}
				if (remap[src] < 0) {
					remap[src] = (int)chunkVerts.size();
					chunkVerts.push_back(vertices[src]);
				}
				chunkIndices.push_back((unsigned short)remap[src]);
			}
			cursor = triEnd;
			if (triEnd - chunkStart >= 3 && (triEnd % 3) == 0) {
				// Continue accumulating; loop condition handles budget.
			}
			if (cursor < indexCount && (cursor % 3) != 0) {
				continue;
			}
		}
		if (chunkIndices.empty()) {
			break;
		}
		if (!backend->Draw_Indexed_Material_Triangles(chunkVerts.data(), (unsigned int)chunkVerts.size(),
			chunkIndices.data(), (unsigned int)chunkIndices.size(), handle, state)) {
			break;
		}
	}
}





