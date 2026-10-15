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

#include "W3DDevice/GameClient/W3DStatusCircle.h"
#include "W3DDevice/GameClient/WorldHeightMap.h"

#include <stdlib.h>
#include <WW3D2/assetmgr.h>
#include <WW3D2/texture.h>
#include <WWMath/tri.h>
#include <WWMath/colmath.h>
#include <WW3D2/coltest.h>
#include <WW3D2/rinfo.h>
#include <WW3D2/camera.h>
#include "WW3D2/ww3d.h"
#include "WW3D2/shader.h"
#include "Common/GlobalData.h"
#include "Common/MapObject.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/ScriptEngine.h"

#define SC_DETAIL_BLEND ( SHADE_CNST(ShaderClass::PASS_LEQUAL, ShaderClass::DEPTH_WRITE_ENABLE, ShaderClass::COLOR_WRITE_ENABLE, ShaderClass::SRCBLEND_ONE, \
	ShaderClass::DSTBLEND_ZERO, ShaderClass::FOG_DISABLE, ShaderClass::GRADIENT_MODULATE, ShaderClass::SECONDARY_GRADIENT_DISABLE, ShaderClass::TEXTURING_ENABLE, \
	ShaderClass::DETAILCOLOR_SCALE, ShaderClass::DETAILALPHA_DISABLE, ShaderClass::ALPHATEST_DISABLE, ShaderClass::CULL_MODE_ENABLE, \
	ShaderClass::DETAILCOLOR_SCALE, ShaderClass::DETAILALPHA_DISABLE) )

// Texturing, no zbuffer, disabled zbuffer write, primary gradient, alpha blending
#define SC_ALPHA ( SHADE_CNST(ShaderClass::PASS_ALWAYS, ShaderClass::DEPTH_WRITE_DISABLE, ShaderClass::COLOR_WRITE_ENABLE, ShaderClass::SRCBLEND_SRC_ALPHA, \
	ShaderClass::DSTBLEND_ONE_MINUS_SRC_ALPHA, ShaderClass::FOG_DISABLE, ShaderClass::GRADIENT_MODULATE, ShaderClass::SECONDARY_GRADIENT_DISABLE, ShaderClass::TEXTURING_ENABLE, \
	ShaderClass::ALPHATEST_DISABLE, ShaderClass::CULL_MODE_ENABLE, \
	ShaderClass::DETAILCOLOR_DISABLE, ShaderClass::DETAILALPHA_DISABLE) )

// Texturing, no zbuffer, disabled zbuffer write, primary gradient, alpha blending
#define SC_ALPHA_Z ( SHADE_CNST(ShaderClass::PASS_LEQUAL, ShaderClass::DEPTH_WRITE_DISABLE, ShaderClass::COLOR_WRITE_ENABLE, ShaderClass::SRCBLEND_SRC_ALPHA, \
	ShaderClass::DSTBLEND_ONE_MINUS_SRC_ALPHA, ShaderClass::FOG_DISABLE, ShaderClass::GRADIENT_MODULATE, ShaderClass::SECONDARY_GRADIENT_DISABLE, ShaderClass::TEXTURING_ENABLE, \
	ShaderClass::DETAILCOLOR_DISABLE, ShaderClass::DETAILALPHA_DISABLE, ShaderClass::ALPHATEST_DISABLE, ShaderClass::CULL_MODE_ENABLE, \
	ShaderClass::DETAILCOLOR_DISABLE, ShaderClass::DETAILALPHA_DISABLE) )

// Texturing, no zbuffer, disabled zbuffer write, no gradient, add src to dest.
#define SC_ADD ( SHADE_CNST(ShaderClass::PASS_ALWAYS, ShaderClass::DEPTH_WRITE_DISABLE, ShaderClass::COLOR_WRITE_ENABLE, ShaderClass::SRCBLEND_ONE, \
	ShaderClass::DSTBLEND_ONE, ShaderClass::FOG_DISABLE, ShaderClass::GRADIENT_DISABLE, ShaderClass::SECONDARY_GRADIENT_DISABLE, ShaderClass::TEXTURING_ENABLE, \
	ShaderClass::ALPHATEST_DISABLE, ShaderClass::CULL_MODE_ENABLE, \
	ShaderClass::DETAILCOLOR_DISABLE, ShaderClass::DETAILALPHA_DISABLE) )



