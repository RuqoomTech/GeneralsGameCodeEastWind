#pragma once

#include <windows.h>
#include <oaidl.h>

// Native child-window browser ownership. The game window, UI and renderer remain
// owned by their existing systems; browser state is never simulation state.
class BrowserHost
{
public:
    enum class State { Closed, Pending, Ready, Failed };
    static bool Initialize(HWND parent);
    static void Shutdown();
    static bool CreateBrowser(const char *name, const char *url,
        int x, int y, int width, int height, IDispatch *game_dispatch);
    static void DestroyBrowser(const char *name);
    static bool Navigate(const char *name, const char *url);
    static State GetState(const char *name);
    static HRESULT GetError(const char *name);
};
