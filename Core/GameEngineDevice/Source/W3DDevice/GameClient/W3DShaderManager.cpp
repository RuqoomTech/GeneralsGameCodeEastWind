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

// FILE: W3DShaderManager.cpp ////////////////////////////////////////////////
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
// File name: W3DShaderManager.cpp
//
// Created:   Mark Wilczynski, August 2001
//
// Desc:      Perform tests on currently selected WW3D/D3D device to determine
//			  which of our rendering features are supported.  The system allows
//			  setting up a few custom shaders that are selected based on video
//			  card features.
//
//			  To add a new shader to the system:
//			  0) Add your shader to the ShaderTypes enum
//			  1) Create shader using W3DShaderInterface
//			  2) Repeat step 1 for any alternate shaders
//			  3) Create list of alternate shaders sorted by order of preference.
//				 The first shader which passes hardware validation will be selected.
//			  4) Add list from step 3) to MasterShaderList[].
//
//-----------------------------------------------------------------------------

#include "WW3D2/ww3d.h"
#include "WW3D2/IRenderBackend.h"
#include "WW3D2/assetmgr.h"
#include "WW3D2/shader.h"
#include "Lib/BaseType.h"
#include "Common/file.h"
#include "Common/FileSystem.h"
#include "W3DDevice/GameClient/W3DShaderManager.h"
#include "W3DDevice/GameClient/W3DShroud.h"
#include "W3DDevice/GameClient/HeightMap.h"
#include "W3DDevice/GameClient/W3DCustomScene.h"
#include "W3DDevice/GameClient/W3DSmudge.h"
#include "GameClient/View.h"
#include "GameClient/CommandXlat.h"
#include "GameClient/Display.h"
#include "GameClient/Water.h"
#include "GameLogic/GameLogic.h"
#include "Common/GlobalData.h"
#include "Common/GameLOD.h"
#include <vector>

namespace
{

// D3D12: renderer-neutral shader math (renamed from the retired D3D matrix
// utilities). Projection/texture-transform uploads have no backend equivalent
// yet; the CPU math below is preserved for documentation and future callers.
Matrix4x4 Multiply_Legacy_Shader_Matrices(const Matrix4x4 &a, const Matrix4x4 &b)
{
	return a * b;
}

void Make_Legacy_Shader_Translation(Matrix4x4 &matrix, float x, float y, float z)
{
	matrix.Make_Identity();
	matrix[3][0] = x;
	matrix[3][1] = y;
	matrix[3][2] = z;
}

void Make_Legacy_Shader_Scaling(Matrix4x4 &matrix, float x, float y, float z)
{
	matrix.Make_Identity();
	matrix[0][0] = x;
	matrix[1][1] = y;
	matrix[2][2] = z;
}

bool Invert_Legacy_Shader_Matrix(Matrix4x4 &inverse, float &determinant, const Matrix4x4 &matrix)
{
	return Matrix4x4::Inverse(&inverse, &determinant, &matrix) != nullptr;
}

// GAP (documented): pixel-shader constants (monochrome weights, crossfade
// tints, water reflection factor) have no backend constant equivalent yet.
// Values are preserved at their call sites in comments; base geometry is
// submitted with the closest material (nothing faked).
struct LegacyPixelShaderConstant
{
	unsigned int shader_register;
	Vector4 value;
};

} // namespace


// Turn this on to turn off pixel shaders. jba[4/3/2003]
#define do_not_DISABLE_PIXEL_SHADERS 1

/** Interface definition for custom shaders we define in our app.  These shaders can perform more complex
	operations than those allowed in the WW3D2 shader system.
*/
class W3DShaderInterface
{
public:
	Int getNumPasses() {return m_numPasses;};	///<return number of passes needed for this shader
	virtual Int set(Int pass) {return TRUE;};		///<setup shader for the specified rendering pass.
	 ///do any custom resetting necessary to bring W3D in sync.
	virtual void reset() {
		ShaderClass::Invalidate();
		if (IRenderBackend *resetBackend = WW3D::Get_Render_Backend())
			resetBackend->Invalidate_Cached_Render_States();};
	virtual Int init() = 0;			///<perform any one time initialization and validation
	virtual Int shutdown() { return TRUE;};			///<release resources used by shader
	// D3D12: last translated material for this shader. set() fills it via
	// ShaderClass::Get_Render_Backend_State (textures/samplers preserved from
	// m_Textures); crossed callers submit geometry with it. No new framework:
	// one cached state per existing shader object.
	const RenderBackendMaterialState &getBackendState() const { return m_backendState; }
	Bool hasBackendState() const { return m_hasBackendState; }
protected:
	void storeBackendState(const RenderBackendMaterialState &state) { m_backendState = state; m_hasBackendState = true; }
	void clearBackendState() { m_hasBackendState = false; }
	Int m_numPasses;						///<number of passes to complete shader
private:
	RenderBackendMaterialState m_backendState;
	Bool m_hasBackendState = false;
};

//this table will contain custom versions of each shader tuned for specific video card and user options.
static W3DFilterInterface *W3DFilters[FT_MAX];
static W3DShaderInterface *W3DShaders[W3DShaderManager::ST_MAX];
static Int W3DShadersPassCount[W3DShaderManager::ST_MAX];	//number of passes for each of the above shaders
TextureClass *W3DShaderManager::m_Textures[8];
W3DShaderManager::ShaderTypes W3DShaderManager::m_currentShader;
FilterTypes W3DShaderManager::m_currentFilter=FT_NULL_FILTER; ///< Last filter that was set.
Int W3DShaderManager::m_currentShaderPass;
ChipsetType W3DShaderManager::m_currentChipset;
GraphicsVenderID W3DShaderManager::m_currentVendor;
__int64 W3DShaderManager::m_driverVersion;

Bool W3DShaderManager::m_renderingToTexture = false;
TextureClass *W3DShaderManager::m_filterTexture = nullptr;	///<backend texture into which rendering is redirected.
RenderBackendTextureHandle W3DShaderManager::m_filterTarget;	///<backend handle for m_filterTexture.

namespace
{

// D3D12: fullscreen filter quads (legacy XYZRHW DrawPrimitiveUP) become
// screen_space material draws. Pixel corners are converted to clip/NDC
// (backend screen_space bypasses the camera); UVs and vertex colors preserved.
struct ScreenSpaceFilterVertex
{
	float px, py; // pixels, top-left origin (legacy XYZRHW convention).
	unsigned int color; // ARGB diffuse.
	float u, v;
};

Bool Submit_Screen_Space_Filter_Quad(
	const ScreenSpaceFilterVertex quad[4],
	TextureClass *texture,
	const RenderBackendMaterialState &baseMaterial)
{
	IRenderBackend *backend = WW3D::Get_Render_Backend();
	if (backend == nullptr)
		return false;
	int outWidth = 0, outHeight = 0, outBits = 0;
	Bool outWindowed = TRUE;
	if (!backend->Get_Output_Description(outWidth, outHeight, outBits, outWindowed)
		|| outWidth <= 0 || outHeight <= 0)
		return false;
	RenderBackendMaterialState material = baseMaterial;
	material.screen_space = true;
	material.cull = RenderBackendCullMode::None;
	material.depth_test = RenderBackendDepthTest::Always;
	material.depth_write = false;
	RenderBackendTextureHandle handle;
	if (texture != nullptr) {
		const TextureFilterClass &filter = texture->Get_Filter();
		if (!filter.Get_Render_Sampler(material.sampler))
			return false;
		material.clamp_texture = true;
		if (!texture->Ensure_Renderer_Texture())
			return false;
		handle = texture->Get_Renderer_Texture();
	}
	RenderBackendTexturedVertex verts[4];
	for (int i = 0; i < 4; ++i) {
		verts[i].x = (2.0f * quad[i].px / static_cast<float>(outWidth)) - 1.0f;
		verts[i].y = 1.0f - (2.0f * quad[i].py / static_cast<float>(outHeight));
		verts[i].z = 0.0f;
		verts[i].a = ((quad[i].color >> 24) & 255) / 255.0f;
		verts[i].r = ((quad[i].color >> 16) & 255) / 255.0f;
		verts[i].g = ((quad[i].color >> 8) & 255) / 255.0f;
		verts[i].b = (quad[i].color & 255) / 255.0f;
		verts[i].u = quad[i].u;
		verts[i].v = quad[i].v;
		verts[i].q = 1.0f;
	}
	// Legacy TRIANGLESTRIP order (BR, TR, BL, TL) -> two triangles.
	const unsigned short indices[6] = { 0, 1, 2, 2, 1, 3 };
	return backend->Draw_Indexed_Material_Triangles(verts, 4, indices, 6,
		handle, material);
}

} // namespace
/*===========================================================================================*/
/*=========      Screen Shaders	=============================================================*/
/*===========================================================================================*/

class ScreenDefaultFilter : public W3DFilterInterface
{
public:
	virtual Int init() override;			///<perform any one time initialization and validation
	virtual Bool preRender(Bool &skipRender, CustomScenePassModes &scenePassMode) override; ///< Set up at start of render.  Only applies to screen filter shaders.
	virtual Bool postRender(FilterModes mode, Coord2D &scrollDelta,Bool &doExtraRender) override; ///< Called after render.  Only applies to screen filter shaders.
	virtual Bool setup(FilterModes mode) override {return true;} ///< Called when the filter is started, one time before the first prerender.
protected:
	virtual Int set(FilterModes mode) override;		///<setup shader for the specified rendering pass.
	virtual void reset() override;		///<do any custom resetting necessary to bring W3D in sync.
};

ScreenDefaultFilter screenDefaultFilter;

///Default filter that just renders screen to off-screen texture and then copies it the the screen.
///Useful because we added some full-time unit effects (microwave tank smudge) to Generals MD that need access
///to the background as a texture.  This filter makes that texture always available for these effects.
W3DFilterInterface *ScreenDefaultFilterList[]=
{
	&screenDefaultFilter,
	nullptr
};

Int ScreenDefaultFilter::init()
{
	if (!W3DShaderManager::canRenderToTexture()) {
		// Have to be able to render to texture.
		return FALSE;
	}

	//Can render to texture, but we don't know if it can read and write to the same texture.
	//Since there is no D3D caps bit to tell you this, we will just hard-code some specific
	//cards that we know should work.

	Int res;

	if ((res=W3DShaderManager::getChipset()) != DC_UNKNOWN)
	{
		if ( res >=	DC_GEFORCE2)
		{
			//Check if their driver is newer than what we tested for this vendor
/*			if (TheGameLODManager)
			{
				if (TheGameLODManager->getTestedDriverVersion(W3DShaderManager::getCurrentVendor()) < W3DShaderManager::getCurrentDriverVersion())
					return FALSE;
			}*/
		}
	}

	W3DFilters[FT_VIEW_DEFAULT]=&screenDefaultFilter;

	return TRUE;
}

Bool ScreenDefaultFilter::preRender(Bool &skipRender, CustomScenePassModes &scenePassMode)
{
	// TheSuperHackers @bugfix Disable Render To Texture redirection for the default filter
	// When MSAA is forced by Nvidia driver profile depth buffer is multisampled internally.
	// Rendering to non-MSAA texture with this depth buffer corrupts depth testing producing black screen
	// The smudge system has its own Copy path that works without Render To Texture.
	return FALSE;
}

Bool ScreenDefaultFilter::postRender(FilterModes mode, Coord2D &scrollDelta,Bool &doExtraRender)
{
	TextureClass *tex =	W3DShaderManager::endRenderToTexture();
	DEBUG_ASSERTCRASH(tex, ("Require rendered texture."));
	if (!tex) return false;
	if (!set(mode)) return false;

	Int xpos, ypos, width, height;

	TheTacticalView->getOrigin(&xpos,&ypos);
	width=TheTacticalView->getWidth();
	height=TheTacticalView->getHeight();

	// D3D12: XYZRHW DrawPrimitiveUP retired; screen_space material draw below
	// preserves corners (bottom-right, top-right, bottom-left, top-left) + UVs.
	ScreenSpaceFilterVertex v[4];
	//bottom right
	v[0].px = xpos+width-0.5f; v[0].py = ypos+height-0.5f;
	v[0].u = (Real)(xpos+width)/(Real)TheDisplay->getWidth();	v[0].v = (Real)(ypos+height)/(Real)TheDisplay->getHeight();
	//top right
	v[1].px = xpos+width-0.5f; v[1].py = ypos-0.5f;
	v[1].u = (Real)(xpos+width)/(Real)TheDisplay->getWidth();	v[1].v = (Real)(ypos)/(Real)TheDisplay->getHeight();
	//bottom left
	v[2].px = xpos-0.5f; v[2].py = ypos+height-0.5f;
	v[2].u = (Real)(xpos)/(Real)TheDisplay->getWidth();	v[2].v = (Real)(ypos+height)/(Real)TheDisplay->getHeight();
	//top left
	v[3].px = xpos-0.5f;  v[3].py = ypos-0.5f;
	v[3].u = (Real)(xpos)/(Real)TheDisplay->getWidth();	v[3].v = (Real)(ypos)/(Real)TheDisplay->getHeight();
	v[0].color = 0xffffffff;
	v[1].color = 0xffffffff;
	v[2].color = 0xffffffff;
	v[3].color = 0xffffffff;

	// Opaque copy of the previously rendered frame (ZFUNC ALWAYS, no z-write).
	ShaderClass copyShader = ShaderClass::_PresetOpaqueShader;
	RenderBackendMaterialState copyMaterial;
	if (!copyShader.Get_Render_Backend_State(copyMaterial))
		return false;
	const Bool submitted = Submit_Screen_Space_Filter_Quad(v, tex, copyMaterial);

	reset();
	return submitted ? true : false;
}

Int ScreenDefaultFilter::set(FilterModes mode)
{
	// D3D12: opaque copy material validated via Get_Render_Backend_State;
	// ZFUNC-ALWAYS/no-z-write preserved via the screen_space submit path.
	ShaderClass copyShader = ShaderClass::_PresetOpaqueShader;
	RenderBackendMaterialState copyMaterial;
	if (!copyShader.Get_Render_Backend_State(copyMaterial))
		return false;

	return true;
}

void ScreenDefaultFilter::reset()
{
	//previously rendered frame inside this texture
	if (IRenderBackend *resetBackend = WW3D::Get_Render_Backend())
		resetBackend->Invalidate_Cached_Render_States();
}

/*=========  ScreenBWFilter	=============================================================*/
///converts viewport to black & white.

Int ScreenBWFilter::m_fadeFrames;
Int ScreenBWFilter::m_curFadeFrame;
Real ScreenBWFilter::m_curFadeValue;
Int ScreenBWFilter::m_fadeDirection;

ScreenBWFilter screenBWFilter;
ScreenBWFilterDOT3 screenBWFilterDOT3;	//slower version for older cards without pixel shaders.

