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

// FILE: W3DBibBuffer.cpp ////////////////////////////////////////////////
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
// File name: W3DBibBuffer.cpp
//
// Created:   John Ahlquist, May 2001
//
// Desc:      Draw buffer to handle all the bibs in a scene.
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//         Includes
//-----------------------------------------------------------------------------

#include "W3DDevice/GameClient/W3DBibBuffer.h"

#include <WW3D2/texture.h>
#include "Common/GlobalData.h"
#include "WW3D2/camera.h"
#include "WW3D2/shader.h"
#include "WW3D2/ww3d.h"
#include "WW3D2/IRenderBackend.h"
#include "WWMath/matrix4.h"
#include <vector>
#include <cstdio>

//-----------------------------------------------------------------------------
//         Private Data
//-----------------------------------------------------------------------------
// A W3D shader that does alpha, texturing, tests zbuffer, doesn't update zbuffer.
#define SC_ALPHA_DETAIL ( SHADE_CNST(ShaderClass::PASS_ALWAYS, ShaderClass::DEPTH_WRITE_DISABLE, ShaderClass::COLOR_WRITE_ENABLE, ShaderClass::SRCBLEND_SRC_ALPHA, \
	ShaderClass::DSTBLEND_ONE_MINUS_SRC_ALPHA, ShaderClass::FOG_DISABLE, ShaderClass::GRADIENT_MODULATE, ShaderClass::SECONDARY_GRADIENT_DISABLE, ShaderClass::TEXTURING_ENABLE, \
	ShaderClass::ALPHATEST_DISABLE, ShaderClass::CULL_MODE_DISABLE, \
	ShaderClass::DETAILCOLOR_DISABLE, ShaderClass::DETAILALPHA_DISABLE) )

static ShaderClass detailAlphaShader(SC_ALPHA_DETAIL);

namespace {
UnsignedInt getBibLighting()
{
    Real r=TheGlobalData->m_terrainAmbient[0].red;
    Real g=TheGlobalData->m_terrainAmbient[0].green;
    Real b=TheGlobalData->m_terrainAmbient[0].blue;
    r+=TheGlobalData->m_terrainDiffuse[0].red;
    g+=TheGlobalData->m_terrainDiffuse[0].green;
    b+=TheGlobalData->m_terrainDiffuse[0].blue;
    if (r>1.0f) r=1.0f;
    if (g>1.0f) g=1.0f;
    if (b>1.0f) b=1.0f;
    r*=255.0f; g*=255.0f; b*=255.0f;
    return static_cast<UnsignedInt>(REAL_TO_INT(b)) |
        (static_cast<UnsignedInt>(REAL_TO_INT(g)) << 8) |
        (static_cast<UnsignedInt>(REAL_TO_INT(r)) << 16) | (UnsignedInt(255) << 24);
}
}

struct W3DBibBuffer::GeometryState
{
    std::vector<RenderBackendTexturedVertex> vertices[2];
    std::vector<UnsignedShort> indices[2];
    RenderBackendGeometryHandle handles[2];
    UnsignedInt lighting=0;
    bool hasLighting=false;
};




//-----------------------------------------------------------------------------
//         Private Functions
//-----------------------------------------------------------------------------


