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

/***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : WW3D                                                         *
 *                                                                                             *
 *                     $Archive:: /Commando/Code/ww3d2/texture.cpp                            $*
 *                                                                                             *
 *                  $Org Author:: Steve_t                                                     $*
 *                                                                                             *
 *                       Author : Kenny Mitchell                                               *
 *                                                                                             *
 *                     $Modtime:: 08/05/02 1:27p                                              $*
 *                                                                                             *
 *                    $Revision:: 85                                                          $*
 *                                                                                             *
 * 06/27/02 KM Texture class abstraction																			*
 * 08/05/02 KM Texture class redesign (revisited)
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 *   FileListTextureClass::Load_Frame_Surface -- Load source texture                           *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include "texture.h"

#include "WWLib/TARGA.h"
#include <WWLib/nstrdup.h>
#include "w3d_file.h"
#include "assetmgr.h"
#include "bitmaphandler.h"
#include "textureloader.h"
#include "missingtexture.h"
#include "WWLib/ffactory.h"
#include "texturethumbnail.h"
#include "WWDebug/wwprofile.h"
#include "ww3d.h"
#include <cstring>
#include <vector>

const unsigned DEFAULT_INACTIVATION_TIME=20000;

/*
** Definitions of static members:
*/

static unsigned unused_texture_id;

// This throttles submissions to the background texture loading queue.
static unsigned TexturesAppliedPerFrame;
const unsigned MAX_TEXTURES_APPLIED_PER_FRAME=2;


/*!
 * KM General base constructor for texture classes
 */
TextureBaseClass::TextureBaseClass
(
	unsigned int width,
	unsigned int height,
	enum MipCountType mip_level_count,
	enum PoolType pool,
	bool rendertarget,
	bool reducible
)
:	MipLevelCount(mip_level_count),
	Initialized(false),
   Name(""),
	FullPath(""),
	texture_id(unused_texture_id++),
	IsLightmap(false),
	IsProcedural(false),
	IsReducible(reducible),
	IsCompressionAllowed(false),
	InactivationTime(0),
	ExtendedInactivationTime(0),
	LastInactivationSyncTime(0),
	LastAccessed(0),
	Width(width),
	Height(height),
	Pool(pool),
	Dirty(false),
	TextureLoadTask(nullptr),
	ThumbnailLoadTask(nullptr),
	HSVShift(0.0f,0.0f,0.0f)
{
}


//**********************************************************************************************
//! Base texture class destructor
/*! KJM
*/
TextureBaseClass::~TextureBaseClass()
{
	delete TextureLoadTask;
	TextureLoadTask=nullptr;
	delete ThumbnailLoadTask;
	ThumbnailLoadTask=nullptr;

	if (RendererTexture.Is_Valid()) {
		// WW3D may already have shut down. A new backend rejects the old generation.
		if (RendererOwner == WW3D::Get_Render_Backend())
			RendererOwner->Release_Texture(RendererTexture);
		RendererTexture = {};
		RendererOwner = nullptr;
		RendererMipLevelCount = 0;
	}
}

RenderBackendTextureHandle TextureBaseClass::Get_Renderer_Texture() const
{
	LastAccessed = WW3D::Get_Sync_Time();
	IRenderBackend *backend = WW3D::Get_Render_Backend();
	return backend != nullptr && RendererOwner == backend && backend->Is_Texture_Valid(RendererTexture)
		? RendererTexture : RenderBackendTextureHandle{};
}

TextureClass::TextureClass(unsigned width, unsigned height, RenderBackendTextureHandle texture, IRenderBackend *owner)
	: TextureBaseClass(width, height, MIP_LEVELS_1, POOL_DEFAULT, true, false),
	  TextureFormat(WW3D_FORMAT_A8R8G8B8), Filter(MIP_LEVELS_1)
{
	RendererTexture = texture;
	RendererOwner = owner;
	RendererMipLevelCount = 1;
	Initialized = IsProcedural = true;
	LastAccessed = WW3D::Get_Sync_Time();
}

bool TextureClass::Ensure_Renderer_Texture()
{
	if (Get_Renderer_Texture().Is_Valid()) return true;
	IRenderBackend *backend = WW3D::Get_Render_Backend();
	if (backend == nullptr || !TextureLoader::Is_Render_Thread() || Get_Texture_Name().Is_Empty()) return false;
	// Signed bump data needs its actual caller/shader migration; it is not a color image.
	if (TextureFormat == WW3D_FORMAT_U8V8 || TextureFormat == WW3D_FORMAT_L6V5U5 ||
		TextureFormat == WW3D_FORMAT_X8L8V8U8) return false;
	std::vector<TextureLoader::RGBA8MipLevel> images;
	const bool missing = !TextureLoader::Load_RGBA8_Mip_Chain(Get_Full_Path(), MipLevelCount,
		IsReducible, IsCompressionAllowed, HSVShift, images);
	if (missing) {
		images.resize(1);
		MissingTexture::Create_RGBA8_Image(images[0].width, images[0].height, images[0].pixels);
	}
	std::vector<RenderBackendTextureMipLevel> levels;
	levels.reserve(images.size());
	for (const auto &image : images) levels.push_back({image.width, image.height, image.width*4, image.pixels.data()});
	const auto handle = backend->Create_Static_RGBA8_Texture(levels.data(), static_cast<unsigned>(levels.size()));
	if (!handle.Is_Valid()) return false;
	RendererOwner = backend; RendererTexture = handle; RendererTextureMissing = missing;
	RendererMipLevelCount = static_cast<unsigned>(images.size());
	Width = images[0].width; Height = images[0].height; TextureFormat = WW3D_FORMAT_A8R8G8B8;
	Initialized = true; LastAccessed = WW3D::Get_Sync_Time();
	return true;
}

bool TextureClass::Copy_From(const TextureClass &source)
{
	IRenderBackend *backend = WW3D::Get_Render_Backend();
	return backend != nullptr && backend == RendererOwner && backend == source.RendererOwner &&
		backend->Copy_Texture(RendererTexture, source.RendererTexture);
}




//**********************************************************************************************
//! Invalidate old unused textures
/*!
*/
void TextureBaseClass::Invalidate_Old_Unused_Textures(unsigned invalidation_time_override)
{
	// (gth) If thumbnails are not enabled, then we don't run this code.
	if (WW3D::Get_Thumbnail_Enabled() == false) {
		return;
	}

	// Zero the texture apply count in this function because this is called every frame...(this wasn't in E&B main branch KJM)
	TexturesAppliedPerFrame=0;

	unsigned synctime=WW3D::Get_Sync_Time();
	HashTemplateIterator<StringClass,TextureClass*> ite(WW3DAssetManager::Get_Instance()->Texture_Hash());
	// Loop through all the textures in the manager

	for (ite.First ();!ite.Is_Done();ite.Next ())
	{
		TextureClass* tex=ite.Peek_Value();

		// Consider invalidating if texture has been initialized and defines inactivation time
		if (tex->Initialized && tex->InactivationTime)
		{
			unsigned age=synctime-tex->LastAccessed;

			if (invalidation_time_override)
			{
				if (age>invalidation_time_override)
				{
					tex->Invalidate();
					tex->LastInactivationSyncTime=synctime;
				}
			}
			else
			{
				// Not used in the last n milliseconds?
				if (age>(tex->InactivationTime+tex->ExtendedInactivationTime))
				{
					tex->Invalidate();
					tex->LastInactivationSyncTime=synctime;
				}
			}
		}
	}
}




