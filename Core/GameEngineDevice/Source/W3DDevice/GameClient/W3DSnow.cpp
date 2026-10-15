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

#include "W3DDevice/GameClient/W3DSnow.h"
#include "W3DDevice/GameClient/HeightMap.h"
#include "GameClient/View.h"
#include "WW3D2/statistics.h"
#include "WW3D2/texture.h"
#include "WW3D2/ww3d.h"
#include "WW3D2/rinfo.h"
#include "WW3D2/camera.h"
#include "WW3D2/assetmgr.h"



#define SNOW_BATCH_SIZE	2048	//we render at most this many particles per draw call.  This number * 6 must be less than 65536 to fit into 16-bit indices.

W3DSnowManager::W3DSnowManager()
{
	m_snowTexture=nullptr;
}

W3DSnowManager::~W3DSnowManager()
{
	ReleaseResources();
}

void W3DSnowManager::init()
{
	SnowManager::init();
	ReAcquireResources();
}

/** Releases all W3D assets before a reset.. */
void W3DSnowManager::ReleaseResources()
{
	REF_PTR_RELEASE(m_snowTexture);
	m_quadIndices.clear();
	m_quadVertices.clear();
	m_snowTextureHandle = {};
}

/** (Re)allocates all W3D assets after a reset.. */
Bool W3DSnowManager::ReAcquireResources()
{
	ReleaseResources();

	if (!TheWeatherSetting->m_snowEnabled)
		return TRUE;	//no need for resources if snow is disabled.

	// D3D12 has no point sprites, so every flake is a camera-facing quad.
	m_quadVertices.resize(SNOW_BATCH_SIZE*4);	//4 verts per flake.
	m_quadIndices.resize(SNOW_BATCH_SIZE*6);	//allocate 2 triangles per flake, each with 3 indices.

	// Fill up the indices that will be used for all flakes.
	{
		UnsignedShort *ib=m_quadIndices.data();
		//quad of 4 triangles:
		//	0-----3
		//  |\   /|
		//  |  X  |
		//	|/   \|
		//  1-----2
		Int vbCount=0;
		for (Int i=0; i<SNOW_BATCH_SIZE; i++)
		{
			//Top
			ib[0]=vbCount+3;
			ib[1]=vbCount;
			ib[2]=vbCount+2;
			//Bottom
			ib[3]=vbCount+2;
			ib[4]=vbCount;
			ib[5]=vbCount+1;

			vbCount += 4;
			ib+=6;
		}
	}

	m_snowTexture = WW3DAssetManager::Get_Instance()->Get_Texture(TheWeatherSetting->m_snowTexture.str());

	return TRUE;
}

void W3DSnowManager::updateIniSettings()
{
	//Call base class
	SnowManager::updateIniSettings();

	if (m_snowTexture && stricmp(m_snowTexture->Get_Texture_Name(),TheWeatherSetting->m_snowTexture.str()) != 0)
	{
		REF_PTR_RELEASE(m_snowTexture);
		m_snowTexture = WW3DAssetManager::Get_Instance()->Get_Texture(TheWeatherSetting->m_snowTexture.str());
	}
}

void W3DSnowManager::reset()
{
	SnowManager::reset();
}

void W3DSnowManager::update()
{
	// TheSuperHackers @tweak The snow render update is now decoupled from the logic step.
	m_time += WW3D::Get_Logic_Frame_Time_Seconds();

	//find current time offset, adjusting for overflow
	m_time=fmod(m_time,m_fullTimePeriod);
}

#define MAXIMUM_CAMERA_DISTANCE 100000	//maximum distance of camera position from world origin.
#define ISPOW2(x)  (x && (x & (x-1)) == 0)	//is a number a power of 2?
#define MODPOW2(x,y) ((x) & (y-1))		//mod '%' operator for powers of 2.