///List of different BW shader implementations in order of preference
W3DFilterInterface *ScreenBWFilterList[]=
{
	&screenBWFilter,
	&screenBWFilterDOT3,	//slower version for older cards without pixel shaders.
	nullptr
};

Int ScreenBWFilter::init()
{
	Int res;

	m_dwBWPixelShader = 0;
	m_curFadeFrame = 0;

	if (!W3DShaderManager::canRenderToTexture()) {
		// Have to be able to render to texture.
		return false;
	}

	if ((res=W3DShaderManager::getChipset()) != 0)
	{
		if (res >= DC_GENERIC_PIXEL_SHADER_1_1)
		{
			// D3D12: shaders\monochrome.pso is archival reference only; the
			// active path submits the base quad with the opaque material (the
			// monochrome pixel-shader tint has no backend equivalent yet).
			m_dwBWPixelShader = 0;

			W3DFilters[FT_VIEW_BW_FILTER]=&screenBWFilter;

			return TRUE;
		}
	}
	return FALSE;
}

Bool ScreenBWFilter::preRender(Bool &skipRender, CustomScenePassModes &scenePassMode)
{
	skipRender = false;
	W3DShaderManager::startRenderToTexture();
	return true;
}

Bool ScreenBWFilter::postRender(FilterModes mode, Coord2D &scrollDelta,Bool &doExtraRender)
{
	TextureClass *tex =	W3DShaderManager::endRenderToTexture();
	DEBUG_ASSERTCRASH(tex, ("Require rendered texture."));
	if (!tex) return false;
	if (!set(mode)) return false;

	Int xpos, ypos, width, height;

	TheTacticalView->getOrigin(&xpos,&ypos);
	width=TheTacticalView->getWidth();
	height=TheTacticalView->getHeight();

	// D3D12: XYZRHW DrawPrimitiveUP retired; screen_space material draw below
	// preserves corners + UVs. GAP: the monochrome pixel shader (luminance
	// weights + fade/tint constants from set()) has no backend equivalent, so
	// the base frame is submitted unmodified (effect documented, not faked).
	ScreenSpaceFilterVertex v[4];
	//bottom right
	v[0].px = xpos+width-0.5f; v[0].py = ypos+height-0.5f;
	v[0].u = (Real)(xpos+width)/(Real)TheDisplay->getWidth();	v[0].v = (Real)(ypos+height)/(Real)TheDisplay->getHeight();
	//top right
	v[1].px = xpos+width-0.5f; v[1].py = ypos-0.5f;
	v[1].u = (Real)(xpos+width)/(Real)TheDisplay->getWidth();	v[1].v = (Real)(ypos)/(Real)TheDisplay->getHeight();
	//bottom left
	v[2].px = xpos-0.5f; v[2].py = ypos+height-0.5f;
	v[2].u = (Real)(xpos)/(Real)TheDisplay->getWidth();	v[2].v = (Real)(ypos+height)/(Real)TheDisplay->getHeight();
	//top left
	v[3].px = xpos-0.5f;  v[3].py = ypos-0.5f;
	v[3].u = (Real)(xpos)/(Real)TheDisplay->getWidth();	v[3].v = (Real)(ypos)/(Real)TheDisplay->getHeight();
	v[0].color = 0xffffffff;
	v[1].color = 0xffffffff;
	v[2].color = 0xffffffff;
	v[3].color = 0xffffffff;

	ShaderClass copyShader = ShaderClass::_PresetOpaqueShader;
	RenderBackendMaterialState copyMaterial;
	if (!copyShader.Get_Render_Backend_State(copyMaterial))
		return false;
	const Bool submitted = Submit_Screen_Space_Filter_Quad(v, tex, copyMaterial);

	reset();
	return submitted ? true : false;
}

Int ScreenBWFilter::set(FilterModes mode)
{

	if (mode > FM_NULL_MODE)
	{	//rendering a quad with redirected rendering surface tinted by pixel shader

		if (m_fadeDirection > 0)
		{	//turning effect on
			m_curFadeFrame++;
			Int fade = m_curFadeFrame;

			if (fade<m_fadeFrames)
			{
				m_curFadeValue = (Real)fade/(Real)m_fadeFrames;
			}
			else
			{
				m_curFadeFrame = 0;
				m_curFadeValue = 1.0f;
				m_fadeDirection = 0;
			}
		}
		else
		if (m_fadeDirection < 0)
		{	//turning effect off
			m_curFadeFrame++;
			Int fade = m_curFadeFrame;
			if (fade<m_fadeFrames)
			{
				m_curFadeValue = 1.0f - (Real)fade/(Real)m_fadeFrames;
			}
			else
			{	m_curFadeValue = 0.0f;
				TheTacticalView->setViewFilterMode(FM_NULL_MODE);
				TheTacticalView->setViewFilter(FT_NULL_FILTER);
				m_curFadeFrame = 0;
				m_fadeDirection = 0;
			}
		}

		// D3D12: opaque material validated via Get_Render_Backend_State
		// (ZFUNC-ALWAYS/no-z-write preserved via the screen_space submit).
		ShaderClass copyShader = ShaderClass::_PresetOpaqueShader;
		RenderBackendMaterialState copyMaterial;
		if (!copyShader.Get_Render_Backend_State(copyMaterial))
			return false;

		// GAP: monochrome.pso constants below are preserved CPU-side only.
		// c0 luminance weights (0.3, 0.59, 0.11).
		const LegacyPixelShaderConstant luminance = { 0, Vector4(0.3f, 0.59f, 0.11f, 1.0f) };
		(void)luminance;

		Vector4	color(1.0f,1.0f,1.0f,1.0f);	//multiply color

		if (mode == FM_VIEW_BW_BLACK_AND_WHITE)
		{	//back & white mode
			color.X=1.0f;
			color.Y=1.0f;
			color.Z=1.0f;
		}
		if (mode == FM_VIEW_BW_RED_AND_WHITE)
		{	//red is on
			color.X = 1.0f;
			color.Y = 0.0f;
			color.Z = 0.0f;
			//inverse red is on
			//red is on
//			color.X = 0.0f;
//			color.Y = 1.0f;
//			color.Z = 1.0f;
		}
		if (mode == FM_VIEW_BW_GREEN_AND_WHITE)
		{
			color.X = 0.0f;
			color.Y = 1.0f;
			color.Z = 0.0f;
		}

		// c1 tint color, c2 fade value (preserved, not uploaded).
		const LegacyPixelShaderConstant tint = { 1, color };
		const LegacyPixelShaderConstant fadeValue = { 2, Vector4(m_curFadeValue, m_curFadeValue, m_curFadeValue, 1.0f) };
		(void)tint;
		(void)fadeValue;
/*		Set_Legacy_Pixel_Shader_Constant(2, Vector4(150.0f/255.0f, 150.0f/255.0f, 150.0f/255.0f, 0.0f));
		Set_Legacy_Pixel_Shader_Constant(3, Vector4((765.0f/450.0f)/3, (765.0f/450.0f)/3, (765.0f/450.0f)/3, 1.0f));
		Set_Legacy_Pixel_Shader_Constant(4, Vector4(0.5f, 0.5f, 0.5f, 0.0f));
		Set_Legacy_Pixel_Shader_Constant(5, Vector4((60.0f)/255.0f, (60.0f)/255.0f, (60.0f)/255.0f, 0.0f));
		Set_Legacy_Pixel_Shader_Constant(6, Vector4((157.0f)/255.0f, (157.0f)/255.0f, (157.0f)/255.0f, 0.0f));
		Set_Legacy_Pixel_Shader_Constant(7, Vector4((30.0f)/255.0f, (30.0f)/255.0f, (30.0f)/255.0f, 0.0f));
*/
		return true;
	}
	return false;
}

void ScreenBWFilter::reset()
{
	//previously rendered frame inside this texture
	if (IRenderBackend *resetBackend = WW3D::Get_Render_Backend())
		resetBackend->Invalidate_Cached_Render_States();
}

Int ScreenBWFilter::shutdown()
{
	// D3D12: monochrome.pso handle retired (archival reference only).
	m_dwBWPixelShader=0;

	return TRUE;
}

/**Alternate version of the above filter which does not require pixel shaders - good for older cards*/
Int ScreenBWFilterDOT3::init()
{
	Int res;

	m_curFadeFrame = 0;

	if (!W3DShaderManager::canRenderToTexture()) {
		// Have to be able to render to texture.
		return false;
	}

	if ((res=W3DShaderManager::getChipset()) != 0)
	{
			W3DFilters[FT_VIEW_BW_FILTER]=&screenBWFilterDOT3;
			return TRUE;
	}
	return FALSE;
}

Bool ScreenBWFilterDOT3::preRender(Bool &skipRender, CustomScenePassModes &scenePassMode)
{
	skipRender = false;
	W3DShaderManager::startRenderToTexture();
	return true;
}

Bool ScreenBWFilterDOT3::postRender(FilterModes mode, Coord2D &scrollDelta,Bool &doExtraRender)
{
	TextureClass *tex =	W3DShaderManager::endRenderToTexture();
	DEBUG_ASSERTCRASH(tex, ("Require rendered texture."));
	if (!tex) return false;
	if (!set(mode)) return false;

	Int xpos, ypos, width, height;

	TheTacticalView->getOrigin(&xpos,&ypos);
	width=TheTacticalView->getWidth();
	height=TheTacticalView->getHeight();

	//bottom right
	ScreenSpaceFilterVertex v[4];
	v[0].px = xpos+width-0.5f; v[0].py = ypos+height-0.5f;
	v[0].u = (Real)(xpos+width)/(Real)TheDisplay->getWidth();	v[0].v = (Real)(ypos+height)/(Real)TheDisplay->getHeight();
	//top right
	v[1].px = xpos+width-0.5f; v[1].py = ypos-0.5f;
	v[1].u = (Real)(xpos+width)/(Real)TheDisplay->getWidth();	v[1].v = (Real)(ypos)/(Real)TheDisplay->getHeight();
	//bottom left
	v[2].px = xpos-0.5f; v[2].py = ypos+height-0.5f;
	v[2].u = (Real)(xpos)/(Real)TheDisplay->getWidth();	v[2].v = (Real)(ypos+height)/(Real)TheDisplay->getHeight();
	//top left
	v[3].px = xpos-0.5f;  v[3].py = ypos-0.5f;
	v[3].u = (Real)(xpos)/(Real)TheDisplay->getWidth();	v[3].v = (Real)(ypos)/(Real)TheDisplay->getHeight();

	unsigned int currentFade=(((Int)((1.0f-m_curFadeValue) * 255.0f))<<24) | 0x00ffffff;	//store alpha value

	v[0].color = currentFade;
	v[1].color = currentFade;
	v[2].color = currentFade;
	v[3].color = currentFade;

	// D3D12: DOT3 MULTIPLYADD/DOTPRODUCT3 grayscale has no backend
	// texture_combine equivalent, so the MODULATE fallback is preserved via
	// texture_combine (see Submit helper). The TFACTOR constant below is
	// preserved CPU-side for documentation.
	const unsigned int dot3TextureFactor = 0x80A5CA8E;
	(void)dot3TextureFactor;
	const unsigned int modulateTextureFactor = 0x60606060;
	(void)modulateTextureFactor;

	//Draw B&W version first (MODULATE fallback via texture_combine).
	ShaderClass grayShader = ShaderClass::_PresetOpaqueShader;
	RenderBackendMaterialState grayMaterial;
	if (!grayShader.Get_Render_Backend_State(grayMaterial))
		return false;
	grayMaterial.texture_combine = RenderBackendTextureCombine::Modulate;
	if (!Submit_Screen_Space_Filter_Quad(v, tex, grayMaterial))
		return false;

	//Draw normal view blended by current fade level
	ShaderClass blendShader=ShaderClass::_PresetAlphaShader;
	blendShader.Set_Depth_Compare(ShaderClass::PASS_ALWAYS);
	RenderBackendMaterialState blendMaterial;
	if (!blendShader.Get_Render_Backend_State(blendMaterial))
		return false;
	//replace texture alpha with vertex alpha (preserved via vertex colors).
	if (!Submit_Screen_Space_Filter_Quad(v, tex, blendMaterial))
		return false;

	reset();
	return true;
}

Int ScreenBWFilterDOT3::set(FilterModes mode)
{
	if (mode > FM_NULL_MODE)
	{	//rendering a quad with redirected rendering surface tinted by pixel shader

		if (m_fadeDirection > 0)
		{	//turning effect on
			m_curFadeFrame++;
			Int fade = m_curFadeFrame;

			if (fade<m_fadeFrames)
			{
				m_curFadeValue = (Real)fade/(Real)m_fadeFrames;
			}
			else
			{
				m_curFadeFrame = 0;
				m_curFadeValue = 1.0f;
				m_fadeDirection = 0;
			}
		}
		else
		if (m_fadeDirection < 0)
		{	//turning effect off
			m_curFadeFrame++;
			Int fade = m_curFadeFrame;
			if (fade<m_fadeFrames)
			{
				m_curFadeValue = 1.0f - (Real)fade/(Real)m_fadeFrames;
			}
			else
			{	m_curFadeValue = 0.0f;
				TheTacticalView->setViewFilterMode(FM_NULL_MODE);
				TheTacticalView->setViewFilter(FT_NULL_FILTER);
				m_curFadeFrame = 0;
				m_fadeDirection = 0;
			}
		}

		// D3D12: opaque material validated via Get_Render_Backend_State
		// (ZFUNC-ALWAYS/no-z-write preserved via the screen_space submit).
		ShaderClass copyShader = ShaderClass::_PresetOpaqueShader;
		RenderBackendMaterialState copyMaterial;
		if (!copyShader.Get_Render_Backend_State(copyMaterial))
			return false;

		return true;
	}
	return false;
}

void ScreenBWFilterDOT3::reset()
{
	//previously rendered frame inside this texture
	if (IRenderBackend *resetBackend = WW3D::Get_Render_Backend())
		resetBackend->Invalidate_Cached_Render_States();
}

Int ScreenBWFilterDOT3::shutdown()
{
	return TRUE;
}

/*=========  ScreenCrossFadeFilter	=============================================================*/
///Fades screen between 2 different views of the scene with both being visible at once.

Int ScreenCrossFadeFilter::m_fadeFrames;
Int ScreenCrossFadeFilter::m_curFadeFrame;
Real ScreenCrossFadeFilter::m_curFadeValue;
Int ScreenCrossFadeFilter::m_fadeDirection;
TextureClass *ScreenCrossFadeFilter::m_fadePatternTexture=nullptr;
Bool ScreenCrossFadeFilter::m_skipRender = FALSE;

ScreenCrossFadeFilter screenCrossFadeFilter;