//**********************************************************************************************
//! Invalidate this texture
/*!
*/
void TextureBaseClass::Invalidate()
{
	if (TextureLoadTask) {
		return;
	}
	if (ThumbnailLoadTask) {
		return;
	}

	// Don't invalidate procedural textures
	if (IsProcedural) {
		return;
	}

	if (RendererTexture.Is_Valid()) {
		if (RendererOwner == WW3D::Get_Render_Backend()) RendererOwner->Release_Texture(RendererTexture);
		RendererTexture = {}; RendererOwner = nullptr; RendererMipLevelCount = 0;
	}
	Initialized=false;

	LastAccessed=WW3D::Get_Sync_Time();
}

//**********************************************************************************************
//! Load locked surface
/*!
*/
void TextureBaseClass::Load_Locked_Surface()
{
	WWPROFILE(("TextureClass::Load_Locked_Surface()"));
	TextureLoader::Request_Thumbnail(this);
	Initialized=false;
}


//**********************************************************************************************
//! Is missing texture
/*!
*/
bool TextureBaseClass::Is_Missing_Texture()
{
	if (RendererTexture.Is_Valid()) return RendererTextureMissing;
	return false;
}


//**********************************************************************************************
//! Set texture name
/*!
*/
void TextureBaseClass::Set_Texture_Name(const char * name)
{
	Name=name;
}




//**********************************************************************************************
//! Get priority
/*!
*/
unsigned int TextureBaseClass::Get_Priority()
{
	return 0;
}


//**********************************************************************************************
//! Set priority
/*!
*/
unsigned int TextureBaseClass::Set_Priority(unsigned int priority)
{
	(void)priority;
	return 0;
}


//**********************************************************************************************
//! Get reduction mip levels
/*!
*/
unsigned TextureBaseClass::Get_Reduction() const
{
	// don't reduce if the texture is too small already or
	// has no mip map levels
	if (MipLevelCount==MIP_LEVELS_1) return 0;
	if (Width <= 32 || Height <= 32) return 0;

	int reduction=WW3D::Get_Texture_Reduction();

	// 'large texture extra reduction' causes textures above 256x256 to be reduced one more step.
	if (WW3D::Is_Large_Texture_Extra_Reduction_Enabled() && (Width > 256 || Height > 256)) {
		reduction++;
	}
	if (MipLevelCount && reduction>MipLevelCount) {
		reduction=MipLevelCount;
	}
	return reduction;
}



//**********************************************************************************************
//! Apply null texture state
/*!
*/
void TextureBaseClass::Apply_Null(unsigned int stage)
{
	// Renderer-neutral: no fixed-function null texture. Sampler state is
	// consumed via Get_Render_Sampler during backend draws.
	(void)stage;
}

// ----------------------------------------------------------------------------
// Setting HSV_Shift value is always relative to the original texture. This function invalidates the
// texture surface and causes the texture to be reloaded. For thumbnailable textures, the hue shifting
// is done in the background loading thread.
// ----------------------------------------------------------------------------
void TextureBaseClass::Set_HSV_Shift(const Vector3 &hsv_shift)
{
	Invalidate();
	HSVShift=hsv_shift;
}

//**********************************************************************************************
//! Get total locked surface size
/*! KM
*/
int TextureBaseClass::_Get_Total_Locked_Surface_Size()
{
	int total_locked_surface_size=0;

	HashTemplateIterator<StringClass,TextureClass*> ite(WW3DAssetManager::Get_Instance()->Texture_Hash());
	// Loop through all the textures in the manager
	for (ite.First ();!ite.Is_Done();ite.Next ())
	{
		// Get the current texture
		TextureBaseClass* tex=ite.Peek_Value();
		if (!tex->Initialized)
		{
			total_locked_surface_size+=tex->Get_Texture_Memory_Usage();
		}
	}
	return total_locked_surface_size;
}

//**********************************************************************************************
//! Get total texture size
/*! KM
*/
int TextureBaseClass::_Get_Total_Texture_Size()
{
	int total_texture_size=0;

	HashTemplateIterator<StringClass,TextureClass*> ite(WW3DAssetManager::Get_Instance()->Texture_Hash());
	// Loop through all the textures in the manager
	for (ite.First ();!ite.Is_Done();ite.Next ())
	{
		// Get the current texture
		TextureBaseClass* tex=ite.Peek_Value();
		total_texture_size+=tex->Get_Texture_Memory_Usage();
	}
	return total_texture_size;
}

// ----------------------------------------------------------------------------


//**********************************************************************************************
//! Get total lightmap texture size
/*!
*/
int TextureBaseClass::_Get_Total_Lightmap_Texture_Size()
{
	int total_texture_size=0;

	HashTemplateIterator<StringClass,TextureClass*> ite(WW3DAssetManager::Get_Instance()->Texture_Hash());
	// Loop through all the textures in the manager
	for (ite.First ();!ite.Is_Done();ite.Next ())
	{
		// Get the current texture
		TextureBaseClass* tex=ite.Peek_Value();
		if (tex->Is_Lightmap())
		{
			total_texture_size+=tex->Get_Texture_Memory_Usage();
		}
	}
	return total_texture_size;
}


//**********************************************************************************************
//! Get total procedural texture size
/*!
*/
int TextureBaseClass::_Get_Total_Procedural_Texture_Size()
{
	int total_texture_size=0;

	HashTemplateIterator<StringClass,TextureClass*> ite(WW3DAssetManager::Get_Instance()->Texture_Hash());
	// Loop through all the textures in the manager
	for (ite.First ();!ite.Is_Done();ite.Next ())
	{
		// Get the current texture
		TextureBaseClass* tex=ite.Peek_Value();
		if (tex->Is_Procedural())
		{
			total_texture_size+=tex->Get_Texture_Memory_Usage();
		}
	}
	return total_texture_size;
}

//**********************************************************************************************
//! Get total texture count
/*!
*/
int TextureBaseClass::_Get_Total_Texture_Count()
{
	int texture_count=0;

	HashTemplateIterator<StringClass,TextureClass*> ite(WW3DAssetManager::Get_Instance()->Texture_Hash());
	// Loop through all the textures in the manager
	for (ite.First ();!ite.Is_Done();ite.Next ())
	{
		texture_count++;
	}

	return texture_count;
}

// ----------------------------------------------------------------------------


