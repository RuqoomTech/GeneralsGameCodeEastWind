/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2026 TheSuperHackers
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

#include "W3DDevice/GameClient/W3DScorch.h"

#include "Common/GameMemory.h"
#include "Common/GameType.h"
#include "Common/GlobalData.h"
#include "Common/MapObject.h"
#include "W3DDevice/GameClient/TerrainTex.h"
#include "W3DDevice/GameClient/WorldHeightMap.h"
#include "WW3D2/IRenderBackend.h"
#include "WW3D2/camera.h"
#include "WW3D2/shader.h"
#include "WW3D2/ww3d.h"
#include "WWMath/matrix4.h"
#include <cstdio>
#include <vector>

struct W3DScorch::GeometryState
{
	std::vector<RenderBackendTexturedVertex> vertices;
	std::vector<UnsignedShort> indices;
	IRenderBackend* backend = nullptr;
	RenderBackendGeometryHandle handle;
};

W3DScorch::W3DScorch(bool deduplicateScorches)
  : m_geometry(new GeometryState)
  , m_scorchTexture(nullptr)
  , m_curNumScorchVertices(0)
  , m_curNumScorchIndices(0)
  , m_needBufferRecompute(true)
  , m_deduplicateScorches(deduplicateScorches)
{}

W3DScorch::~W3DScorch()
{
	freeBuffers();
	delete m_geometry;
}

void W3DScorch::allocateBuffers()
{
	freeBuffers();
	m_geometry->vertices.reserve(MAX_SCORCH_VERTEX);
	m_geometry->indices.reserve(MAX_SCORCH_INDEX);
	m_scorchTexture = NEW ScorchTextureClass;
	invalidateBuffers();
}

void W3DScorch::freeBuffers()
{
	releaseGeometry();
	std::vector<RenderBackendTexturedVertex>().swap(m_geometry->vertices);
	std::vector<UnsignedShort>().swap(m_geometry->indices);
	m_curNumScorchVertices = m_curNumScorchIndices = 0;
	m_needBufferRecompute = true;
	REF_PTR_RELEASE(m_scorchTexture);
}

void W3DScorch::clearAllScorches()
{
	m_scorches.clear();
	invalidateBuffers();
}

void W3DScorch::invalidateBuffers()
{
	m_needBufferRecompute = true;
	m_curNumScorchVertices = 0;
	m_curNumScorchIndices = 0;
}

void W3DScorch::invalidateTexture()
{
	if (m_scorchTexture)
	{
		m_scorchTexture->Invalidate();
	}
}

void W3DScorch::addScorch(Vector3 location, Real radius, Scorches type)
{
	TScorch scorch;
	scorch.location = location;
	scorch.radius = radius;
	if (type >= 0 && (Int)type < SCORCH_MARKS_IN_TEXTURE)
		scorch.scorchType = type;
	else
		scorch.scorchType = SCORCH_1;

	if (m_deduplicateScorches && isDuplicate(scorch))
	{
		return;
	}

	if ((Int)m_scorches.size() >= MAX_SCORCH_MARKS)
	{
		m_scorches.pop_front();
	}
	m_scorches.push_back(scorch);

	invalidateBuffers();
}

Bool W3DScorch::isDuplicate(const TScorch& scorch) const
{
	const Real limit = scorch.radius / 4;
	for (std::deque<TScorch>::const_iterator it = m_scorches.begin(); it != m_scorches.end(); ++it)
	{
		if (it->scorchType == scorch.scorchType &&
		    fabsf(scorch.location.X - it->location.X) < limit &&
		    fabsf(scorch.location.Y - it->location.Y) < limit &&
		    fabsf(scorch.radius - it->radius) < limit)
		{
			return true;
		}
	}
	return false;
}

void W3DScorch::releaseGeometry()
{
	// A previous device's pointer may already be dead. Global generations also
	// prevent a recreated backend at the same address from consuming its handle.
	IRenderBackend* backend = WW3D::Get_Render_Backend();
	if (backend && backend == m_geometry->backend)
		backend->Release_Static_Geometry(m_geometry->handle);
	m_geometry->handle = {};
	m_geometry->backend = nullptr;
}

