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

#include "WW3D2/dx8wrapper.h"
#include "WW3D2/ww3d.h"
#include "WW3D2/assetmgr.h"
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
#include "WW3D2/dx8caps.h"

namespace
{

D3DMATRIX Multiply_Legacy_Shader_Matrices(const D3DMATRIX &a, const D3DMATRIX &b)
{
	D3DMATRIX result = {};
	for (int row = 0; row < 4; ++row) {
		for (int column = 0; column < 4; ++column) {
			result.m[row][column] =
				a.m[row][0] * b.m[0][column] +
				a.m[row][1] * b.m[1][column] +
				a.m[row][2] * b.m[2][column] +
				a.m[row][3] * b.m[3][column];
		}
	}
	return result;
}

void Make_Legacy_Shader_Translation(D3DMATRIX &matrix, float x, float y, float z)
{
	matrix = {};
	matrix.m[0][0] = 1.0f;
	matrix.m[1][1] = 1.0f;
	matrix.m[2][2] = 1.0f;
	matrix.m[3][0] = x;
	matrix.m[3][1] = y;
	matrix.m[3][2] = z;
	matrix.m[3][3] = 1.0f;
}

void Make_Legacy_Shader_Scaling(D3DMATRIX &matrix, float x, float y, float z)
{
	matrix = {};
	matrix.m[0][0] = x;
	matrix.m[1][1] = y;
	matrix.m[2][2] = z;
	matrix.m[3][3] = 1.0f;
}

void Invert_Legacy_Shader_Matrix(D3DMATRIX &inverse, float &determinant, const D3DMATRIX &matrix)
{
	const Matrix4x4 neutral_matrix = To_Matrix4x4(matrix);
	Matrix4x4 neutral_inverse;
	if (Matrix4x4::Inverse(&neutral_inverse, &determinant, &neutral_matrix) != nullptr) {
		To_D3DMATRIX(inverse, neutral_inverse);
	}
}

HRESULT Set_Legacy_Pixel_Shader_Constant(DWORD shader_register, const Vector4 &value)
{
	return DX8Wrapper::_Get_D3D_Device8()->SetPixelShaderConstant(shader_register, &value.X, 1);
}

Bool Build_Screen_Filter_Quad(Int outputWidth, Int outputHeight, RenderBackendTexturedVertex (&vertices)[4])
{
	if (!TheTacticalView || outputWidth <= 0 || outputHeight <= 0) return false;
	Int xpos, ypos;
	TheTacticalView->getOrigin(&xpos, &ypos);
	const Int width = TheTacticalView->getWidth(), height = TheTacticalView->getHeight();
	if (width <= 0 || height <= 0) return false;
	const Real px[4] = {Real(xpos + width), Real(xpos + width), Real(xpos), Real(xpos)};
	const Real py[4] = {Real(ypos + height), Real(ypos), Real(ypos + height), Real(ypos)};
	for (Int i = 0; i < 4; ++i)
	{
		vertices[i] = {};
		vertices[i].x = 2.0f * px[i] / Real(outputWidth) - 1.0f;
		vertices[i].y = 1.0f - 2.0f * py[i] / Real(outputHeight);
		vertices[i].r = vertices[i].g = vertices[i].b = vertices[i].a = 1.0f;
		vertices[i].u = px[i] / Real(outputWidth);
		vertices[i].v = py[i] / Real(outputHeight);
	}
	return true;
}

RenderBackendMaterialState Screen_Filter_Material()
{
	RenderBackendMaterialState material;
	material.depth_test = RenderBackendDepthTest::Always;
	material.depth_write = false;
	material.cull = RenderBackendCullMode::None;
	material.texture_combine = RenderBackendTextureCombine::Replace;
	material.screen_space = true;
	material.clamp_texture = true;
	material.sampler.address_u = material.sampler.address_v = RenderBackendTextureAddress::Clamp;
	material.sampler.min_filter = material.sampler.mag_filter = RenderBackendTextureFilter::Linear;
	material.sampler.mipmaps = false;
	return material;
}

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
		DX8Wrapper::_Get_D3D_Device8()->SetTexture(0, nullptr);
		DX8Wrapper::_Get_D3D_Device8()->SetTexture(1, nullptr);};
	virtual Int init() = 0;			///<perform any one time initialization and validation
	virtual Int shutdown() { return TRUE;};			///<release resources used by shader
protected:
	Int m_numPasses;						///<number of passes to complete shader
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

