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

    RenderBackendViewport viewport{0, 0, 640, 480, 0.0f, 1.0f};
    backend->Set_Viewport(viewport);

    // Exercise WW3D's deferred-present contract first: End_Scene(false) may
    // be followed by another scene before Flip_To_Primary(). This is used by
    // existing multi-pass/off-screen callers and must not dead-end the frame.
    backend->Clear(true, true, Vector3(0.04f, 0.08f, 0.12f), 1.0f, 1.0f, 0);
    backend->Begin_Scene();
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
        backend->End_Scene(true);
    }

    delete backend;
    DestroyWindow(window);
    UnregisterClassW(WindowClassName, instance);

    std::cout << "D3D12 backend smoke passed: WW3D backend factory created, cleared, deferred-presented, depth-cleared, and presented frames.\n";
    return 0;
}
