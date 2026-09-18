#include "D3D12Runtime.h"

#include <d3d12.h>
#include <dxgi1_6.h>
#include <windows.h>

#include <algorithm>
#include <array>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <stdexcept>
#include <utility>

namespace generals::evolution {
namespace {

template <typename T>
void releaseCom(T *&object) noexcept
{
    if (object != nullptr) {
        object->Release();
        object = nullptr;
    }
}

std::string formatHresult(HRESULT result)
{
    std::ostringstream stream;
    stream << "0x" << std::hex << std::uppercase << static_cast<unsigned long>(result);

    char *message = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    const DWORD size = FormatMessageA(
        flags,
        nullptr,
        static_cast<DWORD>(result),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<char *>(&message),
        0,
        nullptr);
    if (size != 0 && message != nullptr) {
        std::string text(message, size);
        while (!text.empty() && (text.back() == '\r' || text.back() == '\n')) {
            text.pop_back();
        }
        stream << " (" << text << ")";
    }
    if (message != nullptr) {
        LocalFree(message);
    }
    return stream.str();
}

[[noreturn]] void throwHresult(const char *operation, HRESULT result)
{
    throw std::runtime_error(std::string(operation) + " failed: " + formatHresult(result));
}

void checkHresult(const char *operation, HRESULT result)
{
    if (FAILED(result)) {
        throwHresult(operation, result);
    }
}

std::wstring featureLevelName(D3D_FEATURE_LEVEL featureLevel)
{
    switch (featureLevel) {
        case D3D_FEATURE_LEVEL_12_1:
            return L"12.1";
        case D3D_FEATURE_LEVEL_12_0:
            return L"12.0";
        case D3D_FEATURE_LEVEL_11_1:
            return L"11.1";
        case D3D_FEATURE_LEVEL_11_0:
            return L"11.0";
        default:
            return L"unknown";
    }
}

} // namespace

D3D12Runtime::~D3D12Runtime()
{
    shutdown();
}

void D3D12Runtime::initialize(
    void *window,
    std::uint32_t width,
    std::uint32_t height,
    const D3D12RuntimeOptions &options)
{
    if (window == nullptr || width == 0 || height == 0) {
        throw std::invalid_argument("D3D12Runtime requires a valid window and non-zero dimensions");
    }
    if (isInitialized()) {
        throw std::logic_error("D3D12Runtime is already initialized");
    }

    m_window = window;
    m_width = width;
    m_height = height;
    m_options = options;

    try {
        enableDebugLayerIfRequested();
        createFactoryAndDevice();
        createCommandObjects();
        createSwapChain();
        createRenderTargets();
        createSynchronizationObjects();
    } catch (...) {
        releaseObjects();
        throw;
    }
}

void D3D12Runtime::enableDebugLayerIfRequested()
{
    if (!m_options.enableDebugLayer) {
        return;
    }

    ID3D12Debug *debug = nullptr;
    const HRESULT result = D3D12GetDebugInterface(IID_PPV_ARGS(&debug));
    if (SUCCEEDED(result) && debug != nullptr) {
        debug->EnableDebugLayer();
        debug->Release();
        std::cout << "D3D12 debug layer enabled.\n";
    } else {
        std::cerr << "D3D12 debug layer requested but unavailable: " << formatHresult(result) << "\n";
    }
}

void D3D12Runtime::createFactoryAndDevice()
{
    UINT factoryFlags = 0;
    if (m_options.enableDebugLayer) {
        factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
    }

    HRESULT result = CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&m_factory));
    if (FAILED(result) && factoryFlags != 0) {
        std::cerr << "DXGI debug factory unavailable; retrying without DXGI debug flag.\n";
        result = CreateDXGIFactory2(0, IID_PPV_ARGS(&m_factory));
    }
    checkHresult("CreateDXGIFactory2", result);

    IDXGIAdapter1 *adapter = nullptr;
    if (m_options.useWarpAdapter) {
        checkHresult("IDXGIFactory4::EnumWarpAdapter", m_factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter)));
    } else {
        selectHardwareAdapter(m_factory, &adapter);
    }

    if (adapter == nullptr) {
        throw std::runtime_error("No D3D12-capable adapter was found");
    }

    DXGI_ADAPTER_DESC1 adapterDesc{};
    adapter->GetDesc1(&adapterDesc);
    m_adapterName = adapterDesc.Description;

    const HRESULT deviceResult = D3D12CreateDevice(
        adapter,
        D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS(&m_device));
    adapter->Release();
    checkHresult("D3D12CreateDevice", deviceResult);

    const std::array<D3D_FEATURE_LEVEL, 4> requestedLevels = {
        D3D_FEATURE_LEVEL_12_1,
        D3D_FEATURE_LEVEL_12_0,
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };
    D3D12_FEATURE_DATA_FEATURE_LEVELS featureLevels{};
    featureLevels.NumFeatureLevels = static_cast<UINT>(requestedLevels.size());
    featureLevels.pFeatureLevelsRequested = requestedLevels.data();
    featureLevels.MaxSupportedFeatureLevel = D3D_FEATURE_LEVEL_11_0;
    if (SUCCEEDED(m_device->CheckFeatureSupport(
            D3D12_FEATURE_FEATURE_LEVELS,
            &featureLevels,
            sizeof(featureLevels)))) {
        m_featureLevel = static_cast<std::uint32_t>(featureLevels.MaxSupportedFeatureLevel);
    } else {
        m_featureLevel = static_cast<std::uint32_t>(D3D_FEATURE_LEVEL_11_0);
    }

    std::wcout << L"Adapter: " << m_adapterName << L"\n";
    std::wcout << L"D3D feature level: "
               << featureLevelName(static_cast<D3D_FEATURE_LEVEL>(m_featureLevel)) << L"\n";
}