TextureClass *W3DShaderManager::m_backendCapture = nullptr;
IRenderBackend *W3DShaderManager::m_backendCaptureOwner = nullptr;
Int W3DShaderManager::m_backendCaptureWidth = 0;
Int W3DShaderManager::m_backendCaptureHeight = 0;
Bool W3DShaderManager::m_backendCapturing = false;
Bool W3DShaderManager::m_backendCaptureHasScene = false;
unsigned int W3DShaderManager::m_backendOutputWriteMask = 15;
Bool W3DShaderManager::m_renderingToTexture = false;
IDirect3DSurface8 *W3DShaderManager::m_oldRenderSurface=nullptr;	///<previous render target
IDirect3DTexture8 *W3DShaderManager::m_renderTexture=nullptr;		///<texture into which rendering will be redirected.
IDirect3DSurface8 *W3DShaderManager::m_newRenderSurface=nullptr;	///<new render target inside m_renderTexture
IDirect3DSurface8 *W3DShaderManager::m_oldDepthSurface=nullptr;	///<previous depth buffer surface
/*===========================================================================================*/
/*=========      Screen Shaders	=============================================================*/
/*===========================================================================================*/

/*=========  ScreenBWFilter	=============================================================*/
///converts viewport to black & white.

Int ScreenBWFilter::m_fadeFrames;
Int ScreenBWFilter::m_curFadeFrame;
Real ScreenBWFilter::m_curFadeValue;
Int ScreenBWFilter::m_fadeDirection;

ScreenBWFilter screenBWFilter;

Int ScreenBWFilter::init()
{
	m_curFadeFrame = 0;
	W3DFilters[FT_VIEW_BW_FILTER] = this;
	return TRUE;
}

Bool W3DShaderManager::restoreBackendOutput()
{
	if (!m_backendCapturing) return true;
	IRenderBackend *backend = WW3D::Get_Render_Backend();
	// Validate both owner and generation before restoring borrowed output state.
	if (backend == m_backendCaptureOwner && m_backendCapture && m_backendCapture->Get_Renderer_Texture().Is_Valid())
	{
		if (!WW3D::Set_Render_Texture(nullptr)) return false;
		backend->Set_Pass_Color_Write_Mask(m_backendOutputWriteMask);
	}
	m_backendCapturing = false;
	m_backendCaptureOwner = nullptr;
	return true;
}

Bool W3DShaderManager::startBackendCapture(Bool preserveColor, Bool *newCapture)
{
	if (newCapture) *newCapture = false;
	if (!restoreBackendOutput()) return false;
	IRenderBackend *backend = WW3D::Get_Render_Backend();
	if (!backend || !backend->Is_Device_Ready()) return false;
	Int width, height, bits;
	bool windowed;
	if (!backend->Get_Output_Description(width, height, bits, windowed) || width <= 0 || height <= 0)
		return false;
	if (m_backendCapture && (m_backendCaptureWidth != width || m_backendCaptureHeight != height ||
		!m_backendCapture->Get_Renderer_Texture().Is_Valid()))
		REF_PTR_RELEASE(m_backendCapture);
	if (!m_backendCapture)
	{
		m_backendCaptureHasScene = false;
		m_backendCapture = WW3D::Create_Render_Texture(width, height, true);
		if (!m_backendCapture) return false;
		m_backendCaptureWidth = width;
		m_backendCaptureHeight = height;
	}
	if (newCapture) *newCapture = !m_backendCaptureHasScene;
	// The scene reapplies its camera after selection; output state is saved by the backend.
	if (!WW3D::Set_Render_Texture(m_backendCapture, true)) return false;
	m_backendOutputWriteMask = backend->Get_Pass_Color_Write_Mask();
	m_backendCaptureOwner = backend;
	m_backendCapturing = true;
	if (!m_backendCaptureHasScene && preserveColor)
		backend->Clear(true, false, Vector3(0.0f,0.0f,0.0f), TheWaterTransparency->m_minWaterOpacity);
	if (TheGlobalData->m_showSoftWaterEdge)
	{
		if (!preserveColor)
			backend->Clear(true, false, Vector3(0.0f, 0.0f, 0.0f), TheWaterTransparency->m_minWaterOpacity);
		else
		{
			// Frozen raw scene RGB must survive skipped renders. The old alpha-only
			// viewport quad quantized opacity through a packed diffuse byte.
			RenderBackendTexturedVertex vertices[4];
			if (!Build_Screen_Filter_Quad(width, height, vertices))
			{
				restoreBackendOutput();
				return false;
			}
			const Real alpha = Real((Int)(TheWaterTransparency->m_minWaterOpacity * 255.0f)) / 255.0f;
			for (Int i = 0; i < 4; ++i) vertices[i].a = alpha;
			const unsigned short indices[6] = {0, 1, 2, 2, 1, 3};
			RenderBackendMaterialState material = Screen_Filter_Material();
			material.color_write_mask = 8;
			material.override_pass_color_write_mask = true;
			if (!backend->Draw_Indexed_Material_Triangles(vertices, 4, indices, 6, {}, material))
			{
				restoreBackendOutput();
				return false;
			}
		}
		if (preserveColor) backend->Set_Pass_Color_Write_Mask(7);
	}
	return true;
}

