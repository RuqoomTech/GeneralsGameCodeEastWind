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
 *                     $Archive:: /Commando/Code/ww3d2/surfaceclass.cpp                       $*
 *                                                                                             *
 *              Original Author:: Nathaniel Hoffman                                            *
 *                                                                                             *
 *                      $Author:: Greg_h2                                                     $*
 *                                                                                             *
 *                     $Modtime:: 8/30/01 2:01p                                               $*
 *                                                                                             *
 *                    $Revision:: 25                                                          $*
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 *   SurfaceClass::Clear -- Clears a surface to 0                                              *
 *   SurfaceClass::Copy -- Copies a region from one surface to another of the same format      *
 *   SurfaceClass::FindBBAlpha -- Finds the bounding box of non zero pixels in the region (x0, *
 *   SurfaceClass::Is_Transparent_Column -- Tests to see if the column is transparent or not   *
 *   SurfaceClass::Copy -- Copies from a byte array to the surface                             *
 *   SurfaceClass::CreateCopy -- Creates a byte array copy of the surface                      *
 *   SurfaceClass::DrawHLine -- draws a horizontal line                                        *
 *   SurfaceClass::DrawPixel -- draws a pixel                                                  *
 *   SurfaceClass::Copy -- Copies a block of system ram to the surface                         *
 *   SurfaceClass::Hue_Shift -- changes the hue of the surface                                 *
 *   SurfaceClass::Is_Monochrome -- Checks if surface is monochrome or not                     *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include "surfaceclass.h"
#include "WWMath/vector2i.h"
#include "colorspace.h"
#include "WWLib/bound.h"
#include "bitmaphandler.h"
#include "ww3dformat.h"
#include "missingtexture.h"
#include "textureloader.h"
#include "formconv.h"
#include <d3d8.h>
#include <algorithm>
#include <cstring>
void Convert_Pixel(Vector3 &rgb, const SurfaceClass::SurfaceDescription &sd, const unsigned char * pixel)
{
	const float scale=1/255.0f;
	switch (sd.Format)
	{
	case WW3D_FORMAT_A8R8G8B8:
	case WW3D_FORMAT_X8R8G8B8:
	case WW3D_FORMAT_R8G8B8:
		{
			rgb.X=pixel[2]; // R
			rgb.Y=pixel[1]; // G
			rgb.Z=pixel[0]; // B
		}
		break;
	case WW3D_FORMAT_A4R4G4B4:
		{
			unsigned short tmp;
			tmp=*(unsigned short*)&pixel[0];
			rgb.X=((tmp&0x0f00)>>4);   // R
			rgb.Y=((tmp&0x00f0));		// G
			rgb.Z=((tmp&0x000f)<<4);	// B
		}
		break;
	case WW3D_FORMAT_A1R5G5B5:
		{
			unsigned short tmp;
			tmp=*(unsigned short*)&pixel[0];
			rgb.X=(tmp>>7)&0xf8; // R
			rgb.Y=(tmp>>2)&0xf8; // G
			rgb.Z=(tmp<<3)&0xf8; // B
		}
		break;
	case WW3D_FORMAT_R5G6B5:
		{
			unsigned short tmp;
			tmp=*(unsigned short*)&pixel[0];
			rgb.X=(tmp>>8)&0xf8;
			rgb.Y=(tmp>>3)&0xfc;
			rgb.Z=(tmp<<3)&0xf8;
		}
		break;

	default:
		// TODO: Implement other pixel formats
		WWASSERT(0);
	}
	rgb*=scale;
}

// Note: This function must never overwrite the original alpha
void Convert_Pixel(unsigned char * pixel,const SurfaceClass::SurfaceDescription &sd, const Vector3 &rgb)
{
	unsigned char r,g,b;
	r=(unsigned char) (rgb.X*255.0f);
	g=(unsigned char) (rgb.Y*255.0f);
	b=(unsigned char) (rgb.Z*255.0f);
	switch (sd.Format)
	{
	case WW3D_FORMAT_A8R8G8B8:
	case WW3D_FORMAT_X8R8G8B8:
	case WW3D_FORMAT_R8G8B8:
		pixel[0]=b;
		pixel[1]=g;
		pixel[2]=r;
		break;
	case WW3D_FORMAT_A4R4G4B4:
		{
			unsigned short tmp;
			tmp=*(unsigned short*)&pixel[0];
			tmp&=0xF000;
			tmp|=(r&0xF0) << 4;
			tmp|=(g&0xF0);
			tmp|=(b&0xF0) >> 4;
			*(unsigned short*)&pixel[0]=tmp;
		}
		break;
	case WW3D_FORMAT_A1R5G5B5:
		{
			unsigned short tmp;
			tmp=*(unsigned short*)&pixel[0];
			tmp&=0x8000;
			tmp|=(r&0xF8) << 7;
			tmp|=(g&0xF8) << 2;
			tmp|=(b&0xF8) >> 3;
			*(unsigned short*)&pixel[0]=tmp;
		}
		break;
	case WW3D_FORMAT_R5G6B5:
		{
			unsigned short tmp;
			tmp=(r&0xf8) << 8;
			tmp|=(g&0xfc) << 3;
			tmp|=(b&0xf8) >> 3;
			*(unsigned short*)&pixel[0]=tmp;
		}
		break;
	default:
		// TODO: Implement other pixel formats
		WWASSERT(0);
	}
}

/*************************************************************************
**                             SurfaceClass
*************************************************************************/
SurfaceClass::SurfaceClass(unsigned width, unsigned height, WW3DFormat format):
	CpuWidth(width),
	CpuHeight(height),
	SurfaceFormat(format)
{
	WWASSERT(width);
	WWASSERT(height);
	const unsigned bpp = ::Get_Bytes_Per_Pixel(format);
	WWASSERT(bpp != 0);
	if (!width || !height || !bpp || width > 16384 || height > 16384) { CpuWidth=CpuHeight=0; return; }
	CpuPitch = static_cast<int>(width * bpp);
	CpuPixels.assign(static_cast<std::size_t>(CpuPitch) * height, 0);
}

SurfaceClass::SurfaceClass(const char *filename): SurfaceFormat(WW3D_FORMAT_A8R8G8B8)
{
    // Share the existing CPU decoder, including palettes and DDS validation.
    std::vector<unsigned char> rgba;
    unsigned width = 0, height = 0;
    if (!TextureLoader::Load_RGBA8_Image(StringClass(filename), width, height, rgba, Vector3(0,0,0)))
        MissingTexture::Create_RGBA8_Image(width, height, rgba);
    CpuWidth = width; CpuHeight = height; CpuPitch = static_cast<int>(width * 4);
    CpuPixels.swap(rgba);
    // Surface/BitmapHandler storage is BGRA; backend upload alone uses RGBA.
    for (std::size_t i = 0; i < CpuPixels.size(); i += 4) std::swap(CpuPixels[i], CpuPixels[i+2]);
}

SurfaceClass::SurfaceClass(IDirect3DSurface8 *surface): SurfaceFormat(WW3D_FORMAT_UNKNOWN) { Attach(surface); }
void SurfaceClass::Attach(IDirect3DSurface8 *surface)
{
    if(surface) surface->AddRef();
    Detach(); NativeSource = surface;
    if(!surface) return;
    D3DSURFACE_DESC desc{}; D3DLOCKED_RECT pixels{};
    if(FAILED(surface->GetDesc(&desc))) return;
    SurfaceFormat = D3DFormat_To_WW3DFormat(desc.Format);
    const unsigned bpp = ::Get_Bytes_Per_Pixel(SurfaceFormat);
    if(!bpp || desc.Width > 16384 || desc.Height > 16384 || FAILED(surface->LockRect(&pixels, nullptr, D3DLOCK_READONLY))) return;
    CpuWidth = desc.Width; CpuHeight = desc.Height; CpuPitch = static_cast<int>(CpuWidth*bpp);
    CpuPixels.resize(static_cast<std::size_t>(CpuPitch)*CpuHeight);
    for(unsigned y = 0; y < CpuHeight; ++y)
        std::memcpy(CpuPixels.data()+static_cast<std::size_t>(y)*CpuPitch,
            static_cast<const unsigned char*>(pixels.pBits)+static_cast<std::size_t>(y)*pixels.Pitch, CpuPitch);
    surface->UnlockRect();
}
void SurfaceClass::Detach()
{
    if(NativeSource) NativeSource->Release();
    NativeSource = nullptr; CpuPixels.clear(); CpuWidth=CpuHeight=0; CpuPitch=0; ++Revision;
}
SurfaceClass::~SurfaceClass() { if(NativeSource) NativeSource->Release(); }

void SurfaceClass::Get_Description(SurfaceDescription &surface_desc) const
{
	surface_desc.Format = SurfaceFormat;
	surface_desc.Height = CpuHeight;
	surface_desc.Width = CpuWidth;
}

unsigned int SurfaceClass::Get_Bytes_Per_Pixel()
{
	return ::Get_Bytes_Per_Pixel(SurfaceFormat);
}

SurfaceClass::LockedSurfacePtr SurfaceClass::Lock(int *pitch)
{
	WWASSERT(!CpuPixels.empty());
	if (!pitch || CpuPixels.empty() || CpuLocked) return nullptr;
	*pitch = CpuPitch;
	CpuLocked = true;
	return static_cast<LockedSurfacePtr>(CpuPixels.data());
}

SurfaceClass::LockedSurfacePtr SurfaceClass::Lock(int *pitch, const Vector2i &min, const Vector2i &max)
{
	WWASSERT(!CpuPixels.empty());
	WWASSERT(min.I >= 0 && min.J >= 0 && max.I <= static_cast<int>(CpuWidth) && max.J <= static_cast<int>(CpuHeight));
	WWASSERT(max.I > min.I && max.J > min.J);
	if (!pitch || CpuPixels.empty() || CpuLocked || min.I < 0 || min.J < 0 ||
		max.I > static_cast<int>(CpuWidth) || max.J > static_cast<int>(CpuHeight) ||
		max.I <= min.I || max.J <= min.J) return nullptr;
	const unsigned bpp = ::Get_Bytes_Per_Pixel(SurfaceFormat);
	*pitch = CpuPitch;
	CpuLocked = true;
	return static_cast<LockedSurfacePtr>(CpuPixels.data() + static_cast<std::size_t>(min.J) * static_cast<std::size_t>(CpuPitch) + static_cast<std::size_t>(min.I) * bpp);
}

void SurfaceClass::Unlock()
{
	CpuLocked = false;
	++Revision;
}

const unsigned char *SurfaceClass::Peek_CPU_Pixels(int *pitch) const
{
	if (pitch) *pitch = CpuPitch;
	return CpuPixels.empty() ? nullptr : CpuPixels.data();
}

unsigned char *SurfaceClass::Peek_CPU_Pixels(int *pitch)
{
	++Revision;
	if (pitch) *pitch = CpuPitch;
	return CpuPixels.empty() ? nullptr : CpuPixels.data();
}

/***********************************************************************************************
 * SurfaceClass::Clear -- Clears a surface to 0                                                *
 *=============================================================================================*/
void SurfaceClass::Clear()
{
	++Revision;
	// size of each pixel in bytes
	unsigned int size=::Get_Bytes_Per_Pixel(SurfaceFormat);
	if (CpuPixels.empty() || size == 0) return;

	unsigned char *mem= CpuPixels.data();
	for (unsigned i=0; i<CpuHeight; i++)
	{
		memset(mem,0,size*CpuWidth);
		mem+=CpuPitch;
	}
}


/***********************************************************************************************
 * SurfaceClass::Copy -- Copies from a byte array to the surface                               *
 *=============================================================================================*/
void SurfaceClass::Copy(const unsigned char *other)
{
	++Revision;
	// size of each pixel in bytes
	unsigned int size=::Get_Bytes_Per_Pixel(SurfaceFormat);
	if (CpuPixels.empty() || size == 0 || !other) return;
	WWASSERT(other != nullptr);

	unsigned char *mem= CpuPixels.data();
	for (unsigned i=0; i<CpuHeight; i++)
	{
		memcpy(mem,&other[i*CpuWidth*size],size*CpuWidth);
		mem+=CpuPitch;
	}
}


/***********************************************************************************************
 * SurfaceClass::Copy -- Copies a block of system ram to the surface                           *
 *=============================================================================================*/
void SurfaceClass::Copy(const Vector2i &min, const Vector2i &max, const unsigned char *other)
{
	++Revision;
	// size of each pixel in bytes
	unsigned int size=::Get_Bytes_Per_Pixel(SurfaceFormat);
	if (CpuPixels.empty() || size == 0 || !other || min.I < 0 || min.J < 0 ||
		max.I > static_cast<int>(CpuWidth) || max.J > static_cast<int>(CpuHeight) ||
		max.I <= min.I || max.J <= min.J) return;
	WWASSERT(other != nullptr);

	// Source image is tightly packed full-surface extents; SurfaceDescription
	// Width/Height below describe the destination surface for offset math.
	SurfaceDescription sd;
	sd.Format = SurfaceFormat;
	sd.Width = CpuWidth;
	sd.Height = CpuHeight;

	int dx=max.I-min.I;

	for (int i=min.J; i<max.J; i++)
	{
		unsigned char *row = CpuPixels.data() + static_cast<std::size_t>(i) * static_cast<std::size_t>(CpuPitch) + static_cast<std::size_t>(min.I) * size;
		memcpy(row,&other[(static_cast<std::size_t>(i)*sd.Width+static_cast<std::size_t>(min.I))*size],size*dx);
	}
}


/***********************************************************************************************
 * SurfaceClass::CreateCopy -- Creates a byte array copy of the surface                        *
 *=============================================================================================*/
unsigned char *SurfaceClass::CreateCopy(int *width,int *height,int*size,bool flip)
{
	// size of each pixel in bytes
	unsigned int mysize=::Get_Bytes_Per_Pixel(SurfaceFormat);

	*width=CpuWidth;
	*height=CpuHeight;
	*size=mysize;

	unsigned char *other=W3DNEWARRAY unsigned char [CpuHeight*CpuWidth*mysize];
	if (CpuPixels.empty()) {
		memset(other, 0, static_cast<std::size_t>(CpuHeight)*CpuWidth*mysize);
		return other;
	}

	unsigned char *mem= CpuPixels.data();

	for (unsigned i=0; i<CpuHeight; i++)
	{
		if (flip)
		{
			memcpy(&other[(CpuHeight-i-1)*CpuWidth*mysize],mem,mysize*CpuWidth);
		} else
		{
			memcpy(&other[i*CpuWidth*mysize],mem,mysize*CpuWidth);
		}
		mem+=CpuPitch;
	}

	return other;
}


/***********************************************************************************************
 * SurfaceClass::Copy -- Copies a region from one surface to another                           *
 *=============================================================================================*/
void SurfaceClass::Copy(
	unsigned int dstx, unsigned int dsty,
	unsigned int srcx, unsigned int srcy,
	unsigned int width, unsigned int height,
	const SurfaceClass *other)
{
	++Revision;
	WWASSERT(other);
	WWASSERT(width);
	WWASSERT(height);
	if (!other || CpuPixels.empty() || other->CpuPixels.empty()) return;

	SurfaceDescription sd,osd;
	sd.Format = SurfaceFormat; sd.Width = CpuWidth; sd.Height = CpuHeight;
	osd.Format = other->SurfaceFormat; osd.Width = other->CpuWidth; osd.Height = other->CpuHeight;

	if (srcx >= osd.Width || srcy >= osd.Height || dstx >= sd.Width || dsty >= sd.Height) return;
	width = std::min(width, std::min(osd.Width-srcx, sd.Width-dstx));
	height = std::min(height, std::min(osd.Height-srcy, sd.Height-dsty));
	unsigned src_right = srcx+width;
	unsigned src_bottom = srcy+height;
	if (src_right>osd.Width) src_right=osd.Width;
	if (src_bottom>osd.Height) src_bottom=osd.Height;

	unsigned dst_right = dstx+width;
	unsigned dst_bottom = dsty+height;
	if (dst_right>sd.Width) dst_right=sd.Width;
	if (dst_bottom>sd.Height) dst_bottom=sd.Height;

	const unsigned copy_width = (dst_right > dstx && src_right > srcx) ? (dst_right - dstx < src_right - srcx ? dst_right - dstx : src_right - srcx) : 0;
	const unsigned copy_height = (dst_bottom > dsty && src_bottom > srcy) ? (dst_bottom - dsty < src_bottom - srcy ? dst_bottom - dsty : src_bottom - srcy) : 0;
	if (copy_width == 0 || copy_height == 0) return;

	const unsigned dst_bpp = ::Get_Bytes_Per_Pixel(sd.Format);
	const unsigned src_bpp = ::Get_Bytes_Per_Pixel(osd.Format);
	if (dst_bpp == 0 || src_bpp == 0) return;

	// Snapshot an overlapping self-copy before any destination row changes.
	std::vector<unsigned char> snapshot;
	const unsigned char *source = other->CpuPixels.data();
	if (other == this) { snapshot = CpuPixels; source = snapshot.data(); }
	if (sd.Format == osd.Format) {
		for (unsigned y = 0; y < copy_height; ++y) {
			const unsigned char *src_row = source + static_cast<std::size_t>(srcy + y) * static_cast<std::size_t>(other->CpuPitch) + static_cast<std::size_t>(srcx) * src_bpp;
			unsigned char *dst_row = CpuPixels.data() + static_cast<std::size_t>(dsty + y) * static_cast<std::size_t>(CpuPitch) + static_cast<std::size_t>(dstx) * dst_bpp;
			memcpy(dst_row, src_row, static_cast<std::size_t>(copy_width) * dst_bpp);
		}
		return;
	}

    for(unsigned y=0; y<copy_height; ++y) for(unsigned x=0; x<copy_width; ++x)
        BitmapHandlerClass::Copy_Pixel(CpuPixels.data()+static_cast<std::size_t>(dsty+y)*CpuPitch+(dstx+x)*dst_bpp,sd.Format,
            other->CpuPixels.data()+static_cast<std::size_t>(srcy+y)*other->CpuPitch+(srcx+x)*src_bpp,osd.Format,nullptr,0);
}

/***********************************************************************************************
 * SurfaceClass::Copy -- Copies a region from one surface to another                           *
 *=============================================================================================*/
void SurfaceClass::Stretch_Copy(
	unsigned int dstx, unsigned int dsty, unsigned int dstwidth, unsigned int dstheight,
	unsigned int srcx, unsigned int srcy, unsigned int srcwidth, unsigned int srcheight,
	const SurfaceClass *other)
{
	++Revision;
	WWASSERT(other);
	if (!other || dstwidth == 0 || dstheight == 0 || srcwidth == 0 || srcheight == 0) return;

	SurfaceDescription sd,osd;
	sd.Format = SurfaceFormat; sd.Width = CpuWidth; sd.Height = CpuHeight;
	osd.Format = other->SurfaceFormat; osd.Width = other->CpuWidth; osd.Height = other->CpuHeight;

	const unsigned dst_bpp = ::Get_Bytes_Per_Pixel(sd.Format);
	const unsigned src_bpp = ::Get_Bytes_Per_Pixel(osd.Format);
	if (dst_bpp == 0 || src_bpp == 0) return;
	if (CpuPixels.empty() || other->CpuPixels.empty()) return;

	if (dstx >= CpuWidth || dsty >= CpuHeight || srcx >= other->CpuWidth || srcy >= other->CpuHeight) return;
	// Clamp before subtracting surface extents.
	unsigned clamped_dst_w = dstwidth;
	unsigned clamped_dst_h = dstheight;
	unsigned clamped_src_w = srcwidth;
	unsigned clamped_src_h = srcheight;
	if (clamped_dst_w > CpuWidth - dstx) clamped_dst_w = CpuWidth - dstx;
	if (clamped_dst_h > CpuHeight - dsty) clamped_dst_h = CpuHeight - dsty;
	if (clamped_src_w > other->CpuWidth - srcx) clamped_src_w = other->CpuWidth - srcx;
	if (clamped_src_h > other->CpuHeight - srcy) clamped_src_h = other->CpuHeight - srcy;
	if (clamped_dst_w == 0 || clamped_dst_h == 0 || clamped_src_w == 0 || clamped_src_h == 0) return;

	std::vector<unsigned char> snapshot;
	const unsigned char *source = other->CpuPixels.data();
	if (other == this) { snapshot = CpuPixels; source = snapshot.data(); }
    for(unsigned y=0; y<clamped_dst_h; ++y) for(unsigned x=0; x<clamped_dst_w; ++x) {
        const unsigned sx=srcx+x*clamped_src_w/clamped_dst_w, sy=srcy+y*clamped_src_h/clamped_dst_h;
        BitmapHandlerClass::Copy_Pixel(CpuPixels.data()+static_cast<std::size_t>(dsty+y)*CpuPitch+(dstx+x)*dst_bpp,sd.Format,
            source+static_cast<std::size_t>(sy)*other->CpuPitch+sx*src_bpp,osd.Format,nullptr,0);
    }
}

/***********************************************************************************************
 * SurfaceClass::FindBB -- Finds the bounding box of non zero pixels in the region             *
 *=============================================================================================*/
void SurfaceClass::FindBB(Vector2i *min,Vector2i*max)
{
	SurfaceDescription sd;
	sd.Format = SurfaceFormat; sd.Width = CpuWidth; sd.Height = CpuHeight;

	WWASSERT(Has_Alpha(sd.Format));

	int alphabits=Alpha_Bits(sd.Format);
	int mask=0;
	switch (alphabits)
	{
	case 1: mask=1;
		break;
	case 4: mask=0xf;
		break;
	case 8: mask=0xff;
		break;
	}

	unsigned int size=::Get_Bytes_Per_Pixel(sd.Format);
	Vector2i realmin=*max;
	Vector2i realmax=*min;

	// the assumption here is that whenever a pixel has alpha it's in the MSB
	for (int y = min->J; y < max->J; y++) {
		for (int x = min->I; x < max->I; x++) {
			// HY - this is not endian safe
			const unsigned char *alpha = CpuPixels.data() + static_cast<std::size_t>(y)*static_cast<std::size_t>(CpuPitch) + static_cast<std::size_t>(x)*size;
			unsigned char myalpha=alpha[size-1];
			myalpha=(myalpha>>(8-alphabits)) & mask;
			if (myalpha) {
				realmin.I = MIN(realmin.I, x);
				realmax.I = MAX(realmax.I, x);
				realmin.J = MIN(realmin.J, y);
				realmax.J = MAX(realmax.J, y);
			}
		}
	}

	*max=realmax;
	*min=realmin;
}


/***********************************************************************************************
 * SurfaceClass::Is_Transparent_Column -- Tests to see if the column is transparent or not     *
 *=============================================================================================*/
bool SurfaceClass::Is_Transparent_Column(unsigned int column)
{
	SurfaceDescription sd;
	sd.Format = SurfaceFormat; sd.Width = CpuWidth; sd.Height = CpuHeight;

	WWASSERT(column<sd.Width);
	WWASSERT(Has_Alpha(sd.Format));

	int alphabits=Alpha_Bits(sd.Format);
	int mask=0;
	switch (alphabits)
	{
	case 1: mask=1;
		break;
	case 4: mask=0xf;
		break;
	case 8: mask=0xff;
		break;
	}

	unsigned int size=::Get_Bytes_Per_Pixel(sd.Format);

	// the assumption here is that whenever a pixel has alpha it's in the MSB
	for (unsigned y = 0; y < sd.Height; y++)
	{
		// HY - this is not endian safe
		const unsigned char *alpha = CpuPixels.data() + static_cast<std::size_t>(y)*static_cast<std::size_t>(CpuPitch) + static_cast<std::size_t>(column)*size;
		unsigned char myalpha=alpha[size-1];
		myalpha=(myalpha>>(8-alphabits)) & mask;
		if (myalpha) {
			return false;
		}
	}

	return true;
}

/***********************************************************************************************
 * SurfaceClass::Get_Pixel -- Returns the pixel's RGB valus to the caller                      *
 *=============================================================================================*/
void SurfaceClass::Get_Pixel(Vector3 &rgb, int x, int y, LockedSurfacePtr pBits, int pitch)
{
	SurfaceDescription sd;
	sd.Format = SurfaceFormat; sd.Width = CpuWidth; sd.Height = CpuHeight;

	unsigned int bytesPerPixel = ::Get_Bytes_Per_Pixel(sd.Format);
	unsigned char* dst = static_cast<unsigned char *>(pBits) + y * pitch + x * bytesPerPixel;
	Convert_Pixel(rgb,sd,dst);
}


/***********************************************************************************************
 * SurfaceClass::DrawPixel -- draws a pixel                                                    *
 *=============================================================================================*/
void SurfaceClass::Draw_Pixel(const unsigned int x, const unsigned int y, unsigned int color,
	unsigned int bytesPerPixel, LockedSurfacePtr pBits, int pitch)
{
	unsigned char* dst = static_cast<unsigned char*>(pBits) + y * pitch + x * bytesPerPixel;
	memcpy(dst, &color, bytesPerPixel);
}



/***********************************************************************************************
 * SurfaceClass::DrawHLine -- draws a horizontal line                                          *
 *=============================================================================================*/
void SurfaceClass::Draw_H_Line(const unsigned int y, const unsigned int x1, const unsigned int x2,
	unsigned int color, unsigned int bytesPerPixel, LockedSurfacePtr pBits, int pitch)
{
	unsigned char* row = static_cast<unsigned char*>(pBits) + y * pitch;

	for (unsigned int x = x1; x <= x2; ++x)
	{
		unsigned char* dst = row + x * bytesPerPixel;
		memcpy(dst, &color, bytesPerPixel);
	}
}


/***********************************************************************************************
 * SurfaceClass::Is_Monochrome -- Checks if surface is monochrome or not                       *
 *=============================================================================================*/
bool SurfaceClass::Is_Monochrome()
{
	unsigned int x,y;
	SurfaceDescription sd;
	sd.Format = SurfaceFormat; sd.Width = CpuWidth; sd.Height = CpuHeight;
	bool is_compressed = false;

	switch (sd.Format)
	{
		// these formats are always monochrome
		case WW3D_FORMAT_A8L8:
		case WW3D_FORMAT_A8:
		case WW3D_FORMAT_L8:
		case WW3D_FORMAT_A4L4:
			return true;
		break;
		// these formats cannot be determined to be monochrome or not
		case WW3D_FORMAT_UNKNOWN:
		case WW3D_FORMAT_A8P8:
		case WW3D_FORMAT_P8:
		case WW3D_FORMAT_U8V8:		// Bumpmap
		case WW3D_FORMAT_L6V5U5:	// Bumpmap
		case WW3D_FORMAT_X8L8V8U8:	// Bumpmap
			return false;
		break;
		// these formats need decompression first
		case WW3D_FORMAT_DXT1:
		case WW3D_FORMAT_DXT2:
		case WW3D_FORMAT_DXT3:
		case WW3D_FORMAT_DXT4:
		case WW3D_FORMAT_DXT5:
			is_compressed = true;
		break;
	}

	// if it's in some compressed texture format, be sure to decompress first
	if (is_compressed) {
		WW3DFormat new_format = Get_Valid_Texture_Format(sd.Format, false);
		SurfaceClass *new_surf = NEW_REF( SurfaceClass, (sd.Width, sd.Height, new_format) );
		new_surf->Copy(0, 0, 0, 0, sd.Width, sd.Height, this);
		bool result = new_surf->Is_Monochrome();
		REF_PTR_RELEASE(new_surf);
		return result;
	}

	int pitch,size;

	size=::Get_Bytes_Per_Pixel(sd.Format);
	unsigned char *bits=static_cast<unsigned char*>(Lock(&pitch));

	Vector3 rgb;
	bool mono=true;

	for (y=0; y<sd.Height; y++)
	{
		for (x=0; x<sd.Width; x++)
		{
			Convert_Pixel(rgb,sd,&bits[x*size]);
			mono&=(rgb.X==rgb.Y);
			mono&=(rgb.X==rgb.Z);
			mono&=(rgb.Z==rgb.Y);
			if (!mono)
			{
				Unlock();
				return false;
			}
		}
		bits+=pitch;
	}

	Unlock();

	return true;
}

/***********************************************************************************************
 * SurfaceClass::Hue_Shift -- changes the hue of the surface                                   *
 *=============================================================================================*/
void SurfaceClass::Hue_Shift(const Vector3 &hsv_shift)
{
	++Revision;
	unsigned int x,y;
	SurfaceDescription sd;
	sd.Format = SurfaceFormat; sd.Width = CpuWidth; sd.Height = CpuHeight;
	int pitch,size;

	size=::Get_Bytes_Per_Pixel(sd.Format);
	unsigned char *bits=static_cast<unsigned char*>(Lock(&pitch));

	Vector3 rgb;

	for (y=0; y<sd.Height; y++)
	{
		for (x=0; x<sd.Width; x++)
		{
			Convert_Pixel(rgb,sd,&bits[x*size]);
			Recolor(rgb,hsv_shift);
			rgb.X=Bound(rgb.X,0.0f,1.0f);
			rgb.Y=Bound(rgb.Y,0.0f,1.0f);
			rgb.Z=Bound(rgb.Z,0.0f,1.0f);
			Convert_Pixel(&bits[x*size],sd,rgb);
		}
		bits+=pitch;
	}

	Unlock();
}

bool SurfaceClass::Copy_RGBA8(std::vector<unsigned char> &pixels) const {
    const auto format=Get_Surface_Format();
    switch(format) {
    case WW3D_FORMAT_R8G8B8: case WW3D_FORMAT_A8R8G8B8: case WW3D_FORMAT_X8R8G8B8:
    case WW3D_FORMAT_A4R4G4B4: case WW3D_FORMAT_A1R5G5B5: case WW3D_FORMAT_R5G6B5:
    case WW3D_FORMAT_L8: case WW3D_FORMAT_A8: break;
    default: return false;
    }
    SurfaceClass::SurfaceDescription desc{}; Get_Description(desc);
    if(!desc.Width || !desc.Height) return false;
    int pitch=0; const unsigned char *data=Peek_CPU_Pixels(&pitch);
    if(!data) return false;
    pixels.resize(static_cast<std::size_t>(desc.Width)*desc.Height*4);
    const unsigned bpp=::Get_Bytes_Per_Pixel(format);
    for(unsigned y=0; y<desc.Height; ++y) for(unsigned x=0; x<desc.Width; ++x) {
        unsigned packed=0;
        BitmapHandlerClass::Read_B8G8R8A8(packed,data+static_cast<std::size_t>(y)*pitch+x*bpp,format,nullptr,0);
        unsigned char *pixel=pixels.data()+(static_cast<std::size_t>(y)*desc.Width+x)*4;
        pixel[0]=(packed>>16)&255; pixel[1]=(packed>>8)&255; pixel[2]=packed&255; pixel[3]=(packed>>24)&255;
        // Match normalized texture sampling, including full-white 16-bit glyphs.
        if (format==WW3D_FORMAT_A4R4G4B4 || format==WW3D_FORMAT_A1R5G5B5 || format==WW3D_FORMAT_R5G6B5) {
            unsigned short value=0;
            std::memcpy(&value,data+static_cast<std::size_t>(y)*pitch+x*bpp,sizeof(value));
            if (format==WW3D_FORMAT_A4R4G4B4) {
                pixel[0]=((value>>8)&15)*17; pixel[1]=((value>>4)&15)*17;
                pixel[2]=(value&15)*17; pixel[3]=(value>>12)*17;
            } else {
                pixel[0]=((value>>(format==WW3D_FORMAT_R5G6B5 ? 11 : 10))&31)*255/31;
                pixel[1]=((value>>5)&(format==WW3D_FORMAT_R5G6B5 ? 63 : 31))*255/(format==WW3D_FORMAT_R5G6B5 ? 63 : 31);
                pixel[2]=(value&31)*255/31;
                pixel[3]=format==WW3D_FORMAT_R5G6B5 || (value&0x8000) ? 255 : 0;
            }
        }
        if (format==WW3D_FORMAT_X8R8G8B8) pixel[3]=255;
    }
    return true;
}