///List of different BW shader implementations in order of preference
///@todo: Add a version that doesn't require pixel shader
W3DFilterInterface *ScreenCrossFadeFilterList[]=
{
	&screenCrossFadeFilter,
	nullptr
};

Int ScreenCrossFadeFilter::init()
{
	if (!TheDisplay)
		return FALSE;	//effect is useless without a view so no point initializing for the WB, etc.

	m_curFadeFrame = 0;

	if (!W3DShaderManager::canRenderToTexture())
		// Have to be able to render to texture.
		return FALSE;

	//Load an alpha mask texture that will mix foreground/background views.
	m_fadePatternTexture=WW3DAssetManager::Get_Instance()->Get_Texture("exmask_g.tga");
	if (!m_fadePatternTexture)
		return FALSE;
	m_fadePatternTexture->Get_Filter().Set_U_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_CLAMP);
	m_fadePatternTexture->Get_Filter().Set_V_Addr_Mode(TextureFilterClass::TEXTURE_ADDRESS_CLAMP);
	m_fadePatternTexture->Get_Filter().Set_Mip_Mapping(TextureFilterClass::FILTER_TYPE_NONE);

	W3DFilters[FT_VIEW_CROSSFADE]=&screenCrossFadeFilter;

	return TRUE;
}

Bool ScreenCrossFadeFilter::updateFadeLevel()
{
	if (m_fadeDirection > 0)
	{	//turning effect on
		m_curFadeFrame++;
		Int fade = m_curFadeFrame;

		if (fade<m_fadeFrames)
		{
			m_curFadeValue = (Real)fade/(Real)m_fadeFrames;
		}
		else
		{
			m_curFadeFrame = 0;
			m_curFadeValue = 1.0f;
			m_fadeDirection = 0;
			return false;
		}
	}
	else
	if (m_fadeDirection < 0)
	{	//turning effect off
		Int fade = m_curFadeFrame;
		if (fade<m_fadeFrames)
		{
			m_curFadeValue = 1.0f - (Real)fade/(Real)m_fadeFrames;
			m_curFadeFrame++;
		}
		else
		{	m_curFadeValue = 0.0f;
			TheTacticalView->setViewFilterMode(FM_NULL_MODE);
			TheTacticalView->setViewFilter(FT_NULL_FILTER);
			m_curFadeFrame = 0;
			m_fadeDirection = 0;
			return false;
		}
	}
	return true;
}

Bool ScreenCrossFadeFilter::preRender(Bool &skipRender, CustomScenePassModes &scenePassMode)
{
	if (updateFadeLevel())
	{	//if fade has not completed
		W3DShaderManager::startRenderToTexture();
		scenePassMode=SCENE_PASS_ALPHA_MASK;
		skipRender = false;
		m_skipRender=true;	//tell the postRender function not to draw into framebuffer yet.
		return true;
	}
	//fade must have completed
	return true;
}

Bool ScreenCrossFadeFilter::postRender(FilterModes mode, Coord2D &scrollDelta,Bool &doExtraRender)
{
	TextureClass *tex;

	if (m_skipRender)
	{
		//don't render anything to frame buffer because we still need to draw the new scene
		//that we're fading into.  Okay to render on the next call.
		m_skipRender = false;
		doExtraRender = TRUE;
		tex =	W3DShaderManager::endRenderToTexture();
		return true;
	}

	tex=W3DShaderManager::getRenderTexture();

	DEBUG_ASSERTCRASH(tex, ("Require last rendered texture."));
	if (!tex) return false;
	if (!set(mode)) return false;

	Int xpos, ypos, width, height;
	Real radius = 0.0f;

	// D3D12: circle-mask radius math preserved CPU-side, but the stage-1 mask
	// texture (second UV set) has no single-texture material equivalent yet.
	// The frame quad below is submitted with frame UVs only (mask documented,
	// not faked).
	if (mode == FM_VIEW_CROSSFADE_CIRCLE)
	{	//Use the current fade level to scale the mask texture, for other modes the texture
		//comes pre-scaled so doesn't require uv scaling.
		radius = (1.0f-m_curFadeValue)*2.0f;
		if (radius <= 0)
			radius = 0.01f;
		radius = 0.5f/radius;
	}
	(void)radius;

	TheTacticalView->getOrigin(&xpos,&ypos);
	width=TheTacticalView->getWidth();
	height=TheTacticalView->getHeight();

	//bottom right
	ScreenSpaceFilterVertex v[4];
	v[0].px = xpos+width-0.5f; v[0].py = ypos+height-0.5f;
	v[0].u = (Real)(xpos+width)/(Real)TheDisplay->getWidth();	v[0].v = (Real)(ypos+height)/(Real)TheDisplay->getHeight();
	//top right
	v[1].px = xpos+width-0.5f; v[1].py = ypos-0.5f;
	v[1].u = (Real)(xpos+width)/(Real)TheDisplay->getWidth();	v[1].v = (Real)(ypos)/(Real)TheDisplay->getHeight();
	//bottom left
	v[2].px = xpos-0.5f; v[2].py = ypos+height-0.5f;
	v[2].u = (Real)(xpos)/(Real)TheDisplay->getWidth();	v[2].v = (Real)(ypos+height)/(Real)TheDisplay->getHeight();
	//top left
	v[3].px = xpos-0.5f;  v[3].py = ypos-0.5f;
	v[3].u = (Real)(xpos)/(Real)TheDisplay->getWidth();	v[3].v = (Real)(ypos)/(Real)TheDisplay->getHeight();

	unsigned int diffuse = 0xffffffff;//((Int)((m_curFadeValue) * 255.0f) << 24) | 0x00ffffff;	//store alpha value in vertex diffuse

	v[0].color = diffuse;
	v[1].color = diffuse;
	v[2].color = diffuse;
	v[3].color = diffuse;

	ShaderClass copyShader = ShaderClass::_PresetAlphaShader;
	RenderBackendMaterialState copyMaterial;
	if (!copyShader.Get_Render_Backend_State(copyMaterial))
		return false;
	const Bool submitted = Submit_Screen_Space_Filter_Quad(v, tex, copyMaterial);

	reset();
	return submitted ? true : false;
}

Int ScreenCrossFadeFilter::set(FilterModes mode)
{
	if (mode > FM_NULL_MODE)
	{	//rendering a quad with redirected rendering surface
		// D3D12: alpha material validated via Get_Render_Backend_State; clamp
		// addressing + ZFUNC-ALWAYS/no-z-write preserved via screen_space
		// submit. The stage-1 circle-mask MODULATE has no backend equivalent
		// (documented above).
		ShaderClass copyShader = ShaderClass::_PresetAlphaShader;
		RenderBackendMaterialState copyMaterial;
		if (!copyShader.Get_Render_Backend_State(copyMaterial))
			return false;

		return true;
	}
	return false;
}

void ScreenCrossFadeFilter::reset()
{
	if (IRenderBackend *resetBackend = WW3D::Get_Render_Backend())
		resetBackend->Invalidate_Cached_Render_States();
}

Int ScreenCrossFadeFilter::shutdown()
{
	REF_PTR_RELEASE(m_fadePatternTexture);

	return TRUE;
}

/*=========  ScreenMotionBlurFilter	=============================================================*/
///applies motion blur to viewport.

ScreenMotionBlurFilter screenMotionBlurFilter;

Coord3D ScreenMotionBlurFilter::m_zoomToPos;
Bool ScreenMotionBlurFilter::m_zoomToValid = false;

ScreenMotionBlurFilter::ScreenMotionBlurFilter():
m_decrement(false),
m_maxCount(0),
m_lastFrame(0),
m_skipRender(false)
{
}
///List of different motion blur implementations in order of preference
W3DFilterInterface *ScreenMotionBlurFilterList[]=
{
	&screenMotionBlurFilter,
	nullptr
};

Int ScreenMotionBlurFilter::init()
{
	if (!W3DShaderManager::canRenderToTexture()) {
		// Have to be able to render to texture.
		return false;
	}
	W3DFilters[FT_VIEW_MOTION_BLUR_FILTER]=this;
	return true;
}

Bool ScreenMotionBlurFilter::preRender(Bool &skipRender, CustomScenePassModes &scenePassMode)
{
	skipRender = m_skipRender;
	W3DShaderManager::startRenderToTexture();
	return true;
}

Bool ScreenMotionBlurFilter::postRender(FilterModes mode, Coord2D &scrollDelta,Bool &doExtraRender)
{
	TextureClass *tex =	W3DShaderManager::endRenderToTexture();
	DEBUG_ASSERTCRASH(tex, ("Require rendered texture."));
	if (!tex) return false;
	if (!set(mode)) return false;

	// D3D12: XYZRHW DrawPrimitiveUP retired; each legacy draw below becomes a
	// screen_space material draw preserving UVs/colors. Zoom/pan state machine
	// preserved CPU-side. Blend intent (additive vs alpha) preserved via
	// Get_Render_Backend_State on the matching presets.
	ShaderClass motionOpaque = ShaderClass::_PresetOpaqueShader;
	ShaderClass motionBlend = m_additive
		? ShaderClass::_PresetAdditiveShader : ShaderClass::_PresetAlphaShader;
	RenderBackendMaterialState motionOpaqueMaterial, motionBlendMaterial;
	if (!motionOpaque.Get_Render_Backend_State(motionOpaqueMaterial))
		return false;
	if (!motionBlend.Get_Render_Backend_State(motionBlendMaterial))
		return false;

	Bool continueEffect = true;
	ScreenSpaceFilterVertex v[4];

	Int xpos, ypos, width, height;

	TheTacticalView->getOrigin(&xpos,&ypos);
	width=TheTacticalView->getWidth();
	height=TheTacticalView->getHeight();

	//bottom right
	v[0].px = xpos+width-0.5f; v[0].py = ypos+height-0.5f;
	v[0].u = (Real)(xpos+width)/(Real)TheDisplay->getWidth();	v[0].v = (Real)(ypos+height)/(Real)TheDisplay->getHeight();
	//top right
	v[1].px = xpos+width-0.5f; v[1].py = ypos-0.5f;
	v[1].u = (Real)(xpos+width)/(Real)TheDisplay->getWidth();	v[1].v = (Real)(ypos)/(Real)TheDisplay->getHeight();
	//bottom left
	v[2].px = xpos-0.5f; v[2].py = ypos+height-0.5f;
	v[2].u = (Real)(xpos)/(Real)TheDisplay->getWidth();	v[2].v = (Real)(ypos+height)/(Real)TheDisplay->getHeight();
	//top left
	v[3].px = xpos-0.5f;  v[3].py = ypos-0.5f;
	v[3].u = (Real)(xpos)/(Real)TheDisplay->getWidth();	v[3].v = (Real)(ypos)/(Real)TheDisplay->getHeight();
	v[0].color = 0xffffffff;
	v[1].color = 0xffffffff;
	v[2].color = 0xffffffff;
	v[3].color = 0xffffffff;


	// Base frame: legacy drew with ALPHABLENDENABLE false (opaque copy).
	// (Additive vs alpha selection below applies to the accumulation loop.)

	Coord2D center;
	center.x = 0.5f;
	center.y = 0.5f;
	Bool pan = false;
	if (mode>=FM_VIEW_MB_PAN_ALPHA) {
		Real len = sqrt(scrollDelta.x*scrollDelta.x + scrollDelta.y*scrollDelta.y);
		//center.x += 0.5f * (scrollDelta.x/len);
		center.y -= 0.5f; // * (scrollDelta.y/len);
		m_decrement = false;
		m_maxCount = (len*200*m_panFactor/(Real)DEFAULT_PAN_FACTOR);
		if (m_maxCount<m_panFactor/2)
			m_maxCount = m_panFactor/2;
		if (m_maxCount>m_panFactor)
			m_maxCount=m_panFactor;
		pan = true;
		m_priorDelta = scrollDelta;
	} else if (mode == FM_VIEW_MB_END_PAN_ALPHA) {
		Real len = sqrt(m_priorDelta.x*m_priorDelta.x + m_priorDelta.y*m_priorDelta.y);
		center.x += 0.5f * (m_priorDelta.x/len);
		center.y -= 0.5f * (m_priorDelta.y/len);
		m_decrement = false;
		m_maxCount--;
		if (m_maxCount<2) {
			continueEffect = false;
		}
		pan = true;
	}


	m_skipRender = false;
	if (!pan && m_lastFrame != TheGameLogic->getFrame()) {
		if (m_decrement) {
			m_maxCount-=COUNT_STEP;
			if (m_maxCount<1) {
				m_decrement = false;
				continueEffect = false;
			}	else {
				m_skipRender = true;
			}
		} else {
			m_maxCount+=COUNT_STEP;
			if (m_maxCount>=MAX_COUNT) {
				m_decrement = true;
				if (m_doZoomTo && m_zoomToValid) {
					TheTacticalView->lookAt(&m_zoomToPos);
				} else {
					continueEffect = false;
				}
			}	else {
				m_skipRender = true;
			}
		}
	}
	Int	 i, j;
	if (!pan) {
		for (i=0; i<4; i++) {
			Real factor = 1.0f - (m_maxCount/(Real)MAX_COUNT)*0.90f;
			factor = sqrt(factor);
			v[i].u = ((v[i].u-center.x)*factor) + center.x;
			v[i].v = ((v[i].v-center.y)*factor) + center.y;
		}
	}
	// Base frame: opaque copy (legacy ALPHABLENDENABLE false). Vertex alpha
	// selection (SELECTARG1) preserved via vertex colors in the submit path.
	if (!Submit_Screen_Space_Filter_Quad(v, tex, motionOpaqueMaterial))
		return false;
	{
		Int limit = m_maxCount;
		if (m_maxCount>30) limit = 30;
		for (j=0; j<limit; j++) {
			for (i=0; i<4; i++) {
				Real factor = 0.99f;
				if (m_additive) factor = 0.98f;
				Int alpha = 0x15;
				if (m_additive) {
					alpha = 0x09;
					if (m_maxCount>limit) {
						alpha += (m_maxCount-limit)/5;
					}
					if (m_maxCount==MAX_COUNT) alpha += 60;
				}
				v[i].color = (alpha<<24)|0x00ffffff; //
				if (pan) {
					v[i].u = ((v[i].u-center.x)*(factor+.006)) + center.x;
					v[i].v = ((v[i].v-center.y)*factor) + center.y;
				} else {
					v[i].u = ((v[i].u-center.x)*factor) + center.x;
					v[i].v = ((v[i].v-center.y)*factor) + center.y;
				}
			}
			// Accumulation pass: additive vs alpha blend preserved via the
			// preset material selected above.
			if (!Submit_Screen_Space_Filter_Quad(v, tex, motionBlendMaterial))
				return false;

		}
	}
	m_lastFrame = TheGameLogic->getFrame();
	if (pan){
		m_skipRender = false;
	}
	reset();
	if (!continueEffect) {
		m_zoomToValid = false;
	}
	return continueEffect;
}

