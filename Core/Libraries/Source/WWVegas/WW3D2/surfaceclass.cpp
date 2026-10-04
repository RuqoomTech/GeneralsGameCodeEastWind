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
#include "WWLib/TARGA.h"
#include "WWLib/ffactory.h"
#include "ww3dformat.h"
#include "missingtexture.h"
#if __has_include("ddsfile.h")
#include "ddsfile.h"
#define SURFACE_HAS_DDS 1
#endif

#if !defined(RTS_EVOLUTION_X64)
// Legacy utility-library surface helpers were retired on x64. The CPU bitmap
// path below (Copy_Surface_Region_CPU) is the only copy/scale implementation.
#endif

namespace
{
// CPU-side locked-rect view. Pixel addressing stays on the native pointer;
// never truncate through 32-bit integers.
struct CpuLockRect
{
	void *pBits;
	int Pitch;
};

bool Copy_Surface_Region_CPU(
	unsigned char *dest_pixels,
	int dest_pitch,
	const SurfaceClass::SurfaceDescription &dest_desc,
	unsigned dest_x,
	unsigned dest_y,
	unsigned dest_w,
	unsigned dest_h,
	const unsigned char *src_pixels,
	int src_pitch,
	const SurfaceClass::SurfaceDescription &src_desc,
	unsigned src_x,
	unsigned src_y,
	unsigned src_w,
	unsigned src_h)
{
	if (dest_w == 0 || dest_h == 0 || src_w == 0 || src_h == 0) return true;
	if (dest_pixels == nullptr || src_pixels == nullptr) return false;
	CpuLockRect lock_rect{dest_pixels + static_cast<std::size_t>(dest_y) * static_cast<std::size_t>(dest_pitch), dest_pitch};
	unsigned char *dest_base = static_cast<unsigned char *>(lock_rect.pBits);
	(void)dest_base;
	BitmapHandlerClass::Copy_Image(
		dest_pixels + static_cast<std::size_t>(dest_y) * static_cast<std::size_t>(dest_pitch),
		dest_w, dest_h, static_cast<unsigned>(dest_pitch), dest_desc.Format,
		const_cast<unsigned char*>(src_pixels + static_cast<std::size_t>(src_y) * static_cast<std::size_t>(src_pitch)),
		src_w, src_h, static_cast<unsigned>(src_pitch), src_desc.Format,
		nullptr, 0, false);
	(void)dest_x; (void)src_x;
	return true;
}
}

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
	CpuPitch = static_cast<int>(width * bpp);
	CpuPixels.assign(static_cast<std::size_t>(CpuPitch) * height, 0);
}

SurfaceClass::SurfaceClass(const char *filename):
	CpuWidth(0),
	CpuHeight(0),
	CpuPitch(0),
	SurfaceFormat(WW3D_FORMAT_A8R8G8B8)
{
	// Renderer-neutral file load: DDS decodes to RGBA8 on the CPU, TGA keeps
	// its authored uncompressed format. Missing files fall back to magenta.
	bool loaded = false;
#if defined(SURFACE_HAS_DDS)
	{
		DDSFileClass dds_file(filename, 0, true);
		if (dds_file.Is_Available() && dds_file.Load() && dds_file.Get_Type() == DDS_TEXTURE) {
			const unsigned w = dds_file.Get_Full_Width();
			const unsigned h = dds_file.Get_Full_Height();
			if (w != 0 && h != 0 && w <= 16384 && h <= 16384) {
				std::vector<unsigned char> rgba(static_cast<std::size_t>(w) * h * 4);
				if (dds_file.Copy_Level_RGBA8(0, rgba.data(), w * 4)) {
					CpuWidth = w;
					CpuHeight = h;
					SurfaceFormat = WW3D_FORMAT_A8R8G8B8;
					CpuPitch = static_cast<int>(w * 4);
					CpuPixels.swap(rgba);
					loaded = true;
				}
			}
		}
	}
#endif
	if (!loaded) {
		Targa targa;
		if (targa.Open(filename, TGA_READMODE) == 0) {
			// Preserve the established game texture orientation.
			targa.Header.ImageDescriptor ^= TGAIDF_YORIGIN;
			WW3DFormat src_format = WW3D_FORMAT_UNKNOWN;
			unsigned src_bpp = 0;
			Get_WW3D_Format(src_format, src_bpp, targa);
			const unsigned w = targa.Header.Width;
			const unsigned h = targa.Header.Height;
			if (src_format != WW3D_FORMAT_UNKNOWN && w != 0 && h != 0 && w <= 16384 && h <= 16384) {
				char palette[256*4]{};
				targa.SetPalette(palette);
				if (targa.Load(filename, TGAF_IMAGE, false) == 0 && targa.GetImage() != nullptr) {
					const unsigned bpp = ::Get_Bytes_Per_Pixel(src_format);
					if (bpp != 0) {
						CpuWidth = w;
						CpuHeight = h;
						SurfaceFormat = src_format;
						CpuPitch = static_cast<int>(w * bpp);
						CpuPixels.assign(static_cast<std::size_t>(CpuPitch) * h, 0);
						BitmapHandlerClass::Copy_Image(
							CpuPixels.data(), w, h, static_cast<unsigned>(CpuPitch), src_format,
							reinterpret_cast<unsigned char *>(targa.GetImage()), w, h, w * src_bpp, src_format,
							reinterpret_cast<const unsigned char *>(targa.GetPalette()),
							static_cast<unsigned>(targa.Header.CMapDepth >> 3), false);
						loaded = true;
					}
				}
			}
		}
	}
	if (!loaded) {
		unsigned w = 0, h = 0;
		std::vector<unsigned char> magenta;
		MissingTexture::Create_RGBA8_Image(w, h, magenta);
		CpuWidth = w;
		CpuHeight = h;
		SurfaceFormat = WW3D_FORMAT_A8R8G8B8;
		CpuPitch = static_cast<int>(w * 4);
		CpuPixels.swap(magenta);
	}
}

