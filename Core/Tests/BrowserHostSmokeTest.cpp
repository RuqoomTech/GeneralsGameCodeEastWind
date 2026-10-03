#include "browserhost.h"
#include <cstdio>
#include <cstring>
#include <string>

namespace {
class Dispatch final : public IDispatch {
    LONG references = 1;
public:
    int calls = 0;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **out) override {
        if(!out) return E_POINTER;
        *out = nullptr;
        if(IsEqualIID(iid, IID_IUnknown) || IsEqualIID(iid, IID_IDispatch)) { *out = static_cast<IDispatch *>(this); AddRef(); return S_OK; }
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return static_cast<ULONG>(InterlockedIncrement(&references)); }
    ULONG STDMETHODCALLTYPE Release() override { const ULONG n = static_cast<ULONG>(InterlockedDecrement(&references)); if(!n) delete this; return n; }
    HRESULT STDMETHODCALLTYPE GetTypeInfoCount(UINT *count) override { if(!count) return E_POINTER; *count = 0; return S_OK; }
    HRESULT STDMETHODCALLTYPE GetTypeInfo(UINT, LCID, ITypeInfo **info) override { if(info) *info = nullptr; return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID, LPOLESTR *names, UINT count, LCID, DISPID *ids) override {
        for(UINT i = 0; i < count; ++i) { if(std::wcscmp(names[i], L"Ping") != 0) return DISP_E_UNKNOWNNAME; ids[i] = 1; }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Invoke(DISPID id, REFIID, LCID, WORD flags, DISPPARAMS *params, VARIANT *result, EXCEPINFO *, UINT *) override {
        if(id != 1 || !(flags & DISPATCH_METHOD)) return DISP_E_MEMBERNOTFOUND;
        if(!params || params->cArgs != 1 || params->rgvarg[0].vt != VT_I4 || params->rgvarg[0].lVal != 1095) return DISP_E_TYPEMISMATCH;
        ++calls;
        if(result) VariantInit(result);
        return S_OK;
    }
};
void pump() {
    MSG message{};
    while(PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&message); DispatchMessageW(&message); }
    MsgWaitForMultipleObjects(0, nullptr, FALSE, 20, QS_ALLINPUT);
}
}

int main() {
    static_assert(sizeof(HWND) == 8, "Native browser test requires x64");
    HWND window = CreateWindowExW(0, L"STATIC", L"Generals native browser test", WS_OVERLAPPEDWINDOW,
        0, 0, 300, 240, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if(!window || !BrowserHost::Initialize(window)) {
        std::fprintf(stderr, "Browser initialization failed %08lx\n", static_cast<unsigned long>(BrowserHost::GetError(nullptr)));
        if(window) DestroyWindow(window);
        return 1;
    }
    auto *dispatch = new Dispatch();
    bool okay = BrowserHost::CreateBrowser("cancel", "about:blank", 0, 0, 100, 100, dispatch);
    BrowserHost::DestroyBrowser("cancel");
    okay = okay && BrowserHost::GetState("cancel") == BrowserHost::State::Closed;
    // Exercise shutdown while an environment/controller completion is pending.
    BrowserHost::Shutdown();
    okay = okay && BrowserHost::Initialize(window);
    wchar_t folder[MAX_PATH]{}, filename[MAX_PATH]{};
    okay = okay && GetTempPathW(MAX_PATH, folder) && GetTempFileNameW(folder, L"geb", 0, filename);
    if(okay) {
        // The browser uses the extension when selecting a local file's MIME type.
        std::wstring html_name = std::wstring(filename) + L".html";
        okay = html_name.size() < MAX_PATH && DeleteFileW(filename);
        if(okay) std::wcscpy(filename, html_name.c_str());
    }
    if(okay) {
        HANDLE file = CreateFileW(filename, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        const char html[] = "<!doctype html><script>try{window.external.gameinterface.Ping(1095)}catch(e){document.title=String(e)}</script>";
        DWORD written = 0;
        okay = file != INVALID_HANDLE_VALUE && WriteFile(file, html, sizeof(html)-1, &written, nullptr) && written == sizeof(html)-1;
        if(file != INVALID_HANDLE_VALUE) CloseHandle(file);
    }
    if(okay) {
        std::wstring path(filename);
        std::string url = "file:///";
        // File-system paths may contain spaces or non-ASCII user names.
        const int count = WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string utf8(static_cast<std::size_t>(count), '\0');
        WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, &utf8[0], count, nullptr, nullptr);
        utf8.resize(static_cast<std::size_t>(count-1));
        const char hex[] = "0123456789ABCDEF";
        for(unsigned char c : utf8) {
            if(c == '\\') url += '/';
            else if(c == ':' || c == '/' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.') url += static_cast<char>(c);
            else { url += '%'; url += hex[c >> 4]; url += hex[c & 15]; }
        }
        okay = BrowserHost::CreateBrowser("real", url.c_str(), 10, 12, 200, 160, dispatch);
    }
    const ULONGLONG deadline = GetTickCount64() + 30000;
    while(okay && dispatch->calls == 0 && GetTickCount64() < deadline && BrowserHost::GetState("real") != BrowserHost::State::Failed) pump();
    okay = okay && dispatch->calls == 1 && BrowserHost::GetState("real") == BrowserHost::State::Ready;
    if(!okay) std::fprintf(stderr, "Real browser/IDispatch bridge failed: state=%d error=%08lx calls=%d\n", static_cast<int>(BrowserHost::GetState("real")), static_cast<unsigned long>(BrowserHost::GetError("real")), dispatch->calls);
    BrowserHost::DestroyBrowser("real");
    okay = okay && BrowserHost::GetState("real") == BrowserHost::State::Closed;
    BrowserHost::Shutdown();
    // Drain outstanding cancelled callbacks on their creating STA.
    const ULONGLONG drain = GetTickCount64() + 1000;
    while(GetTickCount64() < drain) pump();
    dispatch->Release();
    if(*filename) DeleteFileW(filename);
    DestroyWindow(window);
    return okay ? 0 : 1;
}
