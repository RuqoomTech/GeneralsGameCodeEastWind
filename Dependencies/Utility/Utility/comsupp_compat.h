/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2026 TheSuperHackers
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

/**
 * @file comsupp_compat.h
 * @brief Small COM support compatibility shim for MinGW-w64.
 *
 * Modern MinGW-w64 already provides _com_util::ConvertStringToBSTR() and
 * _com_util::ConvertBSTRToString() through <comutil.h>/<comdef.h>. This shim
 * therefore must not redefine those helpers. It only provides the vtMissing
 * storage expected by legacy Generals/Zero Hour COM call sites.
 *
 * Keep this header intentionally small: COM conversion behavior belongs to the
 * platform headers, not to a second project-owned implementation.
 */

#pragma once

#ifdef __MINGW32__

#include <windows.h>
#include <ole2.h>
#include <oleauto.h>
#include <comdef.h>

// Modern MinGW-w64 provides _com_util::ConvertStringToBSTR() and
// _com_util::ConvertBSTRToString() in <comutil.h>. Do not redefine them here.

// Provide vtMissing global variable
// Use inline variable (C++17) to avoid multiple definition errors
inline _variant_t vtMissing(DISP_E_PARAMNOTFOUND, VT_ERROR);

#endif // __MINGW32__
