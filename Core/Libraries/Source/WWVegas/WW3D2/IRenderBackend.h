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

// TheSuperHackers @refactor bobtista 10/04/2026 Abstract W3D-facing rendering
// interface so WW3D2 rendering can be re-targeted to other backends while the
// x64 runtime is migrated to Direct3D 12 while the legacy DX8 path remains
// available only to archival 32-bit/reference builds.

#pragma once

// Forward declarations keep this header includable without pulling in the full
// WW3D2 header graph. All W3D types below are passed by pointer or reference.

class LightEnvironmentClass;
class Vector3;

struct RenderBackendColorVertex
{
    float x;
    float y;
    float z;
    float r;
    float g;
    float b;
    float a;
};

struct RenderBackendViewport
{
    unsigned int x;
    unsigned int y;
    unsigned int width;
    unsigned int height;
    float min_z;
    float max_z;
};

struct RenderBackendGeometryHandle
{
    unsigned int slot;
    unsigned int generation;

    RenderBackendGeometryHandle() : slot(0), generation(0) {}
    RenderBackendGeometryHandle(unsigned int slot_value, unsigned int generation_value)
        : slot(slot_value), generation(generation_value) {}
    bool Is_Valid() const { return slot != 0 && generation != 0; }
};

// A method appears here once a caller routes through it, not in anticipation of
// one. The set below is what current callers route through; the rest of the
// legacy DX8Wrapper API remains reachable only in code that has not yet moved
// across this seam. Each migrated renderer responsibility belongs here (or in a
// renderer-neutral WW3D abstraction), not in a D3D8-on-D3D12 compatibility shim.
//
// Method names intentionally match the existing DX8Wrapper names so migrating a
// caller is a mechanical DX8Wrapper::X(...) -> Get_Render_Backend()->X(...)
// rewrite.

class IRenderBackend
{
public:
    virtual ~IRenderBackend() {}

    virtual void Set_Gamma(float gamma, float bright, float contrast, bool calibrate = true, bool uselimit = true) = 0;

    virtual void Begin_Scene() = 0;
    virtual void End_Scene(bool flip_frame = true) = 0;
    virtual void Flip_To_Primary() = 0;
    virtual void Clear(bool clear_color, bool clear_z_stencil,
                       const Vector3 & color,
                       float dest_alpha = 0.0f, float z = 1.0f, unsigned int stencil = 0) = 0;
    virtual void Set_Viewport(const RenderBackendViewport & viewport) = 0;
    virtual void Invalidate_Cached_Render_States() = 0;

    // First renderer-neutral indexed primitive path. This deliberately uses a
    // small position/color vertex contract; mesh/material formats stay above
    // the backend and can add dedicated paths as they are migrated.
    virtual bool Draw_Indexed_Triangles(
        const RenderBackendColorVertex *vertices,
        unsigned int vertex_count,
        const unsigned short *indices,
        unsigned int index_count)
    {
        (void)vertices;
        (void)vertex_count;
        (void)indices;
        (void)index_count;
        return false;
    }

    // Persistent geometry is the first resource-lifetime contract owned by the
    // renderer-neutral seam. Backends that do not support it return an invalid
    // handle; callers must remain functional without a D3D8 compatibility shim.
    virtual RenderBackendGeometryHandle Create_Static_Indexed_Color_Geometry(
        const RenderBackendColorVertex *vertices,
        unsigned int vertex_count,
        const unsigned short *indices,
        unsigned int index_count)
    {
        (void)vertices;
        (void)vertex_count;
        (void)indices;
        (void)index_count;
        return RenderBackendGeometryHandle();
    }

    virtual bool Draw_Static_Indexed_Color_Geometry(RenderBackendGeometryHandle geometry)
    {
        (void)geometry;
        return false;
    }

    virtual void Release_Static_Geometry(RenderBackendGeometryHandle geometry)
    {
        (void)geometry;
    }

    virtual void Set_Ambient(const Vector3 & color) = 0;
    virtual void Set_Light_Environment(LightEnvironmentClass * light_env) = 0;
};