//**********************************************************************************************
//! Get total light map texture count
/*!
*/
int TextureBaseClass::_Get_Total_Lightmap_Texture_Count()
{
	int texture_count=0;

	HashTemplateIterator<StringClass,TextureClass*> ite(WW3DAssetManager::Get_Instance()->Texture_Hash());
	// Loop through all the textures in the manager
	for (ite.First ();!ite.Is_Done();ite.Next ())
	{
		if (ite.Peek_Value()->Is_Lightmap())
		{
			texture_count++;
		}
	}

	return texture_count;
}

//**********************************************************************************************
//! Get total procedural texture count
/*!
*/
int TextureBaseClass::_Get_Total_Procedural_Texture_Count()
{
	int texture_count=0;

	HashTemplateIterator<StringClass,TextureClass*> ite(WW3DAssetManager::Get_Instance()->Texture_Hash());
	// Loop through all the textures in the manager
	for (ite.First ();!ite.Is_Done();ite.Next ())
	{
		if (ite.Peek_Value()->Is_Procedural())
		{
			texture_count++;
		}
	}

	return texture_count;
}


//**********************************************************************************************
//! Get total locked surface count
/*!
*/
int TextureBaseClass::_Get_Total_Locked_Surface_Count()
{
	int texture_count=0;

	HashTemplateIterator<StringClass,TextureClass*> ite(WW3DAssetManager::Get_Instance()->Texture_Hash());
	// Loop through all the textures in the manager
	for (ite.First ();!ite.Is_Done();ite.Next ())
	{
		// Get the current texture
		TextureBaseClass* tex=ite.Peek_Value();
		if (!tex->Initialized)
		{
			texture_count++;
		}
	}

	return texture_count;
}

/*************************************************************************
**                             TextureClass
*************************************************************************/
TextureClass::TextureClass
(
	unsigned width,
	unsigned height,
	WW3DFormat format,
	MipCountType mip_level_count,
	PoolType pool,
	bool rendertarget,
	bool allow_reduction
)
:	TextureBaseClass(width, height, mip_level_count, pool, rendertarget,allow_reduction),
	Filter(mip_level_count),
	TextureFormat(format)
{
	Initialized=true;
	IsProcedural=true;
	IsReducible=false;

	switch (format)
	{
	case WW3D_FORMAT_DXT1:
	case WW3D_FORMAT_DXT2:
	case WW3D_FORMAT_DXT3:
	case WW3D_FORMAT_DXT4:
	case WW3D_FORMAT_DXT5:
		IsCompressionAllowed=true;
		break;
	default : break;
	}

	// Signed bump data is not a color image; retain format with no upload.
	if (format == WW3D_FORMAT_U8V8 || format == WW3D_FORMAT_L6V5U5 ||
		format == WW3D_FORMAT_X8L8V8U8) {
		LastAccessed=WW3D::Get_Sync_Time();
		return;
	}

	IRenderBackend *backend = WW3D::Get_Render_Backend();
	if (backend != nullptr && TextureLoader::Is_Render_Thread()) {
		if (rendertarget) {
			const auto handle = backend->Create_Render_Texture(width, height);
			if (handle.Is_Valid()) {
				RendererOwner = backend;
				RendererTexture = handle;
				RendererMipLevelCount = 1;
				RendererTextureMissing = false;
			}
		} else if (width != 0 && height != 0) {
			std::vector<unsigned char> blank(static_cast<std::size_t>(width) * height * 4, 0);
			RenderBackendTextureMipLevel level{width, height, width*4, blank.data()};
			const auto handle = backend->Create_Static_RGBA8_Texture(&level, 1);
			if (handle.Is_Valid()) {
				RendererOwner = backend;
				RendererTexture = handle;
				RendererMipLevelCount = 1;
				RendererTextureMissing = false;
				TextureFormat = WW3D_FORMAT_A8R8G8B8;
			}
		}
	}
	LastAccessed=WW3D::Get_Sync_Time();
}



// ----------------------------------------------------------------------------
TextureClass::TextureClass
(
	const char *name,
	const char *full_path,
	MipCountType mip_level_count,
	WW3DFormat texture_format,
	bool allow_compression,
	bool allow_reduction
)
:	TextureBaseClass(0, 0, mip_level_count),
	Filter(mip_level_count),
	TextureFormat(texture_format)
{
	IsCompressionAllowed=allow_compression;
	InactivationTime=DEFAULT_INACTIVATION_TIME;		// Default inactivation time 30 seconds
	IsReducible=allow_reduction;

	switch (TextureFormat)
	{
	case WW3D_FORMAT_DXT1:
	case WW3D_FORMAT_DXT2:
	case WW3D_FORMAT_DXT3:
	case WW3D_FORMAT_DXT4:
	case WW3D_FORMAT_DXT5:
		IsCompressionAllowed=true;
		break;
	case WW3D_FORMAT_U8V8:		// Bumpmap
	case WW3D_FORMAT_L6V5U5:	// Bumpmap
	case WW3D_FORMAT_X8L8V8U8:	// Bumpmap
		// Retain the signed format so an unported bump caller cannot become a color texture.
		IsCompressionAllowed=false;
		MipLevelCount=MIP_LEVELS_1;
		Filter.Set_Mip_Mapping(TextureFilterClass::FILTER_TYPE_NONE);
		break;
	default:	break;
	}

	WWASSERT_PRINT(name && name[0], "TextureClass CTor: null or empty texture name");
	int len=strlen(name);
	for (int i=0;i<len;++i)
	{
		if (name[i]=='+')
		{
			IsLightmap=true;

			// Set bilinear filtering for lightmaps (they are very stretched and
			// low detail so we don't care for anisotropic or trilinear filtering...)
			Filter.Set_Min_Filter(TextureFilterClass::FILTER_TYPE_FAST);
			Filter.Set_Mag_Filter(TextureFilterClass::FILTER_TYPE_FAST);
			if (mip_level_count!=MIP_LEVELS_1) Filter.Set_Mip_Mapping(TextureFilterClass::FILTER_TYPE_FAST);
			break;
		}
	}
	Set_Texture_Name(name);
	Set_Full_Path(full_path);
	WWASSERT(name[0]!='\0');
	if (!WW3D::Is_Texturing_Enabled())
	{
		Initialized=true;
	}

	// Find original size from the thumbnail (but don't create thumbnail texture yet!)
	ThumbnailClass* thumb=ThumbnailManagerClass::Peek_Thumbnail_Instance_From_Any_Manager(Get_Full_Path());
	if (thumb)
	{
		Width=thumb->Get_Original_Texture_Width();
		Height=thumb->Get_Original_Texture_Height();

	}

	LastAccessed=WW3D::Get_Sync_Time();

	// If the thumbnails are not enabled, init the texture at this point to avoid stalling when the
	// mesh is rendered.
	if (!WW3D::Get_Thumbnail_Enabled())
	{
		if (TextureLoader::Is_Render_Thread())
		{
			Init();
		}
	}
}