Bool ScreenMotionBlurFilter::setup(FilterModes mode)
{

	m_additive = false;

	if (mode == FM_VIEW_MB_IN_AND_OUT_SATURATE ||
			mode == FM_VIEW_MB_IN_SATURATE ||
			mode == FM_VIEW_MB_OUT_SATURATE) {
		m_additive = true;
	}

	m_doZoomTo = false;
	if (mode == FM_VIEW_MB_IN_AND_OUT_SATURATE ||
			mode == FM_VIEW_MB_IN_AND_OUT_ALPHA ) {
		m_doZoomTo = true;
	}
	if (mode >= FM_VIEW_MB_PAN_ALPHA)	{
		m_panFactor = (int)mode - FM_VIEW_MB_PAN_ALPHA;
		if (m_panFactor<1) m_panFactor = DEFAULT_PAN_FACTOR;
	}
	m_skipRender = false;
	if (mode != FM_VIEW_MB_END_PAN_ALPHA)
		m_maxCount = 0;
	m_decrement = false;
	m_skipRender = false;
	switch (mode) {
		case FM_VIEW_MB_OUT_SATURATE:
		case FM_VIEW_MB_OUT_ALPHA:
			m_maxCount = MAX_COUNT;
			m_decrement = TRUE;
			break;
	}
	return true;
}

Int ScreenMotionBlurFilter::set(FilterModes mode)
{
	if (mode > FM_NULL_MODE)
	{	//rendering a quad with redirected rendering surface motion blurred

		// D3D12: opaque material validated via Get_Render_Backend_State
		// (ZFUNC-ALWAYS/no-z-write preserved via the screen_space submit).
		ShaderClass copyShader = ShaderClass::_PresetOpaqueShader;
		RenderBackendMaterialState copyMaterial;
		if (!copyShader.Get_Render_Backend_State(copyMaterial))
			return FALSE;
	}
	return TRUE;
}

void ScreenMotionBlurFilter::reset()
{
	//previously rendered frame inside this texture
	if (IRenderBackend *resetBackend = WW3D::Get_Render_Backend())
		resetBackend->Invalidate_Cached_Render_States();
}

Int ScreenMotionBlurFilter::shutdown()
{
	return TRUE;
}

/*===========================================================================================*/
/*=========      Shroud Shaders	=============================================================*/
/*===========================================================================================*/

///Shroud layer rendering shader
class ShroudTextureShader : public W3DShaderInterface
{
	virtual Int set(Int pass) override;		///<setup shader for the specified rendering pass.
	virtual Int init() override;			///<perform any one time initialization and validation
	virtual void reset() override;		///<do any custom resetting necessary to bring W3D in sync.
	Int m_stageOfSet;
} shroudTextureShader;

///List of different shroud shader implementations in order of preference
W3DShaderInterface *ShroudShaderList[]=
{
	&shroudTextureShader,
	nullptr
};

//#define SHROUD_STRETCH_FACTOR	(1.0f/MAP_XY_FACTOR)	//1 texel per heightmap cell width

Int ShroudTextureShader::init()
{
	W3DShaders[W3DShaderManager::ST_SHROUD_TEXTURE]=&shroudTextureShader;
	W3DShadersPassCount[W3DShaderManager::ST_SHROUD_TEXTURE]=1;

	return TRUE;
}

//Setup a texture projection in the given stage that applies our shroud.
Int ShroudTextureShader::set(Int stage)
{
	// D3D12: shroud sprite material translated via Get_Render_Backend_State.
	// Textures/samplers preserved from m_Textures; the camera-space projection
	// (TEXCOORDINDEX CAMERASPACEPOSITION + texture transform) has no backend
	// equivalent yet, so the cell scale/offset math below is preserved
	// CPU-side for future projection-aware callers (not uploaded, not faked).
#if defined(RTS_DEBUG)
	if (TheGlobalData && TheGlobalData->m_fogOfWarOn) {
		RenderBackendMaterialState translatedSprite;
		if (!ShaderClass::_PresetAlphaSpriteShader.Get_Render_Backend_State(translatedSprite))
			return FALSE;
		storeBackendState(translatedSprite);
	} else {
		RenderBackendMaterialState translatedSprite;
		if (!ShaderClass::_PresetMultiplicativeSpriteShader.Get_Render_Backend_State(translatedSprite))
			return FALSE;
		storeBackendState(translatedSprite);
	}
#else
	{
		RenderBackendMaterialState translatedSprite;
		if (!ShaderClass::_PresetMultiplicativeSpriteShader.Get_Render_Backend_State(translatedSprite))
			return FALSE;
		storeBackendState(translatedSprite);
	}
#endif

	//We need to scale so shroud texel stretches over one full terrain cell.  Each texel
	//is 1/128 the size of full texture. (assuming 128x128 vid-mem texture).
	W3DShroud *shroud;
	if (TheTerrainRenderObject != nullptr && (shroud=TheTerrainRenderObject->getShroud()) != nullptr)
	{	///@todo: All this code really only need to be done once per camera/view.  Find a way to optimize it out.
		Matrix4x4 viewIdentity(true);
		Matrix4x4 inv;
		float det = 0.0f;
		Invert_Legacy_Shader_Matrix(inv, det, viewIdentity);

		Matrix4x4 scale,offset;

		//We need to make all world coordinates be relative to the heightmap data origin since that
		//is where the shroud begins.

		float xoffset = 0;
		float yoffset = 0;
		Real width=shroud->getCellWidth();
		Real height=shroud->getCellHeight();

		if (TheTerrainRenderObject->getMap())
		{	//subtract origin position from all coordinates.  Origin is shifted by 1 cell width/height to allow for unused border texels.
			xoffset = -(float)shroud->getDrawOriginX() + width;
			yoffset = -(float)shroud->getDrawOriginY() + height;
		}

		Make_Legacy_Shader_Translation(offset, xoffset, yoffset, 0.0f);

		width = 1.0f/(width*shroud->getTextureWidth());
		height = 1.0f/(height*shroud->getTextureHeight());
		Make_Legacy_Shader_Scaling(scale, width, height, 1.0f);
		const Matrix4x4 projected = Multiply_Legacy_Shader_Matrices(Multiply_Legacy_Shader_Matrices(inv, offset), scale);
		(void)projected; // GAP: texture-transform upload retired (see note above).
	}
	m_stageOfSet=stage;
	return TRUE;
}

void ShroudTextureShader::reset()
{
	clearBackendState();
	ShaderClass::Invalidate();
	if (IRenderBackend *resetBackend = WW3D::Get_Render_Backend())
		resetBackend->Invalidate_Cached_Render_States();
}

///Shroud layer rendering shader
class FlatShroudTextureShader : public W3DShaderInterface
{
	virtual Int set(Int pass) override;		///<setup shader for the specified rendering pass.
	virtual Int init() override;			///<perform any one time initialization and validation
	virtual void reset() override;		///<do any custom resetting necessary to bring W3D in sync.
	Int m_stageOfSet;
} flatShroudTextureShader;

///List of different shroud shader implementations in order of preference
W3DShaderInterface *FlatShroudShaderList[]=
{
	&flatShroudTextureShader,
	nullptr
};

//#define SHROUD_STRETCH_FACTOR	(1.0f/MAP_XY_FACTOR)	//1 texel per heightmap cell width

Int FlatShroudTextureShader::init()
{
	W3DShaders[W3DShaderManager::ST_FLAT_SHROUD_TEXTURE]=&flatShroudTextureShader;
	W3DShadersPassCount[W3DShaderManager::ST_FLAT_SHROUD_TEXTURE]=1;

	return TRUE;
}

//Setup a texture projection in the given stage that applies our shroud.
Int FlatShroudTextureShader::set(Int stage)
{
	// D3D12: stage MODULATE intent translated via Get_Render_Backend_State
	// (texture_combine MODULATE). Caller textures/samplers preserved from
	// m_Textures; the camera-space projection has no backend equivalent yet,
	// so the cell scale/offset math below is preserved CPU-side (not uploaded).
	{
		RenderBackendMaterialState translatedFlat;
		ShaderClass flatShader = ShaderClass::_PresetOpaqueShader;
		if (!flatShader.Get_Render_Backend_State(translatedFlat))
			return FALSE;
		translatedFlat.texture_combine = RenderBackendTextureCombine::Modulate;
		storeBackendState(translatedFlat);
	}

	//We need to scale so shroud texel stretches over one full terrain cell.  Each texel
	//is 1/128 the size of full texture. (assuming 128x128 vid-mem texture).
	W3DShroud *shroud;
	if (TheTerrainRenderObject != nullptr && (shroud=TheTerrainRenderObject->getShroud()) != nullptr)
	{	///@todo: All this code really only need to be done once per camera/view.  Find a way to optimize it out.
		Matrix4x4 viewIdentity(true);
		Matrix4x4 inv;
		float det = 0.0f;
		Invert_Legacy_Shader_Matrix(inv, det, viewIdentity);

		Matrix4x4 scale,offset;

		//We need to make all world coordinates be relative to the heightmap data origin since that
		//is where the shroud begins.

		float xoffset = 0;
		float yoffset = 0;
		Real width=shroud->getCellWidth();
		Real height=shroud->getCellHeight();

		if (TheTerrainRenderObject->getMap())
		{	//subtract origin position from all coordinates.  Origin is shifted by 1 cell width/height to allow for unused border texels.
			xoffset = -(float)shroud->getDrawOriginX() + width;
			yoffset = -(float)shroud->getDrawOriginY() + height;
		}

		Make_Legacy_Shader_Translation(offset, xoffset, yoffset, 0.0f);

		width = 1.0f/(width*shroud->getTextureWidth());
		height = 1.0f/(height*shroud->getTextureHeight());
		Make_Legacy_Shader_Scaling(scale, width, height, 1.0f);
		const Matrix4x4 projected = Multiply_Legacy_Shader_Matrices(Multiply_Legacy_Shader_Matrices(inv, offset), scale);
		(void)projected; // GAP: texture-transform upload retired (see note above).
	}
	m_stageOfSet=stage;
	return TRUE;
}

void FlatShroudTextureShader::reset()
{
	clearBackendState();
	ShaderClass::Invalidate();
	if (IRenderBackend *resetBackend = WW3D::Get_Render_Backend())
		resetBackend->Invalidate_Cached_Render_States();
}

///Mask layer rendering shader
class MaskTextureShader : public W3DShaderInterface
{
	virtual Int set(Int pass) override;		///<setup shader for the specified rendering pass.
	virtual Int init() override;			///<perform any one time initialization and validation
	virtual void reset() override;		///<do any custom resetting necessary to bring W3D in sync.
} maskTextureShader;

///List of different shroud shader implementations in order of preference
W3DShaderInterface *MaskShaderList[]=
{
	&maskTextureShader,
	nullptr
};

Int MaskTextureShader::init()
{
	W3DShaders[W3DShaderManager::ST_MASK_TEXTURE]=&maskTextureShader;
	W3DShadersPassCount[W3DShaderManager::ST_MASK_TEXTURE]=1;

	return TRUE;
}

Int MaskTextureShader::set(Int pass)
{
	(void)pass;
	Real fadeLevel=ScreenCrossFadeFilter::getCurrentFadeValue();

	//Use the current fade level to scale the mask texture
	Real radius = (1.0f-fadeLevel)*2.0f;
	if (radius <= 0)
		radius = 0.01f;
	radius = 0.5f/radius;

	// D3D12: opaque unmodulated (GRADIENT_DISABLE) intent translated via
	// Get_Render_Backend_State. The camera-space mask projection below is
	// preserved CPU-side (radius math intact) but not uploaded: no backend
	// texture-projection equivalent yet (documented, not faked).
	ShaderClass shader=ShaderClass::_PresetOpaqueShader;
	shader.Set_Primary_Gradient(ShaderClass::GRADIENT_DISABLE);
	RenderBackendMaterialState translatedMask;
	if (!shader.Get_Render_Backend_State(translatedMask))
		return FALSE;
	storeBackendState(translatedMask);

	Matrix4x4 viewIdentity(true);

	Matrix4x4 inv;
	float det = 0.0f;

	//Get inverse view matrix so we can transform camera space points back to world space
	Invert_Legacy_Shader_Matrix(inv, det, viewIdentity);

	Matrix4x4 scale,offset,offsetTextureCenter;
	Coord3D centerPos;
	centerPos.zero();

	//Find center of projection (this should be returned from some other filter, etc. but
	//for now assume terrain location at center of screen.
	if (TheTacticalView)
	{	Int xpos,ypos;

		TheTacticalView->getOrigin(&xpos,&ypos);

		ICoord2D screenPos;
		screenPos.x=(Real)TheTacticalView->getWidth()*0.5f;
		screenPos.y=(Real)TheTacticalView->getHeight()*0.5f;
		TheTacticalView->screenToTerrain(&screenPos,&centerPos);
	}

	Make_Legacy_Shader_Translation(offset, -centerPos.x, -centerPos.y, 0.0f);

	Make_Legacy_Shader_Translation(offsetTextureCenter, 0.5f, 0.5f, 0.0f);	//shift coordinates so center of projection falls at uv 0.5,0.5

	Real worldTexelWidth=(1.0f-fadeLevel)*25.0f;	//9 worked well for circle but weird shape requires more stretch to cover.
	Real worldTexelHeight=(1.0f-fadeLevel)*25.0f;

	///@todo: Fix this to work with non 128x128 textures.
	if (worldTexelWidth != 0 && worldTexelHeight != 0)
	{
		Real widthScale = 1.0f/(worldTexelWidth*128.0f);
		Real heightScale = 1.0f/(worldTexelHeight*128.0f);
		Make_Legacy_Shader_Scaling(scale, widthScale, heightScale, 1.0f);
		const Matrix4x4 projected = Multiply_Legacy_Shader_Matrices(Multiply_Legacy_Shader_Matrices(Multiply_Legacy_Shader_Matrices(inv, offset), scale), offsetTextureCenter);
		(void)projected; // GAP: texture-transform upload retired (see note above).
	}
	else
	{
		Make_Legacy_Shader_Scaling(scale, 0.0f, 0.0f, 1.0f);	//scaling by 0 will set uv coordinates to 0,0
		const Matrix4x4 projected = Multiply_Legacy_Shader_Matrices(Multiply_Legacy_Shader_Matrices(inv, offset), scale);
		(void)projected; // GAP: texture-transform upload retired (see note above).
	}

	return TRUE;
}

void MaskTextureShader::reset()
{
	clearBackendState();
	ShaderClass::Invalidate();
	if (IRenderBackend *resetBackend = WW3D::Get_Render_Backend())
		resetBackend->Invalidate_Cached_Render_States();
}

