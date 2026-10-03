#include "browserhost.h"
#include <WebView2.h>
#include <cstring>
#include <cstdio>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>

namespace {
template<class T> class ComPtr {
    T *value = nullptr;
public:
    ~ComPtr() { reset(); }
    ComPtr() = default;
    ComPtr(const ComPtr &) = delete;
    ComPtr &operator=(const ComPtr &) = delete;
    T *get() const { return value; }
    T *operator->() const { return value; }
    T **put() { reset(); return &value; }
    void reset(T *next = nullptr) { if(value) value->Release(); value = next; }
    void retain(T *next) { if(next) next->AddRef(); reset(next); }
};

// WebView2 invokes these completion handlers on the creating STA thread.
template<class Interface, class Argument, const IID *InterfaceId> class Completion final : public Interface {
    LONG references = 1;
    std::function<HRESULT(HRESULT, Argument)> action;
public:
    explicit Completion(std::function<HRESULT(HRESULT, Argument)> fn) : action(std::move(fn)) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **out) override {
        if(!out) return E_POINTER;
        *out = nullptr;
        if(IsEqualIID(iid, IID_IUnknown) || IsEqualIID(iid, *InterfaceId)) {
            *out = static_cast<Interface *>(this); AddRef(); return S_OK;
        }
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return static_cast<ULONG>(InterlockedIncrement(&references)); }
    ULONG STDMETHODCALLTYPE Release() override { const ULONG n = static_cast<ULONG>(InterlockedDecrement(&references)); if(!n) delete this; return n; }
    HRESULT STDMETHODCALLTYPE Invoke(HRESULT result, Argument value) override { return action(result, value); }
};

using EnvironmentDone = Completion<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler, ICoreWebView2Environment *, &IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>;
using ControllerDone = Completion<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler, ICoreWebView2Controller *, &IID_ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>;
using ScriptDone = Completion<ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandler, LPCWSTR, &IID_ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandler>;

struct Browser {
    HWND window = nullptr;
    bool cancelled = false;
    BrowserHost::State state = BrowserHost::State::Pending;
    HRESULT error = S_OK;
    std::wstring url;
    ComPtr<IDispatch> dispatch;
    ComPtr<ICoreWebView2Controller> controller;
    ComPtr<ICoreWebView2> webview;
    void close() {
        cancelled = true;
        if(controller.get()) controller->Close();
        webview.reset(); controller.reset(); dispatch.reset();
        if(window) DestroyWindow(window);
        window = nullptr;
    }
    ~Browser() { close(); }
};

struct Host {
    HWND parent = nullptr;
    DWORD thread = 0;
    bool alive = true;
    bool com_initialized = false;
    HRESULT error = S_OK;
    HMODULE loader = nullptr;
    ComPtr<ICoreWebView2Environment> environment;
    std::map<std::string, std::shared_ptr<Browser>> browsers;
    ~Host() {
        environment.reset();
        // Async handlers retain this owner until their callback is released.
        if(loader) FreeLibrary(loader);
        if(com_initialized) CoUninitialize();
    }
};
std::shared_ptr<Host> host;
HRESULT last_error = S_OK;

bool on_thread() { return host && host->thread == GetCurrentThreadId(); }
std::wstring wide(const char *text) {
    if(!text || !*text) return {};
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, nullptr, 0);
    if(!count) return {};
    std::wstring value(static_cast<std::size_t>(count), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, &value[0], count);
    value.resize(static_cast<std::size_t>(count - 1));
    return value;
}
void fail(const std::shared_ptr<Browser> &browser, HRESULT error, const char *stage = "controller setup") {
    browser->error = FAILED(error) ? error : E_FAIL;
    browser->state = BrowserHost::State::Failed;
    std::fprintf(stderr, "Native browser %s failed (HRESULT %08lx)\n", stage, static_cast<unsigned long>(browser->error));
    if(browser->controller.get()) browser->controller->Close();
    browser->webview.reset(); browser->controller.reset();
    if(browser->window) ShowWindow(browser->window, SW_HIDE);
}

void create_controller(const std::shared_ptr<Host> &owner, const std::shared_ptr<Browser> &browser) {
    if(!owner->alive || browser->cancelled) return;
    auto *done = new ControllerDone([owner, browser](HRESULT result, ICoreWebView2Controller *controller) {
        if(!owner->alive || browser->cancelled) { if(controller) controller->Close(); return S_OK; }
        if(FAILED(result) || !controller) { fail(browser, result, "controller completion"); return S_OK; }
        browser->controller.retain(controller);
        HRESULT hr = controller->get_CoreWebView2(browser->webview.put());
        RECT bounds{};
        if(SUCCEEDED(hr) && !GetClientRect(browser->window, &bounds)) hr = HRESULT_FROM_WIN32(GetLastError());
        if(SUCCEEDED(hr)) hr = controller->put_Bounds(bounds);
        if(SUCCEEDED(hr)) {
            VARIANT object; VariantInit(&object);
            object.vt = VT_DISPATCH; object.pdispVal = browser->dispatch.get();
            hr = browser->webview->AddHostObjectToScript(L"gameinterface", &object);
        }
        if(FAILED(hr)) { fail(browser, hr); return S_OK; }
        // BrowserEngine exposed game commands as window.external.gameinterface.
        // Keep that page contract while the original FEBDispatch remains native.
        const wchar_t *bridge = LR"JS((()=>{const g=chrome.webview.hostObjects.sync.gameinterface;
            Object.defineProperty(window,'external',{value:{gameinterface:g},configurable:true});
            })();)JS";
        auto *script = new ScriptDone([owner, browser](HRESULT installed, LPCWSTR) {
            if(!owner->alive || browser->cancelled) return S_OK;
            if(FAILED(installed)) { fail(browser, installed, "bridge script"); return S_OK; }
            const HRESULT navigated = browser->webview->Navigate(browser->url.c_str());
            if(FAILED(navigated)) { fail(browser, navigated, "navigation request"); return S_OK; }
            browser->state = BrowserHost::State::Ready;
            ShowWindow(browser->window, SW_SHOW);
            return S_OK;
        });
        hr = browser->webview->AddScriptToExecuteOnDocumentCreated(bridge, script);
        script->Release();
        if(FAILED(hr)) fail(browser, hr);
        return S_OK;
    });
    const HRESULT hr = owner->environment->CreateCoreWebView2Controller(browser->window, done);
    done->Release();
    if(FAILED(hr)) fail(browser, hr);
}
}

