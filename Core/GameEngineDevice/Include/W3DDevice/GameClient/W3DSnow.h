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

// FILE: W3DSnow.h /////////////////////////////////////////////////////////

#pragma once

#include "GameClient/Snow.h"
#include "WWMath/vector3.h"
#include <vector>

class RenderInfoClass;
class TextureClass;

class W3DSnowManager : public SnowManager
{
  public :

	W3DSnowManager();
	virtual ~W3DSnowManager() override;

	virtual void init() override;
	virtual void reset() override;
	virtual void update () override;
	virtual void updateIniSettings() override;

	void	render(RenderInfoClass &rinfo);
	void	renderAsQuads(RenderInfoClass &rinfo, Int cubeOriginX, Int cubeOriginY, Int cubeDimX, Int cubeDimY);
	void	renderSubBox(RenderInfoClass &rinfo, Int originX, Int originY, Int cubeDimX, Int cubeDimY );
	void	ReleaseResources();
	Bool	ReAcquireResources();

 private:
	// D3D12: no backend point-list exists, so snow (including former point-sprites)
	// is submitted as indexed triangles (quads). CPU vectors preserve positions/colors.
	// The legacy DX8 index buffer and D3D point-vertex buffer are replaced by these.
	std::vector<unsigned short> m_snowIndices;
	TextureClass *m_snowTexture;
	std::vector<Vector3> m_snowPoints;
	Int m_dwBase;	///<index to beginning of unused vertex buffer space.
    Int m_dwFlush;	///<maximum amount of vertices to sumbit before rendering.
	Int m_dwDiscard;	///<maximum index allowed before needing to discard the buffer.
	Int m_leafDim;		///<horizontal dimensions of leaf nodes that are always rendered without visibility checks.
	Real m_snowCeiling;	///<height at the top of the cube with camera at center.
	Real m_heightTraveled;	///<height that snow flake traveled this frame.
	Int m_totalRendered;	///<total number of snow particles rendered this frame - only for profiling.
	Real m_cullOverscan;	///<how much extra padding to put on the sides of AABoxes when view culling.
};