/*===========================================================================================*/
/*=========      Terrain Shaders	=========================================================*/
/*===========================================================================================*/

///regular terrain shader that should work on all multi-texture video cards (slowest version)
class TerrainShader2Stage : public W3DShaderInterface
{
public:
	float m_xSlidePerSecond ;	 ///< How far the clouds move per second.
	float m_ySlidePerSecond ;	 ///< How far the clouds move per second.
	float m_xOffset;
	float m_yOffset;

	virtual Int set(Int pass) override;		///<setup shader for the specified rendering pass.
	virtual Int init() override;			///<perform any one time initialization and validation
	virtual void reset() override;		///<do any custom resetting necessary to bring W3D in sync.

	void updateCloud();
	void updateNoise1 (Matrix4x4 *destMatrix,Matrix4x4 *curViewInverse, Bool doUpdate=true);	///<generate the uv coordinates for Noise1 (i.e clouds)
	void updateNoise2 (Matrix4x4 *destMatrix,Matrix4x4 *curViewInverse, Bool doUpdate=true);	///<generate the uv coordinates for Noise2 (i.e lightmap)
} terrainShader2Stage;

///regular terrain shader that should work on all multi-texture video cards (slowest version)
class FlatTerrainShader2Stage : public W3DShaderInterface
{
public:
	virtual Int set(Int pass) override;		///<setup shader for the specified rendering pass.
	virtual Int init() override;			///<perform any one time initialization and validation
	virtual void reset() override;		///<do any custom resetting necessary to bring W3D in sync.
} flatTerrainShader2Stage;

///regular terrain shader that should work on all multi-texture video cards (slowest version)
class FlatTerrainShaderPixelShader : public W3DShaderInterface
{
public:
	unsigned int				m_dwBasePixelShader;	///<retired terrain pixel-shader handle (reference only, always 0).
	unsigned int				m_dwBaseNoise1PixelShader;	///<retired terrain pixel-shader handle (reference only, always 0).
	unsigned int				m_dwBaseNoise2PixelShader;	///<retired terrain pixel-shader handle (reference only, always 0).
	unsigned int				m_dwBase0PixelShader;	///<retired terrain pixel-shader handle (reference only, always 0).
	virtual Int set(Int pass) override;		///<setup shader for the specified rendering pass.
	virtual Int init() override;			///<perform any one time initialization and validation
	virtual void reset() override;		///<do any custom resetting necessary to bring W3D in sync.
	virtual Int shutdown() override;			///<release resources used by shader
} flatTerrainShaderPixelShader;

///8 stage terrain shader which only works on certain Nvidia cards.
class TerrainShader8Stage : public W3DShaderInterface
{
	virtual Int set(Int pass) override;		///<setup shader for the specified rendering pass.
	virtual void reset() override;		///<do any custom resetting necessary to bring W3D in sync.
	virtual Int init() override;			///<perform any one time initialization and validation
} terrainShader8Stage;

//Offsets into constant register pool used by vertex shader
#define CV_WORLDVIEWPROJ_0	0	//4 vectors for transform of world->clip space.

///Pixel shader based terrain shader - fastest method for the newest cards.
class TerrainShaderPixelShader : public W3DShaderInterface
{
	unsigned int				m_dwBasePixelShader;	///<retired terrain pixel-shader handle (reference only, always 0).
	unsigned int				m_dwBaseNoise1PixelShader;	///<retired terrain pixel-shader handle (reference only, always 0).
	unsigned int				m_dwBaseNoise2PixelShader;	///<retired terrain pixel-shader handle (reference only, always 0).

	virtual Int set(Int pass) override;		///<setup shader for the specified rendering pass.
	virtual void reset() override;		///<do any custom resetting necessary to bring W3D in sync.
	virtual Int init() override;			///<perform any one time initialization and validation
	virtual Int shutdown() override;			///<release resources used by shader
} terrainShaderPixelShader;

///List of different terrain shader implementations in order of preference
W3DShaderInterface *TerrainShaderList[]=
{
	&terrainShaderPixelShader,
	&terrainShader8Stage,
	&terrainShader2Stage,
	nullptr
};

///List of different terrain shader implementations in order of preference
W3DShaderInterface *FlatTerrainShaderList[]=
{
	&flatTerrainShaderPixelShader,
	&flatTerrainShader2Stage,
	nullptr
};

Int TerrainShader2Stage::init()
{
	//initialize settings for uv animated clouds
	m_xSlidePerSecond = -0.02f;
	m_ySlidePerSecond =  1.50f * m_xSlidePerSecond;
	m_xOffset = 0;
	m_yOffset = 0;

	//no special device validation needed - anything in our min spec should handle this.

	W3DShaders[W3DShaderManager::ST_TERRAIN_BASE]=&terrainShader2Stage;
	W3DShadersPassCount[W3DShaderManager::ST_TERRAIN_BASE]=2;
	W3DShaders[W3DShaderManager::ST_TERRAIN_BASE_NOISE1]=&terrainShader2Stage;
	W3DShadersPassCount[W3DShaderManager::ST_TERRAIN_BASE_NOISE1]=3;
	W3DShaders[W3DShaderManager::ST_TERRAIN_BASE_NOISE2]=&terrainShader2Stage;
	W3DShadersPassCount[W3DShaderManager::ST_TERRAIN_BASE_NOISE2]=3;
	W3DShaders[W3DShaderManager::ST_TERRAIN_BASE_NOISE12]=&terrainShader2Stage;
	W3DShadersPassCount[W3DShaderManager::ST_TERRAIN_BASE_NOISE12]=3;

	return TRUE;
}

void TerrainShader2Stage::reset()
{
	clearBackendState();
	ShaderClass::Invalidate();

	if (IRenderBackend *resetBackend = WW3D::Get_Render_Backend())
		resetBackend->Invalidate_Cached_Render_States();
}

void TerrainShader2Stage::updateCloud()
{
	const float frame_time = WW3D::Get_Logic_Frame_Time_Seconds();
	m_xOffset += m_xSlidePerSecond * frame_time;
	m_yOffset += m_ySlidePerSecond * frame_time;

	// This moves offsets towards zero when smaller -1.0 or larger 1.0
	m_xOffset -= (Int)m_xOffset;
	m_yOffset -= (Int)m_yOffset;
}

void TerrainShader2Stage::updateNoise1(Matrix4x4 *destMatrix,Matrix4x4 *curViewInverse, Bool doUpdate)
{
	(void)doUpdate;
	#define STRETCH_FACTOR ((float)(1/(63.0*MAP_XY_FACTOR/2))) /* covers 63/2 tiles */

	Matrix4x4 scale;

	Make_Legacy_Shader_Scaling(scale, STRETCH_FACTOR, STRETCH_FACTOR, 1.0f);
	*destMatrix = Multiply_Legacy_Shader_Matrices(*curViewInverse, scale);

	Matrix4x4 offset;
	Make_Legacy_Shader_Translation(offset, m_xOffset, m_yOffset, 0.0f);
	*destMatrix = Multiply_Legacy_Shader_Matrices(*destMatrix, offset);
}

void TerrainShader2Stage::updateNoise2(Matrix4x4 *destMatrix,Matrix4x4 *curViewInverse, Bool doUpdate)
{
	(void)doUpdate;

	Matrix4x4 scale;

	Make_Legacy_Shader_Scaling(scale, STRETCH_FACTOR, STRETCH_FACTOR, 1.0f);
	*destMatrix = Multiply_Legacy_Shader_Matrices(*curViewInverse, scale);
}

Int TerrainShader2Stage::set(Int pass)
{
	// D3D12: each pass intent translated via Get_Render_Backend_State.
	// Textures/samplers preserved from m_Textures (caller-owned); the
	// bilinear/trilinear overrides below map onto the sampler. Cloud/noise
	// camera-space projections (updateNoise1/2) are preserved CPU-side but
	// not uploaded (no backend texture-projection equivalent). DOT3
	// MULTIPLYADD has no backend combine: this 2-stage path uses the MODULATE
	// fallback preserved via texture_combine.
	RenderBackendMaterialState translated;
	ShaderClass passShader;
	TextureClass *passTexture = nullptr;
	switch (pass)
	{
		case 0:
			// Opaque base: texture * diffuse, clamp, no blend.
			passShader = ShaderClass::_PresetOpaqueShader;
			passTexture = W3DShaderManager::getShaderTexture(0);
			break;
		case 1:
			// Alpha detail: texture * diffuse with vertex alpha, SRCALPHA/INVSRCALPHA.
			passShader = ShaderClass::_PresetAlphaShader;
			passTexture = W3DShaderManager::getShaderTexture(1);
			break;
		case 2:
			// Noise/cloud multiply pass (DESTCOLOR/ZERO framebuffer multiply
			// with camera-space projected UVs): no backend equivalent yet.
			// Cloud scroll offsets stay authoritative CPU-side (updateCloud).
			return FALSE;
		default:
			return FALSE;
	}
	if (!passShader.Get_Render_Backend_State(translated))
		return FALSE;
	if (passTexture != nullptr) {
		const TextureFilterClass &passFilter = passTexture->Get_Filter();
		if (!passFilter.Get_Render_Sampler(translated.sampler))
			return FALSE;
		if (TheGlobalData && (TheGlobalData->m_bilinearTerrainTex || TheGlobalData->m_trilinearTerrainTex)) {
			translated.sampler.min_filter = RenderBackendTextureFilter::Linear;
			translated.sampler.mag_filter = RenderBackendTextureFilter::Linear;
		} else {
			translated.sampler.min_filter = RenderBackendTextureFilter::Point;
			translated.sampler.mag_filter = RenderBackendTextureFilter::Point;
		}
		if (TheGlobalData && TheGlobalData->m_trilinearTerrainTex) {
			translated.sampler.mip_filter = RenderBackendTextureFilter::Linear;
			translated.sampler.mipmaps = true;
		}
		translated.clamp_texture = true;
		if (!passTexture->Ensure_Renderer_Texture())
			return FALSE;
	}
	storeBackendState(translated);

	return TRUE;
}

Int TerrainShader8Stage::init()
{
	ChipsetType res;

	//this shader will also use the 2Stage shader for some of the passes so initialize it too.
	if (terrainShader2Stage.init() && (res=W3DShaderManager::getChipset()) >= DC_TNT && res <= DC_GEFORCE2)
	{
		W3DShaders[W3DShaderManager::ST_TERRAIN_BASE]=&terrainShader8Stage;
		W3DShadersPassCount[W3DShaderManager::ST_TERRAIN_BASE]=1;
		W3DShaders[W3DShaderManager::ST_TERRAIN_BASE_NOISE1]=&terrainShader8Stage;
		W3DShadersPassCount[W3DShaderManager::ST_TERRAIN_BASE_NOISE1]=2;
		W3DShaders[W3DShaderManager::ST_TERRAIN_BASE_NOISE2]=&terrainShader8Stage;
		W3DShadersPassCount[W3DShaderManager::ST_TERRAIN_BASE_NOISE2]=2;
		W3DShaders[W3DShaderManager::ST_TERRAIN_BASE_NOISE12]=&terrainShader8Stage;
		W3DShadersPassCount[W3DShaderManager::ST_TERRAIN_BASE_NOISE12]=2;
		return TRUE;
	}

	return FALSE;
}

Int TerrainShader8Stage::set(Int pass)
{
	// D3D12: the 8-stage TFACTOR cascade (base MODULATE/ADD plus stages 2-7
	// blend-weight math) has no single-texture backend equivalent. The base
	// textures/samplers stay caller-owned in m_Textures; a translation is
	// attempted for the record, but the cascade itself is preserved for the
	// future multi-stage caller (not faked as MODULATE).
	{
		RenderBackendMaterialState translatedCascade;
		ShaderClass cascadeBase = ShaderClass::_PresetOpaqueShader;
		if (cascadeBase.Get_Render_Backend_State(translatedCascade))
			storeBackendState(translatedCascade);
		else
			clearBackendState();
	}
	if (pass == 0)
	{
		// Base 8-stage combine documented above; cloud/noise split below.
		return FALSE;
	}
	else
	{	//setup cloud noise/pass
		return terrainShader2Stage.set(2);
	}
}

void TerrainShader8Stage::reset()
{
	clearBackendState();
	ShaderClass::Invalidate();
	if (IRenderBackend *resetBackend = WW3D::Get_Render_Backend())
		resetBackend->Invalidate_Cached_Render_States();
}

Int TerrainShaderPixelShader::shutdown()
{
	// D3D12: terrain .pso handles retired (archival reference only).
	m_dwBasePixelShader=0;
	m_dwBaseNoise1PixelShader=0;
	m_dwBaseNoise2PixelShader=0;

	return TRUE;
}

Int TerrainShaderPixelShader::init()
{
	Int res;
#ifdef DISABLE_PIXEL_SHADERS
	return false;
#endif
	//this shader will also use the 2Stage shader for some of the passes so initialize it too.
	if (terrainShader2Stage.init() && (res=W3DShaderManager::getChipset()) >= DC_GENERIC_PIXEL_SHADER_1_1)
	{
		if (res >= DC_GENERIC_PIXEL_SHADER_1_1)
		{
			// D3D12: shaders\terrain.pso, terrainnoise.pso, terrainnoise2.pso
			// are archival reference only; the active path translates the base
			// pass via Get_Render_Backend_State (see set()). No D3D handles.
			m_dwBasePixelShader = 0;
			m_dwBaseNoise1PixelShader = 0;
			m_dwBaseNoise2PixelShader = 0;

			W3DShaders[W3DShaderManager::ST_TERRAIN_BASE]=&terrainShaderPixelShader;
			W3DShaders[W3DShaderManager::ST_TERRAIN_BASE_NOISE1]=&terrainShaderPixelShader;
			W3DShaders[W3DShaderManager::ST_TERRAIN_BASE_NOISE2]=&terrainShaderPixelShader;
			W3DShaders[W3DShaderManager::ST_TERRAIN_BASE_NOISE12]=&terrainShaderPixelShader;
			W3DShadersPassCount[W3DShaderManager::ST_TERRAIN_BASE]=1;
			W3DShadersPassCount[W3DShaderManager::ST_TERRAIN_BASE_NOISE1]=1;
			W3DShadersPassCount[W3DShaderManager::ST_TERRAIN_BASE_NOISE2]=1;
			W3DShadersPassCount[W3DShaderManager::ST_TERRAIN_BASE_NOISE12]=1;
			return TRUE;
		}
	}
	return FALSE;
}

