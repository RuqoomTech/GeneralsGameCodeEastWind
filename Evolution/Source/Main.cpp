#include "D3D12Runtime.h"

#include <windows.h>

#include <charconv>
#include <cstdint>
#include <exception>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace {

constexpr wchar_t WindowClassName[] = L"GeneralsEvolutionD3D12Shell";
constexpr wchar_t WindowTitle[] = L"Generals Evolution - Direct3D 12 Runtime Shell";
constexpr std::uint32_t ClientWidth = 1280;
constexpr std::uint32_t ClientHeight = 720;

struct CommandLineOptions
{
    generals::evolution::D3D12RuntimeOptions runtime;
    std::optional<std::uint64_t> frameLimit;
};

LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                DestroyWindow(window);
                return 0;
            }
            break;
        case WM_CLOSE:
            DestroyWindow(window);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_ERASEBKGND:
            return 1;
        default:
            break;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

std::optional<std::uint64_t> parseUnsigned(std::string_view text)
{
    std::uint64_t value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size()) {
        return std::nullopt;
    }
    return value;
}

CommandLineOptions parseCommandLine(int argc, char **argv)
{
    CommandLineOptions options;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument == "--debug") {
            options.runtime.enableDebugLayer = true;
        } else if (argument == "--warp") {
            options.runtime.useWarpAdapter = true;
        } else if (argument == "--no-vsync") {
            options.runtime.vsync = false;
        } else if (argument == "--frames") {
            if (index + 1 >= argc) {
                throw std::runtime_error("--frames requires a non-negative integer");
            }
            const auto parsed = parseUnsigned(argv[++index]);
            if (!parsed.has_value()) {
                throw std::runtime_error("invalid --frames value");
            }
            options.frameLimit = *parsed;
        } else if (argument == "--help" || argument == "-h") {
            std::cout
                << "Generals Evolution Direct3D 12 runtime shell\n\n"
                << "Options:\n"
                << "  --debug       Request the D3D12 debug layer when installed.\n"
                << "  --warp        Use the Microsoft WARP software adapter.\n"
                << "  --no-vsync    Present without vertical synchronization.\n"
                << "  --frames N    Render N frames and exit (automation/smoke test).\n"
                << "  --help        Show this help.\n";
            ExitProcess(0);
        } else {
            throw std::runtime_error("unknown command-line option: " + std::string(argument));
        }
    }
    return options;
}

HWND createWindow(HINSTANCE instance)
{
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = windowProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.lpszClassName = WindowClassName;

    if (RegisterClassExW(&windowClass) == 0) {
        throw std::runtime_error("RegisterClassExW failed with Win32 error " + std::to_string(GetLastError()));
    }

    RECT windowRect{0, 0, static_cast<LONG>(ClientWidth), static_cast<LONG>(ClientHeight)};
    const DWORD windowStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    if (!AdjustWindowRect(&windowRect, windowStyle, FALSE)) {
        throw std::runtime_error("AdjustWindowRect failed with Win32 error " + std::to_string(GetLastError()));
    }

    HWND window = CreateWindowExW(
        0,
        WindowClassName,
        WindowTitle,
        windowStyle,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        nullptr,
        nullptr,
        instance,
        nullptr);
    if (window == nullptr) {
        throw std::runtime_error("CreateWindowExW failed with Win32 error " + std::to_string(GetLastError()));
    }

    ShowWindow(window, SW_SHOWDEFAULT);
    UpdateWindow(window);
    return window;
}

int run(int argc, char **argv)
{
    const CommandLineOptions options = parseCommandLine(argc, argv);
    HINSTANCE instance = GetModuleHandleW(nullptr);
    HWND window = createWindow(instance);

    generals::evolution::D3D12Runtime runtime;
    runtime.initialize(window, ClientWidth, ClientHeight, options.runtime);

    std::wstring title = WindowTitle;
    title += L" | ";
    title += runtime.adapterName();
    SetWindowTextW(window, title.c_str());

    std::cout << "Runtime shell initialized. Press ESC or close the window to exit.\n";
    if (options.frameLimit.has_value()) {
        std::cout << "Smoke mode: rendering " << *options.frameLimit << " frames.\n";
    }

    MSG message{};
    bool running = !options.frameLimit.has_value() || *options.frameLimit != 0;
    std::uint64_t renderedFrames = 0;
    while (running) {
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
                running = false;
                break;
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        if (!running) {
            break;
        }

        runtime.render();
        ++renderedFrames;
        if (options.frameLimit.has_value() && renderedFrames >= *options.frameLimit) {
            running = false;
        }
    }

    runtime.waitForGpu();
    runtime.shutdown();
    if (IsWindow(window)) {
        DestroyWindow(window);
    }
    UnregisterClassW(WindowClassName, instance);

    std::cout << "D3D12 runtime shell completed successfully after " << renderedFrames << " frame(s).\n";
    return 0;
}

} // namespace

int main(int argc, char **argv)
{
    try {
        return run(argc, argv);
    } catch (const std::exception &error) {
        std::cerr << "Generals Evolution startup failed: " << error.what() << "\n";
        MessageBoxA(nullptr, error.what(), "Generals Evolution startup failed", MB_OK | MB_ICONERROR);
        return 1;
    }
}