Bool W3DStatusCircle::m_needUpdate;
Int W3DStatusCircle::m_diffuse=255; // blue.

namespace
{

// Packed b | g<<8 | r<<16 | a<<24, as the team color is stored.
void Set_Vertex_Color(RenderBackendTexturedVertex &v, Int diffuse)
{
	v.b = ((diffuse) & 0xff) / 255.0f;
	v.g = ((diffuse >> 8) & 0xff) / 255.0f;
	v.r = ((diffuse >> 16) & 0xff) / 255.0f;
	v.a = ((diffuse >> 24) & 0xff) / 255.0f;
}

} // namespace

W3DStatusCircle::~W3DStatusCircle()
{
	freeMapResources();
}

W3DStatusCircle::W3DStatusCircle()
{
	m_numTriangles=0;
	m_initialized=false;
}


bool W3DStatusCircle::Cast_Ray(RayCollisionTestClass & raytest)
{

	return false;

}


//@todo: MW Handle both of these properly!!
W3DStatusCircle::W3DStatusCircle(const W3DStatusCircle & src)
{
	*this = src;
}

W3DStatusCircle & W3DStatusCircle::operator = (const W3DStatusCircle & that)
{
	assert(false);
	return *this;
}

void W3DStatusCircle::Get_Obj_Space_Bounding_Sphere(SphereClass & sphere) const
{
	Vector3	ObjSpaceCenter((float)1000*0.5f,(float)1000*0.5f,(float)0);
	float length = ObjSpaceCenter.Length();

	sphere.Init(ObjSpaceCenter, length);
}

void W3DStatusCircle::Get_Obj_Space_Bounding_Box(AABoxClass & box) const
{
	Vector3	minPt(0,0,0);
	Vector3	maxPt((float)1000,(float)1000,(float)1000);
	box.Init(minPt,maxPt);
}

Int W3DStatusCircle::Class_ID() const
{
	return RenderObjClass::CLASSID_UNKNOWN;
}

RenderObjClass * W3DStatusCircle::Clone() const
{
	return NEW W3DStatusCircle(*this);
}


Int W3DStatusCircle::freeMapResources()
{
	m_initialized = false;
	return 0;
}

//Allocate a heightmap of x by y vertices.
//data must be an array matching this size.
Int W3DStatusCircle::initData()
{
	Int i;

	m_needUpdate = true;
	freeMapResources();	//free old data

	m_numTriangles = NUM_CIRCLE_TRIANGLES;

	// Fill up the indices
	UnsignedShort *ib=m_indices;

	for (i=0; i<3*m_numTriangles; i+=3)
	{
		ib[0]=i;
		ib[1]=i+1;
		ib[2]=i+2;

		ib+=3;	//skip the 3 indices we just filled
	}

	m_shaderClass = ShaderClass(SC_ALPHA);// _PresetOpaque2DShader;//; //_PresetOpaqueShader;

	m_initialized = true;
	return 0;
}


/** updateCircleVB puts a circle with a team color vertex buffer. */

Int W3DStatusCircle::updateCircleVB()
{
	Int i, k;
	if (m_initialized)
	{
		m_needUpdate = false;
		RenderBackendTexturedVertex *vb = m_circleVertices;

		const Real theZ = 0.0f;
		const Real theRadius = 0.02f;
		const Int theAlpha = 127;
	  Int diffuse = m_diffuse + (theAlpha<<24);	 // b g<<8 r<<16 a<<24.
		Int limit = m_numTriangles;
		float curAngle = 0;
		float deltaAngle = 2*PI/limit;
		for (i=0; i<limit; i++)
		{
			for (k=0; k<3; k++) {
				*vb = {};
				vb->z=  theZ;
				if (k==0) {
					vb->x=	0;
					vb->y=	0;
				} else if (k==1) {
					Vector3 vec(theRadius,0,theZ);
					vec.Rotate_Z(curAngle);
					vb->x=	vec.X;
					vb->y=	vec.Y;
				} else if (k==2) {
					Real angle = curAngle+deltaAngle;
					if (i==limit-1) {
						angle = 0;
					}
					Vector3 vec(theRadius,0,theZ);
					vec.Rotate_Z(angle);
					vb->x=	vec.X;
					vb->y=	vec.Y;
				}
				Set_Vertex_Color(*vb, diffuse);
				vb++;
			}
			curAngle += deltaAngle;

		}
		return 0; //success.
	}
	return -1;
}