// ----------------------------------------------------------------------------
TextureClass::TextureClass
(
	SurfaceClass *surface,
	MipCountType mip_level_count
)
:  TextureBaseClass(0,0,mip_level_count),
	Filter(mip_level_count),
	TextureFormat(WW3D_FORMAT_A8R8G8B8)
{
	IsProcedural=true;
	Initialized=true;
	IsReducible=false;

	if (surface == nullptr) {
		LastAccessed=WW3D::Get_Sync_Time();
		return;
	}
	SurfaceClass::SurfaceDescription sd;
	surface->Get_Description(sd);
	Width=sd.Width;
	Height=sd.Height;
	// Signed bump surfaces are not color images; retain metadata without upload.
	if (sd.Format == WW3D_FORMAT_U8V8 || sd.Format == WW3D_FORMAT_L6V5U5 ||
		sd.Format == WW3D_FORMAT_X8L8V8U8) {
		TextureFormat = sd.Format;
		LastAccessed=WW3D::Get_Sync_Time();
		return;
	}
	switch (sd.Format)
	{
	case WW3D_FORMAT_DXT1:
	case WW3D_FORMAT_DXT2:
	case WW3D_FORMAT_DXT3:
	case WW3D_FORMAT_DXT4:
	case WW3D_FORMAT_DXT5:
		IsCompressionAllowed=true;
		break;
	default: break;
	}

	int pitch = 0;
	const unsigned char *pixels = surface->Peek_CPU_Pixels(&pitch);
	const unsigned src_bpp = ::Get_Bytes_Per_Pixel(sd.Format);
	if (pixels != nullptr && src_bpp != 0 && Width > 0 && Height > 0) {
		std::vector<unsigned char> rgba(static_cast<std::size_t>(Width) * Height * 4);
		if (sd.Format == WW3D_FORMAT_A8R8G8B8) {
			for (int y = 0; y < Height; ++y) {
				memcpy(rgba.data() + static_cast<std::size_t>(y) * Width * 4,
					pixels + static_cast<std::size_t>(y) * pitch, static_cast<std::size_t>(Width) * 4);
			}
		} else {
			BitmapHandlerClass::Copy_Image(
				rgba.data(), Width, Height, Width*4, WW3D_FORMAT_A8R8G8B8,
				const_cast<unsigned char*>(pixels), Width, Height, static_cast<unsigned>(pitch), sd.Format,
				nullptr, 0, false);
		}
		// Procedural mip chains reuse the shared CPU box filter.
		std::vector<std::vector<unsigned char>> mip_pixels;
		std::vector<unsigned> mip_widths, mip_heights;
		mip_pixels.push_back(std::move(rgba));
		mip_widths.push_back(static_cast<unsigned>(Width));
		mip_heights.push_back(static_cast<unsigned>(Height));
		if (mip_level_count != MIP_LEVELS_1) {
			const unsigned requested = mip_level_count == MIP_LEVELS_ALL ? 12u : static_cast<unsigned>(mip_level_count);
			for (unsigned i = 1; i < requested; ++i) {
				const unsigned prev_w = mip_widths.back();
				const unsigned prev_h = mip_heights.back();
				if (prev_w <= 1 && prev_h <= 1) break;
				const unsigned next_w = prev_w > 1 ? prev_w / 2 : 1;
				const unsigned next_h = prev_h > 1 ? prev_h / 2 : 1;
				std::vector<unsigned char> next(static_cast<std::size_t>(next_w) * next_h * 4);
				BitmapHandlerClass::Create_Mipmap(
					next.data(), next_w*4, WW3D_FORMAT_A8R8G8B8,
					mip_pixels.back().data(), prev_w*4, WW3D_FORMAT_A8R8G8B8,
					prev_w, prev_h);
				mip_pixels.push_back(std::move(next));
				mip_widths.push_back(next_w);
				mip_heights.push_back(next_h);
			}
		}
		IRenderBackend *backend = WW3D::Get_Render_Backend();
		if (backend != nullptr && TextureLoader::Is_Render_Thread()) {
			std::vector<RenderBackendTextureMipLevel> levels;
			levels.reserve(mip_pixels.size());
			for (std::size_t i = 0; i < mip_pixels.size(); ++i) {
				levels.push_back({mip_widths[i], mip_heights[i], mip_widths[i]*4, mip_pixels[i].data()});
			}
			const auto handle = backend->Create_Static_RGBA8_Texture(levels.data(), static_cast<unsigned>(levels.size()));
			if (handle.Is_Valid()) {
				RendererOwner = backend;
				RendererTexture = handle;
				RendererMipLevelCount = static_cast<unsigned>(levels.size());
				RendererTextureMissing = false;
			}
		}
	}
	LastAccessed=WW3D::Get_Sync_Time();
}

//**********************************************************************************************
//! Initialise the texture
/*!
*/
void TextureClass::Init()
{
	if (WW3D::Get_Render_Backend() != nullptr && !IsProcedural && !Get_Texture_Name().Is_Empty()) {
		Ensure_Renderer_Texture();
		return;
	}
	// If the texture has already been initialised we should exit now
	if (Initialized) return;

	WWPROFILE("TextureClass::Init");

	// If the texture has recently been inactivated, increase the inactivation time (this texture obviously
	// should not have been inactivated yet).
	if (InactivationTime && LastInactivationSyncTime)
	{
		if ((WW3D::Get_Sync_Time()-LastInactivationSyncTime)<InactivationTime)
		{
			ExtendedInactivationTime=3*InactivationTime;
		}
		LastInactivationSyncTime=0;
	}


	if (!Get_Renderer_Texture().Is_Valid())
	{
		if (!WW3D::Get_Thumbnail_Enabled() || MipLevelCount==MIP_LEVELS_1)
		{
			TextureLoader::Request_Foreground_Loading(this);
		}
		else
		{
			WW3DFormat format=TextureFormat;
			Load_Locked_Surface();
			TextureFormat=format;
		}
	}

	if (!Initialized)
	{
		TextureLoader::Request_Background_Loading(this);
	}

	LastAccessed=WW3D::Get_Sync_Time();
}

