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

// FILE: W3DShaderManager.h /////////////////////////////////////////////////////////
//
// Custom shader system that allows more options and easier device validation than
// possible with W3D2.
//
// Author: Mark Wilczynski, August 2001
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "WW3D2/texture.h"
enum FilterTypes CPP_11(: Int);
enum FilterModes CPP_11(: Int);
enum CustomScenePassModes CPP_11(: Int);
enum StaticGameLODLevel CPP_11(: Int);
enum ChipsetType CPP_11(: Int);
enum CpuType CPP_11(: Int);
enum GraphicsVenderID CPP_11(: Int);

class TextureClass;	///forward reference
/** System for managing complex rendering settings which are either not handled by
	WW3D2 or need custom paths depending on the video card.  This system will determine
	the proper shader given video card limitations and also allow the app to query the
	hardware for specific features.
*/
class W3DShaderManager
{
public:

	//put any custom shaders (not going through W3D) in here.
	enum ShaderTypes
	{	ST_INVALID,			//invalid shader type.
		ST_SHROUD_TEXTURE,		//shader to apply shroud texture projection.
		ST_FLAT_TERRAIN_BASE,	//shader to apply base terrain texture only
		ST_FLAT_TERRAIN_BASE_NOISE1,	//shader to apply base texture and cloud/noise 1.
		ST_FLAT_TERRAIN_BASE_NOISE2,	//shader to apply base texture and cloud/noise 2.
		ST_FLAT_TERRAIN_BASE_NOISE12,//shader to apply base texture and both cloud/noise
		ST_MAX
	};


	W3DShaderManager();	///<constructor
	static void init();	///<determine optimal shaders for current device.
	static void shutdown();	///<release resources used by shaders
	static void getTerrainNoiseProjection(float &scale, float &offsetX, float &offsetY);
	static void updateCloud();	///<update the cloud position once every render frame.

	static ChipsetType getChipset();	///<return current device chipset.
	static GraphicsVenderID getCurrentVendor() {return m_currentVendor;}	///<return current card vendor.
	static __int64 getCurrentDriverVersion() {return m_driverVersion; }	///<return current driver version.
	static Int getShaderPasses(ShaderTypes shader);	///<rendering passes required for shader
	static Int setShader(ShaderTypes shader, Int pass);	///<enable specific shader pass.
	static void resetShader(ShaderTypes shader);	///<make sure W3D2 gets restored to normal
	///Specify all textures (up to 8) which can be accessed by the shaders.
	static void setTexture(Int stage,TextureClass* texture) {m_Textures[stage]=texture;}
	///Return current texture available to shaders.
	static TextureClass *getShaderTexture(Int stage) { return m_Textures[stage];}	///<returns currently selected texture for given stage
	///Return last activated shader.
	static ShaderTypes getCurrentShader() {return m_currentShader;}
	/// Loads a .vso file and creates a vertex shader for it
	static HRESULT LoadAndCreateD3DShader(const char* strFilePath, const DWORD* pDeclaration, DWORD Usage, Bool ShaderType, DWORD* pHandle);

	static Bool testMinimumRequirements(ChipsetType *videoChipType, CpuType *cpuType, Int *cpuFreq, MemValueType *numRAM, Real *intBenchIndex, Real *floatBenchIndex, Real *memBenchIndex);
	static StaticGameLODLevel getGPUPerformanceIndex();
	static Real GetCPUBenchTime();

	// Filter methods
	static Bool filterPreRender(FilterTypes filter, Bool &skipRender, CustomScenePassModes &scenePassMode); ///< Set up at start of render.  Only applies to screen filter shaders.
	static Bool filterPostRender(FilterTypes filter, FilterModes mode, Coord2D &scrollDelta, Bool &doExtraRender); ///< Called after render.  Only applies to screen filter shaders.
	static Bool filterSetup(FilterTypes filter, FilterModes mode);

	// Support routines for filter methods.
	static Bool canRenderToTexture() { return (m_oldRenderSurface && m_newRenderSurface);}
	static void startRenderToTexture(); ///< Sets render target to texture.
	static IDirect3DTexture8 * endRenderToTexture(); ///< Ends render to texture, & returns texture.
	static IDirect3DTexture8 * getRenderTexture();	///< returns last used render target texture
	static Bool isRenderingToTexture() {return m_renderingToTexture; }
	static void drawViewport(Int color);	///<draws 2 triangles covering the current tactical viewport
	static Bool startBackendCapture(Bool preserveColor, Bool *newCapture = nullptr);
	static Bool restoreBackendOutput();
	static TextureClass *endBackendCapture();
	static TextureClass *getBackendCapture() {return m_backendCaptureHasScene ? m_backendCapture : nullptr;}


protected:
	static TextureClass *m_Textures[8];	///textures assigned to each of the possible stages
	static ChipsetType m_currentChipset;	///<last video card chipset that was detected.
	static GraphicsVenderID m_currentVendor;	///<last video card vendor
	static __int64 m_driverVersion;			///<driver version of last chipset.
	static ShaderTypes m_currentShader;	///<last shader that was set.
	static Int m_currentShaderPass;		///<pass of last shader that was set.

