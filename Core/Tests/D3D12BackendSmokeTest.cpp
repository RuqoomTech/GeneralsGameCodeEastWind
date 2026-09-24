#include "Utility/CppMacros.h"

#include "WW3D2/Backend/RenderBackend.h"
#include "WW3D2/IRenderBackend.h"
#include "WWMath/vector3.h"

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
    backend->Set_Viewport(viewport);

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

    for (int frame = 0; frame < 6; ++frame)
    {
        const float phase = static_cast<float>(frame) / 5.0f;
        backend->Clear(true, true, Vector3(0.08f + phase * 0.08f, 0.08f, 0.12f), 1.0f, 1.0f, 0);
        backend->Begin_Scene();
        const bool color_draw_ok = frame == 0
            ? backend->Draw_Indexed_Triangles(triangle_vertices, 3, triangle_indices, 3)
            : backend->Draw_Static_Indexed_Color_Geometry(static_triangle);
        const bool textured_draw_ok = backend->Draw_Static_Indexed_Textured_Geometry(textured_quad, checker_texture);
        const bool screen_draw_ok = backend->Draw_2D_Indexed_Triangles(
            triangle_vertices, 3, triangle_indices, 3, RenderBackend2DBlendMode::Alpha);
        if (!color_draw_ok || !textured_draw_ok || !screen_draw_ok)
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

    backend->Release_Static_Texture(checker_texture);
    backend->Release_Static_Geometry(textured_quad);
    backend->Release_Static_Geometry(static_triangle);
    delete backend;
    DestroyWindow(window);
    UnregisterClassW(WindowClassName, instance);

    std::cout << "D3D12 backend smoke passed: WW3D uploaded RGBA8 texture data, bound shader-visible SRV/static-sampler state, exercised the real 2D blend PSO path, reused textured/default-heap geometry across frames, and released resources safely.\n";
    return 0;
}