/*Recursively subdivide the large snow box enclosing the camera until we reach some predefined leaf size.  This
method is used so that very few off-screen particles end up getting rendered.  Culling them individually would
be too expensive since we're dealing with 1000's for this effect.*/
void W3DSnowManager::renderSubBox(RenderInfoClass &rinfo, Int originX, Int originY, Int cubeDimX, Int cubeDimY )
{
	//check if this box is too large and needs subdivision
	Int boxDimX=cubeDimX - originX;
	Int boxDimY=cubeDimY - originY;
	Int halfX=REAL_TO_INT_CEIL(boxDimX*0.5f);
	Int halfY=REAL_TO_INT_CEIL(boxDimY*0.5f);

	CameraClass &camera=rinfo.Camera;
	MinMaxAABoxClass mmbox;

	if (boxDimX > m_leafDim)
	{
		//subdivide the box
		if (boxDimY > m_leafDim)
		{
			//subdivide in both directions
			//Upper left
			mmbox.MinCorner.Set(originX*m_emitterSpacing-m_cullOverscan, (originY + halfY)*m_emitterSpacing-m_cullOverscan, m_snowCeiling-m_boxDimensions);
			mmbox.MaxCorner.Set((originX + halfX)*m_emitterSpacing+m_cullOverscan, cubeDimY*m_emitterSpacing+m_cullOverscan, m_snowCeiling);
			if (CollisionMath::Overlap_Test(camera.Get_Frustum(),mmbox) != CollisionMath::OUTSIDE)
				renderSubBox(rinfo, originX, originY + halfY, originX + halfX, cubeDimY);
			//Upper right
			mmbox.MinCorner.Set((originX + halfX)*m_emitterSpacing-m_cullOverscan, (originY + halfY)*m_emitterSpacing-m_cullOverscan, m_snowCeiling-m_boxDimensions);
			mmbox.MaxCorner.Set(cubeDimX*m_emitterSpacing+m_cullOverscan, cubeDimY*m_emitterSpacing+m_cullOverscan, m_snowCeiling);
			if (CollisionMath::Overlap_Test(camera.Get_Frustum(),mmbox) != CollisionMath::OUTSIDE)
				renderSubBox(rinfo, originX + halfX, originY + halfY,cubeDimX, cubeDimY);
			//Lower left
			mmbox.MinCorner.Set(originX*m_emitterSpacing-m_cullOverscan, originY*m_emitterSpacing-m_cullOverscan, m_snowCeiling-m_boxDimensions);
			mmbox.MaxCorner.Set((originX + halfX)*m_emitterSpacing+m_cullOverscan, (originY + halfY)*m_emitterSpacing+m_cullOverscan, m_snowCeiling);
			if (CollisionMath::Overlap_Test(camera.Get_Frustum(),mmbox) != CollisionMath::OUTSIDE)
				renderSubBox(rinfo, originX,originY,originX + halfX, originY + halfY);
			//Lower right
			mmbox.MinCorner.Set((originX + halfX)*m_emitterSpacing-m_cullOverscan, originY*m_emitterSpacing-m_cullOverscan, m_snowCeiling-m_boxDimensions);
			mmbox.MaxCorner.Set(cubeDimX*m_emitterSpacing+m_cullOverscan, (originY + halfY)*m_emitterSpacing+m_cullOverscan, m_snowCeiling);
			if (CollisionMath::Overlap_Test(camera.Get_Frustum(),mmbox) != CollisionMath::OUTSIDE)
				renderSubBox(rinfo, originX + halfX, originY, cubeDimX, originY + halfY);
			return;
		}
		else
		{
			//only subdivide in x direction.
			//Left
			mmbox.MinCorner.Set(originX*m_emitterSpacing-m_cullOverscan, originY*m_emitterSpacing-m_cullOverscan, m_snowCeiling-m_boxDimensions);
			mmbox.MaxCorner.Set((originX + halfX)*m_emitterSpacing+m_cullOverscan, cubeDimY*m_emitterSpacing+m_cullOverscan, m_snowCeiling);
			if (CollisionMath::Overlap_Test(camera.Get_Frustum(),mmbox) != CollisionMath::OUTSIDE)
				renderSubBox(rinfo, originX, originY, originX + halfX, cubeDimY);
			//Right
			mmbox.MinCorner.Set((originX + halfX)*m_emitterSpacing-m_cullOverscan, originY*m_emitterSpacing-m_cullOverscan, m_snowCeiling-m_boxDimensions);
			mmbox.MaxCorner.Set(cubeDimX*m_emitterSpacing+m_cullOverscan, cubeDimY*m_emitterSpacing+m_cullOverscan, m_snowCeiling);
			if (CollisionMath::Overlap_Test(camera.Get_Frustum(),mmbox) != CollisionMath::OUTSIDE)
				renderSubBox(rinfo, originX + halfX, originY, cubeDimX, cubeDimY);
			return;
		}
	}
	else
	if (boxDimY > m_leafDim)
	{
		//only subdivide in y direction
		//Top
		mmbox.MinCorner.Set(originX*m_emitterSpacing-m_cullOverscan, (originY+halfY)*m_emitterSpacing-m_cullOverscan, m_snowCeiling-m_boxDimensions);
		mmbox.MaxCorner.Set(cubeDimX*m_emitterSpacing+m_cullOverscan, cubeDimY*m_emitterSpacing+m_cullOverscan, m_snowCeiling);
		if (CollisionMath::Overlap_Test(camera.Get_Frustum(),mmbox) != CollisionMath::OUTSIDE)
			renderSubBox(rinfo, originX, originY+halfY,cubeDimX, cubeDimY);
		//Bottom
		mmbox.MinCorner.Set(originX*m_emitterSpacing-m_cullOverscan, originY*m_emitterSpacing-m_cullOverscan, m_snowCeiling-m_boxDimensions);
		mmbox.MaxCorner.Set(cubeDimX*m_emitterSpacing+m_cullOverscan, (originY + halfY)*m_emitterSpacing+m_cullOverscan, m_snowCeiling);
		if (CollisionMath::Overlap_Test(camera.Get_Frustum(),mmbox) != CollisionMath::OUTSIDE)
			renderSubBox(rinfo, originX, originY, cubeDimX, originY + halfY);
		return;
	}

	//Box too small to subdivide so render it.
	renderAsQuads(rinfo, originX, originY, cubeDimX, cubeDimY);
}