Int TerrainShaderPixelShader::set(Int pass)
{
	(void)pass;
	// D3D12: base pass intent (texture0/1 clamp, per-UV-set sampling,
	// bilinear/trilinear overrides) translated via Get_Render_Backend_State.
	// The noise/cloud pixel-shader stages (terrainnoise .pso combines with
	// camera-space projected UVs) have no backend equivalent yet; the base
	// material below is the closest translatable state (not a fake combine).
	RenderBackendMaterialState translated;
	ShaderClass baseShader = ShaderClass::_PresetOpaqueShader;
	if (!baseShader.Get_Render_Backend_State(translated))
		return FALSE;

	TextureClass *baseTexture = W3DShaderManager::getShaderTexture(0);
	TextureClass *blendTexture = W3DShaderManager::getShaderTexture(1);
	if (baseTexture != nullptr) {
		const TextureFilterClass &baseFilter = baseTexture->Get_Filter();
		if (!baseFilter.Get_Render_Sampler(translated.sampler))
			return FALSE;
		if (TheGlobalData && (TheGlobalData->m_bilinearTerrainTex || TheGlobalData->m_trilinearTerrainTex)) {
			translated.sampler.min_filter = RenderBackendTextureFilter::Linear;
			translated.sampler.mag_filter = RenderBackendTextureFilter::Linear;
		} else {
			translated.sampler.min_filter = RenderBackendTextureFilter::Point;
			translated.sampler.mag_filter = RenderBackendTextureFilter::Point;
		}
		if (TheGlobalData && TheGlobalData->m_trilinearTerrainTex) {
			translated.sampler.mip_filter = RenderBackendTextureFilter::Linear;
			translated.sampler.mipmaps = true;
		}
		translated.clamp_texture = true;
		if (!baseTexture->Ensure_Renderer_Texture())
			return FALSE;
	}
	if (blendTexture != nullptr && !blendTexture->Ensure_Renderer_Texture())
		return FALSE;
	// GAP: second texture stage + noise .pso combines + camera-space UV
	// projection (updateNoise1/2 math preserved in TerrainShader2Stage, not
	// uploaded). Base material stored for crossed callers.
	storeBackendState(translated);

	return TRUE;
}

void TerrainShaderPixelShader::reset()
{
	clearBackendState();
	ShaderClass::Invalidate();
	if (IRenderBackend *resetBackend = WW3D::Get_Render_Backend())
		resetBackend->Invalidate_Cached_Render_States();
}

///Cloud layer rendering shader - used for objects similar to terrain which only need the cloud layer.
class CloudTextureShader : public W3DShaderInterface
{
	virtual Int set(Int stage) override;		///<setup shader for the specified rendering pass.
	virtual Int init() override;			///<perform any one time initialization and validation
	virtual void reset() override;		///<do any custom resetting necessary to bring W3D in sync.
	Int m_stageOfSet;
} cloudTextureShader;

///List of different cloud shader implementations in order of preference
W3DShaderInterface *CloudShaderList[]=
{
	&cloudTextureShader,
	nullptr
};

Int CloudTextureShader::init()
{
	W3DShaders[W3DShaderManager::ST_CLOUD_TEXTURE]=&cloudTextureShader;
	W3DShadersPassCount[W3DShaderManager::ST_CLOUD_TEXTURE]=1;

	return TRUE;
}

/**Setup a certain texture stage to project our cloud texture*/
Int CloudTextureShader::set(Int stage)
{
	// D3D12: cloud MODULATE intent translated via Get_Render_Backend_State.
	// Cloud scroll offsets (m_xOffset/m_yOffset via updateNoise1 math) stay
	// CPU-side; the camera-space projection upload has no backend equivalent
	// (documented, not faked). Wrap addressing + linear filtering preserved
	// via the sampler.
	{
		RenderBackendMaterialState translatedCloud;
		ShaderClass cloudShader = ShaderClass::_PresetOpaqueShader;
		if (!cloudShader.Get_Render_Backend_State(translatedCloud))
			return FALSE;
		translatedCloud.texture_combine = RenderBackendTextureCombine::Modulate;
		TextureClass *cloudTexture = W3DShaderManager::getShaderTexture(stage);
		if (cloudTexture != nullptr) {
			TextureFilterClass &cloudFilter = cloudTexture->Get_Filter();
			cloudFilter.Set_Mip_Mapping(TextureFilterClass::FILTER_TYPE_NONE);
			if (!cloudFilter.Get_Render_Sampler(translatedCloud.sampler))
				return FALSE;
			translatedCloud.sampler.address_u = RenderBackendTextureAddress::Wrap;
			translatedCloud.sampler.address_v = RenderBackendTextureAddress::Wrap;
			translatedCloud.sampler.min_filter = RenderBackendTextureFilter::Linear;
			translatedCloud.sampler.mag_filter = RenderBackendTextureFilter::Linear;
			translatedCloud.clamp_texture = false;
			if (!cloudTexture->Ensure_Renderer_Texture())
				return FALSE;
		}
		// Cloud position math preserved for future projection-aware callers.
		Matrix4x4 viewIdentity(true);
		Matrix4x4 inv;
		float det = 0.0f;
		Invert_Legacy_Shader_Matrix(inv, det, viewIdentity);
		Matrix4x4 cloudMatrix = viewIdentity;
		terrainShader2Stage.updateNoise1(&cloudMatrix,&inv,false);
		(void)cloudMatrix; // GAP: texture-transform upload retired.
		storeBackendState(translatedCloud);
	}

	m_stageOfSet=stage;
	return TRUE;
}

void CloudTextureShader::reset()
{
	clearBackendState();
	ShaderClass::Invalidate();
	if (IRenderBackend *resetBackend = WW3D::Get_Render_Backend())
		resetBackend->Invalidate_Cached_Render_States();
}

/*===========================================================================================*/
/*=========      Road Shaders	=========================================================*/
/*===========================================================================================*/
class RoadShaderPixelShader : public W3DShaderInterface
{
	unsigned int				m_dwBaseNoise2PixelShader;	///<retired road pixel-shader handle (reference only, always 0).

	virtual Int set(Int pass) override;		///<setup shader for the specified rendering pass.
	virtual void reset() override;		///<do any custom resetting necessary to bring W3D in sync.
	virtual Int init() override;			///<perform any one time initialization and validation
	virtual Int shutdown() override;			///<release resources used by shader
} roadShaderPixelShader;

class RoadShader2Stage : public W3DShaderInterface
{	friend class RoadShaderPixelShader;	//pixel shader version uses some of the same features.

	virtual Int set(Int pass) override;		///<setup shader for the specified rendering pass.
	virtual Int init() override;			///<perform any one time initialization and validation
	virtual void reset() override;
} roadShader2Stage;

///List of different terrain shader implementations in order of preference
W3DShaderInterface *RoadShaderList[]=
{
	&roadShaderPixelShader,
	&roadShader2Stage,
	nullptr
};

Int RoadShaderPixelShader::shutdown()
{
	// D3D12: roadnoise2.pso handle retired (archival reference only).
	m_dwBaseNoise2PixelShader=0;

	return TRUE;
}

Int RoadShaderPixelShader::init()
{
	Int res;

	//this shader will also use the 2Stage shader for some of the passes so initialize it too.
	if (roadShader2Stage.init() && (res=W3DShaderManager::getChipset()) >= DC_GENERIC_PIXEL_SHADER_1_1)
	{
		if (res >= DC_GENERIC_PIXEL_SHADER_1_1)
		{
			// D3D12: shaders\roadnoise2.pso is archival reference only; the
			// active path translates the base pass via Get_Render_Backend_State
			// (see set()). No D3D handles.
			m_dwBaseNoise2PixelShader = 0;

			//Only set this shader for use in dual noise mode.  The 2Stage shader will take care of
			//all the other modes.
			W3DShaders[W3DShaderManager::ST_ROAD_BASE_NOISE12]=&roadShaderPixelShader;
			W3DShadersPassCount[W3DShaderManager::ST_ROAD_BASE_NOISE12]=1;
			return TRUE;
		}
	}
	return FALSE;
}

Int RoadShaderPixelShader::set(Int pass)
{
	(void)pass;
	// D3D12: road base intent (texture0 + alpha blend into terrain, LESSEQUAL
	// no-z-write, unlit) translated via Get_Render_Backend_State. The
	// roadnoise2.pso cloud/noise stages with camera-space projected UVs have
	// no backend equivalent yet; the base material below is the closest
	// translatable state (not a fake combine).
	RenderBackendMaterialState translated;
	ShaderClass roadShader = ShaderClass::_PresetAlphaShader;
	// Legacy LESSEQUAL maps to the closest ShaderClass compare (LEQUAL);
	// no-z-write + unlit intent preserved below via the material.
	roadShader.Set_Depth_Compare(ShaderClass::PASS_LEQUAL);
	roadShader.Set_Depth_Mask(ShaderClass::DEPTH_WRITE_DISABLE);
	if (!roadShader.Get_Render_Backend_State(translated))
		return FALSE;
	TextureClass *roadTexture = W3DShaderManager::getShaderTexture(0);
	if (roadTexture != nullptr) {
		const TextureFilterClass &roadFilter = roadTexture->Get_Filter();
		if (!roadFilter.Get_Render_Sampler(translated.sampler))
			return FALSE;
		translated.clamp_texture = true;
		if (!roadTexture->Ensure_Renderer_Texture())
			return FALSE;
	}
	// GAP: noise/cloud textures (stages 1-2) + updateNoise1/2 projection math
	// preserved CPU-side in TerrainShader2Stage (not uploaded).
	{
		Matrix4x4 viewIdentity(true);
		Matrix4x4 inv;
		float det = 0.0f;
		Invert_Legacy_Shader_Matrix(inv, det, viewIdentity);
		Matrix4x4 noiseMatrix = viewIdentity;
		terrainShader2Stage.updateNoise1(&noiseMatrix,&inv, false);
		terrainShader2Stage.updateNoise2(&noiseMatrix,&inv, false);
		(void)noiseMatrix; // GAP: texture-transform upload retired.
	}
	storeBackendState(translated);

	return TRUE;
}

void RoadShaderPixelShader::reset()
{

	clearBackendState();
	ShaderClass::Invalidate();
	if (IRenderBackend *resetBackend = WW3D::Get_Render_Backend())
		resetBackend->Invalidate_Cached_Render_States();
}

Int RoadShader2Stage::init()
{
	//no special device validation needed - anything in our min spec should handle this.
	W3DShaders[W3DShaderManager::ST_ROAD_BASE]=&roadShader2Stage;
	W3DShadersPassCount[W3DShaderManager::ST_ROAD_BASE]=1;
	W3DShaders[W3DShaderManager::ST_ROAD_BASE_NOISE1]=&roadShader2Stage;
	W3DShadersPassCount[W3DShaderManager::ST_ROAD_BASE_NOISE1]=1;
	W3DShaders[W3DShaderManager::ST_ROAD_BASE_NOISE2]=&roadShader2Stage;
	W3DShadersPassCount[W3DShaderManager::ST_ROAD_BASE_NOISE2]=1;
	W3DShaders[W3DShaderManager::ST_ROAD_BASE_NOISE12]=&roadShader2Stage;
	W3DShadersPassCount[W3DShaderManager::ST_ROAD_BASE_NOISE12]=2;

	return TRUE;
}

Int RoadShader2Stage::set(Int pass)
{
	// D3D12: pass-0 road base (texture*diffuse, alpha blend into terrain,
	// LESSEQUAL no-z-write, unlit) translated via Get_Render_Backend_State.
	// The noise/cloud second stage with camera-space projection has no
	// backend equivalent (preserved CPU-side, not uploaded). Pass 1
	// (BLENDCURRENTALPHA + ZERO/SRCCOLOR framebuffer multiply) has no backend
	// equivalent either.
	if (pass != 0)
		return FALSE;
	RenderBackendMaterialState translated;
	ShaderClass roadShader = ShaderClass::_PresetAlphaShader;
	// Legacy LESSEQUAL maps to the closest ShaderClass compare (LEQUAL).
	roadShader.Set_Depth_Compare(ShaderClass::PASS_LEQUAL);
	roadShader.Set_Depth_Mask(ShaderClass::DEPTH_WRITE_DISABLE);
	if (!roadShader.Get_Render_Backend_State(translated))
		return FALSE;
	TextureClass *roadTexture = W3DShaderManager::getShaderTexture(0);
	if (roadTexture != nullptr) {
		const TextureFilterClass &roadFilter = roadTexture->Get_Filter();
		if (!roadFilter.Get_Render_Sampler(translated.sampler))
			return FALSE;
		translated.clamp_texture = true;
		if (!roadTexture->Ensure_Renderer_Texture())
			return FALSE;
	}
	// GAP: noise texture selection (cloud vs light map by current shader) +
	// updateNoise1/2 projection preserved CPU-side (not uploaded, not faked).
	storeBackendState(translated);

	return TRUE;
}

void RoadShader2Stage::reset()
{
	clearBackendState();
	ShaderClass::Invalidate();

	if (IRenderBackend *resetBackend = WW3D::Get_Render_Backend())
		resetBackend->Invalidate_Cached_Render_States();
}

/** List of all custom shader lists - each list in this list contains variations of the same
	shader to allow it to work on different hardware configurations.
*/
W3DShaderInterface **MasterShaderList[]=
{
	TerrainShaderList,
	ShroudShaderList,
	FlatShroudShaderList,
	RoadShaderList,
	MaskShaderList,
	CloudShaderList,
	FlatTerrainShaderList,
	nullptr
};

/** List of all custom filter lists - each list in this list contains variations of the same
	filter to allow it to work on different hardware configurations.
*/
W3DFilterInterface **MasterFilterList[]=
{
	ScreenDefaultFilterList,
	ScreenBWFilterList,
	ScreenMotionBlurFilterList,
	ScreenCrossFadeFilterList,
	nullptr
};

// W3DShaderManager::W3DShaderManager =========================================
/** Constructor - just clears some variables */
//=============================================================================
W3DShaderManager::W3DShaderManager()
{
	m_currentShader = ST_INVALID;
	m_currentFilter = FT_NULL_FILTER;
	m_filterTexture = nullptr;
	m_filterTarget = RenderBackendTextureHandle();
	m_renderingToTexture = false;
	Int i;
	for (i=0; i<W3DShaderManager::ST_MAX; i++)
	{	W3DShaders[i]=nullptr;
		W3DShadersPassCount[i]=0;
	}
	for (i=0; i<FT_MAX; i++)
	{	W3DFilters[i]=nullptr;
	}
	for (i=0; i<8; i++)
	{
		m_Textures[i]=nullptr;
	}
	m_currentShader=(W3DShaderManager::ShaderTypes)-1;
}