//=============================================================================
// W3DBibBuffer::loadBibsInVertexAndIndexBuffers
//=============================================================================
/** Loads the bibs into the vertex buffer for drawing. */
//=============================================================================
void W3DBibBuffer::loadBibsInVertexAndIndexBuffers()
{
    if (!m_initialized || !m_anythingChanged) return;
    m_curNumBibVertices = m_curNumBibIndices = 0;
    m_curNumNormalBibIndices = m_curNumNormalBibVertex = 0;
    for (unsigned batch=0; batch<2; ++batch) {
        m_geometry->vertices[batch].clear();
        m_geometry->indices[batch].clear();
    }

    // Preserve the original packed ambient-plus-diffuse terrain light.
    const UnsignedInt diffuse=getBibLighting();
    m_geometry->lighting = diffuse;
    m_geometry->hasLighting = true;
    const Real red = ((diffuse >> 16) & 255u) / 255.0f;
    const Real green = ((diffuse >> 8) & 255u) / 255.0f;
    const Real blue = (diffuse & 255u) / 255.0f;
    const Real uv[4][2]={{0,1},{1,1},{1,0},{0,0}};
    for (unsigned batch=0; batch<2; ++batch) {
        if (batch==1) {
            m_curNumNormalBibIndices=m_curNumBibIndices;
            m_curNumNormalBibVertex=m_curNumBibVertices;
        }
        for (Int bib=0; bib<m_numBibs; ++bib) {
            if (m_bibs[bib].m_unused || m_bibs[bib].m_highlight != static_cast<Bool>(batch)) continue;
            // Retain the legacy aggregate capacity and conservative boundary checks.
            if (m_curNumBibVertices+4+2 >= m_vertexBibSize ||
                m_curNumBibIndices+6+6 >= m_indexBibSize) break;
            const UnsignedShort start=static_cast<UnsignedShort>(m_geometry->vertices[batch].size());
            for (unsigned corner=0; corner<4; ++corner) {
                const Vector3 &p=m_bibs[bib].m_corners[corner];
                m_geometry->vertices[batch].push_back({p.X,p.Y,p.Z,red,green,blue,1.0f,uv[corner][0],uv[corner][1]});
            }
            const UnsignedShort order[6]={0,1,2,0,2,3};
            for (unsigned i=0; i<6; ++i)
                m_geometry->indices[batch].push_back(static_cast<UnsignedShort>(start+order[i]));
            m_curNumBibVertices+=4;
            m_curNumBibIndices+=6;
        }
    }
}

//-----------------------------------------------------------------------------
//         Public Functions
//-----------------------------------------------------------------------------

//=============================================================================
// W3DBibBuffer::~W3DBibBuffer
//=============================================================================
/** Destructor. Releases w3d assets. */
//=============================================================================
W3DBibBuffer::~W3DBibBuffer()
{
	freeBibBuffers();
	delete m_geometry;
	REF_PTR_RELEASE(m_bibTexture);
	REF_PTR_RELEASE(m_highlightBibTexture);
}

//=============================================================================
// W3DBibBuffer::W3DBibBuffer
//=============================================================================
/** Constructor. Sets m_initialized to true if it finds the w3d models it needs
for the bibs. */
//=============================================================================
W3DBibBuffer::W3DBibBuffer()
{
	m_initialized = false;
	m_geometry = new GeometryState;
	m_bibTexture = nullptr;
	m_curNumBibVertices=0;
	m_curNumBibIndices=0;
	clearAllBibs();
	m_indexBibSize = INITIAL_BIB_INDEX;
	m_vertexBibSize = INITIAL_BIB_VERTEX;
	allocateBibBuffers();

	m_bibTexture = NEW_REF(TextureClass, ("TBBib.tga"));
	m_highlightBibTexture = NEW_REF(TextureClass, ("TBRedBib.tga"));
	m_bibTexture->Get_Filter().Set_U_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_CLAMP);
	m_bibTexture->Get_Filter().Set_V_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_CLAMP);
	m_highlightBibTexture->Get_Filter().Set_U_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_CLAMP);
	m_highlightBibTexture->Get_Filter().Set_V_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_CLAMP);
	m_initialized = true;
}


//=============================================================================
// W3DBibBuffer::freeBibBuffers
//=============================================================================
/** Frees the index and vertex buffers. */
//=============================================================================
void W3DBibBuffer::freeBibBuffers()
{
    IRenderBackend *backend=WW3D::Get_Render_Backend();
    for (unsigned batch=0; batch<2; ++batch) {
        if (backend) backend->Release_Static_Geometry(m_geometry->handles[batch]);
        m_geometry->handles[batch]={};
        std::vector<RenderBackendTexturedVertex>().swap(m_geometry->vertices[batch]);
        std::vector<UnsignedShort>().swap(m_geometry->indices[batch]);
    }
    m_curNumBibVertices=m_curNumBibIndices=0;
    m_curNumNormalBibIndices=m_curNumNormalBibVertex=0;
    m_anythingChanged=true;
}

//=============================================================================
// W3DBibBuffer::allocateBibBuffers
//=============================================================================
/** Allocates the index and vertex buffers. */
//=============================================================================
void W3DBibBuffer::allocateBibBuffers()
{
    freeBibBuffers();
    for (unsigned batch=0; batch<2; ++batch) {
        m_geometry->vertices[batch].reserve(m_vertexBibSize+4);
        m_geometry->indices[batch].reserve(m_indexBibSize+4);
    }
}