void W3DSnowManager::render(RenderInfoClass &rinfo)
{
	if (!TheWeatherSetting->m_snowEnabled || !m_isVisible)
		return;

	//make sure the noise table is powers of 2 in dimensions.
	WWASSERT(ISPOW2(SNOW_NOISE_X) && ISPOW2(SNOW_NOISE_Y));

	//CameraClass &camera=rinfo.Camera;

	const Coord3D &cPos=TheTacticalView->get3DCameraPosition();
	Vector3 camPos(cPos.x,cPos.y,cPos.z);

	//Number of emitters from cube center to edge of visible extent.
	Int mumEmittersInHalf=(Int)floor(m_boxDimensions / m_emitterSpacing * 0.5f);

	//Find origin of visible cube surrounding camera.
	Int cubeCenterX=(Int)floor(camPos.X/m_emitterSpacing);
	Int cubeCenterY=(Int)floor(camPos.Y/m_emitterSpacing);

	//Find extents of visible cube surrounding camera.
	Int cubeOriginX=cubeCenterX - mumEmittersInHalf;	//top/left extents.
	Int cubeOriginY=cubeCenterY - mumEmittersInHalf;
	Int cubeDimX=cubeCenterX + mumEmittersInHalf;		//bottom/right extents.
	Int cubeDimY=cubeCenterY + mumEmittersInHalf;

 	const FrustumClass & frustum = rinfo.Camera.Get_Frustum();
	AABoxClass bbox;

	//Get a bounding box around our visible universe.  Bounded by terrain and the sky
	//so much tighter fitting volume than what's actually visible.  This will cull
	//particles falling under the ground.

 	TheTerrainRenderObject->getMaximumVisibleBox(frustum, &bbox, TRUE);

	//Particles move outside the visible box as a result of local sine movement
	//so adjust bounding box to include them.
	bbox.Extent.X += m_amplitude+m_quadSize;
	bbox.Extent.Y += m_amplitude+m_quadSize;

	//Clip our visible snow rendering box
	if ((cubeOriginX * m_emitterSpacing ) < (bbox.Center.X - bbox.Extent.X))
		cubeOriginX = (Int)floor ((bbox.Center.X - bbox.Extent.X)/m_emitterSpacing);

	if ((cubeOriginY * m_emitterSpacing ) < (bbox.Center.Y - bbox.Extent.Y))
		cubeOriginY = (Int)floor ((bbox.Center.Y - bbox.Extent.Y)/m_emitterSpacing);

	if ((cubeDimX * m_emitterSpacing ) > (bbox.Center.X + bbox.Extent.X))
		cubeDimX = (Int)floor ((bbox.Center.X + bbox.Extent.X)/m_emitterSpacing);

	if ((cubeDimY * m_emitterSpacing ) > (bbox.Center.Y + bbox.Extent.Y))
		cubeDimY = (Int)floor ((bbox.Center.Y + bbox.Extent.Y)/m_emitterSpacing);

	if ((cubeDimY - cubeOriginY) < 0 || (cubeDimX-cubeOriginX) < 0)
		return;	//entire snow box is culled by either x or y screen boundary.

	//Find total number of particles that need rendering.
	Int totalPart=(cubeDimY-cubeOriginY)*(cubeDimX-cubeOriginX);

	if (totalPart <= 0)
		return;	//nothing to render.

	//Height at the top of the cube with camera at center.
	m_snowCeiling = camPos.Z + m_boxDimensions/2.0f;

	//Offset to allow cube extents to move with camera.
	Real cameraOffset = fmod (camPos.Z,m_boxDimensions);
	m_heightTraveled=m_time*m_velocity+cameraOffset;	//height that snow flake traveled this frame.

	IRenderBackend *backend = WW3D::Get_Render_Backend();
	if (!backend)
		return;

	//make sure we have all the resources we need
	if (m_quadIndices.empty())
		ReAcquireResources();

	if (!ShaderClass::_PresetAlphaShader.Get_Render_Backend_State(m_material))
		return;
	m_material.cull = RenderBackendCullMode::None;
	m_snowTextureHandle = {};
	if (m_snowTexture)
	{
		if (!m_snowTexture->Ensure_Renderer_Texture() || !m_snowTexture->Get_Filter().Get_Render_Sampler(m_material.sampler))
			return;
		m_snowTextureHandle = m_snowTexture->Get_Renderer_Texture();
	}

	// Quad vertices are built in view space, so only the camera projection is applied.
	Matrix4x4 savedViewProjection;
	backend->Get_View_Projection(savedViewProjection);
	Matrix4x4 projection;
	rinfo.Camera.Get_Zero_To_One_Projection_Matrix(&projection);
	backend->Set_View_Projection(projection);

	m_leafDim = 45;	//cull boxes that are 20x20 emitters in size. Making them much smaller will result in too many draw calls.
	m_totalRendered = 0;	//keep track of how many particles were rendered.

	//Particle centers can deviate from center by by amplitude of sine offset.  They also have radius m_quadSize.
	//Enlarge culling bounds to compensate.
	m_cullOverscan = m_amplitude+m_quadSize;
	renderSubBox(rinfo,cubeOriginX,cubeOriginY,cubeDimX,cubeDimY);

	backend->Set_View_Projection(savedViewProjection);
}