bool BrowserHost::Initialize(HWND parent) {
    if(host) return on_thread() && host->parent == parent && SUCCEEDED(host->error);
    if(!IsWindow(parent) || GetWindowThreadProcessId(parent, nullptr) != GetCurrentThreadId()) { last_error = E_INVALIDARG; return false; }
    auto owner = std::make_shared<Host>();
    owner->parent = parent; owner->thread = GetCurrentThreadId();
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if(FAILED(hr)) { last_error = hr; return false; }
    owner->com_initialized = true;
    wchar_t executable[32768]{};
    const DWORD length = GetModuleFileNameW(nullptr, executable, 32768);
    std::wstring loader_path(executable, length);
    const auto separator = loader_path.find_last_of(L"\\/");
    if(length == 0 || length == 32768 || separator == std::wstring::npos) { last_error = E_FAIL; return false; }
    loader_path.resize(separator + 1); loader_path += L"WebView2Loader.dll";
    owner->loader = LoadLibraryExW(loader_path.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!owner->loader) { last_error = HRESULT_FROM_WIN32(GetLastError()); return false; }
    using CreateEnvironment = HRESULT (STDAPICALLTYPE *)(PCWSTR, PCWSTR, ICoreWebView2EnvironmentOptions *, ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *);
    const FARPROC exported = GetProcAddress(owner->loader, "CreateCoreWebView2EnvironmentWithOptions");
    CreateEnvironment create = nullptr;
    static_assert(sizeof(create) == sizeof(exported), "Windows native function pointer width");
    std::memcpy(&create, &exported, sizeof(create));
    if(!create) { last_error = HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND); return false; }
    wchar_t appdata[32768]{};
    const DWORD appdata_size = GetEnvironmentVariableW(L"LOCALAPPDATA", appdata, 32768);
    if(!appdata_size || appdata_size >= 32768) { last_error = E_FAIL; return false; }
    const std::wstring data_folder = std::wstring(appdata) + L"\\GeneralsEvolution\\WebView2";
    host = owner;
    auto *done = new EnvironmentDone([owner](HRESULT result, ICoreWebView2Environment *environment) {
        if(!owner->alive) return S_OK;
        if(FAILED(result) || !environment) {
            owner->error = FAILED(result) ? result : E_FAIL;
            for(auto &entry : owner->browsers) fail(entry.second, owner->error, "environment completion");
            return S_OK;
        }
        owner->environment.retain(environment);
        for(auto &entry : owner->browsers) create_controller(owner, entry.second);
        return S_OK;
    });
    hr = create(nullptr, data_folder.c_str(), nullptr, done);
    done->Release();
    if(FAILED(hr)) { owner->alive = false; last_error = hr; host.reset(); return false; }
    last_error = S_OK;
    return true;
}

void BrowserHost::Shutdown() {
    if(!on_thread()) return;
    auto owner = host;
    owner->alive = false;
    for(auto &entry : owner->browsers) entry.second->close();
    owner->browsers.clear();
    host.reset();
}
bool BrowserHost::CreateBrowser(const char *name, const char *url, int x, int y, int width, int height, IDispatch *dispatch) {
    if(!on_thread() || FAILED(host->error) || !name || !*name || !dispatch || width <= 0 || height <= 0) return false;
    const std::wstring location = wide(url);
    if(location.empty()) return false;
    DestroyBrowser(name);
    auto browser = std::make_shared<Browser>();
    browser->url = location;
    browser->dispatch.retain(dispatch);
    browser->window = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
        x, y, width, height, host->parent, nullptr, GetModuleHandleW(nullptr), nullptr);
    if(!browser->window) { last_error = HRESULT_FROM_WIN32(GetLastError()); return false; }
    host->browsers.emplace(name, browser);
    if(host->environment.get()) create_controller(host, browser);
    return browser->state != State::Failed;
}
void BrowserHost::DestroyBrowser(const char *name) {
    if(!on_thread() || !name) return;
    const auto found = host->browsers.find(name);
    if(found == host->browsers.end()) return;
    found->second->close(); host->browsers.erase(found);
}
bool BrowserHost::Navigate(const char *name, const char *url) {
    if(!on_thread() || !name) return false;
    const auto found = host->browsers.find(name);
    const std::wstring location = wide(url);
    if(found == host->browsers.end() || location.empty() || found->second->state == State::Failed) return false;
    found->second->url = location;
    if(found->second->state == State::Pending) return true;
    const HRESULT hr = found->second->webview->Navigate(location.c_str());
    if(FAILED(hr)) fail(found->second, hr);
    return SUCCEEDED(hr);
}
BrowserHost::State BrowserHost::GetState(const char *name) {
    if(!on_thread() || !name) return State::Closed;
    const auto found = host->browsers.find(name);
    return found == host->browsers.end() ? State::Closed : found->second->state;
}
HRESULT BrowserHost::GetError(const char *name) {
    if(!on_thread() || !name) return last_error;
    const auto found = host->browsers.find(name);
    return found == host->browsers.end() ? host->error : found->second->error;
}
