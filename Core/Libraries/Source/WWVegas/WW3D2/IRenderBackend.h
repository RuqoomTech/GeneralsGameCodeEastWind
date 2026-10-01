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
// interface for WW3D2's Direct3D 12 renderer. Legacy callers are migrated
// through this seam; they do not select a separate compatibility backend.

#pragma once

#include <vector>

// Forward declarations keep this header includable without pulling in the full
// WW3D2 header graph. All W3D types below are passed by pointer or reference.

class LightEnvironmentClass;
class Matrix4x4;
class Vector3;

struct RenderBackendFrameStatistics
{
    unsigned int draw_calls = 0;
    unsigned int triangles = 0;
    unsigned int vertices = 0;
};

struct RenderBackendColorVertex
{
    float x;
    float y;
    float z;
    float r;
    float g;
    float b;
    float a;

    bool operator == (const RenderBackendColorVertex &other) const
    {
        return x == other.x && y == other.y && z == other.z &&
               r == other.r && g == other.g && b == other.b && a == other.a;
    }

    bool operator != (const RenderBackendColorVertex &other) const
    {
        return !(*this == other);
    }
};

struct RenderBackendTexturedVertex
{
    float x;
    float y;
    float z;
    float r;
    float g;
    float b;
    float a;
    float u;
    float v;
    float q = 1.0f; // Undivided projected coordinate; ordinary UVs use q=1.

    bool operator == (const RenderBackendTexturedVertex &other) const
    {
        return x == other.x && y == other.y && z == other.z &&
               r == other.r && g == other.g && b == other.b && a == other.a &&
               u == other.u && v == other.v && q == other.q;
    }

    bool operator != (const RenderBackendTexturedVertex &other) const
    {
        return !(*this == other);
    }
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

enum class RenderBackend2DBlendMode
{
    Opaque,
    Alpha,
    Additive,
};

enum class RenderBackendDecalBlendMode
{
    Multiply,
    Alpha,
    Additive,
};

// Explicit draw state for migrated W3D material passes. This is runtime-only;
// the asset's fixed-width ShaderClass representation remains above the backend.
enum class RenderBackendDepthTest { Never, Less, Equal, LessEqual, Greater, NotEqual, GreaterEqual, Always, Disabled };
enum class RenderBackendBlendFactor { Zero, One, SourceColor, InverseSourceColor, SourceAlpha, InverseSourceAlpha, DestinationColor };
enum class RenderBackendCullMode { None, Clockwise, CounterClockwise };
enum class RenderBackendTextureCombine { Replace, Modulate, Add, Modulate2X };
enum class RenderBackendAlphaTest { Disabled, GreaterEqual, LessEqual };

struct RenderBackendMaterialState
{
    RenderBackendDepthTest depth_test = RenderBackendDepthTest::LessEqual;
    RenderBackendBlendFactor source_blend = RenderBackendBlendFactor::One;
    RenderBackendBlendFactor destination_blend = RenderBackendBlendFactor::Zero;
    RenderBackendCullMode cull = RenderBackendCullMode::Clockwise;
    RenderBackendTextureCombine texture_combine = RenderBackendTextureCombine::Modulate;
    RenderBackendAlphaTest alpha_test = RenderBackendAlphaTest::Disabled;
    float alpha_reference = 96.0f / 255.0f;
    bool depth_write = true;
    bool color_write = true;
    bool clamp_texture = false;
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

struct RenderBackendTextureHandle
{
    unsigned int slot;
    unsigned int generation;

    RenderBackendTextureHandle() : slot(0), generation(0) {}
    RenderBackendTextureHandle(unsigned int slot_value, unsigned int generation_value)
        : slot(slot_value), generation(generation_value) {}
    bool Is_Valid() const { return slot != 0 && generation != 0; }
};

// Non-owning CPU views valid for the duration of texture creation.
struct RenderBackendTextureMipLevel
{
    unsigned int width;
    unsigned int height;
    unsigned int row_pitch;
    const unsigned char *pixels;
};

// A method appears here once a caller routes through it, not in anticipation of
// one. The set below is what current callers route through; the rest of the
// legacy DX8Wrapper API remains reachable only in code that has not yet moved
// across this seam. Each migrated renderer responsibility belongs here (or in a
// renderer-neutral WW3D abstraction), not in a D3D8-on-D3D12 compatibility shim.
//
// Add renderer-neutral capabilities for actual WW3D callers. Do not mirror the
// legacy DX8Wrapper state machine or its resource interfaces here.

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
    // World-space primitive draws use WWMath column-vector math and a 0..1
    // clip-depth projection. Screen-space draws remain independent of camera.
    virtual void Set_View_Projection(const Matrix4x4 &view_projection) = 0;
    // Projected-shadow textures use RGBA8 color-only targets. Selection is
    // between scenes; an empty handle restores output viewport/camera state.
    virtual RenderBackendTextureHandle Create_Render_Texture(unsigned int width, unsigned int height) = 0;
    virtual bool Set_Render_Texture(RenderBackendTextureHandle texture) = 0;
    virtual bool Get_Render_Target_Size(int &width, int &height) const = 0;
    // Whole RGBA8 level-zero copy, between scenes, with matching dimensions.
    virtual bool Copy_Texture(RenderBackendTextureHandle destination, RenderBackendTextureHandle source) = 0;
    virtual void Invalidate_Cached_Render_States() = 0;