//**********************************************************************************************
//! Apply CPU RGBA8 mip chain to texture
/*!
*/
void TextureClass::Apply_RGBA8_Mip_Chain(
	const std::vector<TextureRGBA8MipLevel> &levels,
	bool missing,
	bool initialized,
	bool disable_auto_invalidation
)
{
	if (levels.empty()) return;
	IRenderBackend *backend = WW3D::Get_Render_Backend();
	if (backend == nullptr) return;
	std::vector<RenderBackendTextureMipLevel> uploads;
	uploads.reserve(levels.size());
	for (const auto &image : levels) {
		if (image.pixels.empty() || image.width == 0 || image.height == 0) return;
		uploads.push_back({image.width, image.height, image.width*4, image.pixels.data()});
	}
	const auto handle = backend->Create_Static_RGBA8_Texture(uploads.data(), static_cast<unsigned>(uploads.size()));
	if (!handle.Is_Valid()) return;
	if (RendererTexture.Is_Valid() && RendererOwner == backend) RendererOwner->Release_Texture(RendererTexture);
	RendererOwner = backend;
	RendererTexture = handle;
	RendererTextureMissing = missing;
	RendererMipLevelCount = static_cast<unsigned>(levels.size());
	Width = levels[0].width;
	Height = levels[0].height;
	TextureFormat = WW3D_FORMAT_A8R8G8B8;

	if (initialized) Initialized=true;
	if (disable_auto_invalidation) InactivationTime = 0;

	LastAccessed=WW3D::Get_Sync_Time();
}


//**********************************************************************************************
//! Apply texture states
/*!
*/
void TextureClass::Apply(unsigned int stage)
{
	// Initialization needs to be done when texture is used if it hasn't been done before.
	if (!Initialized)
	{
		Init();
	}
	LastAccessed=WW3D::Get_Sync_Time();

	RENDER_RECORD_TEXTURE(this);

	// Renderer-neutral: texture binding happens via Get_Renderer_Texture()
	// in backend draws. Filter state is consumed via Get_Render_Sampler().
	(void)stage;

	Filter.Apply(stage);
}

//**********************************************************************************************
//! Generate the allocated mip chain using renderer-neutral CPU filtering.
/*!
*/
bool TextureClass::Generate_Mipmaps()
{
	// Mipmaps are generated on the CPU during RGBA8 decode.
	return true;
}

//**********************************************************************************************
//! Get surface from mip level
/*!
*/
SurfaceClass *TextureClass::Get_Surface_Level(unsigned int level)
{
	(void)level;
	// Renderer-neutral: decode the top-level CPU image for 2D callers.
	// Only level 0 is supported; backend resources have no CPU readback.
	unsigned width = 0, height = 0;
	std::vector<unsigned char> pixels;
	Vector3 hsv(0.0f, 0.0f, 0.0f);
	if (!Get_Texture_Name().Is_Empty() && !IsProcedural &&
		TextureLoader::Load_RGBA8_Image(Get_Full_Path(), width, height, pixels, hsv) &&
		width != 0 && height != 0) {
		SurfaceClass *surface = NEW_REF(SurfaceClass, (width, height, WW3D_FORMAT_A8R8G8B8));
		surface->Copy(pixels.data());
		return surface;
	}
	const unsigned w = Width > 0 ? static_cast<unsigned>(Width) : 4;
	const unsigned h = Height > 0 ? static_cast<unsigned>(Height) : 4;
	SurfaceClass *surface = NEW_REF(SurfaceClass, (w, h, WW3D_FORMAT_A8R8G8B8));
	surface->Clear();
	return surface;
}

//**********************************************************************************************
//! Get surface description for a mip level
/*!
*/
void TextureClass::Get_Level_Description( SurfaceClass::SurfaceDescription & desc, unsigned int level )
{
	if (RendererTexture.Is_Valid()) {
		desc.Format = level == 0 ? TextureFormat : WW3D_FORMAT_UNKNOWN;
		desc.Width = level == 0 ? static_cast<unsigned>(Width) : 0;
		desc.Height = level == 0 ? static_cast<unsigned>(Height) : 0;
		return;
	}
	SurfaceClass * surf = Get_Surface_Level(level);
	if (surf != nullptr) {
		surf->Get_Description(desc);
	}
	REF_PTR_RELEASE(surf);
}

//**********************************************************************************************
//! Get texture memory usage
/*!
*/
unsigned TextureClass::Get_Texture_Memory_Usage() const
{
	if (Width <= 0 || Height <= 0) return 0;
	return static_cast<unsigned>(Width) * static_cast<unsigned>(Height) * 4u;
}


// Utility functions
TextureClass* Load_Texture(ChunkLoadClass & cload)
{
	// Assume failure
	TextureClass *newtex = nullptr;

	char name[256];
	if (cload.Open_Chunk () && (cload.Cur_Chunk_ID () == W3D_CHUNK_TEXTURE))
	{

		W3dTextureInfoStruct texinfo;
		bool hastexinfo = false;

		/*
		** Read in the texture filename, and a possible texture info structure.
		*/
		while (cload.Open_Chunk()) {
			switch (cload.Cur_Chunk_ID()) {
				case W3D_CHUNK_TEXTURE_NAME:
					cload.Read(&name,cload.Cur_Chunk_Length());
					break;

				case W3D_CHUNK_TEXTURE_INFO:
					cload.Read(&texinfo,sizeof(W3dTextureInfoStruct));
					hastexinfo = true;
					break;
			};
			cload.Close_Chunk();
		}
		cload.Close_Chunk();

		/*
		** Get the texture from the asset manager
		*/
		if (hastexinfo)
		{

			MipCountType mipcount;

			bool no_lod = ((texinfo.Attributes & W3DTEXTURE_NO_LOD) == W3DTEXTURE_NO_LOD);

			if (no_lod)
			{
				mipcount = MIP_LEVELS_1;
			}
			else
			{
				switch (texinfo.Attributes & W3DTEXTURE_MIP_LEVELS_MASK) {

					case W3DTEXTURE_MIP_LEVELS_ALL:
						mipcount = MIP_LEVELS_ALL;
						break;

					case W3DTEXTURE_MIP_LEVELS_2:
						mipcount = MIP_LEVELS_2;
						break;

					case W3DTEXTURE_MIP_LEVELS_3:
						mipcount = MIP_LEVELS_3;
						break;

					case W3DTEXTURE_MIP_LEVELS_4:
						mipcount = MIP_LEVELS_4;
						break;

					default:
						WWASSERT (false);
						mipcount = MIP_LEVELS_ALL;
						break;
				}
			}

			WW3DFormat format=WW3D_FORMAT_UNKNOWN;

			switch (texinfo.Attributes & W3DTEXTURE_TYPE_MASK)
			{

				case W3DTEXTURE_TYPE_COLORMAP:
					// Do nothing.
					break;

				case W3DTEXTURE_TYPE_BUMPMAP:
				{
					// Preserve the W3D bump request; signed resource conversion migrates with its shader.
					mipcount=MIP_LEVELS_1;
					format=WW3D_FORMAT_U8V8;
					break;
				}

				default:
					WWASSERT (false);
					break;
			}

			newtex = WW3DAssetManager::Get_Instance()->Get_Texture (name, mipcount, format);

			if (no_lod)
			{
				newtex->Get_Filter().Set_Mip_Mapping(TextureFilterClass::FILTER_TYPE_NONE);
			}
			bool u_clamp = ((texinfo.Attributes & W3DTEXTURE_CLAMP_U) != 0);
			newtex->Get_Filter().Set_U_Addr_Mode(u_clamp ? TextureFilterClass::TEXTURE_ADDRESS_CLAMP : TextureFilterClass::TEXTURE_ADDRESS_REPEAT);
			bool v_clamp = ((texinfo.Attributes & W3DTEXTURE_CLAMP_V) != 0);
			newtex->Get_Filter().Set_V_Addr_Mode(v_clamp ? TextureFilterClass::TEXTURE_ADDRESS_CLAMP : TextureFilterClass::TEXTURE_ADDRESS_REPEAT);

		} else
		{
			newtex = WW3DAssetManager::Get_Instance()->Get_Texture(name);
		}

		WWASSERT(newtex);
	}

	// Return a pointer to the new texture
	return newtex;
}

