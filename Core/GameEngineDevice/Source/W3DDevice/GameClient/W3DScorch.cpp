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
#include "WW3D2/shader.h"
#include "WW3D2/texture.h"
#include "WW3D2/ww3d.h"

W3DScorch::W3DScorch(bool deduplicateScorches)
  : m_scorchTexture(nullptr)
  , m_curNumScorchVertices(0)
  , m_curNumScorchIndices(0)
  , m_needBufferRecompute(true)
  , m_deduplicateScorches(deduplicateScorches)
{}

W3DScorch::~W3DScorch() { freeBuffers(); }

void W3DScorch::allocateBuffers()
{
	freeBuffers();
	m_scorchVertices.reserve(MAX_SCORCH_VERTEX);
	m_scorchIndices.reserve(MAX_SCORCH_INDEX);
	m_scorchTexture = NEW ScorchTextureClass;
	invalidateBuffers();
}

void W3DScorch::freeBuffers()
{
	m_scorchVertices.clear();
	m_scorchIndices.clear();
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

void W3DScorch::drawScorches(WorldHeightMap& map)
{
	updateScorches(map);
	if (m_curNumScorchIndices == 0 || m_scorchTexture == nullptr)
	{
		return;
	}
	IRenderBackend* backend = WW3D::Get_Render_Backend();
	if (backend == nullptr)
	{
		return;
	}
	// Translate the legacy PresetAlpha shader instead of pushing DX8 render states.
	RenderBackendMaterialState materialState;
	if (!ShaderClass::_PresetAlphaShader.Get_Render_Backend_State(materialState))
	{
		return;
	}
	RenderBackendTextureHandle textureHandle;
	if (ShaderClass::_PresetAlphaShader.Get_Texturing() == ShaderClass::TEXTURING_ENABLE)
	{
		if (!m_scorchTexture->Get_Filter().Get_Render_Sampler(materialState.sampler))
		{
			return;
		}
		materialState.clamp_texture = false;
		if (!m_scorchTexture->Ensure_Renderer_Texture())
		{
			return;
		}
		textureHandle = m_scorchTexture->Get_Renderer_Texture();
		if (!textureHandle.Is_Valid())
		{
			return;
		}
	}
	backend->Draw_Indexed_Material_Triangles(m_scorchVertices.data(), (UnsignedInt)m_curNumScorchVertices,
		m_scorchIndices.data(), (UnsignedInt)m_curNumScorchIndices, textureHandle, materialState);
}

static Real getMapHeight(WorldHeightMap& map, Int x, Int y)
{
	x += map.getBorderSizeInline();
	y += map.getBorderSizeInline();
	return map.getDataPtr()[x + y * map.getXExtent()] * MAP_HEIGHT_SCALE;
}

void W3DScorch::updateScorches(WorldHeightMap& map)
{
	if (!m_needBufferRecompute || m_scorches.empty() || m_scorchTexture == nullptr)
	{
		return;
	}

	m_needBufferRecompute = false;
	m_curNumScorchVertices = 0;
	m_curNumScorchIndices = 0;

	// Stage into the full-size CPU store. The data pointers stay stable because
	// nothing reallocates between the staging resize and the trim below.
	m_scorchVertices.resize(MAX_SCORCH_VERTEX);
	m_scorchIndices.resize(MAX_SCORCH_INDEX);
	RenderBackendTexturedVertex* vb = m_scorchVertices.data();
	UnsignedShort* ib = m_scorchIndices.data();

	Real shadeR = (TheGlobalData->m_terrainAmbient[0].red + TheGlobalData->m_terrainDiffuse[0].red) / 2.0f;
	Real shadeG = (TheGlobalData->m_terrainAmbient[0].green + TheGlobalData->m_terrainDiffuse[0].green) / 2.0f;
	Real shadeB = (TheGlobalData->m_terrainAmbient[0].blue + TheGlobalData->m_terrainDiffuse[0].blue) / 2.0f;
	// Clamp exactly as DX8Wrapper::Clamp_Color did, then pack exactly as
	// Vector3::Convert_To_ARGB(float) does for DX8Wrapper::Convert_Color_Clamp.
	if (shadeR < 0.0f) shadeR = 0.0f; else if (shadeR > 1.0f) shadeR = 1.0f;
	if (shadeG < 0.0f) shadeG = 0.0f; else if (shadeG > 1.0f) shadeG = 1.0f;
	if (shadeB < 0.0f) shadeB = 0.0f; else if (shadeB > 1.0f) shadeB = 1.0f;
	UnsignedInt diffuse = (UnsignedInt)Vector3(shadeR, shadeG, shadeB).Convert_To_ARGB(1.0f);

	// TheSuperHackers @info Scorches are written in reverse order to ensure that the last added scorches fit in the buffers.
	for (std::deque<TScorch>::reverse_iterator it = m_scorches.rbegin(); it != m_scorches.rend(); ++it)
	{
		if (writeScorchToBuffer(*it, map, diffuse,
		                        vb + m_curNumScorchVertices, ib + m_curNumScorchIndices) == SCORCH_BUFFER_FULL)
		{
			break;
		}
	}

	m_scorchVertices.resize(m_curNumScorchVertices);
	m_scorchIndices.resize(m_curNumScorchIndices);
}

W3DScorch::WriteScorchResult W3DScorch::writeScorchToBuffer(const TScorch& scorch, WorldHeightMap& map,
                                                          UnsignedInt diffuse, RenderBackendTexturedVertex* curVb,
                                                          UnsignedShort* curIb)
{
	// Unpack the legacy ARGB diffuse for the backend's float color channels.
	const Real red = ((diffuse >> 16) & 255) / 255.0f;
	const Real green = ((diffuse >> 8) & 255) / 255.0f;
	const Real blue = (diffuse & 255) / 255.0f;
	const Real alpha = ((diffuse >> 24) & 255) / 255.0f;
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
			curVb->r = red;
			curVb->g = green;
			curVb->b = blue;
			curVb->a = alpha;
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
