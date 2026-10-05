#include "browserhost.h"
#include <cstdio>
#include <cstring>
#include <string>

namespace {
class Dispatch final : public IDispatch {
    LONG references = 1;
public:
    int calls = 0;
    LONG referenceCount() const { return references; }
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
    std::string page_url;
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
        page_url = "file:///";
        // File-system paths may contain spaces or non-ASCII user names.
        const int count = WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string utf8(static_cast<std::size_t>(count), '\0');
        WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, &utf8[0], count, nullptr, nullptr);
        utf8.resize(static_cast<std::size_t>(count-1));
        const char hex[] = "0123456789ABCDEF";
        for(unsigned char c : utf8) {
            if(c == '\\') page_url += '/';
            else if(c == ':' || c == '/' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.') page_url += static_cast<char>(c);
            else { page_url += '%'; page_url += hex[c >> 4]; page_url += hex[c & 15]; }
        }
        okay = BrowserHost::CreateBrowser("real", page_url.c_str(), 10, 12, 200, 160, dispatch);
    }
    const ULONGLONG deadline = GetTickCount64() + 30000;
    while(okay && (dispatch->calls == 0 || BrowserHost::GetState("real") == BrowserHost::State::Pending) && GetTickCount64() < deadline && BrowserHost::GetState("real") != BrowserHost::State::Failed) pump();
    okay = okay && dispatch->calls == 1 && BrowserHost::GetState("real") == BrowserHost::State::Ready;
    if(!okay) std::fprintf(stderr, "Real browser/IDispatch bridge failed: state=%d error=%08lx calls=%d\n", static_cast<int>(BrowserHost::GetState("real")), static_cast<unsigned long>(BrowserHost::GetError("real")), dispatch->calls);
    if(okay) {
        // A controller that already exists must return to Pending on navigation;
        // its Ready state belongs to the completed document, not request acceptance.
        okay = BrowserHost::Navigate("real", page_url.c_str()) &&
            BrowserHost::GetState("real") == BrowserHost::State::Pending;
        const ULONGLONG reload_deadline = GetTickCount64() + 15000;
        while(okay && BrowserHost::GetState("real") == BrowserHost::State::Pending && GetTickCount64() < reload_deadline) pump();
        okay = okay && BrowserHost::GetState("real") == BrowserHost::State::Ready &&
            SUCCEEDED(BrowserHost::GetError("real")) && dispatch->calls == 2;
        if(!okay) std::fprintf(stderr, "Existing browser reload failed: state=%d error=%08lx calls=%d\n", static_cast<int>(BrowserHost::GetState("real")), static_cast<unsigned long>(BrowserHost::GetError("real")), dispatch->calls);
    }
    if(okay) {
        // Supersede a failing request before pumping its events. An obsolete
        // completion must not fail the new page or skip its native bridge call.
        const std::string superseded = page_url + ".superseded";
        okay = BrowserHost::Navigate("real", superseded.c_str()) &&
            BrowserHost::Navigate("real", page_url.c_str());
        const ULONGLONG replace_deadline = GetTickCount64() + 15000;
        while(okay && BrowserHost::GetState("real") == BrowserHost::State::Pending && GetTickCount64() < replace_deadline) pump();
        okay = okay && BrowserHost::GetState("real") == BrowserHost::State::Ready &&
            SUCCEEDED(BrowserHost::GetError("real")) && dispatch->calls == 3;
        if(!okay) std::fprintf(stderr, "Superseded navigation changed current state: state=%d error=%08lx calls=%d\n", static_cast<int>(BrowserHost::GetState("real")), static_cast<unsigned long>(BrowserHost::GetError("real")), dispatch->calls);
    }
    if(okay) {
        const std::string missing = page_url + ".missing";
        okay = BrowserHost::Navigate("real", missing.c_str());
        const ULONGLONG failure_deadline = GetTickCount64() + 15000;
        while(okay && BrowserHost::GetState("real") == BrowserHost::State::Pending && GetTickCount64() < failure_deadline) pump();
        okay = okay && BrowserHost::GetState("real") == BrowserHost::State::Failed && FAILED(BrowserHost::GetError("real"));
        if(!okay) std::fprintf(stderr, "Existing browser missing-page failure lost: state=%d error=%08lx\n", static_cast<int>(BrowserHost::GetState("real")), static_cast<unsigned long>(BrowserHost::GetError("real")));
    }
    BrowserHost::DestroyBrowser("real");
    okay = okay && BrowserHost::GetState("real") == BrowserHost::State::Closed;
    if(okay) {
        // An accepted Navigate request is not proof that the page loaded.
        const std::string missing = page_url + ".missing";
        okay = BrowserHost::CreateBrowser("missing", missing.c_str(), 0, 0, 100, 100, dispatch);
        const ULONGLONG failure_deadline = GetTickCount64() + 15000;
        while(okay && BrowserHost::GetState("missing") != BrowserHost::State::Failed && GetTickCount64() < failure_deadline) pump();
        okay = okay && BrowserHost::GetState("missing") == BrowserHost::State::Failed && FAILED(BrowserHost::GetError("missing"));
        if(!okay) std::fprintf(stderr, "Missing page did not report Failed and HRESULT error: state=%d error=%08lx\n", static_cast<int>(BrowserHost::GetState("missing")), static_cast<unsigned long>(BrowserHost::GetError("missing")));
        BrowserHost::DestroyBrowser("missing");
    }
    BrowserHost::Shutdown();
    // Drain outstanding cancelled callbacks on their creating STA.
    const ULONGLONG drain = GetTickCount64() + 1000;
    while(GetTickCount64() < drain) pump();
    if(dispatch->referenceCount() != 1) {
        std::fprintf(stderr, "Native dispatch reference retained after shutdown: %ld\n", dispatch->referenceCount());
        okay = false;
    }
    dispatch->Release();
    if(*filename) DeleteFileW(filename);
    DestroyWindow(window);
    return okay ? 0 : 1;
}