// W3DShaderManager::init =======================================================
/** Walk through all shaders and find versions suitable for current hardware */
//=============================================================================
void W3DShaderManager::init()
{
	int i,j;

	// D3D12: filter render target is a backend texture sized to the output.
	// (MSAA depth-mismatch handling is backend-owned now.)
	ChipsetType res=DC_UNKNOWN;
	if ((res=W3DShaderManager::getChipset()) != 0)
	{
		m_currentChipset = res;	//cache the current chipset.

		//Some of our effects require an offscreen render target, so try creating it here.
		IRenderBackend *initBackend = WW3D::Get_Render_Backend();
		int outWidth = 0, outHeight = 0, outBits = 0;
		Bool outWindowed = TRUE;
		if (initBackend != nullptr && initBackend->Is_Device_Ready()
			&& initBackend->Get_Output_Description(outWidth, outHeight, outBits, outWindowed)
			&& outWidth > 0 && outHeight > 0)
		{
			REF_PTR_RELEASE(m_filterTexture);
			m_filterTexture = WW3D::Create_Render_Texture(
				static_cast<unsigned int>(outWidth), static_cast<unsigned int>(outHeight));
			if (m_filterTexture != nullptr)
				m_filterTarget = m_filterTexture->Get_Renderer_Texture();
		}
	}

	W3DShaderInterface **shaders;

	for (i=0; MasterShaderList[i] != nullptr; i++)
	{
		shaders=MasterShaderList[i];
		for (j=0; shaders[j] != nullptr; j++)
		{
			if (shaders[j]->init())
				break;	//found a working shader
		}
	}
	W3DFilterInterface **filters;

	for (i=0; MasterFilterList[i] != nullptr; i++)
	{
		filters=MasterFilterList[i];
		for (j=0; filters[j] != nullptr; j++)
		{
			if (filters[j]->init())
				break;	//found a working shader
		}
	}

	DEBUG_LOG(("ShaderManager ChipsetID %d", res));
}

// W3DShaderManager::shutdown =======================================================
/** Any shaders which allocate resources will be allowed to free them */
//=============================================================================
void W3DShaderManager::shutdown()
{
	REF_PTR_RELEASE(m_filterTexture);
	m_filterTarget = RenderBackendTextureHandle();
	m_renderingToTexture = false;
	m_currentShader = ST_INVALID;
	m_currentFilter = FT_NULL_FILTER;
	//release any assets associated with a shader (vertex/pixel shaders, textures, etc.)
	Int i=0;
	for (; i<W3DShaderManager::ST_MAX; i++) {
		if (W3DShaders[i]) {
			W3DShaders[i]->shutdown();
		}
	}

	for (i=0; i < FT_MAX; i++)
	{
		if (W3DFilters[i])
		{
			W3DFilters[i]->shutdown();
		}
	}
}

//=============================================================================
void W3DShaderManager::updateCloud()
{
	terrainShader2Stage.updateCloud();
}

// W3DShaderManager::getShaderPasses =======================================================
/** Return number of renderig passes required in perform the desired shader on current
	hardware.  App will need to re-render the polygons this many times to complete the
	effect.
 */
//=============================================================================
Int W3DShaderManager::getShaderPasses(ShaderTypes shader)
{
	return W3DShadersPassCount[shader];
}

// W3DShaderManager::setShader =======================================================
/** Must call this method before each rendering pass in order to perform proper D3D
	setup for each shader.
 */
//=============================================================================
Int W3DShaderManager::setShader(ShaderTypes shader, Int pass)
{
	if (shader == m_currentShader && pass == m_currentShaderPass)
		return TRUE;	//shader is already set
	m_currentShader=shader;
	m_currentShaderPass = pass;
	if (W3DShaders[shader])
		return W3DShaders[shader]->set(pass);
	return FALSE;
}

// W3DShaderManager::resetShader =======================================================
/** Must call this method after all polygons and rendering passes have been submitted.
	This method allows D3D to reset itself to a default state that doesn't conflict
	with the WW3D2 Shader system.
 */
//=============================================================================
void W3DShaderManager::resetShader(ShaderTypes shader)
{
	if (m_currentShader == ST_INVALID)
		return;	//last shader is already reset.
	if (W3DShaders[shader])
		W3DShaders[shader]->reset();
	m_currentShader = ST_INVALID;
}
// W3DShaderManager::filterPreRender =======================================================
/** Call to view filter shaders before rendering starts.
 */
//=============================================================================
Bool W3DShaderManager::filterPreRender(FilterTypes filter, Bool &skipRender, CustomScenePassModes &scenePassMode)
{
	if (W3DFilters[filter])
	{	Bool result=W3DFilters[filter]->preRender(skipRender,scenePassMode);
		if (result)
			m_currentFilter = filter;
		return result;
	}
	return FALSE;
}

// W3DShaderManager::filterPostRender =======================================================
/** Call to view filter shaders after rendering is complete.
 */
//=============================================================================
Bool W3DShaderManager::filterPostRender(FilterTypes filter, FilterModes mode, Coord2D &scrollDelta, Bool &doExtraRender)
{
	if (W3DFilters[filter])
		return W3DFilters[filter]->postRender(mode, scrollDelta,doExtraRender);

	m_currentFilter = FT_NULL_FILTER;
	return FALSE;
}

// W3DShaderManager::filterPostRender =======================================================
/** Call to view filter shaders after rendering is complete.
 */
//=============================================================================
	static Bool filterSetup(FilterTypes filter, FilterModes mode);
Bool W3DShaderManager::filterSetup(FilterTypes filter, FilterModes mode)
{
	if (W3DFilters[filter])
		return W3DFilters[filter]->setup(mode);
	return FALSE;
}

/*Draws 2 triangles covering the viewport given the current render states*/
void W3DShaderManager::drawViewport(Int color)
{
	Int xpos, ypos, width, height;

	TheTacticalView->getOrigin(&xpos,&ypos);
	width=TheTacticalView->getWidth();
	height=TheTacticalView->getHeight();

	// D3D12: XYZRHW DrawPrimitiveUP retired; screen_space material draw below
	// preserves corners + UVs (untextured: null texture handle).
	ScreenSpaceFilterVertex v[4];
	//bottom right
	v[0].px = xpos+width-0.5f; v[0].py = ypos+height-0.5f;
	v[0].u = (Real)(xpos+width)/(Real)TheDisplay->getWidth();	v[0].v = (Real)(ypos+height)/(Real)TheDisplay->getHeight();
	//top right
	v[1].px = xpos+width-0.5f; v[1].py = ypos-0.5f;
	v[1].u = (Real)(xpos+width)/(Real)TheDisplay->getWidth();	v[1].v = (Real)(ypos)/(Real)TheDisplay->getHeight();
	//bottom left
	v[2].px = xpos-0.5f; v[2].py = ypos+height-0.5f;
	v[2].u = (Real)(xpos)/(Real)TheDisplay->getWidth();	v[2].v = (Real)(ypos+height)/(Real)TheDisplay->getHeight();
	//top left
	v[3].px = xpos-0.5f;  v[3].py = ypos-0.5f;
	v[3].u = (Real)(xpos)/(Real)TheDisplay->getWidth();	v[3].v = (Real)(ypos)/(Real)TheDisplay->getHeight();
	v[0].color = static_cast<unsigned int>(color);
	v[1].color = static_cast<unsigned int>(color);
	v[2].color = static_cast<unsigned int>(color);
	v[3].color = static_cast<unsigned int>(color);

	ShaderClass copyShader = ShaderClass::_PresetOpaqueShader;
	RenderBackendMaterialState copyMaterial;
	if (!copyShader.Get_Render_Backend_State(copyMaterial))
		return;
	Submit_Screen_Space_Filter_Quad(v, nullptr, copyMaterial);
}

// W3DShaderManager::startRenderToTexture =======================================================
/** Starts rendering to a texture.
 */
//=============================================================================
void W3DShaderManager::startRenderToTexture()
{
	DEBUG_ASSERTCRASH(!m_renderingToTexture, ("Already rendering to texture - cannot nest calls."));

	if (m_renderingToTexture || m_filterTexture == nullptr) return;
	// D3D12: redirect via the backend render texture (not D3D8 surfaces).
	if (!WW3D::Set_Render_Texture(m_filterTexture))
	{
		// Permanently disable RTT to prevent repeated failures.
		REF_PTR_RELEASE(m_filterTexture);
		m_filterTarget = RenderBackendTextureHandle();
		return;
	}

	m_renderingToTexture = true;
	if (TheGlobalData->m_showSoftWaterEdge)
	{	//Soft water edges use frame buffer destination alpha so we must clear it to a known value.
		if (m_currentFilter == FT_VIEW_MOTION_BLUR_FILTER || m_currentFilter == FT_VIEW_CROSSFADE)
		{	//these filters rely on the previous frame being visible so we must be careful about clearing
			//frame buffer.  Only clear the alpha channel
			// D3D12: per-channel color-write masking has no backend equivalent;
			// the alpha-seed quad below preserves the intent (opaque, no depth).
			ShaderClass shader=ShaderClass::_PresetOpaqueSolidShader;
			shader.Set_Depth_Compare(ShaderClass::PASS_ALWAYS);
			shader.Set_Depth_Mask(ShaderClass::DEPTH_WRITE_DISABLE);
			RenderBackendMaterialState seedMaterial;
			if (shader.Get_Render_Backend_State(seedMaterial))
				drawViewport(0x00ffffff | (((Int)(TheWaterTransparency->m_minWaterOpacity*255.0f)) <<24));
		}
		else {	//normal clear that overwrites everything.
			if (IRenderBackend *clearBackend = WW3D::Get_Render_Backend())
				clearBackend->Clear(true, false, Vector3( 0.0f, 0.0f, 0.0f ), TheWaterTransparency->m_minWaterOpacity);
		}
	}
}

// W3DShaderManager::startRenderToTexture =======================================================
/** Ends rendering to a texture.
 */
//=============================================================================
TextureClass *W3DShaderManager::endRenderToTexture()
{
	DEBUG_ASSERTCRASH(m_renderingToTexture, ("Not rendering to texture."));
	if (!m_renderingToTexture) return nullptr;
	//restore original render target. Non-power-of-2 clamp/linear/no-mip intent
	// is preserved via the submit-time sampler (see Submit helper).
	WW3D::Set_Render_Texture(nullptr);
	m_renderingToTexture = false;
	return m_filterTexture;
}

/**Returns texture containing the image that was last rendered using any of the effects requiring render target
textures.  Used mostly for cross-fading effects that need an unmodified version of the view before the effect
was applied.  NOTE: This texture does not survive device reset.. so quit effect on reset!*/
TextureClass *W3DShaderManager::getRenderTexture()
{
	return m_filterTexture;
}

enum GraphicsVenderID CPP_11(: Int)
{
	DC_NVIDIA_VENDOR_ID	= 0x10DE,
	DC_3DFX_VENDOR_ID	= 0x121A,
	DC_ATI_VENDOR_ID	= 0x1002
};

// W3DShaderManager::ChipsetType =======================================================
/** Returns the chipset used by the currently active rendering device.  Can be useful
	for coding around specific driver bugs.
 */
//=============================================================================
ChipsetType W3DShaderManager::getChipset()
{
	//check if globaldata has an override for current chipset
	if (TheGlobalData && TheGlobalData->m_chipSetType != DC_UNKNOWN)
		return (ChipsetType)TheGlobalData->m_chipSetType;

	// D3D12: per-device branching retired. The x64 renderer is shader-model
	// capable by construction; report the pixel-shader baseline so shader
	// selection keeps its historical preference order (pixel-shader variants
	// first, fixed-function fallbacks after). Vendor/device ID queries and
	// caps-bit branching (max textures, shader versions) are retired with DX8.
	m_currentVendor = DC_NVIDIA_VENDOR_ID;
	return DC_GENERIC_PIXEL_SHADER_1_1;
}

//=============================================================================
// WaterRenderObjClass::LoadAndCreateShader
//=============================================================================
/** Loads and creates a D3D pixel or vertex shader.*/
//=============================================================================
HRESULT W3DShaderManager::LoadAndCreateD3DShader(const char* strFilePath, const unsigned int* pDeclaration, unsigned int Usage, Bool ShaderType, unsigned int* pHandle)
{
	(void)strFilePath;
	(void)pDeclaration;
	(void)Usage;
	(void)ShaderType;
	// D3D12: .vso/.pso blobs are archival reference only; the active path
	// translates shaders via ShaderClass::Get_Render_Backend_State. Always
	// report failure so historical preference lists fall through to the
	// backend-translated variants. No new shader framework is invented here.
	if (pHandle != nullptr)
		*pHandle = 0;
	return E_FAIL;
}

//For the MP test, we're enforcing high min-spec requirements that need to be verified.
#define MIN_INTEL_CPU_FREQ	1300
#define MIN_AMD_CPU_FREQ	1100
#define MIN_ACCEPTED_FREQUENCY	1300
#define MIN_ACCEPTED_MEMORY	(1024*1024*256)	//256 MB
#define MIN_ACCEPTED_TEXTURE_MEMORY	(1024*1024*30)	//30 MB

/**Hack to give gameengine access to this function*/
Bool testMinimumRequirements(ChipsetType *videoChipType, CpuType *cpuType, Int *cpuFreq, MemValueType *numRAM, Real *intBenchIndex, Real *floatBenchIndex, Real *memBenchIndex)
{
	return W3DShaderManager::testMinimumRequirements(videoChipType,cpuType,cpuFreq,numRAM,intBenchIndex,floatBenchIndex,memBenchIndex);
}

Bool W3DShaderManager::testMinimumRequirements(ChipsetType *videoChipType, CpuType *cpuType, Int *cpuFreq, MemValueType *numRAM, Real *intBenchIndex, Real *floatBenchIndex, Real *memBenchIndex)
{
	if (videoChipType)
		*videoChipType = getChipset();

	if (cpuType)
	{
		*cpuType = XX;	//unknown

		//Check if it's an Athlon
		if (CPUDetectClass::Get_Processor_Manufacturer() == CPUDetectClass::MANUFACTURER_AMD &&
				CPUDetectClass::Get_AMD_Processor() >= CPUDetectClass::AMD_PROCESSOR_ATHLON_025)
				*cpuType = K7;

		//Check if it's a P3
		if (CPUDetectClass::Get_Processor_Manufacturer() == CPUDetectClass::MANUFACTURER_INTEL &&
				CPUDetectClass::Get_Intel_Processor() >= CPUDetectClass::INTEL_PROCESSOR_PENTIUM_III_MODEL_7)
				*cpuType = P3;
		//Check if it's a P4
		if (CPUDetectClass::Get_Processor_Manufacturer() == CPUDetectClass::MANUFACTURER_INTEL &&
				CPUDetectClass::Get_Intel_Processor() >= CPUDetectClass::INTEL_PROCESSOR_PENTIUM4)
				*cpuType = P4;
	}

	if (cpuFreq)
		*cpuFreq=CPUDetectClass::Get_Processor_Speed();

	if (numRAM)
		*numRAM=CPUDetectClass::Get_Total_Physical_Memory();

	if (intBenchIndex && floatBenchIndex && memBenchIndex)
	{
		// TheSuperHackers @tweak Aliendroid1 19/06/2025 Legacy benchmarking code was removed.
		// Since modern hardware always meets the minimum requirements, we preset the benchmark "results" to a high value.
		*intBenchIndex = 10.0f;
		*floatBenchIndex = 10.0f;
		*memBenchIndex = 10.0f;
	}

	return TRUE;
}

