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

#pragma once

#include <deque>

#include "WWMath/vector3.h"
#include "Common/GameType.h"
#include "Lib/BaseTypeCore.h"

struct RenderBackendTexturedVertex;
class CameraClass;
class Matrix3D;
class TextureClass;
class WorldHeightMap;

class W3DScorchInterface
{
public:
	virtual ~W3DScorchInterface() {}

	virtual void allocateBuffers() = 0;
	virtual void freeBuffers() = 0;
	virtual void clearAllScorches() = 0;
	virtual void invalidateBuffers() = 0;
	virtual void invalidateTexture() = 0;
	virtual void addScorch(Vector3 location, Real radius, Scorches type) = 0;
	virtual void drawScorches(WorldHeightMap& map, CameraClass& camera, const Matrix3D& worldTransform) = 0;
};

class W3DScorch : public W3DScorchInterface
{
public:
	W3DScorch(bool deduplicateScorches);
	virtual ~W3DScorch() override;

	virtual void allocateBuffers() override;    ///< allocate static buffers for drawing scorch marks.
	virtual void freeBuffers() override;    ///< frees up scorch buffers.
	virtual void clearAllScorches() override;
	virtual void invalidateBuffers() override;
	virtual void invalidateTexture() override;
	virtual void addScorch(Vector3 location, Real radius, Scorches type) override;
	virtual void drawScorches(WorldHeightMap& map, CameraClass& camera, const Matrix3D& worldTransform) override;    ///< Draws retained scorch geometry through the renderer backend.

private:
	typedef struct
	{
		Vector3 location;
		Real radius;
		Int scorchType;
	} TScorch;

	enum
	{
		MAX_SCORCH_VERTEX = 32768,
		MAX_SCORCH_INDEX = 65535,
		MAX_SCORCH_MARKS = 500,
		SCORCH_MARKS_IN_TEXTURE = 9,
		SCORCH_PER_ROW = 3,
	};

	enum WriteScorchResult
	{
		SCORCH_WRITTEN,
		SCORCH_SKIPPED,
		SCORCH_BUFFER_FULL,
	};

	Bool isDuplicate(const TScorch& scorch) const;
	void updateScorches(WorldHeightMap& map);    ///< Rebuild CPU geometry only when marks, terrain or lighting change.
	void releaseGeometry();
	WriteScorchResult writeScorchToBuffer(const TScorch& scorch, WorldHeightMap& map, UnsignedInt diffuse,
	                                      RenderBackendTexturedVertex* curVb, UnsignedShort* curIb);

	struct GeometryState;
	GeometryState* m_geometry;
	TextureClass* m_scorchTexture;    ///< Scorch mark texture
	Int m_curNumScorchVertices;    ///< Number of retained scorch vertices.
	Int m_curNumScorchIndices;    ///< Number of retained scorch indices.
	std::deque<TScorch> m_scorches;
	Bool m_needBufferRecompute;
	Bool m_deduplicateScorches;
};

class W3DScorchDummy : public W3DScorchInterface
{
public:
	virtual void allocateBuffers() override {}
	virtual void freeBuffers() override {}
	virtual void clearAllScorches() override {}
	virtual void invalidateBuffers() override {}
	virtual void invalidateTexture() override {}
	virtual void addScorch(Vector3, Real, Scorches) override {}
	virtual void drawScorches(WorldHeightMap&, CameraClass&, const Matrix3D&) override {}
};
