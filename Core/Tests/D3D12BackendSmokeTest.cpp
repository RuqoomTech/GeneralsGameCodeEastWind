#include "Utility/CppMacros.h"

#include "WW3D2/Backend/RenderBackend.h"
#include "WW3D2/IRenderBackend.h"
#include "WWMath/vector3.h"
#include "WWMath/matrix4.h"

#include <windows.h>

#include <iostream>
#include <algorithm>
#include <cmath>

namespace
{
constexpr wchar_t WindowClassName[] = L"GeneralsD3D12BackendSmoke";

LRESULT CALLBACK smokeWindowProc(HWND window, UINT message, WPARAM w_param, LPARAM l_param)
{
    if (message == WM_CLOSE)
    {
        DestroyWindow(window);
        return 0;
    }
    return DefWindowProcW(window, message, w_param, l_param);
}

HWND createHiddenWindow(HINSTANCE instance)
{
    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = smokeWindowProc;
    window_class.hInstance = instance;
    window_class.lpszClassName = WindowClassName;
    if (RegisterClassExW(&window_class) == 0)
    {
        return nullptr;
    }

    return CreateWindowExW(
        0,
        WindowClassName,
        L"Generals D3D12 backend smoke",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        640,
        480,
        nullptr,
        nullptr,
        instance,
        nullptr);
}
bool verifyDecals(IRenderBackend &backend)
{
    const unsigned char rgba[] = {128,64,192,128};
    const auto texture = backend.Create_Static_RGBA8_Texture(1, 1, rgba, 4);
    const unsigned char edge_rgba[] = {255,0,0,255, 0,255,0,255};
    const auto edge = backend.Create_Static_RGBA8_Texture(2, 1, edge_rgba, 8);
    RenderBackendTexturedVertex quad[] = {
        {-0.8f,-0.8f,0.5f, .5f,.5f,.5f,.5f, 0,0},
        {-0.8f, 0.8f,0.5f, .5f,.5f,.5f,.5f, 0,0},
        { 0.8f, 0.8f,0.5f, .5f,.5f,.5f,.5f, 0,0},
        { 0.8f,-0.8f,0.5f, .5f,.5f,.5f,.5f, 0,0},
    };
    const unsigned short front[] = {0,2,1,0,3,2};
    const unsigned short back[] = {0,1,2,0,2,3};
    const unsigned short invalid[] = {0,1,4};
    unsigned width = 0, height = 0;
    std::vector<unsigned char> pixels;
    auto pixelMatches = [](unsigned char value, float expected) {
        return std::abs(int(value) - int(std::lround(std::clamp(expected, 0.0f, 1.0f)*255))) <= 2;
    };
    auto capture = [&]() {
        backend.End_Scene(false);
        return backend.Read_Output_RGBA8(width, height, pixels) && width == 640 && height == 480;
    };
    backend.Set_Viewport({0,0,640,480,0,1});
    backend.Set_View_Projection(Matrix4x4(true));
    bool ok = texture.Is_Valid() && edge.Is_Valid() && backend.Is_Texture_Valid(texture);
    for (const auto blend : {RenderBackendDecalBlendMode::Multiply, RenderBackendDecalBlendMode::Alpha, RenderBackendDecalBlendMode::Additive}) {
        backend.Clear(true, true, Vector3(.2f,.4f,.6f), 1, 1, 0);
        backend.Begin_Scene();
        ok = !backend.Draw_Indexed_Decal_Triangles(quad,4,front,6,{},blend) &&
             !backend.Draw_Indexed_Decal_Triangles(quad,4,invalid,3,texture,blend) &&
             !backend.Draw_Indexed_Decal_Triangles(quad,4,front,6,texture,static_cast<RenderBackendDecalBlendMode>(3)) && ok;
        ok = backend.Draw_Indexed_Decal_Triangles(quad,4,front,6,texture,blend) && ok;
        const bool read = capture(); ok = read && ok;
        if (read) {
            const float source[] = {128.f/510,64.f/510,192.f/510,128.f/510};
            const float destination[] = {.2f,.4f,.6f,1};
            for (unsigned channel = 0; channel < 4; ++channel) {
                const float expected = blend == RenderBackendDecalBlendMode::Multiply ? destination[channel]*source[channel] :
                    blend == RenderBackendDecalBlendMode::Alpha ? source[channel]*source[3]+destination[channel]*(1-source[3]) :
                    source[channel]+destination[channel];
                ok = pixelMatches(pixels[(240*640+320)*4+channel], expected) && ok;
            }
        }
        backend.Flip_To_Primary();
    }
    for (auto &v : quad) { v.r=v.g=v.b=v.a=1; v.u=2.0f; v.v=.5f; }
    // Outside UV range clamps to green. The opposite winding is culled.
    for (unsigned winding = 0; winding < 2; ++winding) {
        backend.Clear(true,true,Vector3(0,0,0),1,1,0); backend.Begin_Scene();
        ok = backend.Draw_Indexed_Decal_Triangles(quad,4,winding ? back : front,6,edge,RenderBackendDecalBlendMode::Alpha) && ok;
        const bool read = capture(); ok = read && ok;
        if (read) ok = pixels[(240*640+320)*4] == 0 && pixels[(240*640+320)*4+1] == (winding ? 0 : 255) && ok;
        backend.Flip_To_Primary();
    }
    RenderBackendColorVertex base[4], probe[4];
    for (unsigned i = 0; i < 4; ++i) {
        base[i] = {quad[i].x,quad[i].y,.6f,1,0,0,1};
        probe[i] = {quad[i].x,quad[i].y,.55f,0,0,1,1};
    }
    for (unsigned depth_case = 0; depth_case < 3; ++depth_case) {
        for (auto &v : quad) v.z = depth_case == 1 ? .7f : depth_case == 2 ? .6f : .5f;
        backend.Clear(true,true,Vector3(0,0,0),1,1,0); backend.Begin_Scene();
        ok = backend.Draw_Indexed_Triangles(base,4,front,6) && ok;
        ok = backend.Draw_Indexed_Decal_Triangles(quad,4,front,6,edge,RenderBackendDecalBlendMode::Alpha) && ok;
        if (depth_case == 0) ok = backend.Draw_Indexed_Triangles(probe,4,front,6) && ok;
        const bool read = capture(); ok = read && ok;
        if (read) {
            const auto center = (240*640+320)*4;
            // Blue proves no depth write; red proves depth rejection; green proves LEQUAL.
            ok = pixels[center+(depth_case == 0 ? 2 : depth_case == 1 ? 0 : 1)] == 255 && ok;
        }
        backend.Flip_To_Primary();
    }
    const auto offscreen = backend.Create_Render_Texture(8,8);
    ok = backend.Set_Render_Texture(offscreen) && ok;
    backend.Clear(true,false,Vector3(0,0,0),1,1,0); backend.Begin_Scene();
    ok = !backend.Draw_Indexed_Decal_Triangles(quad,4,front,6,texture,RenderBackendDecalBlendMode::Alpha) && ok;
    backend.End_Scene(false); ok = backend.Set_Render_Texture({}) && ok;
    backend.Release_Texture(offscreen); backend.Release_Texture(edge); backend.Release_Texture(texture);
    return !backend.Is_Texture_Valid(texture) && ok;
}
} // namespace

