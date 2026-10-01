#include "Utility/CppMacros.h"

#include "WW3D2/Backend/RenderBackend.h"
#include "WW3D2/IRenderBackend.h"
#include "WWMath/vector3.h"
#include "WWMath/matrix4.h"

#include <windows.h>

#include <iostream>

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
        backend->Release_Static_Texture(checker_texture);
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
        backend->Release_Static_Texture(checker_texture);
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
            backend->Release_Static_Texture(checker_texture);
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
    backend->Release_Static_Texture(checker_texture);
    backend->Release_Static_Geometry(textured_quad);
    backend->Release_Static_Geometry(static_triangle);
    if (!camera_ok)
    {
        delete backend;
        DestroyWindow(window);
        UnregisterClassW(WindowClassName, instance);
        std::cerr << "D3D12 backend smoke failed: camera transform or screen-space isolation is incorrect.\n";
        return 15;
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
    DestroyWindow(window);
    UnregisterClassW(WindowClassName, instance);

    if (!capture_ok)
    {
        std::cerr << "D3D12 backend smoke failed: resized/deferred output capture is incorrect.\n";
        return 14;
    }
    std::cout << "D3D12 backend smoke passed: camera transforms, textured geometry, isolated 2D blending, presentation intervals, and resized/deferred RGBA8 readback.\n";
    return 0;
}