void W3DScorch::drawScorches(WorldHeightMap& map, CameraClass& camera, const Matrix3D& worldTransform)
{
	IRenderBackend* backend = WW3D::Get_Render_Backend();
	if (!backend || !m_scorchTexture) return;
	updateScorches(map);
	if (m_curNumScorchIndices == 0) return;
	if (m_geometry->backend != backend || !backend->Is_Geometry_Valid(m_geometry->handle)) {
		m_geometry->handle = {};
		m_geometry->backend = backend;
		m_geometry->handle = backend->Create_Static_Indexed_Textured_Geometry(
			m_geometry->vertices.data(), static_cast<unsigned>(m_curNumScorchVertices),
			m_geometry->indices.data(), static_cast<unsigned>(m_curNumScorchIndices));
		if (!m_geometry->handle.Is_Valid()) {
			std::fputs("Scorch geometry upload failed.\n", stderr);
			return;
		}
	}
	RenderBackendMaterialState material;
	if (!ShaderClass::_PresetAlphaShader.Get_Render_Backend_State(material) ||
		!m_scorchTexture->Ensure_Renderer_Texture() ||
		!m_scorchTexture->Get_Filter().Get_Render_Sampler(material.sampler)) {
		std::fputs("Scorch texture or material translation failed.\n", stderr);
		return;
	}
	material.color_write_mask = 7; // Terrain destination alpha belongs to shoreline rendering.
	const bool linear = TheGlobalData &&
		(TheGlobalData->m_bilinearTerrainTex || TheGlobalData->m_trilinearTerrainTex);
	material.sampler.min_filter = material.sampler.mag_filter =
		linear ? RenderBackendTextureFilter::Linear : RenderBackendTextureFilter::Point;
	material.sampler.mip_filter = TheGlobalData && TheGlobalData->m_trilinearTerrainTex ?
		RenderBackendTextureFilter::Linear : RenderBackendTextureFilter::Point;
	material.sampler.max_anisotropy = 1;
	material.sampler.mipmaps = true;
	material.sampler.address_u = material.sampler.address_v = RenderBackendTextureAddress::Clamp;
	Matrix4x4 savedProjection;
	backend->Get_View_Projection(savedProjection);
	camera.Apply();
	Matrix4x4 cameraProjection;
	backend->Get_View_Projection(cameraProjection);
	backend->Set_View_Projection(cameraProjection * Matrix4x4(worldTransform));
	const bool drawn = backend->Draw_Static_Indexed_Material_Geometry(
		m_geometry->handle, m_scorchTexture->Get_Renderer_Texture(), material);
	backend->Set_View_Projection(savedProjection);
	if (!drawn) std::fputs("Scorch textured material draw failed.\n", stderr);
}

static Real getMapHeight(WorldHeightMap& map, Int x, Int y)
{
	x += map.getBorderSizeInline();
	y += map.getBorderSizeInline();
	return map.getDataPtr()[x + y * map.getXExtent()] * MAP_HEIGHT_SCALE;
}

void W3DScorch::updateScorches(WorldHeightMap& map)
{
	if (!m_needBufferRecompute || !m_scorchTexture)
	{
		return;
	}

	releaseGeometry();
	m_needBufferRecompute = false;
	m_curNumScorchVertices = 0;
	m_curNumScorchIndices = 0;
	if (m_scorches.empty()) {
		m_geometry->vertices.clear();
		m_geometry->indices.clear();
		return;
	}

	m_geometry->vertices.resize(MAX_SCORCH_VERTEX);
	m_geometry->indices.resize(MAX_SCORCH_INDEX);
	RenderBackendTexturedVertex* vb = m_geometry->vertices.data();
	UnsignedShort* ib = m_geometry->indices.data();

	Real shadeR = (TheGlobalData->m_terrainAmbient[0].red + TheGlobalData->m_terrainDiffuse[0].red) / 2.0f;
	Real shadeG = (TheGlobalData->m_terrainAmbient[0].green + TheGlobalData->m_terrainDiffuse[0].green) / 2.0f;
	Real shadeB = (TheGlobalData->m_terrainAmbient[0].blue + TheGlobalData->m_terrainDiffuse[0].blue) / 2.0f;
	Vector3 shade(shadeR, shadeG, shadeB);
	for (unsigned component = 0; component < 3; ++component) {
		const Real nonnegative = shade[component] < 0.0f ? 0.0f : shade[component];
		shade[component] = nonnegative > 1.0f ? 1.0f : nonnegative;
	}
	const UnsignedInt diffuse = static_cast<UnsignedInt>(shade.Convert_To_ARGB(1.0f));

	// TheSuperHackers @info Scorches are written in reverse order to ensure that the last added scorches fit in the buffers.
	for (std::deque<TScorch>::reverse_iterator it = m_scorches.rbegin(); it != m_scorches.rend(); ++it)
	{
		if (writeScorchToBuffer(*it, map, diffuse,
		                        vb + m_curNumScorchVertices, ib + m_curNumScorchIndices) == SCORCH_BUFFER_FULL)
		{
			break;
		}
	}
	m_geometry->vertices.resize(m_curNumScorchVertices);
	m_geometry->indices.resize(m_curNumScorchIndices);
}