//=============================================================================
// W3DBibBuffer::clearAllBibs
//=============================================================================
/** Removes all bibs. */
//=============================================================================
void W3DBibBuffer::clearAllBibs()
{
	m_numBibs=0;
	m_anythingChanged = true;
/* test bib
	Vector3 corners[4];
	corners[0].Set(0, 0, 20);
	corners[1].Set(100, 0, 20);
	corners[2].Set(100,100,20);
	corners[3].Set(0,100,20);
	addBib(corners, 1, false);
*/
}

//=============================================================================
// W3DBibBuffer::removeHighlighting
//=============================================================================
/** Clears highlighting flag.   */
//=============================================================================
void W3DBibBuffer::removeHighlighting()
{
	Int bibIndex;
	for (bibIndex=0; bibIndex<m_numBibs; bibIndex++) {
		if (m_bibs[bibIndex].m_highlight) m_anythingChanged=true;
		m_bibs[bibIndex].m_highlight = false;
	}
}

//=============================================================================
// W3DBibBuffer::addBib
//=============================================================================
/** Adds a bib.   */
//=============================================================================
void W3DBibBuffer::addBib(Vector3 corners[4], ObjectID id, Bool highlight)
{
	Int bibIndex;
	for (bibIndex=0; bibIndex<m_numBibs; bibIndex++) {
		if (!m_bibs[bibIndex].m_unused && m_bibs[bibIndex].m_objectID == id) {
			break;
		}
	}
	if (bibIndex==m_numBibs) {
		for (bibIndex=0; bibIndex<m_numBibs; bibIndex++) {
			if (m_bibs[bibIndex].m_unused) {
				break;
			}
		}
	}
	if (bibIndex==m_numBibs) {
		if (m_numBibs >= MAX_BIBS) {
			return;
		}
		m_numBibs++;
	}
	m_anythingChanged = true;
	m_bibs[bibIndex].m_corners[0] = corners[0];
	m_bibs[bibIndex].m_corners[1] = corners[1];
	m_bibs[bibIndex].m_corners[2] = corners[2];
	m_bibs[bibIndex].m_corners[3] = corners[3];
	m_bibs[bibIndex].m_highlight = highlight;
	m_bibs[bibIndex].m_color = 0; // for now.
	m_bibs[bibIndex].m_unused = false; // for now.
	m_bibs[bibIndex].m_objectID = id;
	m_bibs[bibIndex].m_drawableID = INVALID_DRAWABLE_ID;
}

//=============================================================================
// W3DBibBuffer::addBib
//=============================================================================
/** Adds a bib.   */
//=============================================================================
void W3DBibBuffer::addBibDrawable(Vector3 corners[4], DrawableID id, Bool highlight)
{
	Int bibIndex;
	for (bibIndex=0; bibIndex<m_numBibs; bibIndex++) {
		if (!m_bibs[bibIndex].m_unused && m_bibs[bibIndex].m_drawableID == id) {
			break;
		}
	}
	if (bibIndex==m_numBibs) {
		for (bibIndex=0; bibIndex<m_numBibs; bibIndex++) {
			if (m_bibs[bibIndex].m_unused) {
				break;
			}
		}
	}
	if (bibIndex==m_numBibs) {
		if (m_numBibs >= MAX_BIBS) {
			return;
		}
		m_numBibs++;
	}
	m_anythingChanged = true;
	m_bibs[bibIndex].m_corners[0] = corners[0];
	m_bibs[bibIndex].m_corners[1] = corners[1];
	m_bibs[bibIndex].m_corners[2] = corners[2];
	m_bibs[bibIndex].m_corners[3] = corners[3];
	m_bibs[bibIndex].m_highlight = highlight;
	m_bibs[bibIndex].m_color = 0; // for now.
	m_bibs[bibIndex].m_unused = false; // for now.
	m_bibs[bibIndex].m_objectID = INVALID_ID;
	m_bibs[bibIndex].m_drawableID = id;
}

//=============================================================================
// W3DBibBuffer::removeBib
//=============================================================================
/** Removes a bib.  */
//=============================================================================
void W3DBibBuffer::removeBib(ObjectID id)
{
	Int bibIndex;
	for (bibIndex=0; bibIndex<m_numBibs; bibIndex++) {
		if (m_bibs[bibIndex].m_objectID == id) {
			m_bibs[bibIndex].m_unused = true;
			m_bibs[bibIndex].m_objectID = INVALID_ID;
			m_bibs[bibIndex].m_drawableID = INVALID_DRAWABLE_ID;
			m_anythingChanged = true;
		}
	}
}