/** updateScreenVB puts a 2 triangle quad covering the screen with a fade color. */

Int W3DStatusCircle::updateScreenVB(Int diffuse)
{
	if (m_initialized)
	{
		static const Real corners[6][2] = {{-1,-1},{1,1},{-1,1},{-1,-1},{1,-1},{1,1}};
		for (Int i=0; i<6; i++)
		{
			RenderBackendTexturedVertex &v = m_screenVertices[i];
			v = {};
			v.x = corners[i][0];
			v.y = corners[i][1];
			v.z = 0;
			Set_Vertex_Color(v, diffuse);
		}
		return 0; //success.
	}
	return -1;
}

void W3DStatusCircle::Render(RenderInfoClass & rinfo)
{
	if (!TheGameLogic->isInGame() || TheGameLogic->getGameMode() == GAME_SHELL)
		return;

	if (!m_initialized) {
		initData();
	}
	IRenderBackend *backend = WW3D::Get_Render_Backend();
	if (!m_initialized || !backend) {
		return;
	}

	// The 2D scene's camera maps z=0 positions onto -1..1 of the screen. Both shapes are
	// untextured and always face that camera, so no culling is needed.
	if( TheGlobalData->m_showTeamDot )
	{
		if (m_needUpdate) {
			updateCircleVB();
		}
		RenderBackendMaterialState material;
		if (m_shaderClass.Get_Render_Backend_State(material))
		{
			material.cull = RenderBackendCullMode::None;
			RenderBackendTexturedVertex vertices[NUM_CIRCLE_TRIANGLES*3];
			const Vector3 vec(0.95f, 0.67f, 0);
			for (Int i=0; i<m_numTriangles*3; i++)
			{
				vertices[i] = m_circleVertices[i];
				vertices[i].x += vec.X;
				vertices[i].y += vec.Y;
				vertices[i].z += vec.Z;
			}
			backend->Draw_Indexed_Material_Triangles(vertices, m_numTriangles*3, m_indices, m_numTriangles*3, {}, material);
		}
	}


	ScriptEngine::TFade fade = TheScriptEngine->getFade();
	if (fade == ScriptEngine::FADE_NONE) {
		return;
	}

	Real intensity = TheScriptEngine->getFadeValue();
	Int clr = 255*intensity;
	Int diffuse = (0xff<<24)|(clr<<16)|(clr<<8)|clr;	 // b g<<8 r<<16 a<<24.
	updateScreenVB(diffuse);
	RenderBackendMaterialState material;
	if (!ShaderClass(SC_ADD).Get_Render_Backend_State(material)) {
		return;
	}
	material.cull = RenderBackendCullMode::None;
	switch (fade) {
		default:
		case ScriptEngine::FADE_ADD:
			backend->Draw_Indexed_Material_Triangles(m_screenVertices, 6, m_indices, 6, {}, material);
			break;
		case ScriptEngine::FADE_SUBTRACT:
			material.blend_operation = RenderBackendBlendOperation::ReverseSubtract;
			backend->Draw_Indexed_Material_Triangles(m_screenVertices, 6, m_indices, 6, {}, material);
			break;
		case ScriptEngine::FADE_SATURATE:
			// 4x multiply
			material.source_blend = RenderBackendBlendFactor::DestinationColor;
			material.destination_blend = RenderBackendBlendFactor::SourceColor;
			backend->Draw_Indexed_Material_Triangles(m_screenVertices, 6, m_indices, 6, {}, material);
			backend->Draw_Indexed_Material_Triangles(m_screenVertices, 6, m_indices, 6, {}, material);
			break;
		case ScriptEngine::FADE_MULTIPLY:
			// Straight multiply
			material.source_blend = RenderBackendBlendFactor::Zero;
			material.destination_blend = RenderBackendBlendFactor::SourceColor;
			backend->Draw_Indexed_Material_Triangles(m_screenVertices, 6, m_indices, 6, {}, material);
			break;
	}
}