	static FilterTypes m_currentFilter; ///< Last filter that was set.
	// Info for a render to texture surface for special effects.
	static Bool m_renderingToTexture;
	static IDirect3DSurface8 *m_oldRenderSurface;	///<previous render target
	static IDirect3DTexture8 *m_renderTexture;		///<texture into which rendering will be redirected.
	static IDirect3DSurface8 *m_newRenderSurface;	///<new render target inside m_renderTexture
	static IDirect3DSurface8 *m_oldDepthSurface;	///<previous depth buffer surface
	static TextureClass *m_backendCapture;
	static IRenderBackend *m_backendCaptureOwner;
	static Int m_backendCaptureWidth, m_backendCaptureHeight;
	static Bool m_backendCapturing;
	static Bool m_backendCaptureHasScene;
	static unsigned int m_backendOutputWriteMask;


};

class W3DFilterInterface
{
public:
	virtual Int init() = 0;			///<perform any one time initialization and validation
	virtual Int shutdown() { return TRUE;};			///<release resources used by shader
	virtual Bool preRender(Bool &skipRender, CustomScenePassModes &scenePassMode) {skipRender=false; return false;} ///< Set up at start of render.  Only applies to screen filter shaders.
	virtual Bool postRender(FilterModes mode, Coord2D &scrollDelta, Bool &doExtraRender){return false;} ///< Called after render.  Only applies to screen filter shaders.
	virtual Bool setup(FilterModes mode){return false;} ///< Called when the filter is started, one time before the first prerender.
};


/*=========  ScreenMotionBlurFilter	=============================================================*/
///applies motion blur to viewport.
class ScreenMotionBlurFilter : public W3DFilterInterface
{
public:
	virtual Int init() override;			///<perform any one time initialization and validation
	virtual Bool preRender(Bool &skipRender, CustomScenePassModes &scenePassMode) override; ///< Set up at start of render.  Only applies to screen filter shaders.
	virtual Bool postRender(FilterModes mode, Coord2D &scrollDelta, Bool &doExtraRender) override; ///< Called after render.  Only applies to screen filter shaders.
	virtual Bool setup(FilterModes mode) override; ///< Called when the filter is started, one time before the first prerender.
	ScreenMotionBlurFilter();

	static void setZoomToPos(const Coord3D *pos) {m_zoomToPos = *pos; m_zoomToValid = true;}

protected:
	enum {MAX_COUNT = 60,
				MAX_LIMIT = 30,
				COUNT_STEP = 5,
				DEFAULT_PAN_FACTOR = 30};
	Int m_maxCount;
	Int m_lastFrame;
	Bool m_decrement;
	Bool m_skipRender;
	Bool m_additive;
	Bool m_doZoomTo;
	Coord2D m_priorDelta;
	Int m_panFactor;


	static Coord3D m_zoomToPos;
	static Bool m_zoomToValid;
} ;

/*=========  ScreenBWFilter	=============================================================*/
///converts viewport to black & white.
class ScreenBWFilter : public W3DFilterInterface
{
public:
	virtual Int init() override;			///<perform any one time initialization and validation
	virtual Bool preRender(Bool &skipRender, CustomScenePassModes &scenePassMode) override; ///< Set up at start of render.  Only applies to screen filter shaders.
	virtual Bool postRender(FilterModes mode, Coord2D &scrollDelta,Bool &doExtraRender) override; ///< Called after render.  Only applies to screen filter shaders.
	virtual Bool setup(FilterModes mode) override {return true;} ///< Called when the filter is started, one time before the first prerender.
	static void setFadeParameters(Int fadeFrames, Int direction)
	{
		m_curFadeFrame = 0;
		m_fadeFrames = fadeFrames;
		m_fadeDirection = direction;
	}
protected:
	void advanceFade();
	static Int m_fadeFrames;
	static Int m_fadeDirection;
	static Int m_curFadeFrame;
	static Real m_curFadeValue;
};

/*=========  ScreenCrossFadeFilter	=============================================================*/
///Fades between 2 different rendered frames.
class ScreenCrossFadeFilter : public W3DFilterInterface
{
public:
	virtual Int init() override;			///<perform any one time initialization and validation
	virtual Int shutdown() override;		///<release resources used by shader
	virtual Bool preRender(Bool &skipRender, CustomScenePassModes &scenePassMode) override; ///< Set up at start of render.  Only applies to screen filter shaders.
	virtual Bool postRender(FilterModes mode, Coord2D &scrollDelta,Bool &doExtraRender) override; ///< Called after render.  Only applies to screen filter shaders.
	virtual Bool setup(FilterModes mode) override {return true;} ///< Called when the filter is started, one time before the first prerender.
	static void setFadeParameters(Int fadeFrames, Int direction)
	{
		m_curFadeFrame = 0;
		m_fadeFrames = fadeFrames;
		m_fadeDirection = direction;
	}
	static Real getCurrentFadeValue()	{ return m_curFadeValue;}
	static TextureClass *getCurrentMaskTexture() { return m_fadePatternTexture;}
protected:
	Bool updateFadeLevel();		///<updated current state of fade and return true if not finished.
	static Int m_fadeFrames;
	static Int m_fadeDirection;
	static Int m_curFadeFrame;
	static Real m_curFadeValue;
	static Bool m_skipRender;
	static TextureClass *m_fadePatternTexture;	///<shape/pattern of the fade
};