void D3D12Runtime::selectHardwareAdapter(IDXGIFactory4 *factory, IDXGIAdapter1 **adapterOut)
{
    if (adapterOut == nullptr) {
        throw std::invalid_argument("adapterOut is null");
    }
    *adapterOut = nullptr;

    IDXGIAdapter1 *bestAdapter = nullptr;
    SIZE_T bestDedicatedVideoMemory = 0;

    for (UINT adapterIndex = 0;; ++adapterIndex) {
        IDXGIAdapter1 *candidate = nullptr;
        const HRESULT enumResult = factory->EnumAdapters1(adapterIndex, &candidate);
        if (enumResult == DXGI_ERROR_NOT_FOUND) {
            break;
        }
        checkHresult("IDXGIFactory4::EnumAdapters1", enumResult);

        DXGI_ADAPTER_DESC1 desc{};
        candidate->GetDesc1(&desc);
        const bool software = (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0;
        const bool supportsD3D12 = !software && SUCCEEDED(D3D12CreateDevice(
            candidate,
            D3D_FEATURE_LEVEL_11_0,
            __uuidof(ID3D12Device),
            nullptr));

        if (supportsD3D12 && (bestAdapter == nullptr || desc.DedicatedVideoMemory > bestDedicatedVideoMemory)) {
            releaseCom(bestAdapter);
            bestAdapter = candidate;
            bestDedicatedVideoMemory = desc.DedicatedVideoMemory;
            candidate = nullptr;
        }

        releaseCom(candidate);
    }

    *adapterOut = bestAdapter;
}

void D3D12Runtime::createCommandObjects()
{
    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.NodeMask = 0;
    checkHresult("ID3D12Device::CreateCommandQueue", m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

    for (auto &allocator : m_commandAllocators) {
        checkHresult(
            "ID3D12Device::CreateCommandAllocator",
            m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)));
    }

    checkHresult(
        "ID3D12Device::CreateCommandList",
        m_device->CreateCommandList(
            0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            m_commandAllocators[0],
            nullptr,
            IID_PPV_ARGS(&m_commandList)));
    checkHresult("ID3D12GraphicsCommandList::Close", m_commandList->Close());
}

void D3D12Runtime::createSwapChain()
{
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
    swapChainDesc.Width = m_width;
    swapChainDesc.Height = m_height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.Stereo = FALSE;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = FrameCount;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    swapChainDesc.Flags = 0;

    IDXGISwapChain1 *swapChain1 = nullptr;
    checkHresult(
        "IDXGIFactory4::CreateSwapChainForHwnd",
        m_factory->CreateSwapChainForHwnd(
            m_commandQueue,
            static_cast<HWND>(m_window),
            &swapChainDesc,
            nullptr,
            nullptr,
            &swapChain1));
    const HRESULT swapChainQueryResult = swapChain1->QueryInterface(IID_PPV_ARGS(&m_swapChain));
    swapChain1->Release();
    checkHresult("IDXGISwapChain1 -> IDXGISwapChain3", swapChainQueryResult);

    checkHresult(
        "IDXGIFactory4::MakeWindowAssociation",
        m_factory->MakeWindowAssociation(static_cast<HWND>(m_window), DXGI_MWA_NO_ALT_ENTER));

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}

void D3D12Runtime::createRenderTargets()
{
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heapDesc.NumDescriptors = FrameCount;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    heapDesc.NodeMask = 0;
    checkHresult("ID3D12Device::CreateDescriptorHeap", m_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_rtvHeap)));

    m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();

    for (std::uint32_t index = 0; index < FrameCount; ++index) {
        checkHresult("IDXGISwapChain3::GetBuffer", m_swapChain->GetBuffer(index, IID_PPV_ARGS(&m_renderTargets[index])));
        m_device->CreateRenderTargetView(m_renderTargets[index], nullptr, handle);
        handle.ptr += m_rtvDescriptorSize;
    }
}

void D3D12Runtime::createSynchronizationObjects()
{
    checkHresult("ID3D12Device::CreateFence", m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));

    HANDLE eventHandle = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (eventHandle == nullptr) {
        throw std::runtime_error("CreateEventW failed with Win32 error " + std::to_string(GetLastError()));
    }
    m_fenceEvent = eventHandle;
}

