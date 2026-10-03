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

#pragma once
#include "IRenderBackend.h"
class TextureClass;
class CameraClass;
class SphereClass;

// Owns copied, world-space CPU geometry and transient render state until the
// existing WW3D sorting flush. No native renderer buffer/state is captured.
class SortingRendererClass
{
    static bool _EnableTriangleDraw;
public:
    static bool Submit_CPU_Triangles(const RenderBackendTexturedVertex *vertices,
        unsigned vertex_count, const unsigned short *indices, unsigned index_count,
        TextureClass *texture, const RenderBackendMaterialState &material,
        const CameraClass &camera, const SphereClass *world_bounds = nullptr,
        bool sort = true);
    static bool Flush();
    static void Deinit();
    static void SetMinVertexBufferSize(unsigned value);
    static void _Enable_Triangle_Draw(bool enable) { _EnableTriangleDraw = enable; }
    static bool _Is_Triangle_Draw_Enabled() { return _EnableTriangleDraw; }
};