// Utility function used by Save_Texture
void setup_texture_attributes(TextureClass * tex, W3dTextureInfoStruct * texinfo)
{
	texinfo->Attributes = 0;

	if (tex->Get_Filter().Get_Mip_Mapping() == TextureFilterClass::FILTER_TYPE_NONE) texinfo->Attributes |= W3DTEXTURE_NO_LOD;
	if (tex->Get_Filter().Get_U_Addr_Mode() == TextureFilterClass::TEXTURE_ADDRESS_CLAMP) texinfo->Attributes |= W3DTEXTURE_CLAMP_U;
	if (tex->Get_Filter().Get_V_Addr_Mode() == TextureFilterClass::TEXTURE_ADDRESS_CLAMP) texinfo->Attributes |= W3DTEXTURE_CLAMP_V;
}


void Save_Texture(TextureClass * texture,ChunkSaveClass & csave)
{
	const char * filename;
	W3dTextureInfoStruct texinfo;
	memset(&texinfo,0,sizeof(texinfo));

	filename = texture->Get_Full_Path();

	setup_texture_attributes(texture, &texinfo);

	csave.Begin_Chunk(W3D_CHUNK_TEXTURE_NAME);
	csave.Write(filename,strlen(filename)+1);
	csave.End_Chunk();

	if ((texinfo.Attributes != 0) || (texinfo.AnimType != 0) || (texinfo.FrameCount != 0)) {
		csave.Begin_Chunk(W3D_CHUNK_TEXTURE_INFO);
		csave.Write(&texinfo, sizeof(texinfo));
		csave.End_Chunk();
	}
}


/*!
 *	KJM depth stencil texture constructor
 */
ZTextureClass::ZTextureClass
(
	unsigned width,
	unsigned height,
	WW3DZFormat zformat,
	MipCountType mip_level_count,
	PoolType pool
)
:	TextureBaseClass(width,height, mip_level_count, pool),
	DepthStencilTextureFormat(zformat)
{
	// Renderer-neutral: depth targets are owned by the D3D12 backend.
	// Preserve dimensions/format for asset compatibility without a color upload.
	Initialized=true;
	IsProcedural=true;
	IsReducible=false;

	LastAccessed=WW3D::Get_Sync_Time();
}


//**********************************************************************************************
//! Apply depth stencil texture
/*! KM
*/
void ZTextureClass::Apply(unsigned int stage)
{
	(void)stage;
}

//**********************************************************************************************
//! Apply CPU mip chain to depth texture (no color upload)
/*! KM
*/
void ZTextureClass::Apply_RGBA8_Mip_Chain(
	const std::vector<TextureRGBA8MipLevel> &levels,
	bool missing,
	bool initialized,
	bool disable_auto_invalidation
)
{
	(void)missing;
	if (!levels.empty()) {
		Width = levels[0].width;
		Height = levels[0].height;
	}
	if (initialized) Initialized=true;
	if (disable_auto_invalidation) InactivationTime = 0;
	LastAccessed=WW3D::Get_Sync_Time();
}

//**********************************************************************************************
//! Get texture memory usage
/*!
*/
unsigned ZTextureClass::Get_Texture_Memory_Usage() const
{
	if (Width <= 0 || Height <= 0) return 0;
	return static_cast<unsigned>(Width) * static_cast<unsigned>(Height) * 4u;
}



/*************************************************************************
**                             CubeTextureClass
*************************************************************************/
CubeTextureClass::CubeTextureClass
(
	unsigned width,
	unsigned height,
	WW3DFormat format,
	MipCountType mip_level_count,
	PoolType pool,
	bool rendertarget,
	bool allow_reduction
)
: TextureClass(width, height, format, mip_level_count, pool, rendertarget)
{
	Initialized=true;
	IsProcedural=true;
	IsReducible=false;

	switch (format)
	{
	case WW3D_FORMAT_DXT1:
	case WW3D_FORMAT_DXT2:
	case WW3D_FORMAT_DXT3:
	case WW3D_FORMAT_DXT4:
	case WW3D_FORMAT_DXT5:
		IsCompressionAllowed=true;
		break;
	default : break;
	}

	// Cube faces have no dedicated backend resource yet; keep a 2D RGBA8
	// placeholder so procedural callers remain functional. File loads resolve
	// through Ensure/Copy paths with magenta fallback when unsupported.
	if (format == WW3D_FORMAT_U8V8 || format == WW3D_FORMAT_L6V5U5 ||
		format == WW3D_FORMAT_X8L8V8U8) {
		LastAccessed=WW3D::Get_Sync_Time();
		return;
	}
	IRenderBackend *backend = WW3D::Get_Render_Backend();
	if (backend != nullptr && TextureLoader::Is_Render_Thread() && width != 0 && height != 0) {
		if (rendertarget) {
			const auto handle = backend->Create_Render_Texture(width, height);
			if (handle.Is_Valid()) {
				RendererOwner = backend;
				RendererTexture = handle;
				RendererMipLevelCount = 1;
				RendererTextureMissing = false;
			}
		} else {
			std::vector<unsigned char> blank(static_cast<std::size_t>(width) * height * 4, 0);
			RenderBackendTextureMipLevel level{width, height, width*4, blank.data()};
			const auto handle = backend->Create_Static_RGBA8_Texture(&level, 1);
			if (handle.Is_Valid()) {
				RendererOwner = backend;
				RendererTexture = handle;
				RendererMipLevelCount = 1;
				RendererTextureMissing = false;
				TextureFormat = WW3D_FORMAT_A8R8G8B8;
			}
		}
	}
	LastAccessed=WW3D::Get_Sync_Time();
}