SurfaceClass::~SurfaceClass()
{
}

void SurfaceClass::Get_Description(SurfaceDescription &surface_desc)
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
	*pitch = CpuPitch;
	CpuLocked = true;
	return static_cast<LockedSurfacePtr>(CpuPixels.data());
}

SurfaceClass::LockedSurfacePtr SurfaceClass::Lock(int *pitch, const Vector2i &min, const Vector2i &max)
{
	WWASSERT(!CpuPixels.empty());
	WWASSERT(min.I >= 0 && min.J >= 0 && max.I <= static_cast<int>(CpuWidth) && max.J <= static_cast<int>(CpuHeight));
	WWASSERT(max.I > min.I && max.J > min.J);
	const unsigned bpp = ::Get_Bytes_Per_Pixel(SurfaceFormat);
	*pitch = CpuPitch;
	CpuLocked = true;
	return static_cast<LockedSurfacePtr>(CpuPixels.data() + static_cast<std::size_t>(min.J) * static_cast<std::size_t>(CpuPitch) + static_cast<std::size_t>(min.I) * bpp);
}

void SurfaceClass::Unlock()
{
	CpuLocked = false;
}

const unsigned char *SurfaceClass::Peek_CPU_Pixels(int *pitch) const
{
	if (pitch) *pitch = CpuPitch;
	return CpuPixels.empty() ? nullptr : CpuPixels.data();
}

unsigned char *SurfaceClass::Peek_CPU_Pixels(int *pitch)
{
	if (pitch) *pitch = CpuPitch;
	return CpuPixels.empty() ? nullptr : CpuPixels.data();
}

/***********************************************************************************************
 * SurfaceClass::Clear -- Clears a surface to 0                                                *
 *=============================================================================================*/
