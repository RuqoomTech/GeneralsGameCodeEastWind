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
 *                     $Archive:: ww3d2/texturefilter.cpp												$*
 *                                                                                             *
 *                  $Org Author:: Kenny Mitchell                                              $*
 *                                                                                             *
 *                       Author : Kenny Mitchell                                               *
 *                                                                                             *
 *                     $Modtime:: 08/05/02 1:27p                                              $*
 *                                                                                             *
 *                    $Revision:: 1                                                          $*
 *                                                                                             *
 * 08/05/02 KM Texture filter class abstraction																			*
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include "texturefilter.h"
#include "IRenderBackend.h"
#include <algorithm>

const char* const TextureFilterClass::TextureFilterModeString[TEXTURE_FILTER_COUNT] = {
	"None",
	"Point",
	"Bilinear",
	"Trilinear",
	"Anisotropic"
};

TextureFilterClass::TextureFilterMode TextureFilterClass::getTextureFilterMode(const char* str) {
	for (int i = 0; i < TextureFilterClass::TEXTURE_FILTER_COUNT; ++i) {
		if (stricmp(str, TextureFilterClass::TextureFilterModeString[i]) == 0) {
			return (TextureFilterClass::TextureFilterMode)i;
		}
	}

	return TextureFilterClass::TEXTURE_FILTER_NONE;
}

namespace {
enum class FilterChoice { None, Point, Linear, Anisotropic };
constexpr unsigned FilterStages = 8;
FilterChoice _MinTextureFilters[FilterStages][TextureFilterClass::FILTER_TYPE_COUNT];
FilterChoice _MagTextureFilters[FilterStages][TextureFilterClass::FILTER_TYPE_COUNT];
FilterChoice _MipMapFilters[FilterStages][TextureFilterClass::FILTER_TYPE_COUNT];
unsigned MaxAnisotropy = 2;
bool FiltersInitialized = false;
void ensureFilters()
{
    if (!FiltersInitialized) TextureFilterClass::_Init_Filters(
        TextureFilterClass::TEXTURE_FILTER_BILINEAR, TextureFilterClass::TEXTURE_FILTER_ANISOTROPIC_2X);
}
}

/*************************************************************************
**                             TextureFilterClass
*************************************************************************/
TextureFilterClass::TextureFilterClass(MipCountType mip_level_count)
:	TextureMinFilter(FILTER_TYPE_DEFAULT),
	TextureMagFilter(FILTER_TYPE_DEFAULT),
	UAddressMode(TEXTURE_ADDRESS_REPEAT),
	VAddressMode(TEXTURE_ADDRESS_REPEAT)
{
	if (mip_level_count!=MIP_LEVELS_1)
	{
		MipMapFilter=FILTER_TYPE_DEFAULT;
	}
	else
	{
		MipMapFilter=FILTER_TYPE_NONE;
	}
}

//**********************************************************************************************
//! Apply filters (legacy)
/*!
*/
void TextureFilterClass::Apply(unsigned int stage)
{
    // Renderer-neutral: sampler state is consumed via Get_Render_Sampler()
    // during backend draws. Legacy fixed-function stage state no longer exists.
    ensureFilters();
    if (stage>=FilterStages) return;
}

//**********************************************************************************************
//! Init filters (legacy)
/*!
*/
void TextureFilterClass::_Init_Filters(TextureFilterMode mode, AnisotropicFilterMode anisotropy_level)
{
    // D3D12 supports the normalized filter modes; device-capability fallback
    // belongs to the retired renderer and is not part of texture asset policy.
    for (unsigned stage=0;stage<FilterStages;++stage) {
        _MinTextureFilters[stage][FILTER_TYPE_NONE]=FilterChoice::Point;
        _MagTextureFilters[stage][FILTER_TYPE_NONE]=FilterChoice::Point;
        _MipMapFilters[stage][FILTER_TYPE_NONE]=FilterChoice::None;
        FilterChoice min=FilterChoice::Linear, mag=FilterChoice::Linear, mip=FilterChoice::Point;
        if (mode==TEXTURE_FILTER_NONE || mode==TEXTURE_FILTER_POINT) min=mag=FilterChoice::Point;
        if (mode==TEXTURE_FILTER_NONE) mip=FilterChoice::None;
        _MinTextureFilters[stage][FILTER_TYPE_FAST]=min;
        _MagTextureFilters[stage][FILTER_TYPE_FAST]=mag;
        _MipMapFilters[stage][FILTER_TYPE_FAST]=mip;
        if (mode==TEXTURE_FILTER_TRILINEAR || mode==TEXTURE_FILTER_ANISOTROPIC) mip=FilterChoice::Linear;
        if (mode==TEXTURE_FILTER_ANISOTROPIC && stage==0) min=mag=FilterChoice::Anisotropic;
        _MinTextureFilters[stage][FILTER_TYPE_BEST]=min;
        _MagTextureFilters[stage][FILTER_TYPE_BEST]=mag;
        _MipMapFilters[stage][FILTER_TYPE_BEST]=mip;
        _MinTextureFilters[stage][FILTER_TYPE_DEFAULT]=min;
        _MagTextureFilters[stage][FILTER_TYPE_DEFAULT]=mag;
        _MipMapFilters[stage][FILTER_TYPE_DEFAULT]=mip;
    }
    MaxAnisotropy=std::clamp(static_cast<unsigned>(anisotropy_level),1u,16u);
    FiltersInitialized=true;
}

