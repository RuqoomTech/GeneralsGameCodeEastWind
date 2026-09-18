#pragma once

#include <cstdint>
#include <string>

struct IDXGIAdapter1;
struct IDXGIFactory4;
struct IDXGISwapChain3;
struct ID3D12CommandAllocator;
struct ID3D12CommandQueue;
struct ID3D12DescriptorHeap;
struct ID3D12Device;
struct ID3D12Fence;
struct ID3D12GraphicsCommandList;
struct ID3D12Resource;

namespace generals::evolution {

struct D3D12RuntimeOptions
{
    bool enableDebugLayer = false;
    bool useWarpAdapter = false;
    bool vsync = true;
};

class D3D12Runtime
{
public:
    static constexpr std::uint32_t FrameCount = 2;

    D3D12Runtime() = default;
    D3D12Runtime(const D3D12Runtime &) = delete;
    D3D12Runtime &operator=(const D3D12Runtime &) = delete;
    ~D3D12Runtime();

    void initialize(void *window, std::uint32_t width, std::uint32_t height, const D3D12RuntimeOptions &options);
    void render();
    void waitForGpu();
    void shutdown();

    [[nodiscard]] bool isInitialized() const noexcept { return m_device != nullptr; }
    [[nodiscard]] const std::wstring &adapterName() const noexcept { return m_adapterName; }
    [[nodiscard]] std::uint32_t featureLevel() const noexcept { return m_featureLevel; }

private:
    void enableDebugLayerIfRequested();
    void createFactoryAndDevice();
    void selectHardwareAdapter(IDXGIFactory4 *factory, IDXGIAdapter1 **adapterOut);
    void createCommandObjects();
    void createSwapChain();
    void createRenderTargets();
    void createSynchronizationObjects();
    void moveToNextFrame();
    void waitForFrame(std::uint32_t frameIndex);
    void releaseRenderTargets();
    void releaseObjects() noexcept;

    void *m_window = nullptr;
    std::uint32_t m_width = 0;
    std::uint32_t m_height = 0;
    D3D12RuntimeOptions m_options{};

    IDXGIFactory4 *m_factory = nullptr;
    ID3D12Device *m_device = nullptr;
    ID3D12CommandQueue *m_commandQueue = nullptr;
    IDXGISwapChain3 *m_swapChain = nullptr;
    ID3D12DescriptorHeap *m_rtvHeap = nullptr;
    ID3D12Resource *m_renderTargets[FrameCount]{};
    ID3D12CommandAllocator *m_commandAllocators[FrameCount]{};
    ID3D12GraphicsCommandList *m_commandList = nullptr;
    ID3D12Fence *m_fence = nullptr;
    void *m_fenceEvent = nullptr;

    std::uint32_t m_rtvDescriptorSize = 0;
    std::uint32_t m_frameIndex = 0;
    std::uint64_t m_nextFenceValue = 1;
    std::uint64_t m_frameFenceValues[FrameCount]{};
    std::wstring m_adapterName;
    std::uint32_t m_featureLevel = 0;
};

} // namespace generals::evolution