//=============================================================================
// W3DBibBuffer::removeBib
//=============================================================================
/** Removes a bib.  */
//=============================================================================
void W3DBibBuffer::removeBibDrawable(DrawableID id)
{
	Int bibIndex;
	for (bibIndex=0; bibIndex<m_numBibs; bibIndex++) {
		if (m_bibs[bibIndex].m_drawableID == id) {
			m_bibs[bibIndex].m_unused = true;
			m_bibs[bibIndex].m_objectID = INVALID_ID;
			m_bibs[bibIndex].m_drawableID = INVALID_DRAWABLE_ID;
			m_anythingChanged = true;
		}
	}
}


//=============================================================================
// W3DBibBuffer::drawBibs
//=============================================================================
/** Draws the bibs.  Uses camera to cull. */
//=============================================================================
void W3DBibBuffer::renderBibs(CameraClass *camera, const Matrix3D &worldTransform)
{
    IRenderBackend *backend=WW3D::Get_Render_Backend();
    if (!camera || !TheGlobalData || !m_initialized || !backend || !backend->Is_Device_Ready()) return;
    const UnsignedInt lighting=getBibLighting();
    if (!m_geometry->hasLighting || lighting!=m_geometry->lighting) m_anythingChanged=true;
    for (unsigned batch=0; batch<2; ++batch)
        if (!m_geometry->indices[batch].empty() && !backend->Is_Geometry_Valid(m_geometry->handles[batch]))
            m_anythingChanged=true;
    if (m_anythingChanged) {
        loadBibsInVertexAndIndexBuffers();
        RenderBackendGeometryHandle replacement[2];
        bool uploaded=true;
        for (unsigned batch=0; batch<2; ++batch) {
            if (m_geometry->indices[batch].empty()) continue;
            replacement[batch]=backend->Create_Static_Indexed_Textured_Geometry(
                m_geometry->vertices[batch].data(),static_cast<unsigned>(m_geometry->vertices[batch].size()),
                m_geometry->indices[batch].data(),static_cast<unsigned>(m_geometry->indices[batch].size()));
            if (!replacement[batch].Is_Valid()) { uploaded=false; break; }
        }
        if (!uploaded) {
            for (unsigned batch=0; batch<2; ++batch) backend->Release_Static_Geometry(replacement[batch]);
            std::fprintf(stderr,"Bib geometry upload failed.\n");
            return;
        }
        for (unsigned batch=0; batch<2; ++batch) {
            backend->Release_Static_Geometry(m_geometry->handles[batch]);
            m_geometry->handles[batch]=replacement[batch];
        }
        m_anythingChanged=false;
    }
    if (m_curNumBibIndices==0) return;
    RenderBackendMaterialState material;
    if (!detailAlphaShader.Get_Render_Backend_State(material)) {
        std::fprintf(stderr,"Bib material translation failed.\n");
        return;
    }
    material.color_write_mask=7; // Destination alpha belongs to the water shoreline pass.
    Matrix4x4 savedProjection;
    backend->Get_View_Projection(savedProjection);
    struct RestoreProjection {
        IRenderBackend *backend;
        const Matrix4x4 &projection;
        ~RestoreProjection() { backend->Set_View_Projection(projection); }
    } restore={backend,savedProjection};
    camera->Apply();
    Matrix4x4 cameraProjection;
    backend->Get_View_Projection(cameraProjection);
    backend->Set_View_Projection(cameraProjection*Matrix4x4(worldTransform));
    TextureClass *textures[2]={m_bibTexture,m_highlightBibTexture};
    for (unsigned batch=0; batch<2; ++batch) {
        if (m_geometry->indices[batch].empty()) continue;
        if (!textures[batch] || !textures[batch]->Ensure_Renderer_Texture() ||
            !textures[batch]->Get_Filter().Get_Render_Sampler(material.sampler) ||
            !backend->Draw_Static_Indexed_Material_Geometry(m_geometry->handles[batch],
                textures[batch]->Get_Renderer_Texture(),material)) {
            std::fprintf(stderr,"Bib textured material draw failed.\n");
            return;
        }
    }
}