TextureClass *W3DShaderManager::endBackendCapture()
{
	const Bool captured = m_backendCapturing;
	if (!restoreBackendOutput()) return nullptr;
	if (!captured || !m_backendCapture || !m_backendCapture->Get_Renderer_Texture().Is_Valid()) return nullptr;
	m_backendCaptureHasScene = true;
	return m_backendCapture;
}

Bool ScreenBWFilter::preRender(Bool &skipRender, CustomScenePassModes &scenePassMode)
{
	skipRender = false;
	return W3DShaderManager::startBackendCapture(false);
}

void ScreenBWFilter::advanceFade()
{
	if (m_fadeDirection > 0)
	{
		m_curFadeFrame++;
		if (m_curFadeFrame < m_fadeFrames)
			m_curFadeValue = (Real)m_curFadeFrame / (Real)m_fadeFrames;
		else
		{
			m_curFadeFrame = 0;
			m_curFadeValue = 1.0f;
			m_fadeDirection = 0;
		}
	}
	else if (m_fadeDirection < 0)
	{
		m_curFadeFrame++;
		if (m_curFadeFrame < m_fadeFrames)
			m_curFadeValue = 1.0f - (Real)m_curFadeFrame / (Real)m_fadeFrames;
		else
		{
			m_curFadeValue = 0.0f;
			TheTacticalView->setViewFilterMode(FM_NULL_MODE);
			TheTacticalView->setViewFilter(FT_NULL_FILTER);
			m_curFadeFrame = 0;
			m_fadeDirection = 0;
		}
	}
}