// ----------------------------------------------------------------------------
CubeTextureClass::CubeTextureClass
(
	const char *name,
	const char *full_path,
	MipCountType mip_level_count,
	WW3DFormat texture_format,
	bool allow_compression,
	bool allow_reduction
)
:	TextureClass(0,0,mip_level_count, POOL_MANAGED, false, texture_format)
{
	IsCompressionAllowed=allow_compression;
	InactivationTime=DEFAULT_INACTIVATION_TIME;		// Default inactivation time 30 seconds

	switch (TextureFormat)
	{
	case WW3D_FORMAT_DXT1:
	case WW3D_FORMAT_DXT2:
	case WW3D_FORMAT_DXT3:
	case WW3D_FORMAT_DXT4:
	case WW3D_FORMAT_DXT5:
		IsCompressionAllowed=true;
		break;
	case WW3D_FORMAT_U8V8:		// Bumpmap
	case WW3D_FORMAT_L6V5U5:	// Bumpmap
	case WW3D_FORMAT_X8L8V8U8:	// Bumpmap
		// Retain the signed format so an unported bump caller cannot become a color texture.
		IsCompressionAllowed=false;
		MipLevelCount=MIP_LEVELS_1;
		Filter.Set_Mip_Mapping(TextureFilterClass::FILTER_TYPE_NONE);
		break;
	default:	break;
	}

	WWASSERT_PRINT(name && name[0], "TextureClass CTor: null or empty texture name");
	int len=strlen(name);
	for (int i=0;i<len;++i)
	{
		if (name[i]=='+')
		{
			IsLightmap=true;

			// Set bilinear filtering for lightmaps (they are very stretched and
			// low detail so we don't care for anisotropic or trilinear filtering...)
			Filter.Set_Min_Filter(TextureFilterClass::FILTER_TYPE_FAST);
			Filter.Set_Mag_Filter(TextureFilterClass::FILTER_TYPE_FAST);
			if (mip_level_count!=MIP_LEVELS_1) Filter.Set_Mip_Mapping(TextureFilterClass::FILTER_TYPE_FAST);
			break;
		}
	}
	Set_Texture_Name(name);
	Set_Full_Path(full_path);
	WWASSERT(name[0]!='\0');
	if (!WW3D::Is_Texturing_Enabled())
	{
		Initialized=true;
	}

	// Find original size from the thumbnail (but don't create thumbnail texture yet!)
	ThumbnailClass* thumb=ThumbnailManagerClass::Peek_Thumbnail_Instance_From_Any_Manager(Get_Full_Path());
	if (thumb)
	{
		Width=thumb->Get_Original_Texture_Width();
		Height=thumb->Get_Original_Texture_Height();
	}

	LastAccessed=WW3D::Get_Sync_Time();

	// If the thumbnails are not enabled, init the texture at this point to avoid stalling when the
	// mesh is rendered.
	if (!WW3D::Get_Thumbnail_Enabled())
	{
		if (TextureLoader::Is_Render_Thread())
		{
			Init();
		}
	}
}

// ----------------------------------------------------------------------------
CubeTextureClass::CubeTextureClass
(
	SurfaceClass *surface,
	MipCountType mip_level_count
)
:	TextureClass(0,0,mip_level_count, POOL_MANAGED, false, WW3D_FORMAT_A8R8G8B8)
{
	IsProcedural=true;
	Initialized=true;
	IsReducible=false;

	if (surface == nullptr) {
		LastAccessed=WW3D::Get_Sync_Time();
		return;
	}
	SurfaceClass::SurfaceDescription sd;
	surface->Get_Description(sd);
	Width=sd.Width;
	Height=sd.Height;
	TextureFormat = WW3D_FORMAT_A8R8G8B8;
	switch (sd.Format)
	{
	case WW3D_FORMAT_DXT1:
	case WW3D_FORMAT_DXT2:
	case WW3D_FORMAT_DXT3:
	case WW3D_FORMAT_DXT4:
	case WW3D_FORMAT_DXT5:
		IsCompressionAllowed=true;
		break;
	default: break;
	}

	int pitch = 0;
	const unsigned char *pixels = surface->Peek_CPU_Pixels(&pitch);
	const unsigned src_bpp = ::Get_Bytes_Per_Pixel(sd.Format);
	if (pixels != nullptr && src_bpp != 0 && Width > 0 && Height > 0) {
		std::vector<unsigned char> rgba(static_cast<std::size_t>(Width) * Height * 4);
		if (sd.Format == WW3D_FORMAT_A8R8G8B8) {
			for (int y = 0; y < Height; ++y) {
				memcpy(rgba.data() + static_cast<std::size_t>(y) * Width * 4,
					pixels + static_cast<std::size_t>(y) * pitch, static_cast<std::size_t>(Width) * 4);
			}
		} else {
			BitmapHandlerClass::Copy_Image(
				rgba.data(), Width, Height, Width*4, WW3D_FORMAT_A8R8G8B8,
				const_cast<unsigned char*>(pixels), Width, Height, static_cast<unsigned>(pitch), sd.Format,
				nullptr, 0, false);
		}
		IRenderBackend *backend = WW3D::Get_Render_Backend();
		if (backend != nullptr && TextureLoader::Is_Render_Thread()) {
			RenderBackendTextureMipLevel level{static_cast<unsigned>(Width), static_cast<unsigned>(Height), static_cast<unsigned>(Width)*4, rgba.data()};
			const auto handle = backend->Create_Static_RGBA8_Texture(&level, 1);
			if (handle.Is_Valid()) {
				RendererOwner = backend;
				RendererTexture = handle;
				RendererMipLevelCount = 1;
				RendererTextureMissing = false;
			}
		}
	}
	LastAccessed=WW3D::Get_Sync_Time();
}

//**********************************************************************************************
//! Apply CPU mip chain to cube texture (2D placeholder)
/*!
*/
void CubeTextureClass::Apply_RGBA8_Mip_Chain(
	const std::vector<TextureRGBA8MipLevel> &levels,
	bool missing,
	bool initialized,
	bool disable_auto_invalidation
)
{
	TextureClass::Apply_RGBA8_Mip_Chain(levels, missing, initialized, disable_auto_invalidation);
}


/*************************************************************************
**                             VolumeTextureClass
*************************************************************************/
VolumeTextureClass::VolumeTextureClass
(
	unsigned width,
	unsigned height,
	unsigned depth,
	WW3DFormat format,
	MipCountType mip_level_count,
	PoolType pool,
	bool rendertarget,
	bool allow_reduction
)
: TextureClass(width, height, format, mip_level_count, pool, rendertarget),
  Depth(depth)
{
	Initialized=true;
	IsProcedural=true;
	IsReducible=false;

	switch (format)
	{
	case WW3D_FORMAT_DXT1:
	case WW3D_FORMAT_DXT2:
	case WW3D_FORMAT_DXT3:
	case WW3D_FORMAT_DXT4:
	case WW3D_FORMAT_DXT5:
		IsCompressionAllowed=true;
		break;
	default : break;
	}

	// Volume slices have no dedicated backend resource yet; keep a 2D RGBA8
	// placeholder. File loads resolve with magenta fallback when unsupported.
	if (format == WW3D_FORMAT_U8V8 || format == WW3D_FORMAT_L6V5U5 ||
		format == WW3D_FORMAT_X8L8V8U8) {
		LastAccessed=WW3D::Get_Sync_Time();
		return;
	}
	IRenderBackend *backend = WW3D::Get_Render_Backend();
	if (backend != nullptr && TextureLoader::Is_Render_Thread() && width != 0 && height != 0) {
		if (rendertarget) {
			const auto handle = backend->Create_Render_Texture(width, height);
			if (handle.Is_Valid()) {
				RendererOwner = backend;
				RendererTexture = handle;
				RendererMipLevelCount = 1;
				RendererTextureMissing = false;
			}
		} else {
			std::vector<unsigned char> blank(static_cast<std::size_t>(width) * height * 4, 0);
			RenderBackendTextureMipLevel level{width, height, width*4, blank.data()};
			const auto handle = backend->Create_Static_RGBA8_Texture(&level, 1);
			if (handle.Is_Valid()) {
				RendererOwner = backend;
				RendererTexture = handle;
				RendererMipLevelCount = 1;
				RendererTextureMissing = false;
				TextureFormat = WW3D_FORMAT_A8R8G8B8;
			}
		}
	}
	LastAccessed=WW3D::Get_Sync_Time();
}