/**Render the flakes of one box as camera-facing quads.*/
void W3DSnowManager::renderAsQuads(RenderInfoClass &rinfo, Int cubeOriginX, Int cubeOriginY, Int cubeDimX, Int cubeDimY)
{

	Matrix4x4 proj;
	Matrix3D view;
	Vector3 snowCenter;
	Vector3 snowCenterVS;

	CameraClass &camera=rinfo.Camera;

	camera.Get_View_Matrix(&view);
	camera.Get_Projection_Matrix(&proj);

	Vector3 vertex_offsets[4] = {
		Vector3(-0.5f, 0.5f, 0.0f),
		Vector3(-0.5f, -0.5f, 0.0f),
		Vector3(0.5f, -0.5f, 0.0f),
		Vector3(0.5f, 0.5f, 0.0f)
	};

	Vector2 quad_uvs[4] = {
		Vector2(0.0f, 0.0f),
		Vector2(0.0f, 1.0f),
		Vector2(1.0f, 1.0f),
		Vector2(1.0f, 0.0f)
	};


	//pre-multiple the offsets by particle size
	for (Int i=0; i<4; i++)
	{
		vertex_offsets[i] *= m_quadSize;
	}

	IRenderBackend *backend = WW3D::Get_Render_Backend();
	if (!backend || m_quadIndices.empty())
		return;

	Int y=cubeOriginY;	//loop counter.
	Int cubeOriginXRemainder = cubeOriginX;	//loop counter - adjusted when not all particles fit into render buffer.

	//Find total number of particles that need rendering.
	Int totalPart=(cubeDimY-cubeOriginY)*(cubeDimX-cubeOriginX);

	m_totalRendered += totalPart;

	while (totalPart)
	{
		Int batchSize=totalPart;

		if (batchSize > SNOW_BATCH_SIZE)
			batchSize = SNOW_BATCH_SIZE;

		Int numberInBatch=0;

		{
			RenderBackendTexturedVertex* verts=m_quadVertices.data();

			for (;y<cubeDimY; y++)
			{
				for (Int x=cubeOriginXRemainder; x<cubeDimX; x++)
				{
					if (numberInBatch >= batchSize)
					{	cubeOriginXRemainder = x;
						goto flush_particles;
					}

					//Get initial height from noise table.  We add a large value to make sure it's positive.  Then
					//modulate by table dimensions to find a value.
					Int noiseOffset=MODPOW2(x+MAXIMUM_CAMERA_DISTANCE,SNOW_NOISE_X)+MODPOW2(y+MAXIMUM_CAMERA_DISTANCE,SNOW_NOISE_Y)*SNOW_NOISE_X;
					if (noiseOffset > (SNOW_NOISE_X * SNOW_NOISE_Y))
						noiseOffset = 0;	//this should never happen but check to prevent buffer over/under flow.

					//find current height
					Real h0=m_snowCeiling-fmod(m_heightTraveled+m_startingHeights[noiseOffset],m_boxDimensions);

					//find world-space position of snow flake
					snowCenter.Set(x*m_emitterSpacing,y*m_emitterSpacing,h0);

					//Get view-space position
					Matrix3D::Transform_Vector(view,snowCenter,&snowCenterVS);

					//Adjust position so snow flakes don't fall straight down.
					snowCenterVS.X += m_amplitude * WWMath::Fast_Sin( h0 * m_frequencyScaleX + (Real)x);
					snowCenterVS.Y += m_amplitude * WWMath::Fast_Sin( h0 * m_frequencyScaleY + (Real)y);

					for (Int i=0; i<4; i++)
					{
						const Vector3 position=snowCenterVS + vertex_offsets[i];
						verts->x=position.X;
						verts->y=position.Y;
						verts->z=position.Z;
						verts->r=verts->g=verts->b=verts->a=1.0f;	//opaque white
						verts->u=quad_uvs[i].X;
						verts->v=quad_uvs[i].Y;
						verts++;
					}

					numberInBatch++;
				}
				//getting here means we did not overflow the render buffer, so reset x origin to normal.
				cubeOriginXRemainder = cubeOriginX;	//reset to normal amount
			}
flush_particles:
			numberInBatch;	//need something at goto destination - stupid c compiler.
		}

		//Render any particles that may be queued up.
		if (numberInBatch)
		{
			Debug_Statistics::Record_Rendered_Polys_And_Vertices(numberInBatch*2,numberInBatch*4,ShaderClass::_PresetAlphaShader);
			backend->Draw_Indexed_Material_Triangles(m_quadVertices.data(), numberInBatch*4,
				m_quadIndices.data(), numberInBatch*6, m_snowTextureHandle, m_material);
			totalPart -= numberInBatch;
		}
	}
}