Bool ScreenBWFilter::postRender(FilterModes mode, Coord2D &scrollDelta, Bool &doExtraRender)
{
	TextureClass *capture = W3DShaderManager::endBackendCapture();
	IRenderBackend *backend = WW3D::Get_Render_Backend();
	if (!capture || !backend || mode <= FM_NULL_MODE) return false;
	// One step per successful capture, including the final fade-out frame.
	advanceFade();
	RenderBackendTexturedVertex vertices[4];
	if (!Build_Screen_Filter_Quad(capture->Get_Width(), capture->Get_Height(), vertices)) return false;
	const unsigned short indices[6] = {0, 1, 2, 2, 1, 3};
	RenderBackendMaterialState material = Screen_Filter_Material();
	// SCENE_PASS_DEFAULT uses RGBA; monochrome alpha is luminance even at fade zero.
	material.monochrome = true;
	material.monochrome_fade = m_curFadeValue;
	if (mode == FM_VIEW_BW_RED_AND_WHITE)
		material.monochrome_tint[1] = material.monochrome_tint[2] = 0.0f;
	else if (mode == FM_VIEW_BW_GREEN_AND_WHITE)
		material.monochrome_tint[0] = material.monochrome_tint[2] = 0.0f;
	return backend->Draw_Indexed_Material_Triangles(vertices, 4, indices, 6, capture->Get_Renderer_Texture(), material);
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

Int ScreenCrossFadeFilter::init()
{
	if (!TheDisplay)
		return FALSE;	//effect is useless without a view so no point initializing for the WB, etc.

	m_curFadeFrame = 0;

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
	skipRender = false;
	m_skipRender = false;
	if (!updateFadeLevel()) return true; // Composite the final retained frame.
	Bool newCapture = false;
	if (!W3DShaderManager::startBackendCapture(true, &newCapture)) return false;
	// A lost/resized target has no previous RGB. Seed it with a real scene once;
	// subsequent frames update only mask alpha/depth in this shared capture.
	scenePassMode = newCapture ? SCENE_PASS_DEFAULT : SCENE_PASS_ALPHA_MASK;
	m_skipRender = true;
	return true;
}

Bool ScreenCrossFadeFilter::postRender(FilterModes mode, Coord2D &scrollDelta, Bool &doExtraRender)
{
	if (m_skipRender)
	{
		m_skipRender = false;
		if (!W3DShaderManager::endBackendCapture()) return false;
		doExtraRender = true;
		return true;
	}
	TextureClass *capture = W3DShaderManager::getBackendCapture();
	IRenderBackend *backend = WW3D::Get_Render_Backend();
	if (!capture || !backend || mode <= FM_NULL_MODE || !capture->Get_Renderer_Texture().Is_Valid()) return false;
	RenderBackendTexturedVertex vertices[4];
	if (!Build_Screen_Filter_Quad(capture->Get_Width(), capture->Get_Height(), vertices)) return false;
	const unsigned short indices[6] = {0, 1, 2, 2, 1, 3};
	RenderBackendMaterialState material = Screen_Filter_Material();
	material.source_blend = RenderBackendBlendFactor::SourceAlpha;
	material.destination_blend = RenderBackendBlendFactor::InverseSourceAlpha;
	material.color_write_mask = 7; // The mask scene never changed output alpha.
	if (mode != FM_VIEW_CROSSFADE_CIRCLE)
		return backend->Draw_Indexed_Material_Triangles(vertices, 4, indices, 6, capture->Get_Renderer_Texture(), material);
	if (!m_fadePatternTexture || !m_fadePatternTexture->Ensure_Renderer_Texture()) return false;
	Real radius = (1.0f - m_curFadeValue) * 2.0f;
	if (radius <= 0.0f) radius = 0.01f;
	radius = 0.5f / radius;
	RenderBackendTerrainVertex masked[4];
	const Real u[4] = {.5f+radius, .5f+radius, .5f-radius, .5f-radius};
	const Real v[4] = {.5f+radius, .5f-radius, .5f+radius, .5f-radius};
	for (Int i=0; i<4; ++i)
		masked[i] = {vertices[i].x,vertices[i].y,vertices[i].z,1,1,1,1,vertices[i].u,vertices[i].v,u[i],v[i],0,0,0,0};
	RenderBackendTerrainState terrain;
	terrain.shroud_texture = m_fadePatternTexture->Get_Renderer_Texture();
	if (!m_fadePatternTexture->Get_Filter().Get_Render_Sampler(terrain.shroud_sampler)) return false;
	material.texture_combine = RenderBackendTextureCombine::Modulate;
	// This existing layer operation multiplies both RGB and alpha, with independent UVs.
	return backend->Draw_Indexed_Terrain_Triangles(masked,4,indices,6,capture->Get_Renderer_Texture(),material,terrain);
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
	m_priorDelta.x = m_priorDelta.y = 0.0f;
}
Int ScreenMotionBlurFilter::init()
{
	W3DFilters[FT_VIEW_MOTION_BLUR_FILTER] = this;
	return true;
}

Bool ScreenMotionBlurFilter::preRender(Bool &skipRender, CustomScenePassModes &scenePassMode)
{
	skipRender = false;
	Bool newCapture = false;
	if (!W3DShaderManager::startBackendCapture(true, &newCapture)) return false;
	// A resize or backend replacement has no retained raw scene to reuse.
	skipRender = m_skipRender && !newCapture;
	return true;
}

Bool ScreenMotionBlurFilter::postRender(FilterModes mode, Coord2D &scrollDelta, Bool &doExtraRender)
{
	TextureClass *capture = W3DShaderManager::endBackendCapture();
	IRenderBackend *backend = WW3D::Get_Render_Backend();
	if (!capture || !backend) return false;
	const RenderBackendTextureHandle texture = capture->Get_Renderer_Texture();
	RenderBackendTexturedVertex vertices[4];
	if (!Build_Screen_Filter_Quad(capture->Get_Width(), capture->Get_Height(), vertices)) return false;
	const unsigned short indices[6] = {0, 1, 2, 2, 1, 3};
	RenderBackendMaterialState material = Screen_Filter_Material();
	material.vertex_alpha = true; // Stage zero selected diffuse alpha, not capture alpha.
	material.color_write_mask = TheGlobalData->m_showSoftWaterEdge ? 7 : 15;
	Bool continueEffect = true;
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
		if (len > 0.0f) {
			center.x += 0.5f * (m_priorDelta.x/len);
			center.y -= 0.5f * (m_priorDelta.y/len);
		}
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
			vertices[i].u = ((vertices[i].u-center.x)*factor) + center.x;
			vertices[i].v = ((vertices[i].v-center.y)*factor) + center.y;
		}
	}
	if (!backend->Draw_Indexed_Material_Triangles(vertices, 4, indices, 6, texture, material)) return false;
	material.source_blend = RenderBackendBlendFactor::SourceAlpha;
	material.destination_blend = m_additive ? RenderBackendBlendFactor::One : RenderBackendBlendFactor::InverseSourceAlpha;
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
				vertices[i].a = Real(alpha) / 255.0f;
				if (pan) {
					vertices[i].u = ((vertices[i].u-center.x)*(factor+.006)) + center.x;
					vertices[i].v = ((vertices[i].v-center.y)*factor) + center.y;
				} else {
					vertices[i].u = ((vertices[i].u-center.x)*factor) + center.x;
					vertices[i].v = ((vertices[i].v-center.y)*factor) + center.y;
				}
			}
			if (!backend->Draw_Indexed_Material_Triangles(vertices, 4, indices, 6, texture, material)) return false;

		}
	}
	m_lastFrame = TheGameLogic->getFrame();
	if (pan) m_skipRender = false;
	if (!continueEffect) m_zoomToValid = false;
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
	//force WW3D2 system to set it's states so it won't later overwrite our custom settings.
	VertexMaterialClass *vmat=VertexMaterialClass::Get_Preset(VertexMaterialClass::PRELIT_DIFFUSE);
	DX8Wrapper::Set_Material(vmat);
	REF_PTR_RELEASE(vmat);	//no need to keep a reference since it's a preset.
	DX8Wrapper::Set_Texture(stage, W3DShaderManager::getShaderTexture(0));	//shroud always stored in texture 0

	if (stage == 0)
	{
#if defined(RTS_DEBUG)
	if (TheGlobalData && TheGlobalData->m_fogOfWarOn)
		DX8Wrapper::Set_Shader(ShaderClass::_PresetAlphaSpriteShader);
	else
		DX8Wrapper::Set_Shader(ShaderClass::_PresetMultiplicativeSpriteShader);
#else
	DX8Wrapper::Set_Shader(ShaderClass::_PresetMultiplicativeSpriteShader);
#endif
	}
	DX8Wrapper::Apply_Render_State_Changes();

	DX8Wrapper::Set_DX8_Texture_Stage_State(stage,  D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_CAMERASPACEPOSITION);
	DX8Wrapper::Set_DX8_Texture_Stage_State(stage,  D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ZFUNC,D3DCMP_EQUAL);

	//We need to scale so shroud texel stretches over one full terrain cell.  Each texel
	//is 1/128 the size of full texture. (assuming 128x128 vid-mem texture).
	W3DShroud *shroud;
	if ((shroud=TheTerrainRenderObject->getShroud()) != nullptr)
	{	///@todo: All this code really only need to be done once per camera/view.  Find a way to optimize it out.
		D3DMATRIX curView;
		DX8Wrapper::_Get_DX8_Transform(D3DTS_VIEW, curView);

		D3DMATRIX inv;
		float det;
		Invert_Legacy_Shader_Matrix(inv, det, curView);

		D3DMATRIX scale,offset;

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
		curView = Multiply_Legacy_Shader_Matrices(Multiply_Legacy_Shader_Matrices(inv, offset), scale);
		DX8Wrapper::_Set_DX8_Transform((D3DTRANSFORMSTATETYPE )(D3DTS_TEXTURE0+stage), curView);
	}
	m_stageOfSet=stage;
	return TRUE;
}