// ----------------------------------------------------------------------------
VolumeTextureClass::VolumeTextureClass
(
	const char *name,
	const char *full_path,
	MipCountType mip_level_count,
	WW3DFormat texture_format,
	bool allow_compression,
	bool allow_reduction
)
:	TextureClass(0,0,mip_level_count, POOL_MANAGED, false, texture_format),
	Depth(0)
{
	IsCompressionAllowed=allow_compression;
	InactivationTime=DEFAULT_INACTIVATION_TIME;		// Default inactivation time 30 seconds

	switch (TextureFormat)
	{
	case WW3D_FORMAT_DXT1:
	case WW3D_FORMAT_DXT2:
	case WW3D_FORMAT_DXT3:
	case WW3D_FORMAT_DXT4:
	case WW3D_FORMAT_DXT5:
		IsCompressionAllowed=true;
		break;
	case WW3D_FORMAT_U8V8:		// Bumpmap
	case WW3D_FORMAT_L6V5U5:	// Bumpmap
	case WW3D_FORMAT_X8L8V8U8:	// Bumpmap
		// Retain the signed format so an unported bump caller cannot become a color texture.
		IsCompressionAllowed=false;
		MipLevelCount=MIP_LEVELS_1;
		Filter.Set_Mip_Mapping(TextureFilterClass::FILTER_TYPE_NONE);
		break;
	default:	break;
	}

	WWASSERT_PRINT(name && name[0], "TextureClass CTor: null or empty texture name");
	int len=strlen(name);
	for (int i=0;i<len;++i)
	{
		if (name[i]=='+')
		{
			IsLightmap=true;

			// Set bilinear filtering for lightmaps (they are very stretched and
			// low detail so we don't care for anisotropic or trilinear filtering...)
			Filter.Set_Min_Filter(TextureFilterClass::FILTER_TYPE_FAST);
			Filter.Set_Mag_Filter(TextureFilterClass::FILTER_TYPE_FAST);
			if (mip_level_count!=MIP_LEVELS_1) Filter.Set_Mip_Mapping(TextureFilterClass::FILTER_TYPE_FAST);
			break;
		}
	}
	Set_Texture_Name(name);
	Set_Full_Path(full_path);
	WWASSERT(name[0]!='\0');
	if (!WW3D::Is_Texturing_Enabled())
	{
		Initialized=true;
	}

	// Find original size from the thumbnail (but don't create thumbnail texture yet!)
	ThumbnailClass* thumb=ThumbnailManagerClass::Peek_Thumbnail_Instance_From_Any_Manager(Get_Full_Path());
	if (thumb)
	{
		Width=thumb->Get_Original_Texture_Width();
		Height=thumb->Get_Original_Texture_Height();
	}

	LastAccessed=WW3D::Get_Sync_Time();

	// If the thumbnails are not enabled, init the texture at this point to avoid stalling when the
	// mesh is rendered.
	if (!WW3D::Get_Thumbnail_Enabled())
	{
		if (TextureLoader::Is_Render_Thread())
		{
			Init();
		}
	}
}

// ----------------------------------------------------------------------------
VolumeTextureClass::VolumeTextureClass
(
	SurfaceClass *surface,
	MipCountType mip_level_count
)
:	TextureClass(0,0,mip_level_count, POOL_MANAGED, false, WW3D_FORMAT_A8R8G8B8),
	Depth(1)
{
	IsProcedural=true;
	Initialized=true;
	IsReducible=false;

	if (surface == nullptr) {
		LastAccessed=WW3D::Get_Sync_Time();
		return;
	}
	SurfaceClass::SurfaceDescription sd;
	surface->Get_Description(sd);
	Width=sd.Width;
	Height=sd.Height;
	TextureFormat = WW3D_FORMAT_A8R8G8B8;

	int pitch = 0;
	const unsigned char *pixels = surface->Peek_CPU_Pixels(&pitch);
	const unsigned src_bpp = ::Get_Bytes_Per_Pixel(sd.Format);
	if (pixels != nullptr && src_bpp != 0 && Width > 0 && Height > 0) {
		std::vector<unsigned char> rgba(static_cast<std::size_t>(Width) * Height * 4);
		if (sd.Format == WW3D_FORMAT_A8R8G8B8) {
			for (int y = 0; y < Height; ++y) {
				memcpy(rgba.data() + static_cast<std::size_t>(y) * Width * 4,
					pixels + static_cast<std::size_t>(y) * pitch, static_cast<std::size_t>(Width) * 4);
			}
		} else {
			BitmapHandlerClass::Copy_Image(
				rgba.data(), Width, Height, Width*4, WW3D_FORMAT_A8R8G8B8,
				const_cast<unsigned char*>(pixels), Width, Height, static_cast<unsigned>(pitch), sd.Format,
				nullptr, 0, false);
		}
		IRenderBackend *backend = WW3D::Get_Render_Backend();
		if (backend != nullptr && TextureLoader::Is_Render_Thread()) {
			RenderBackendTextureMipLevel level{static_cast<unsigned>(Width), static_cast<unsigned>(Height), static_cast<unsigned>(Width)*4, rgba.data()};
			const auto handle = backend->Create_Static_RGBA8_Texture(&level, 1);
			if (handle.Is_Valid()) {
				RendererOwner = backend;
				RendererTexture = handle;
				RendererMipLevelCount = 1;
				RendererTextureMissing = false;
			}
		}
	}
	LastAccessed=WW3D::Get_Sync_Time();
}

//**********************************************************************************************
//! Apply CPU mip chain to volume texture (2D placeholder)
/*!
*/
void VolumeTextureClass::Apply_RGBA8_Mip_Chain(
	const std::vector<TextureRGBA8MipLevel> &levels,
	bool missing,
	bool initialized,
	bool disable_auto_invalidation
)
{
	TextureClass::Apply_RGBA8_Mip_Chain(levels, missing, initialized, disable_auto_invalidation);
}