    // Device availability is renderer-owned. Frame callers use this before
    // updating view data that will be consumed by the active render device.
    virtual bool Is_Device_Ready() const { return false; }
    virtual bool Has_Stencil() const = 0;
    virtual void Reset_Frame_Statistics() = 0;
    virtual RenderBackendFrameStatistics Get_Frame_Statistics() const = 0;

    // Presentation intervals count vertical retraces (0 is immediate, 1-4
    // wait for retraces). They are local renderer state, never simulation time.
    virtual bool Set_Swap_Interval(unsigned int interval) = 0;
    virtual unsigned int Get_Swap_Interval() const = 0;

    // Capture the latest submitted output in tightly packed top-down RGBA8.
    // CPU pixels have no GPU/native object representation and are local only.
    virtual bool Read_Output_RGBA8(unsigned int &width, unsigned int &height,
                                  std::vector<unsigned char> &pixels) = 0;

    // WW3D's normal display caller configures the active swapchain after
    // creating the backend and queries its actual pixel dimensions for 2D UI.
    virtual bool Configure_Output(unsigned int width, unsigned int height, bool windowed)
    {
        (void)width;
        (void)height;
        (void)windowed;
        return false;
    }

    virtual bool Get_Output_Description(
        int &width, int &height, int &bits, bool &windowed) const
    {
        (void)width;
        (void)height;
        (void)bits;
        (void)windowed;
        return false;
    }

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

    // Real WW3D screen-space primitive path. Render2D owns coordinate and
    // color generation; the backend owns the explicit depth/blend PSO.
    virtual bool Draw_2D_Indexed_Triangles(
        const RenderBackendColorVertex *vertices,
        unsigned int vertex_count,
        const unsigned short *indices,
        unsigned int index_count,
        RenderBackend2DBlendMode blend_mode)
    {
        (void)vertices;
        (void)vertex_count;
        (void)indices;
        (void)index_count;
        (void)blend_mode;
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

    virtual RenderBackendGeometryHandle Create_Static_Indexed_Textured_Geometry(
        const RenderBackendTexturedVertex *vertices,
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

    virtual RenderBackendTextureHandle Create_Static_RGBA8_Texture(
        unsigned int width,
        unsigned int height,
        const unsigned char *pixels,
        unsigned int row_pitch)
    {
        (void)width;
        (void)height;
        (void)pixels;
        (void)row_pitch;
        return RenderBackendTextureHandle();
    }

    virtual bool Draw_Static_Indexed_Textured_Geometry(
        RenderBackendGeometryHandle geometry,
        RenderBackendTextureHandle texture)
    {
        (void)geometry;
        (void)texture;
        return false;
    }

    virtual RenderBackendTextureHandle Create_Static_RGBA8_Texture(
        const RenderBackendTextureMipLevel *levels, unsigned int level_count) = 0;

    virtual bool Is_Texture_Valid(RenderBackendTextureHandle texture) const = 0;
    // World-space decal batches: texture * diffuse, clamp/no mipmaps, LEQUAL,
    // no depth writes, clockwise culling, and the selected blend equation.
    virtual bool Draw_Indexed_Decal_Triangles(
        const RenderBackendTexturedVertex *vertices, unsigned int vertex_count,
        const unsigned short *indices, unsigned int index_count,
        RenderBackendTextureHandle texture, RenderBackendDecalBlendMode blend_mode) = 0;
    virtual bool Draw_Indexed_Material_Triangles(
        const RenderBackendTexturedVertex *vertices, unsigned int vertex_count,
        const unsigned short *indices, unsigned int index_count,
        RenderBackendTextureHandle texture, const RenderBackendMaterialState &material) = 0;
    virtual void Release_Texture(RenderBackendTextureHandle texture) = 0;

    virtual void Set_Ambient(const Vector3 & color) = 0;
    virtual void Set_Light_Environment(LightEnvironmentClass * light_env) = 0;
};