void D3D12Runtime::render()
{
    if (!isInitialized()) {
        throw std::logic_error("D3D12Runtime::render called before initialize");
    }

    waitForFrame(m_frameIndex);

    checkHresult(
        "ID3D12CommandAllocator::Reset",
        m_commandAllocators[m_frameIndex]->Reset());
    checkHresult(
        "ID3D12GraphicsCommandList::Reset",
        m_commandList->Reset(m_commandAllocators[m_frameIndex], nullptr));

    D3D12_RESOURCE_BARRIER toRenderTarget{};
    toRenderTarget.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    toRenderTarget.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    toRenderTarget.Transition.pResource = m_renderTargets[m_frameIndex];
    toRenderTarget.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    toRenderTarget.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    toRenderTarget.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    m_commandList->ResourceBarrier(1, &toRenderTarget);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr += static_cast<SIZE_T>(m_frameIndex) * m_rtvDescriptorSize;
    m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    constexpr float clearColor[4] = {0.035f, 0.075f, 0.105f, 1.0f};
    m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

    std::swap(toRenderTarget.Transition.StateBefore, toRenderTarget.Transition.StateAfter);
    m_commandList->ResourceBarrier(1, &toRenderTarget);

    checkHresult("ID3D12GraphicsCommandList::Close", m_commandList->Close());
    ID3D12CommandList *commandLists[] = {m_commandList};
    m_commandQueue->ExecuteCommandLists(1, commandLists);

    checkHresult("IDXGISwapChain3::Present", m_swapChain->Present(m_options.vsync ? 1 : 0, 0));
    moveToNextFrame();
}

void D3D12Runtime::moveToNextFrame()
{
    const std::uint32_t submittedFrame = m_frameIndex;
    const std::uint64_t signalValue = m_nextFenceValue++;
    checkHresult("ID3D12CommandQueue::Signal", m_commandQueue->Signal(m_fence, signalValue));
    m_frameFenceValues[submittedFrame] = signalValue;
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}

void D3D12Runtime::waitForFrame(std::uint32_t frameIndex)
{
    const std::uint64_t fenceValue = m_frameFenceValues[frameIndex];
    if (fenceValue == 0 || m_fence->GetCompletedValue() >= fenceValue) {
        return;
    }

    checkHresult(
        "ID3D12Fence::SetEventOnCompletion",
        m_fence->SetEventOnCompletion(fenceValue, static_cast<HANDLE>(m_fenceEvent)));
    const DWORD waitResult = WaitForSingleObject(static_cast<HANDLE>(m_fenceEvent), INFINITE);
    if (waitResult != WAIT_OBJECT_0) {
        throw std::runtime_error("WaitForSingleObject failed while waiting for a frame fence");
    }
}

void D3D12Runtime::waitForGpu()
{
    if (m_commandQueue == nullptr || m_fence == nullptr || m_fenceEvent == nullptr) {
        return;
    }

    const std::uint64_t signalValue = m_nextFenceValue++;
    checkHresult("ID3D12CommandQueue::Signal", m_commandQueue->Signal(m_fence, signalValue));
    if (m_fence->GetCompletedValue() < signalValue) {
        checkHresult(
            "ID3D12Fence::SetEventOnCompletion",
            m_fence->SetEventOnCompletion(signalValue, static_cast<HANDLE>(m_fenceEvent)));
        const DWORD waitResult = WaitForSingleObject(static_cast<HANDLE>(m_fenceEvent), INFINITE);
        if (waitResult != WAIT_OBJECT_0) {
            throw std::runtime_error("WaitForSingleObject failed while waiting for GPU idle");
        }
    }
}

void D3D12Runtime::releaseRenderTargets()
{
    for (auto &renderTarget : m_renderTargets) {
        releaseCom(renderTarget);
    }
}

void D3D12Runtime::shutdown()
{
    if (isInitialized()) {
        try {
            waitForGpu();
        } catch (const std::exception &error) {
            std::cerr << "D3D12 shutdown synchronization failed: " << error.what() << "\n";
        }
    }
    releaseObjects();
}

void D3D12Runtime::releaseObjects() noexcept
{
    releaseRenderTargets();
    releaseCom(m_fence);
    releaseCom(m_commandList);
    for (auto &allocator : m_commandAllocators) {
        releaseCom(allocator);
    }
    releaseCom(m_rtvHeap);
    releaseCom(m_swapChain);
    releaseCom(m_commandQueue);
    releaseCom(m_device);
    releaseCom(m_factory);

    if (m_fenceEvent != nullptr) {
        CloseHandle(static_cast<HANDLE>(m_fenceEvent));
        m_fenceEvent = nullptr;
    }

    m_window = nullptr;
    m_width = 0;
    m_height = 0;
    m_rtvDescriptorSize = 0;
    m_frameIndex = 0;
    m_nextFenceValue = 1;
    std::fill(std::begin(m_frameFenceValues), std::end(m_frameFenceValues), 0);
}

} // namespace generals::evolution