void ShroudTextureShader::reset()
{
	DX8Wrapper::Set_Texture(m_stageOfSet,nullptr);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ZFUNC,D3DCMP_LESSEQUAL);
	DX8Wrapper::Set_DX8_Texture_Stage_State(m_stageOfSet,  D3DTSS_TEXCOORDINDEX, m_stageOfSet);
	DX8Wrapper::Set_DX8_Texture_Stage_State(m_stageOfSet,  D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
}

// Local renderer cloud animation; projection is evaluated by canonical HLSL.
static float terrainCloudOffsetX = 0;
static float terrainCloudOffsetY = 0;

/** List of all custom shader lists - each list in this list contains variations of the same
	shader to allow it to work on different hardware configurations.
*/
W3DShaderInterface **MasterShaderList[]=
{
	ShroudShaderList,
	nullptr
};

// W3DShaderManager::W3DShaderManager =========================================
/** Constructor - just clears some variables */
//=============================================================================
W3DShaderManager::W3DShaderManager()
{
	m_currentShader = ST_INVALID;
	m_currentFilter = FT_NULL_FILTER;
	m_oldRenderSurface = nullptr;
	m_renderTexture = nullptr;
	m_newRenderSurface = nullptr;
	m_oldDepthSurface = nullptr;
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
	// Backend filters are independent of legacy native RTT/capability setup.
	screenBWFilter.init();
	screenMotionBlurFilter.init();
	screenCrossFadeFilter.init();
	terrainCloudOffsetX = terrainCloudOffsetY = 0;
	int i,j;

	D3DSURFACE_DESC desc;
	// For now, check & see if we are gf3 or higher on the food chain.

	ChipsetType res=DC_UNKNOWN;
	if ((res=W3DShaderManager::getChipset()) != 0)
	{
		m_currentChipset = res;	//cache the current chipset.

		//Some of our effects require an offscreen render target, so try creating it here.
		HRESULT hr=DX8Wrapper::_Get_D3D_Device8()->GetRenderTarget(&m_oldRenderSurface);

		if (hr != S_OK || !m_oldRenderSurface)
			return;

		m_oldRenderSurface->GetDesc(&desc);
		
		// TheSuperHackers @bugfix Redirecting rendering to a non-multisampled texture
		// while using a multisampled depth buffer is an API violation in DX8.
		if (desc.MultiSampleType == D3DMULTISAMPLE_NONE)
		{
			hr=DX8Wrapper::_Get_D3D_Device8()->CreateTexture(desc.Width,desc.Height,1,D3DUSAGE_RENDERTARGET,desc.Format,D3DPOOL_DEFAULT,&m_renderTexture);
		}
		else
		{
			// Force failure path to avoid MSAA mismatch
			hr = E_FAIL;
		}

		if (hr != S_OK)
		{
			SAFE_RELEASE(m_oldRenderSurface);
			m_renderTexture = nullptr;
		} else {
			hr = m_renderTexture->GetSurfaceLevel(0, &m_newRenderSurface);
			if (hr != S_OK)
			{
				SAFE_RELEASE(m_renderTexture);
				m_newRenderSurface = nullptr;
			}	else {
				hr = DX8Wrapper::_Get_D3D_Device8()->GetDepthStencilSurface(&m_oldDepthSurface);
				if (hr != S_OK)
				{
					SAFE_RELEASE(m_newRenderSurface);
					SAFE_RELEASE(m_renderTexture);
					m_oldDepthSurface = nullptr;
				}
			}
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
	DEBUG_LOG(("ShaderManager ChipsetID %d", res));
}

// W3DShaderManager::shutdown =======================================================
/** Any shaders which allocate resources will be allowed to free them */
//=============================================================================
void W3DShaderManager::shutdown()
{
	if (restoreBackendOutput())
	{
		REF_PTR_RELEASE(m_backendCapture);
		m_backendCaptureWidth = m_backendCaptureHeight = 0;
		m_backendCaptureHasScene = false;
	}
	SAFE_RELEASE(m_newRenderSurface);
	SAFE_RELEASE(m_renderTexture);
	SAFE_RELEASE(m_oldRenderSurface);
	SAFE_RELEASE(m_oldDepthSurface);
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
void W3DShaderManager::getTerrainNoiseProjection(float &scale, float &offsetX, float &offsetY)
{
    // The shader projects world XY; retain the existing local cloud movement.
    scale = static_cast<float>(1 / (63.0 * MAP_XY_FACTOR / 2));
    offsetX = terrainCloudOffsetX;
    offsetY = terrainCloudOffsetY;
}

void W3DShaderManager::updateCloud()
{
	const float frameTime = WW3D::Get_Logic_Frame_Time_Seconds();
	const float slideX = -0.02f;
	const float slideY = 1.50f * slideX;
	terrainCloudOffsetX += slideX * frameTime;
	terrainCloudOffsetY += slideY * frameTime;
	// Preserve the original truncate-towards-zero wrap.
	terrainCloudOffsetX -= static_cast<Int>(terrainCloudOffsetX);
	terrainCloudOffsetY -= static_cast<Int>(terrainCloudOffsetY);
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
	// Default rendering stays on the current output; no off-screen pass follows.
	if (filter == FT_VIEW_DEFAULT) return FALSE;
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
	if (filter == FT_VIEW_DEFAULT) return TRUE;
	if (W3DFilters[filter])
		return W3DFilters[filter]->setup(mode);
	return FALSE;
}

/*Draws 2 triangles covering the viewport given the current render states*/
void W3DShaderManager::drawViewport(Int color)
{
	LPDIRECT3DDEVICE8 pDev=DX8Wrapper::_Get_D3D_Device8();

	struct _TRANS_LIT_TEX_VERTEX {
		Vector4 p;
		DWORD color;   // diffuse color
		float	u;
		float	v;
	} v[4];

	Int xpos, ypos, width, height;

	TheTacticalView->getOrigin(&xpos,&ypos);
	width=TheTacticalView->getWidth();
	height=TheTacticalView->getHeight();

	//bottom right
	v[0].p = Vector4( xpos+width-0.5f, ypos+height-0.5f, 0.0f, 1.0f );
	v[0].u = (Real)(xpos+width)/(Real)TheDisplay->getWidth();	v[0].v = (Real)(ypos+height)/(Real)TheDisplay->getHeight();
	//top right
	v[1].p = Vector4( xpos+width-0.5f, ypos-0.5f, 0.0f, 1.0f );
	v[1].u = (Real)(xpos+width)/(Real)TheDisplay->getWidth();	v[1].v = (Real)(ypos)/(Real)TheDisplay->getHeight();
	//bottom left
	v[2].p = Vector4(  xpos-0.5f, ypos+height-0.5f, 0.0f, 1.0f );
	v[2].u = (Real)(xpos)/(Real)TheDisplay->getWidth();	v[2].v = (Real)(ypos+height)/(Real)TheDisplay->getHeight();
	//top left
	v[3].p = Vector4(  xpos-0.5f,  ypos-0.5f, 0.0f, 1.0f );
	v[3].u = (Real)(xpos)/(Real)TheDisplay->getWidth();	v[3].v = (Real)(ypos)/(Real)TheDisplay->getHeight();
	v[0].color = color;
	v[1].color = color;
	v[2].color = color;
	v[3].color = color;

	//draw polygons like this is very inefficient but for only 2 triangles, it's
	//not worth bothering with index/vertex buffers.
	pDev->SetVertexShader(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);

	pDev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(_TRANS_LIT_TEX_VERTEX));
}

// W3DShaderManager::startRenderToTexture =======================================================
/** Starts rendering to a texture.
 */
//=============================================================================
void W3DShaderManager::startRenderToTexture()
{
	DEBUG_ASSERTCRASH(!m_renderingToTexture, ("Already rendering to texture - cannot nest calls."));

	if (m_renderingToTexture || m_newRenderSurface==nullptr || m_oldDepthSurface==nullptr) return;
	HRESULT hr = DX8Wrapper::_Get_D3D_Device8()->SetRenderTarget(m_newRenderSurface,m_oldDepthSurface);

	// TheSuperHackers @bugfix If SetRenderTarget fails (e.g. due to MSAA forced by driver
	// profile causing a depth buffer mismatch that D3DSURFACE_DESC doesn't report), permanently
	// disable RTT to prevent repeated failures and accidental backbuffer clears.
	if (hr != S_OK)
	{
		// Permanently disable RTT
		SAFE_RELEASE(m_newRenderSurface);
		SAFE_RELEASE(m_renderTexture);
		SAFE_RELEASE(m_oldRenderSurface);
		SAFE_RELEASE(m_oldDepthSurface);
		return;
	}

	m_renderingToTexture = true;
	if (TheGlobalData->m_showSoftWaterEdge)
	{	//Soft water edges use frame buffer destination alpha so we must clear it to a known value.
		if (m_currentFilter == FT_VIEW_MOTION_BLUR_FILTER || m_currentFilter == FT_VIEW_CROSSFADE)
		{	//these filters rely on the previous frame being visible so we must be careful about clearing
			//frame buffer.  Only clear the alpha channel
			DX8Wrapper::Set_DX8_Render_State(D3DRS_COLORWRITEENABLE,D3DCOLORWRITEENABLE_ALPHA);	//only clear alpha
			ShaderClass shader=ShaderClass::_PresetOpaqueSolidShader;
			shader.Set_Depth_Compare(ShaderClass::PASS_ALWAYS);
			shader.Set_Depth_Mask(ShaderClass::DEPTH_WRITE_DISABLE);
			DX8Wrapper::Set_Shader(shader);

			VertexMaterialClass *vmat=VertexMaterialClass::Get_Preset(VertexMaterialClass::PRELIT_DIFFUSE);
			DX8Wrapper::Set_Material(vmat);
			REF_PTR_RELEASE(vmat);	//no need to keep a reference since it's a preset.

			drawViewport(0x00ffffff | (((Int)(TheWaterTransparency->m_minWaterOpacity*255.0f)) <<24));
			DX8Wrapper::Set_DX8_Render_State(D3DRS_COLORWRITEENABLE,D3DCOLORWRITEENABLE_RED|D3DCOLORWRITEENABLE_GREEN|D3DCOLORWRITEENABLE_BLUE);	//disable writes to alpha
		}
		else	//normal clear that overwrites everything.
			DX8Wrapper::Clear(true, false, Vector3( 0.0f, 0.0f, 0.0f ), TheWaterTransparency->m_minWaterOpacity);
	}
}

// W3DShaderManager::startRenderToTexture =======================================================
/** Ends rendering to a texture.
 */
//=============================================================================
IDirect3DTexture8 *W3DShaderManager::endRenderToTexture()
{
	DEBUG_ASSERTCRASH(m_renderingToTexture, ("Not rendering to texture."));
	if (!m_renderingToTexture) return nullptr;
	HRESULT hr = DX8Wrapper::_Get_D3D_Device8()->SetRenderTarget(m_oldRenderSurface,m_oldDepthSurface);	//restore original render target
	DEBUG_ASSERTCRASH(hr==S_OK, ("Set target failed unexpectedly."));
	if (hr == S_OK)
	{
		//assume render target texture will be in stage 0.  Most hardware has "conditional" support for
		//non-power-of-2 textures so we must force some required states:
		DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_ADDRESSW, D3DTADDRESS_CLAMP);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
		DX8Wrapper::Set_DX8_Texture_Stage_State( 0, D3DTSS_MIPFILTER, D3DTEXF_NONE);

		m_renderingToTexture = false;
	}
	return m_renderTexture;
}

/**Returns texture containing the image that was last rendered using any of the effects requiring render target
textures.  Used mostly for cross-fading effects that need an unmodified version of the view before the effect
was applied.  NOTE: This texture does not survive device reset.. so quit effect on reset!*/
IDirect3DTexture8 *W3DShaderManager::getRenderTexture()
{
	return m_renderTexture;
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

	ChipsetType chip=DC_UNKNOWN;
	IDirect3D8* d3d8Interface=DX8Wrapper::_Get_D3D8();

	if (d3d8Interface && DX8Wrapper::_Get_D3D_Device8())
	{

		D3DADAPTER_IDENTIFIER8 did;
		::ZeroMemory(&did, sizeof(D3DADAPTER_IDENTIFIER8));
	/*	HRESULT res = */ d3d8Interface->GetAdapterIdentifier(0,D3DENUM_NO_WHQL_LEVEL,&did);
		*((LARGE_INTEGER*)&m_driverVersion) = did.DriverVersion;

		if(did.VendorId == DC_NVIDIA_VENDOR_ID)
		{
			m_currentVendor = DC_NVIDIA_VENDOR_ID;

			if (did.DeviceId == 0x20)
				return DC_TNT;

			if (did.DeviceId >= 0x28 && did.DeviceId < 0x100)
				return DC_TNT2;

			if ( (did.DeviceId >= 0x100 && did.DeviceId <= 0x103) ||	//GeForce
				 (did.DeviceId >= 0x110 && did.DeviceId <= 0x113) ||	//GeForce2 MX
						 (did.DeviceId >= 0x150 && did.DeviceId <= 0x153) )	//GeForce2
           		return DC_GEFORCE2;

			if (did.DeviceId >= 0x200 && did.DeviceId < 0x250)
				return DC_GEFORCE3;

			if (did.DeviceId >= 0x250)
				return DC_GEFORCE4;
		}
		else
		if(did.VendorId == DC_3DFX_VENDOR_ID)
		{
			m_currentVendor = DC_3DFX_VENDOR_ID;

			if (did.DeviceId == 0x0002)
				return DC_VOODOO2;
			if (did.DeviceId == 0x0005)
				return DC_VOODOO3;
			if (did.DeviceId == 0x0008)	///@todo: Just guessing on this one - find actual Voodoo4 deviceID.
				return DC_VOODOO4;
			if (did.DeviceId == 0x0009)
				return DC_VOODOO5;
		}
		else
		if(did.VendorId == DC_ATI_VENDOR_ID)
		{
			m_currentVendor = DC_ATI_VENDOR_ID;

			if (did.DeviceId == 0x5144)
				return DC_RADEON;
			if (did.DeviceId == 0x514C)
				return DC_RADEON_8500;
			if (did.DeviceId == 0x4e44)
				return DC_RADEON_9700;
		}

		//None of the vendor specific ID's matched so use generic means to classify the card
		Int maxTextures=DX8Wrapper::Get_Current_Caps()->Get_Max_Simultaneous_Textures();
		Real pixelShaderVersion;

		char buf[256];

		//Convert version to Real
		sprintf(buf,"%d.%d",DX8Wrapper::Get_Current_Caps()->Get_Pixel_Shader_Major_Version(),DX8Wrapper::Get_Current_Caps()->Get_Pixel_Shader_Minor_Version());
		sscanf(buf,"%f",&pixelShaderVersion);

		if (maxTextures >= 4)
		{	if (pixelShaderVersion >= 1.1f)
				chip=DC_GENERIC_PIXEL_SHADER_1_1;
			if (pixelShaderVersion >= 1.4f)
				chip=DC_GENERIC_PIXEL_SHADER_1_4;
			if (maxTextures >= 8 && pixelShaderVersion >= 2.0f)
				chip=DC_GENERIC_PIXEL_SHADER_2_0;
		}
	}

	return chip;
}

//=============================================================================
// WaterRenderObjClass::LoadAndCreateShader
//=============================================================================
/** Loads and creates a D3D pixel or vertex shader.*/
//=============================================================================
HRESULT W3DShaderManager::LoadAndCreateD3DShader(const char* strFilePath, const DWORD* pDeclaration, DWORD Usage, Bool ShaderType, DWORD* pHandle)
{
	if (getChipset() < DC_GENERIC_PIXEL_SHADER_1_1)
		return E_FAIL;	//don't allow loading any shaders if hardware can't handle it.

	try
	{
		File *file = nullptr;
		HRESULT hr;

		file = TheFileSystem->openFile(strFilePath, File::READ | File::BINARY);
		if (file == nullptr)
		{
			OutputDebugString("Could not find file \n" );
			return E_FAIL;
		}

		FileInfo fileInfo;
		TheFileSystem->getFileInfo(AsciiString(strFilePath), &fileInfo);
		DWORD dwFileSize = fileInfo.sizeLow;

		const DWORD* pShader = (DWORD*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dwFileSize);
		if (!pShader)
		{
			OutputDebugString( "Failed to allocate memory to load shader\n " );
			return E_FAIL;
		}

		file->read((void *)pShader, dwFileSize);

		file->close();
		file = nullptr;

		if (ShaderType) // SHADERTYPE_VERTEX
		{
			hr = DX8Wrapper::_Get_D3D_Device8()->CreateVertexShader(pDeclaration, pShader, pHandle, Usage);
		}
		else // SHADERTYPE_PIXEL
		{
			hr = DX8Wrapper::_Get_D3D_Device8()->CreatePixelShader(pShader, pHandle);
		}

		HeapFree(GetProcessHeap(), 0, (void*)pShader);

		if (FAILED(hr))
		{
			OutputDebugString( "Failed to create shader\n ");
			return E_FAIL;
		}
	}
	catch(...)
	{
		OutputDebugString( "Error opening file \n" );
		return E_FAIL;
	}

	return S_OK;
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