void SurfaceClass::Clear()
{
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
	// size of each pixel in bytes
	unsigned int size=::Get_Bytes_Per_Pixel(SurfaceFormat);
	if (CpuPixels.empty() || size == 0) return;
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
	// size of each pixel in bytes
	unsigned int size=::Get_Bytes_Per_Pixel(SurfaceFormat);
	if (CpuPixels.empty() || size == 0) return;
	WWASSERT(other != nullptr);

	// Source image is tightly packed full-surface extents; SurfaceDescription
	// Width/Height below describe the destination surface for offset math.
	SurfaceDescription sd;
	sd.Format = SurfaceFormat;
	sd.Width = CpuWidth;
	sd.Height = CpuHeight;

	unsigned char *mem= CpuPixels.data() + static_cast<std::size_t>(min.J) * static_cast<std::size_t>(CpuPitch) + static_cast<std::size_t>(min.I) * size;
	// Rect lock emulation: Lock returns the sub-rect pointer, so advance by pitch.
	// Recompute base pointer for row iteration.
	mem = CpuPixels.data() + static_cast<std::size_t>(min.J) * static_cast<std::size_t>(CpuPitch);
	// Adjust for column offset below per row.
	int dx=max.I-min.I;

	for (int i=min.J; i<max.J; i++)
	{
		unsigned char *row = CpuPixels.data() + static_cast<std::size_t>(i) * static_cast<std::size_t>(CpuPitch) + static_cast<std::size_t>(min.I) * size;
		memcpy(row,&other[(static_cast<std::size_t>(i)*sd.Width+static_cast<std::size_t>(min.I))*size],size*dx);
		(void)mem;
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
	WWASSERT(other);
	WWASSERT(width);
	WWASSERT(height);

	SurfaceDescription sd,osd;
	sd.Format = SurfaceFormat; sd.Width = CpuWidth; sd.Height = CpuHeight;
	osd.Format = other->SurfaceFormat; osd.Width = other->CpuWidth; osd.Height = other->CpuHeight;

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

	if (sd.Format == osd.Format) {
		for (unsigned y = 0; y < copy_height; ++y) {
			const unsigned char *src_row = other->CpuPixels.data() + static_cast<std::size_t>(srcy + y) * static_cast<std::size_t>(other->CpuPitch) + static_cast<std::size_t>(srcx) * src_bpp;
			unsigned char *dst_row = CpuPixels.data() + static_cast<std::size_t>(dsty + y) * static_cast<std::size_t>(CpuPitch) + static_cast<std::size_t>(dstx) * dst_bpp;
			memcpy(dst_row, src_row, static_cast<std::size_t>(copy_width) * dst_bpp);
		}
		return;
	}

	BitmapHandlerClass::Copy_Image(
		CpuPixels.data() + static_cast<std::size_t>(dsty) * static_cast<std::size_t>(CpuPitch) + static_cast<std::size_t>(dstx) * dst_bpp,
		copy_width, copy_height, static_cast<unsigned>(CpuPitch), sd.Format,
		other->CpuPixels.data() + static_cast<std::size_t>(srcy) * static_cast<std::size_t>(other->CpuPitch) + static_cast<std::size_t>(srcx) * src_bpp,
		copy_width, copy_height, static_cast<unsigned>(other->CpuPitch), osd.Format,
		nullptr, 0, false);
}

/***********************************************************************************************
 * SurfaceClass::Copy -- Copies a region from one surface to another                           *
 *=============================================================================================*/
void SurfaceClass::Stretch_Copy(
	unsigned int dstx, unsigned int dsty, unsigned int dstwidth, unsigned int dstheight,
	unsigned int srcx, unsigned int srcy, unsigned int srcwidth, unsigned int srcheight,
	const SurfaceClass *other)
{
	WWASSERT(other);
	if (dstwidth == 0 || dstheight == 0 || srcwidth == 0 || srcheight == 0) return;

	SurfaceDescription sd,osd;
	sd.Format = SurfaceFormat; sd.Width = CpuWidth; sd.Height = CpuHeight;
	osd.Format = other->SurfaceFormat; osd.Width = other->CpuWidth; osd.Height = other->CpuHeight;

	const unsigned dst_bpp = ::Get_Bytes_Per_Pixel(sd.Format);
	const unsigned src_bpp = ::Get_Bytes_Per_Pixel(osd.Format);
	if (dst_bpp == 0 || src_bpp == 0) return;
	if (CpuPixels.empty() || other->CpuPixels.empty()) return;

	// Clamp to surface extents for determinism.
	unsigned clamped_dst_w = dstwidth;
	unsigned clamped_dst_h = dstheight;
	unsigned clamped_src_w = srcwidth;
	unsigned clamped_src_h = srcheight;
	if (dstx + clamped_dst_w > CpuWidth) clamped_dst_w = CpuWidth - dstx;
	if (dsty + clamped_dst_h > CpuHeight) clamped_dst_h = CpuHeight - dsty;
	if (srcx + clamped_src_w > other->CpuWidth) clamped_src_w = other->CpuWidth - srcx;
	if (srcy + clamped_src_h > other->CpuHeight) clamped_src_h = other->CpuHeight - srcy;
	if (clamped_dst_w == 0 || clamped_dst_h == 0 || clamped_src_w == 0 || clamped_src_h == 0) return;

	BitmapHandlerClass::Copy_Image(
		CpuPixels.data() + static_cast<std::size_t>(dsty) * static_cast<std::size_t>(CpuPitch) + static_cast<std::size_t>(dstx) * dst_bpp,
		clamped_dst_w, clamped_dst_h, static_cast<unsigned>(CpuPitch), sd.Format,
		other->CpuPixels.data() + static_cast<std::size_t>(srcy) * static_cast<std::size_t>(other->CpuPitch) + static_cast<std::size_t>(srcx) * src_bpp,
		clamped_src_w, clamped_src_h, static_cast<unsigned>(other->CpuPitch), osd.Format,
		nullptr, 0, false);
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