W3DScorch::WriteScorchResult W3DScorch::writeScorchToBuffer(const TScorch& scorch, WorldHeightMap& map,
                                                          UnsignedInt diffuse, RenderBackendTexturedVertex* curVb,
                                                          UnsignedShort* curIb)
{
	Real radius = scorch.radius;
	Vector3 loc = scorch.location;
	Int type = scorch.scorchType;
	Real amtToFloat = MAP_HEIGHT_SCALE / 10;

	Int minX = REAL_TO_INT_FLOOR((loc.X - radius) / MAP_XY_FACTOR);
	Int minY = REAL_TO_INT_FLOOR((loc.Y - radius) / MAP_XY_FACTOR);
	if (minX < -map.getBorderSizeInline())
		minX = -map.getBorderSizeInline();
	if (minY < -map.getBorderSizeInline())
		minY = -map.getBorderSizeInline();
	Int maxX = REAL_TO_INT_CEIL((loc.X + radius) / MAP_XY_FACTOR);
	Int maxY = REAL_TO_INT_CEIL((loc.Y + radius) / MAP_XY_FACTOR);
	maxX++;
	maxY++;
	if (maxX > map.getXExtent() - map.getBorderSizeInline())
	{
		maxX = map.getXExtent() - map.getBorderSizeInline();
	}
	if (maxY > map.getYExtent() - map.getBorderSizeInline())
	{
		maxY = map.getYExtent() - map.getBorderSizeInline();
	}

	const Int vertexCountX = maxX - minX;
	const Int vertexCountY = maxY - minY;
	if (vertexCountX <= 0 || vertexCountY <= 0)
	{
		return SCORCH_SKIPPED;
	}

	const Int requiredVertices = vertexCountX * vertexCountY;
	const Int requiredIndices = 6 * (vertexCountX - 1) * (vertexCountY - 1);
	if (m_curNumScorchVertices + requiredVertices > MAX_SCORCH_VERTEX ||
	    m_curNumScorchIndices + requiredIndices > MAX_SCORCH_INDEX)
	{
		return SCORCH_BUFFER_FULL;
	}

	Int startVertex = m_curNumScorchVertices;
	Int i, j;
	for (j = minY; j < maxY; j++)
	{
		for (i = minX; i < maxX; i++)
		{
			curVb->r = ((diffuse >> 16) & 255u) / 255.0f;
			curVb->g = ((diffuse >> 8) & 255u) / 255.0f;
			curVb->b = (diffuse & 255u) / 255.0f;
			curVb->a = 1.0f;
			Real theZ = amtToFloat + getMapHeight(map, i, j);
			// The scorchmarks are spaced out by 1.5 in the texture.
			Real uOffset = (type % SCORCH_PER_ROW) * 1.5f;
			Real vOffset = (type / SCORCH_PER_ROW) * 1.5f;
			Real X = i * MAP_XY_FACTOR;
			Real Y = j * MAP_XY_FACTOR;
			curVb->u = (uOffset + 0.5f + (X - loc.X) / (2 * radius)) / (SCORCH_PER_ROW + 1);
			curVb->v = (vOffset + 0.5f + (Y - loc.Y) / (2 * radius)) / (SCORCH_PER_ROW + 1);
			curVb->x = X;
			curVb->y = Y;
			curVb->z = theZ;
			curVb++;
			m_curNumScorchVertices++;
		}
	}
	Int yOffset = maxX - minX;
	for (j = 0; j < maxY - minY - 1; j++)
	{
		for (i = 0; i < maxX - minX - 1; i++)
		{
			Int xNdx = i + minX + map.getBorderSizeInline();
			Int yNdx = j + minY + map.getBorderSizeInline();
			Bool flipForBlend = map.getFlipState(xNdx, yNdx);
#if 0
			UnsignedByte alpha[4];
			float UA[4], VA[4];
			map.getAlphaUVData(xNdx, yNdx, UA, VA, alpha, &flipForBlend);
#endif
			if (flipForBlend)
			{
				*curIb++ = startVertex + j * yOffset + i + 1;
				*curIb++ = startVertex + j * yOffset + i + yOffset;
				*curIb++ = startVertex + j * yOffset + i;
				*curIb++ = startVertex + j * yOffset + i + 1;
				*curIb++ = startVertex + j * yOffset + i + 1 + yOffset;
				*curIb++ = startVertex + j * yOffset + i + yOffset;
			}
			else
			{
				*curIb++ = startVertex + j * yOffset + i;
				*curIb++ = startVertex + j * yOffset + i + 1 + yOffset;
				*curIb++ = startVertex + j * yOffset + i + yOffset;
				*curIb++ = startVertex + j * yOffset + i;
				*curIb++ = startVertex + j * yOffset + i + 1;
				*curIb++ = startVertex + j * yOffset + i + 1 + yOffset;
			}
			m_curNumScorchIndices += 6;
		}
	}

	return SCORCH_WRITTEN;
}