bool TextureFilterClass::Get_Render_Sampler(RenderBackendSamplerState &result, unsigned int stage) const
{
    if (stage>=FilterStages || static_cast<unsigned>(TextureMinFilter)>=FILTER_TYPE_COUNT ||
        static_cast<unsigned>(TextureMagFilter)>=FILTER_TYPE_COUNT ||
        static_cast<unsigned>(MipMapFilter)>=FILTER_TYPE_COUNT ||
        static_cast<unsigned>(UAddressMode)>1 || static_cast<unsigned>(VAddressMode)>1) return false;
    ensureFilters();
    const auto min=_MinTextureFilters[stage][TextureMinFilter];
    const auto mag=_MagTextureFilters[stage][TextureMagFilter];
    const auto mip=_MipMapFilters[stage][MipMapFilter];
    RenderBackendSamplerState sampler;
    sampler.min_filter=min==FilterChoice::Point ? RenderBackendTextureFilter::Point : RenderBackendTextureFilter::Linear;
    sampler.mag_filter=mag==FilterChoice::Point ? RenderBackendTextureFilter::Point : RenderBackendTextureFilter::Linear;
    sampler.mip_filter=mip==FilterChoice::Linear ? RenderBackendTextureFilter::Linear : RenderBackendTextureFilter::Point;
    sampler.mipmaps=mip!=FilterChoice::None;
    sampler.max_anisotropy=min==FilterChoice::Anisotropic || mag==FilterChoice::Anisotropic ? MaxAnisotropy : 1;
    sampler.address_u=UAddressMode==TEXTURE_ADDRESS_REPEAT ? RenderBackendTextureAddress::Wrap : RenderBackendTextureAddress::Clamp;
    sampler.address_v=VAddressMode==TEXTURE_ADDRESS_REPEAT ? RenderBackendTextureAddress::Wrap : RenderBackendTextureAddress::Clamp;
    result=sampler;
    return true;
}


//**********************************************************************************************
//! Set mip mapping filter (legacy)
/*!
*/
void TextureFilterClass::Set_Mip_Mapping(FilterType mipmap)
{
//	if (mipmap != FILTER_TYPE_NONE && Get_Mip_Level_Count() <= 1 && Is_Initialized())
//	{
//		WWASSERT_PRINT(0, "Trying to enable MipMapping on texture w/o Mip levels!");
//		return;
//	}
	MipMapFilter=mipmap;
}

//**********************************************************************************************
//! Set anisotropic filter level
/*!
*/
void TextureFilterClass::_Set_Max_Anisotropy(AnisotropicFilterMode mode)
{
    ensureFilters();
    MaxAnisotropy=std::clamp(static_cast<unsigned>(mode),1u,16u);
}

//**********************************************************************************************
//! Set default min filter (legacy)
/*!
*/
void TextureFilterClass::_Set_Default_Min_Filter(FilterType filter)
{
    ensureFilters();
    if (static_cast<unsigned>(filter)>=FILTER_TYPE_COUNT) return;
	for (unsigned i=0;i<FilterStages;++i)
	{
		_MinTextureFilters[i][FILTER_TYPE_DEFAULT]=_MinTextureFilters[i][filter];
	}
}


//**********************************************************************************************
//! Set default mag filter (legacy)
/*!
*/
void TextureFilterClass::_Set_Default_Mag_Filter(FilterType filter)
{
    ensureFilters();
    if (static_cast<unsigned>(filter)>=FILTER_TYPE_COUNT) return;
	for (unsigned i=0;i<FilterStages;++i)
	{
		_MagTextureFilters[i][FILTER_TYPE_DEFAULT]=_MagTextureFilters[i][filter];
	}
}

//**********************************************************************************************
//! Set default mip filter (legacy)
/*!
*/
void TextureFilterClass::_Set_Default_Mip_Filter(FilterType filter)
{
    ensureFilters();
    if (static_cast<unsigned>(filter)>=FILTER_TYPE_COUNT) return;
	for (unsigned i=0;i<FilterStages;++i)
	{
		_MipMapFilters[i][FILTER_TYPE_DEFAULT]=_MipMapFilters[i][filter];
	}
}