/**Try to guess how well the video card will handle the game assuming very fast CPU*/
StaticGameLODLevel W3DShaderManager::getGPUPerformanceIndex()
{
	ChipsetType	chipType;
	StaticGameLODLevel detailSetting=STATIC_GAME_LOD_LOW;	//assume lowest settings for now.

	if ((chipType=getChipset()) != DC_UNKNOWN)
	{	//a known video card so we can make some assumptions
		if (chipType >=	DC_GEFORCE2)
			detailSetting=STATIC_GAME_LOD_LOW;	//these cards need multiple terrain passes.
		if (chipType >= DC_GENERIC_PIXEL_SHADER_1_1)	//these cards can do terrain in single pass.
			detailSetting=STATIC_GAME_LOD_VERY_HIGH;
	}

	return detailSetting;
}

/**We need a hardware independent method to compare different CPU's.  For lack of anything better, we'll
use time to calculate PIE using a slow random number algorithm.*/

/**Used to test function call overhead*/
void add(float *sum,float *addend)
{
	*sum = *sum + *addend;
}

/**Returns seconds needed to run the test*/
Real W3DShaderManager::GetCPUBenchTime()
{
	float ztot, yran, ymult, ymod, x, y, z, pi, prod;
    long int low, ixran, itot, j, iprod;

  	__int64 endTime64,freq64,startTime64;
	QueryPerformanceFrequency((LARGE_INTEGER *)&freq64);
	QueryPerformanceCounter((LARGE_INTEGER *)&startTime64);

    ztot = 0.0;
    low = 1;
    ixran = 1907;
    yran = 5813.0;
    ymult = 1307.0;
    ymod = 5471.0;
    itot = 560000;	//total iterations. This value ends up running at ~30 fps on our P4-2.2Ghz.

    for(j=1; j<=itot; j++)
    {
		iprod = 27611 * ixran;
		ixran = iprod - 74383*(long int)(iprod/74383);
		x = (float)ixran / 74383.0;
		prod = ymult * yran;
		yran = (prod - ymod*(long int)(prod/ymod));
		y = yran / ymod;
		z = x*x + y*y;
		add(&ztot,&z);
		if ( z <= 1.0 )
		{
		  low = low + 1;
		}
	}
	pi = 4.0 * (float)low/(float)itot;

	QueryPerformanceCounter((LARGE_INTEGER *)&endTime64);
	return ((double)(endTime64-startTime64)/(double)(freq64));
}


// W3DShaderManager::setShroudTex =======================================================
/** Puts the shroud texture into a texture stage.
 */
//=============================================================================
Int W3DShaderManager::setShroudTex(Int stage)
{
	//We need to scale so shroud texel stretches over one full terrain cell.  Each texel
	//is 1/128 the size of full texture. (assuming 128x128 vid-mem texture).
	W3DShroud *shroud;
	if (TheTerrainRenderObject != nullptr && (shroud=TheTerrainRenderObject->getShroud()) != nullptr)
	{
		// D3D12: shroud texture ref stays caller-owned (m_Textures via
		// setTexture by Install_Materials); MODULATE intent preserved via
		// texture_combine on crossed callers. The camera-space projection
		// below is preserved CPU-side but not uploaded (no backend
		// texture-projection equivalent; documented, not faked).
		TextureClass *shroudTexture = shroud->getShroudTexture();
		if (shroudTexture != nullptr && !shroudTexture->Ensure_Renderer_Texture())
			return FALSE;

		Matrix4x4 viewIdentity(true);
		Matrix4x4 inv;
		float det = 0.0f;
		Invert_Legacy_Shader_Matrix(inv, det, viewIdentity);

		Matrix4x4 scale,offset;

		//We need to make all world coordinates be relative to the heightmap data origin since that
		//is where the shroud begins.

		float xoffset = 0;
		float yoffset = 0;
		Real width=shroud->getCellWidth();
		Real height=shroud->getCellHeight();

		if (TheTerrainRenderObject->getMap())
		{	//subtract origin position from all coordinates.  Origin is shifted by 1 cell width/height to allow for unused border texels.
			xoffset = -(float)shroud->getDrawOriginX() + width;
			yoffset = -(float)shroud->getDrawOriginY() + height;
		}

		Make_Legacy_Shader_Translation(offset, xoffset, yoffset, 0.0f);

		width = 1.0f/(width*shroud->getTextureWidth());
		height = 1.0f/(height*shroud->getTextureHeight());
		Make_Legacy_Shader_Scaling(scale, width, height, 1.0f);
		const Matrix4x4 projected = Multiply_Legacy_Shader_Matrices(Multiply_Legacy_Shader_Matrices(inv, offset), scale);
		(void)projected; // GAP: texture-transform upload retired.
		(void)stage;
		return TRUE;
	}
	return FALSE;
}



Int FlatTerrainShader2Stage::init()
{
	//no special device validation needed - anything in our min spec should handle this.

	W3DShaders[W3DShaderManager::ST_FLAT_TERRAIN_BASE]=&flatTerrainShader2Stage;
	W3DShadersPassCount[W3DShaderManager::ST_FLAT_TERRAIN_BASE]=1;
	W3DShaders[W3DShaderManager::ST_FLAT_TERRAIN_BASE_NOISE1]=&flatTerrainShader2Stage;
	W3DShadersPassCount[W3DShaderManager::ST_FLAT_TERRAIN_BASE_NOISE1]=2;
	W3DShaders[W3DShaderManager::ST_FLAT_TERRAIN_BASE_NOISE2]=&flatTerrainShader2Stage;
	W3DShadersPassCount[W3DShaderManager::ST_FLAT_TERRAIN_BASE_NOISE2]=2;
	W3DShaders[W3DShaderManager::ST_FLAT_TERRAIN_BASE_NOISE12]=&flatTerrainShader2Stage;
	W3DShadersPassCount[W3DShaderManager::ST_FLAT_TERRAIN_BASE_NOISE12]=2;

	return TRUE;
}

void FlatTerrainShader2Stage::reset()
{
	clearBackendState();
	ShaderClass::Invalidate();

	if (IRenderBackend *resetBackend = WW3D::Get_Render_Backend())
		resetBackend->Invalidate_Cached_Render_States();
}


Int FlatTerrainShader2Stage::set(Int pass)
{
	// D3D12: pass-0 flat-terrain base (opaque texture*diffuse, clamp,
	// bilinear/trilinear overrides) translated via Get_Render_Backend_State.
	// The shroud stage-0 projection and pass-1 noise multiply (DESTCOLOR/ZERO)
	// have no backend equivalent (preserved CPU-side, not faked). DOT3
	// MULTIPLYADD fallback: MODULATE preserved via texture_combine.
	RenderBackendMaterialState translated;
	ShaderClass passShader = ShaderClass::_PresetOpaqueShader;
	TextureClass *passTexture = W3DShaderManager::getShaderTexture(0);

	switch (pass)
	{
		case 0:

			// Modulate the diffuse color with the texture as lighting comes from diffuse.
			// GAP: optional shroud stage-0 camera-space projection below is
			// preserved CPU-side (cell math intact) but not uploaded.
			if (!passShader.Get_Render_Backend_State(translated))
				return FALSE;
			{
				W3DShroud *flatShroud = (TheTerrainRenderObject != nullptr)
					? TheTerrainRenderObject->getShroud() : nullptr;
				if (flatShroud != nullptr)
				{
					Matrix4x4 viewIdentity(true);
					Matrix4x4 inv;
					float det = 0.0f;
					Invert_Legacy_Shader_Matrix(inv, det, viewIdentity);

					Matrix4x4 scale,offset;

					float xoffset = 0;
					float yoffset = 0;
					Real width=flatShroud->getCellWidth();
					Real height=flatShroud->getCellHeight();

					if (TheTerrainRenderObject->getMap())
					{
						xoffset = -(float)flatShroud->getDrawOriginX() + width;
						yoffset = -(float)flatShroud->getDrawOriginY() + height;
					}

					Make_Legacy_Shader_Translation(offset, xoffset, yoffset, 0.0f);

					width = 1.0f/(width*flatShroud->getTextureWidth());
					height = 1.0f/(height*flatShroud->getTextureHeight());
					Make_Legacy_Shader_Scaling(scale, width, height, 1.0f);
					const Matrix4x4 projected = Multiply_Legacy_Shader_Matrices(Multiply_Legacy_Shader_Matrices(inv, offset), scale);
					(void)projected; // GAP: texture-transform upload retired.
				}
			}
			if (passTexture != nullptr) {
				const TextureFilterClass &flatFilter = passTexture->Get_Filter();
				if (!flatFilter.Get_Render_Sampler(translated.sampler))
					return FALSE;
				if (TheGlobalData && (TheGlobalData->m_bilinearTerrainTex || TheGlobalData->m_trilinearTerrainTex)) {
					translated.sampler.min_filter = RenderBackendTextureFilter::Linear;
					translated.sampler.mag_filter = RenderBackendTextureFilter::Linear;
				} else {
					translated.sampler.min_filter = RenderBackendTextureFilter::Point;
					translated.sampler.mag_filter = RenderBackendTextureFilter::Point;
				}
				if (TheGlobalData && TheGlobalData->m_trilinearTerrainTex) {
					translated.sampler.mip_filter = RenderBackendTextureFilter::Linear;
					translated.sampler.mipmaps = true;
				}
				translated.clamp_texture = true;
				if (!passTexture->Ensure_Renderer_Texture())
					return FALSE;
			}
			translated.texture_combine = RenderBackendTextureCombine::Modulate;
			storeBackendState(translated);
			break;
		case 1:
			// Noise/cloud multiply pass (DESTCOLOR/ZERO with camera-space
			// projected UVs): no backend equivalent yet. Cloud scroll offsets
			// stay authoritative CPU-side (updateCloud).
			return FALSE;
	}

	return TRUE;
}






Int FlatTerrainShaderPixelShader::shutdown()
{
	// D3D12: fterrain .pso handles retired (archival reference only).
	m_dwBasePixelShader=0;
	m_dwBase0PixelShader=0;
	m_dwBaseNoise1PixelShader=0;
	m_dwBaseNoise2PixelShader=0;

	return TRUE;
}

Int FlatTerrainShaderPixelShader::init()
{
	Int res;

#ifdef DISABLE_PIXEL_SHADERS
	return false;
#endif

	//this shader will also use the 2Stage shader for some of the passes so initialize it too.
	if ((res=W3DShaderManager::getChipset()) >= DC_GENERIC_PIXEL_SHADER_1_1)
	{
		if (res >= DC_GENERIC_PIXEL_SHADER_1_1)
		{
			// D3D12: shaders\fterrain*.pso are archival reference only; the
			// active path translates the base pass via Get_Render_Backend_State
			// (see set()). No D3D handles.
			m_dwBasePixelShader = 0;
			m_dwBase0PixelShader = 0;
			m_dwBaseNoise1PixelShader = 0;
			m_dwBaseNoise2PixelShader = 0;

			W3DShaders[W3DShaderManager::ST_FLAT_TERRAIN_BASE]=&flatTerrainShaderPixelShader;
			W3DShaders[W3DShaderManager::ST_FLAT_TERRAIN_BASE_NOISE1]=&flatTerrainShaderPixelShader;
			W3DShaders[W3DShaderManager::ST_FLAT_TERRAIN_BASE_NOISE2]=&flatTerrainShaderPixelShader;
			W3DShaders[W3DShaderManager::ST_FLAT_TERRAIN_BASE_NOISE12]=&flatTerrainShaderPixelShader;
			W3DShadersPassCount[W3DShaderManager::ST_FLAT_TERRAIN_BASE]=1;
			W3DShadersPassCount[W3DShaderManager::ST_FLAT_TERRAIN_BASE_NOISE1]=1;
			W3DShadersPassCount[W3DShaderManager::ST_FLAT_TERRAIN_BASE_NOISE2]=1;
			W3DShadersPassCount[W3DShaderManager::ST_FLAT_TERRAIN_BASE_NOISE12]=1;
			return TRUE;
		}
	}
	return FALSE;
}

Int FlatTerrainShaderPixelShader::set(Int pass)
{
	(void)pass;
	// D3D12: flat-terrain base pass (texture2 clamp, per-UV-set sampling,
	// bilinear/trilinear overrides) translated via Get_Render_Backend_State.
	// The shroud stage-0 projection, cloud/noise camera-space stages, and the
	// fterrain .pso selection have no backend equivalent yet; the base material
	// below is the closest translatable state (not a fake combine).
	RenderBackendMaterialState translated;
	ShaderClass baseShader = ShaderClass::_PresetOpaqueShader;
	if (!baseShader.Get_Render_Backend_State(translated))
		return FALSE;
	TextureClass *baseTexture = W3DShaderManager::getShaderTexture(2);
	if (baseTexture != nullptr) {
		const TextureFilterClass &baseFilter = baseTexture->Get_Filter();
		if (!baseFilter.Get_Render_Sampler(translated.sampler))
			return FALSE;
		if (TheGlobalData && (TheGlobalData->m_bilinearTerrainTex || TheGlobalData->m_trilinearTerrainTex)) {
			translated.sampler.min_filter = RenderBackendTextureFilter::Linear;
			translated.sampler.mag_filter = RenderBackendTextureFilter::Linear;
		} else {
			translated.sampler.min_filter = RenderBackendTextureFilter::Point;
			translated.sampler.mag_filter = RenderBackendTextureFilter::Point;
		}
		if (TheGlobalData && TheGlobalData->m_trilinearTerrainTex) {
			translated.sampler.mip_filter = RenderBackendTextureFilter::Linear;
			translated.sampler.mipmaps = true;
		}
		translated.clamp_texture = true;
		if (!baseTexture->Ensure_Renderer_Texture())
			return FALSE;
	}
	// GAP: shroud cell scale/offset + cloud/noise updateNoise1/2 projection
	// math preserved CPU-side (see TerrainShader2Stage, ShroudTextureShader);
	// fterrain*.pso selection retired (archival reference in init()).
	storeBackendState(translated);
	return TRUE;
}

void FlatTerrainShaderPixelShader::reset()
{
	clearBackendState();
	ShaderClass::Invalidate();
	if (IRenderBackend *resetBackend = WW3D::Get_Render_Backend())
		resetBackend->Invalidate_Cached_Render_States();
}