int main()
{
    HINSTANCE instance = GetModuleHandleW(nullptr);
    HWND window = createHiddenWindow(instance);
    if (window == nullptr)
    {
        std::cerr << "D3D12 backend smoke failed: could not create Win32 window.\n";
        return 1;
    }

    IRenderBackend *backend = Create_Render_Backend(window, false);
    if (backend == nullptr)
    {
        DestroyWindow(window);
        UnregisterClassW(WindowClassName, instance);
        std::cerr << "D3D12 backend smoke failed: backend initialization failed.\n";
        return 2;
    }

    int output_width = 0, output_height = 0, output_bits = 0;
    bool output_windowed = false;
    if (!backend->Configure_Output(640, 480, true) ||
        !backend->Is_Device_Ready() ||
        !backend->Has_Stencil() ||
        !backend->Get_Output_Description(
            output_width, output_height, output_bits, output_windowed) ||
        output_width != 640 || output_height != 480 || output_bits != 32 || !output_windowed)
    {
        delete backend;
        DestroyWindow(window);
        UnregisterClassW(WindowClassName, instance);
        std::cerr << "D3D12 backend smoke failed: output resize/description contract failed.\n";
        return 10;
    }

    RenderBackendViewport viewport{0, 0, 640, 480, 0.0f, 1.0f};
    if (backend->Get_Swap_Interval() != 1 ||
        !backend->Set_Swap_Interval(0) || backend->Get_Swap_Interval() != 0 ||
        !backend->Set_Swap_Interval(4) || backend->Get_Swap_Interval() != 4 ||
        backend->Set_Swap_Interval(5) || backend->Get_Swap_Interval() != 4 ||
        !backend->Set_Swap_Interval(0))
    {
        delete backend;
        DestroyWindow(window);
        UnregisterClassW(WindowClassName, instance);
        std::cerr << "D3D12 backend smoke failed: presentation interval contract failed.\n";
        return 11;
    }
    backend->Set_Viewport(viewport);

    unsigned int captured_width = 99, captured_height = 99;
    std::vector<unsigned char> captured_pixels{99};
    if (backend->Read_Output_RGBA8(captured_width, captured_height, captured_pixels) ||
        captured_width != 0 || captured_height != 0 || !captured_pixels.empty())
    {
        delete backend;
        DestroyWindow(window);
        UnregisterClassW(WindowClassName, instance);
        std::cerr << "D3D12 backend smoke failed: capture before submission was accepted.\n";
        return 12;
    }

    const RenderBackendColorVertex triangle_vertices[] = {
        {-0.65f, -0.55f, 0.50f, 1.00f, 0.15f, 0.10f, 1.00f},
        {0.00f, 0.65f, 0.50f, 0.10f, 1.00f, 0.20f, 1.00f},
        {0.65f, -0.55f, 0.50f, 0.10f, 0.25f, 1.00f, 1.00f},
    };
    const unsigned short triangle_indices[] = {0, 1, 2};
    const RenderBackendGeometryHandle static_triangle =
        backend->Create_Static_Indexed_Color_Geometry(triangle_vertices, 3, triangle_indices, 3);
    if (!static_triangle.Is_Valid())
    {
        delete backend;
        DestroyWindow(window);
        UnregisterClassW(WindowClassName, instance);
        std::cerr << "D3D12 backend smoke failed: persistent geometry creation failed.\n";
        return 3;
    }

    const RenderBackendTexturedVertex textured_vertices[] = {
        {-0.75f, -0.70f, 0.40f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f},
        {-0.75f,  0.70f, 0.40f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f},
        { 0.75f,  0.70f, 0.40f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f},
        { 0.75f, -0.70f, 0.40f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f},
    };
    const unsigned short textured_indices[] = {0, 1, 2, 0, 2, 3};
    const unsigned char checker_rgba[] = {
        255,  64,  64, 255,    64, 255,  64, 255,
         64,  64, 255, 255,   255, 255,  64, 255,
    };
    const RenderBackendGeometryHandle textured_quad =
        backend->Create_Static_Indexed_Textured_Geometry(textured_vertices, 4, textured_indices, 6);
    const RenderBackendTextureHandle checker_texture =
        backend->Create_Static_RGBA8_Texture(2, 2, checker_rgba, 8);
    if (!textured_quad.Is_Valid() || !checker_texture.Is_Valid())
    {
        backend->Release_Texture(checker_texture);
        backend->Release_Static_Geometry(textured_quad);
        backend->Release_Static_Geometry(static_triangle);
        delete backend;
        DestroyWindow(window);
        UnregisterClassW(WindowClassName, instance);
        std::cerr << "D3D12 backend smoke failed: persistent textured resources could not be created.\n";
        return 4;
    }

    // Exercise WW3D's deferred-present contract first: End_Scene(false) may
    // be followed by another scene before Flip_To_Primary(). This is used by
    // existing multi-pass/off-screen callers and must not dead-end the frame.
    backend->Clear(true, true, Vector3(0.04f, 0.08f, 0.12f), 1.0f, 1.0f, 0);
    backend->Begin_Scene();
    if (!backend->Draw_Static_Indexed_Textured_Geometry(textured_quad, checker_texture))
    {
        backend->Release_Texture(checker_texture);
        backend->Release_Static_Geometry(textured_quad);
        backend->Release_Static_Geometry(static_triangle);
        delete backend;
        DestroyWindow(window);
        UnregisterClassW(WindowClassName, instance);
        std::cerr << "D3D12 backend smoke failed: textured indexed geometry/2D submission failed.\n";
        return 5;
    }
    backend->End_Scene(false);

    backend->Clear(true, true, Vector3(0.06f, 0.08f, 0.12f), 1.0f, 1.0f, 0);
    backend->Begin_Scene();
    backend->End_Scene(false);
    backend->Flip_To_Primary();

    if (!backend->Read_Output_RGBA8(captured_width, captured_height, captured_pixels) ||
        captured_width != 640 || captured_height != 480 ||
        captured_pixels.size() != 640u * 480u * 4u ||
        captured_pixels[0] != 15 || captured_pixels[1] != 20 ||
        captured_pixels[2] != 31 || captured_pixels[3] != 255)
    {
        delete backend;
        DestroyWindow(window);
        UnregisterClassW(WindowClassName, instance);
        std::cerr << "D3D12 backend smoke failed: latest presented output capture is incorrect.\n";
        return 13;
    }

    for (int frame = 0; frame < 6; ++frame)
    {
        backend->Reset_Frame_Statistics();
        backend->Set_Swap_Interval(static_cast<unsigned int>(frame % 2));
        const float phase = static_cast<float>(frame) / 5.0f;
        backend->Clear(true, true, Vector3(0.08f + phase * 0.08f, 0.08f, 0.12f), 1.0f, 1.0f, 0);
        backend->Begin_Scene();
        const bool color_draw_ok = frame == 0
            ? backend->Draw_Indexed_Triangles(triangle_vertices, 3, triangle_indices, 3)
            : backend->Draw_Static_Indexed_Color_Geometry(static_triangle);
        const bool textured_draw_ok = backend->Draw_Static_Indexed_Textured_Geometry(textured_quad, checker_texture);
        const bool screen_draw_ok = backend->Draw_2D_Indexed_Triangles(
            triangle_vertices, 3, triangle_indices, 3, RenderBackend2DBlendMode::Alpha);
        const RenderBackendFrameStatistics counters = backend->Get_Frame_Statistics();
        if (!color_draw_ok || !textured_draw_ok || !screen_draw_ok ||
            counters.draw_calls != 3 || counters.triangles != 4 || counters.vertices != 10)
        {
            backend->Release_Texture(checker_texture);
            backend->Release_Static_Geometry(textured_quad);
            backend->Release_Static_Geometry(static_triangle);
            delete backend;
            DestroyWindow(window);
            UnregisterClassW(WindowClassName, instance);
            std::cerr << "D3D12 backend smoke failed: indexed geometry submission failed.\n";
            return 6;
        }
        backend->End_Scene(true);
    }

    // A non-symmetric transform catches row/column transposition. Change it
    // within a scene and interleave UI to catch stale or shared root constants.
    Matrix4x4 view_projection(true);
    view_projection[0][0] = 0.25f;
    view_projection[1][1] = 0.25f;
    view_projection[0][3] = 0.55f;
    view_projection[1][3] = -0.5f;
    const RenderBackendColorVertex top_left_vertices[] = {
        {-1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f},
        { 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f},
        {-1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f},
    };
    bool camera_ok = true;
    for (unsigned int path = 0; path < 3; ++path)
    {
        view_projection[0][3] = 0.55f;
        backend->Set_View_Projection(view_projection);
        backend->Clear(true, true, Vector3(0.0f, 0.0f, 0.0f), 1.0f, 1.0f, 0);
        backend->Begin_Scene();
        auto draw_world = [&]() {
            if (path == 0) return backend->Draw_Indexed_Triangles(triangle_vertices, 3, triangle_indices, 3);
            if (path == 1) return backend->Draw_Static_Indexed_Color_Geometry(static_triangle);
            return backend->Draw_Static_Indexed_Textured_Geometry(textured_quad, checker_texture);
        };
        camera_ok = draw_world() && camera_ok;
        camera_ok = backend->Draw_2D_Indexed_Triangles(
            top_left_vertices, 3, triangle_indices, 3, RenderBackend2DBlendMode::Opaque) && camera_ok;
        view_projection[0][3] = -0.55f;
        backend->Set_View_Projection(view_projection);
        camera_ok = draw_world() && camera_ok;
        backend->End_Scene(false);
        camera_ok = backend->Read_Output_RGBA8(captured_width, captured_height, captured_pixels) && camera_ok;
        if (camera_ok)
        {
            auto colored_pixel = [&](unsigned int x, unsigned int y) {
                const std::size_t offset = (y * captured_width + x) * 4u;
                return captured_pixels[offset] || captured_pixels[offset + 1] || captured_pixels[offset + 2];
            };
            const std::size_t ui_pixel = (10u * captured_width + 10u) * 4u;
            camera_ok = captured_width == 640 && captured_height == 480 &&
                colored_pixel(496, 365) && colored_pixel(144, 365) && !colored_pixel(320, 240) &&
                captured_pixels[ui_pixel] == 255 && captured_pixels[ui_pixel + 1] == 0 &&
                captured_pixels[ui_pixel + 2] == 0;
        }
        backend->Flip_To_Primary();
    }
    // Projected shadows render into one texture, copy to another, and sample
    // that copy later. Exercise actual GPU pixels and restore the output state.
    const auto render_texture = backend->Create_Render_Texture(61, 37);
    const auto shadow_texture = backend->Create_Render_Texture(61, 37);
    const auto wrong_size = backend->Create_Render_Texture(17, 19);
    bool target_ok = render_texture.Is_Valid() && shadow_texture.Is_Valid() && wrong_size.Is_Valid() &&
        !backend->Create_Render_Texture(0, 37).Is_Valid() &&
        !backend->Set_Render_Texture(checker_texture) && !backend->Set_Render_Texture({1, 0}) &&
        !backend->Copy_Texture(shadow_texture, wrong_size) &&
        !backend->Copy_Texture(shadow_texture, shadow_texture);
    const Matrix4x4 identity(true);
    for (unsigned int path = 0; target_ok && path < 3; ++path)
    {
        backend->Set_Viewport({80, 60, 480, 360, 0.0f, 1.0f});
        backend->Set_View_Projection(view_projection);
        backend->Clear(true, true, Vector3(0.0f, 0.0f, 0.0f), 1.0f, 1.0f, 0);
        backend->Begin_Scene();
        target_ok = backend->Draw_Static_Indexed_Color_Geometry(static_triangle) && target_ok;
        backend->End_Scene(false); // A shadow pass must preserve this deferred output.
        target_ok = backend->Read_Output_RGBA8(captured_width, captured_height, captured_pixels) && target_ok;
        const auto retained_output = captured_pixels;
        const std::size_t restored_pixel = (330u * captured_width + 188u) * 4u;
        target_ok = target_ok && (captured_pixels[restored_pixel] || captured_pixels[restored_pixel + 1]);

        target_ok = backend->Set_Render_Texture(render_texture) && target_ok;
        int width = 0, height = 0, bits = 0;
        bool windowed = false;
        target_ok = backend->Get_Render_Target_Size(width, height) && width == 61 && height == 37 && target_ok;
        target_ok = backend->Get_Output_Description(width, height, bits, windowed) &&
            width == 640 && height == 480 && target_ok;
        backend->Set_View_Projection(identity);
        backend->Clear(true, true, Vector3(0.25f, 0.5f, 0.75f), 1.0f, 1.0f, 0);
        backend->Begin_Scene();
        target_ok = !backend->Set_Render_Texture({}) &&
            !backend->Create_Render_Texture(61, 37).Is_Valid() &&
            !backend->Copy_Texture(shadow_texture, render_texture) &&
            !backend->Draw_Static_Indexed_Textured_Geometry(textured_quad, render_texture) && target_ok;
        if (path == 0) target_ok = backend->Draw_Indexed_Triangles(triangle_vertices, 3, triangle_indices, 3) && target_ok;
        if (path == 1) target_ok = backend->Draw_Static_Indexed_Color_Geometry(static_triangle) && target_ok;
        if (path == 2) target_ok = backend->Draw_Static_Indexed_Textured_Geometry(textured_quad, checker_texture) && target_ok;
        target_ok = backend->Draw_2D_Indexed_Triangles(top_left_vertices, 3, triangle_indices, 3,
            path == 0 ? RenderBackend2DBlendMode::Opaque :
            path == 1 ? RenderBackend2DBlendMode::Alpha : RenderBackend2DBlendMode::Additive) && target_ok;
        backend->End_Scene(true); // Offscreen submission must never present or replace output capture.
        target_ok = backend->Read_Output_RGBA8(captured_width, captured_height, captured_pixels) &&
            captured_pixels == retained_output && target_ok;
        target_ok = backend->Copy_Texture(shadow_texture, render_texture) && target_ok;
        target_ok = backend->Set_Render_Texture({}) && target_ok;
        backend->Flip_To_Primary();

        // Selection restored both the saved camera and pixel viewport.
        backend->Clear(true, true, Vector3(0.0f, 0.0f, 0.0f), 1.0f, 1.0f, 0);
        backend->Begin_Scene();
        target_ok = backend->Draw_Static_Indexed_Color_Geometry(static_triangle) && target_ok;
        backend->End_Scene(false);
        target_ok = backend->Read_Output_RGBA8(captured_width, captured_height, captured_pixels) &&
            captured_pixels == retained_output && target_ok;
        backend->Flip_To_Primary();

        backend->Set_Viewport(viewport);
        backend->Set_View_Projection(identity);
        backend->Clear(true, true, Vector3(0.0f, 0.0f, 0.0f), 1.0f, 1.0f, 0);
        backend->Begin_Scene();
        target_ok = backend->Draw_Static_Indexed_Textured_Geometry(textured_quad, shadow_texture) && target_ok;
        backend->End_Scene(false);
        target_ok = backend->Read_Output_RGBA8(captured_width, captured_height, captured_pixels) && target_ok;
        const std::size_t lower_right = (420u * captured_width + 540u) * 4u;
        const std::size_t top_left = (100u * captured_width + 100u) * 4u;
        const std::size_t middle = (240u * captured_width + 320u) * 4u;
        target_ok = target_ok && captured_pixels[top_left] == 255 &&
            captured_pixels[top_left + 1] == (path == 2 ? 128 : 0) &&
            captured_pixels[top_left + 2] == (path == 2 ? 191 : 0) &&
            captured_pixels[lower_right] == 0 && // Outside the sampled quad.
            (captured_pixels[middle] != 64 || captured_pixels[middle + 1] != 128 || captured_pixels[middle + 2] != 191);
        // Inside the quad, away from every offscreen draw, the copied clear is exact.
        const std::size_t background = (390u * captured_width + 540u) * 4u;
        target_ok = target_ok && captured_pixels[background] == 64 &&
            captured_pixels[background + 1] == 128 && captured_pixels[background + 2] == 191 &&
            captured_pixels[background + 3] == 255;
        backend->Flip_To_Primary();
    }
    target_ok = backend->Set_Render_Texture(render_texture) && target_ok;
    backend->Release_Texture(render_texture); // Release of a selected target restores output.
    const auto replacement_texture = backend->Create_Render_Texture(61, 37);
    output_width = output_height = 0;
    target_ok = backend->Get_Render_Target_Size(output_width, output_height) &&
        output_width == 640 && output_height == 480 && replacement_texture.Is_Valid() &&
        replacement_texture.slot == render_texture.slot && replacement_texture.generation != render_texture.generation &&
        !backend->Set_Render_Texture(render_texture) && !backend->Copy_Texture(shadow_texture, render_texture) && target_ok;
    backend->Release_Texture(replacement_texture);
    backend->Release_Texture(wrong_size);
    backend->Release_Texture(shadow_texture);
    backend->Release_Texture(checker_texture);
    backend->Release_Static_Geometry(textured_quad);
    backend->Release_Static_Geometry(static_triangle);
    if (!target_ok)
    {
        delete backend;
        DestroyWindow(window);
        UnregisterClassW(WindowClassName, instance);
        std::cerr << "D3D12 backend smoke failed: render texture lifecycle, pixels, or output restoration is incorrect.\n";
        return 16;
    }
    if (!camera_ok)
    {
        delete backend;
        DestroyWindow(window);
        UnregisterClassW(WindowClassName, instance);
        std::cerr << "D3D12 backend smoke failed: camera transform or screen-space isolation is incorrect.\n";
        return 15;
    }
    if (!verifyDecals(*backend)) {
        delete backend; DestroyWindow(window); UnregisterClassW(WindowClassName, instance);
        std::cerr << "D3D12 decal blending, clamp sampling, culling, depth, or validation failed.\n";
        return 18;
    }
    // A width not divisible by the GPU row alignment catches padding leaks;
    // an asymmetric draw catches vertical inversion and RGBA channel swaps.
    bool capture_ok = backend->Configure_Output(643, 479, true);
    capture_ok = capture_ok && !backend->Read_Output_RGBA8(captured_width, captured_height, captured_pixels);
    backend->Set_Viewport(RenderBackendViewport{0, 0, 643, 479, 0.0f, 1.0f});
    backend->Clear(true, true, Vector3(0.25f, 0.5f, 0.75f), 1.0f, 1.0f, 0);
    backend->Begin_Scene();
    capture_ok = capture_ok && !backend->Read_Output_RGBA8(captured_width, captured_height, captured_pixels);
    capture_ok = capture_ok && backend->Draw_2D_Indexed_Triangles(
        top_left_vertices, 3, triangle_indices, 3, RenderBackend2DBlendMode::Opaque);
    backend->End_Scene(false);
    capture_ok = capture_ok && backend->Read_Output_RGBA8(captured_width, captured_height, captured_pixels) &&
        captured_width == 643 && captured_height == 479 && captured_pixels.size() == 643u * 479u * 4u;
    if (capture_ok)
    {
        const std::size_t lower_right = (643u * 479u - 1u) * 4u;
        capture_ok = captured_pixels[0] == 255 && captured_pixels[1] == 0 && captured_pixels[2] == 0 &&
            captured_pixels[3] == 255 && captured_pixels[lower_right] == 64 &&
            captured_pixels[lower_right + 1] == 128 && captured_pixels[lower_right + 2] == 191 &&
            captured_pixels[lower_right + 3] == 255;
    }
    backend->Flip_To_Primary();
    delete backend;
    // Texture owners may outlive WW3D shutdown. A replacement device must not
    // reinterpret an old slot/generation as its newly allocated render target.
    backend = Create_Render_Backend(window, false);
    bool device_generation_ok = backend != nullptr;
    if (backend != nullptr)
    {
        const auto new_target = backend->Create_Render_Texture(61, 37);
        device_generation_ok = new_target.Is_Valid() && new_target.slot == checker_texture.slot &&
            new_target.generation != checker_texture.generation && !backend->Set_Render_Texture(checker_texture);
        backend->Release_Texture(checker_texture);
        device_generation_ok = backend->Set_Render_Texture(new_target) && device_generation_ok;
        backend->Release_Texture(new_target);
        delete backend;
    }
    DestroyWindow(window);
    UnregisterClassW(WindowClassName, instance);

    if (!capture_ok)
    {
        std::cerr << "D3D12 backend smoke failed: resized/deferred output capture is incorrect.\n";
        return 14;
    }
    if (!device_generation_ok)
    {
        std::cerr << "D3D12 backend smoke failed: stale texture handle crossed device recreation.\n";
        return 17;
    }
    std::cout << "D3D12 backend smoke passed: camera transforms, render textures/copies, device generations, textured geometry, isolated 2D blending, presentation intervals, and resized/deferred RGBA8 readback.\n";
    return 0;
}
