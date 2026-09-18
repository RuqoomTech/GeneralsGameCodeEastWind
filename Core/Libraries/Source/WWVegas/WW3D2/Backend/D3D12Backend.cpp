/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2026 TheSuperHackers
**
** This program is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
*/

#include "D3D12Backend.h"
#include "RenderBackend.h"

#include "WWMath/vector3.h"

#include <d3d12.h>
#include <dxgi1_6.h>
#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <new>
#include <stdexcept>

namespace
{
template <typename T>
void releaseCom(T *&object) noexcept
{
    if (object != nullptr)
    {
        object->Release();
        object = nullptr;
    }
}

void checkHresult(const char *operation, HRESULT result)
{
    if (FAILED(result))
    {
        char buffer[192]{};
        std::snprintf(buffer, sizeof(buffer), "%s failed with HRESULT 0x%08lX",
                      operation, static_cast<unsigned long>(result));
        throw std::runtime_error(buffer);
    }
}

void reportStartupFailure(const char *message)
{
    OutputDebugStringA("D3D12 backend initialization failed: ");
    OutputDebugStringA(message);
    OutputDebugStringA("\n");
}

void selectHardwareAdapter(IDXGIFactory4 *factory, IDXGIAdapter1 **adapter_out)
{
    *adapter_out = nullptr;

    IDXGIFactory6 *factory6 = nullptr;
    if (SUCCEEDED(factory->QueryInterface(IID_PPV_ARGS(&factory6))))
    {
        for (UINT index = 0;; ++index)
        {
            IDXGIAdapter1 *adapter = nullptr;
            if (factory6->EnumAdapterByGpuPreference(
                    index,
                    DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                    IID_PPV_ARGS(&adapter)) == DXGI_ERROR_NOT_FOUND)
            {
                break;
            }

            DXGI_ADAPTER_DESC1 desc{};
            adapter->GetDesc1(&desc);
            if ((desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 &&
                SUCCEEDED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)))
            {
                *adapter_out = adapter;
                factory6->Release();
                return;
            }
            adapter->Release();
        }
        factory6->Release();
    }

    for (UINT index = 0;; ++index)
    {
        IDXGIAdapter1 *adapter = nullptr;
        if (factory->EnumAdapters1(index, &adapter) == DXGI_ERROR_NOT_FOUND)
        {
            break;
        }

        DXGI_ADAPTER_DESC1 desc{};
        adapter->GetDesc1(&desc);
        if ((desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 &&
            SUCCEEDED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)))
        {
            *adapter_out = adapter;
            return;
        }
        adapter->Release();
    }
}

D3D12_CPU_DESCRIPTOR_HANDLE offsetHandle(
    D3D12_CPU_DESCRIPTOR_HANDLE base,
    std::uint32_t index,
    std::uint32_t descriptor_size)
{
    base.ptr += static_cast<SIZE_T>(index) * descriptor_size;
    return base;
}
} // namespace

IRenderBackend *Create_Render_Backend(void *window, bool lite)
{
    return D3D12Backend::Create(window, lite);
}

D3D12Backend *D3D12Backend::Create(void *window, bool lite)
{
    if (lite || window == nullptr)
    {
        return nullptr;
    }

    D3D12Backend *backend = new (std::nothrow) D3D12Backend();
    if (backend == nullptr)
    {
        return nullptr;
    }

    try
    {
        backend->initialize(window);
        return backend;
    }
    catch (const std::exception &error)
    {
        reportStartupFailure(error.what());
        delete backend;
        return nullptr;
    }
}

D3D12Backend::~D3D12Backend()
{
    try
    {
        waitForGpu();
    }
    catch (...)
    {
    }
    releaseObjects();
}

void D3D12Backend::initialize(void *window)
{
    m_window = window;

    RECT client_rect{};
    if (!GetClientRect(static_cast<HWND>(window), &client_rect))
    {
        throw std::runtime_error("GetClientRect failed");
    }
    m_width = static_cast<std::uint32_t>(std::max<LONG>(client_rect.right - client_rect.left, 1));
    m_height = static_cast<std::uint32_t>(std::max<LONG>(client_rect.bottom - client_rect.top, 1));
    m_viewport = {0, 0, m_width, m_height, 0.0f, 1.0f};

    createFactoryAndDevice();
    createCommandObjects();
    createSwapChain();
    createRenderTargets();
    createDepthStencil();
    createSynchronizationObjects();
}

void D3D12Backend::createFactoryAndDevice()
{
    checkHresult("CreateDXGIFactory2", CreateDXGIFactory2(0, IID_PPV_ARGS(&m_factory)));

    IDXGIAdapter1 *adapter = nullptr;
    selectHardwareAdapter(m_factory, &adapter);
    if (adapter == nullptr)
    {
        throw std::runtime_error("no D3D12-capable hardware adapter was found");
    }

    const HRESULT result = D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device));
    adapter->Release();
    checkHresult("D3D12CreateDevice", result);
}

void D3D12Backend::createCommandObjects()
{
    D3D12_COMMAND_QUEUE_DESC queue_desc{};
    queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queue_desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    checkHresult("ID3D12Device::CreateCommandQueue", m_device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&m_command_queue)));

    for (auto &allocator : m_command_allocators)
    {
        checkHresult("ID3D12Device::CreateCommandAllocator",
                     m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)));
    }

    checkHresult("ID3D12Device::CreateCommandList",
                 m_device->CreateCommandList(
                     0,
                     D3D12_COMMAND_LIST_TYPE_DIRECT,
                     m_command_allocators[0],
                     nullptr,
                     IID_PPV_ARGS(&m_command_list)));
    checkHresult("ID3D12GraphicsCommandList::Close", m_command_list->Close());
}

void D3D12Backend::createSwapChain()
{
    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width = m_width;
    desc.Height = m_height;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = FrameCount;
    desc.Scaling = DXGI_SCALING_STRETCH;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

    IDXGISwapChain1 *swap_chain = nullptr;
    checkHresult("IDXGIFactory4::CreateSwapChainForHwnd",
                 m_factory->CreateSwapChainForHwnd(
                     m_command_queue,
                     static_cast<HWND>(m_window),
                     &desc,
                     nullptr,
                     nullptr,
                     &swap_chain));
    checkHresult("IDXGISwapChain1::QueryInterface", swap_chain->QueryInterface(IID_PPV_ARGS(&m_swap_chain)));
    swap_chain->Release();

    m_factory->MakeWindowAssociation(static_cast<HWND>(m_window), DXGI_MWA_NO_ALT_ENTER);
    m_frame_index = m_swap_chain->GetCurrentBackBufferIndex();
}

void D3D12Backend::createRenderTargets()
{
    D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};
    heap_desc.NumDescriptors = FrameCount;
    heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    checkHresult("ID3D12Device::CreateDescriptorHeap(RTV)",
                 m_device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&m_rtv_heap)));

    m_rtv_descriptor_size = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    const D3D12_CPU_DESCRIPTOR_HANDLE start = m_rtv_heap->GetCPUDescriptorHandleForHeapStart();
    for (std::uint32_t index = 0; index < FrameCount; ++index)
    {
        checkHresult("IDXGISwapChain3::GetBuffer", m_swap_chain->GetBuffer(index, IID_PPV_ARGS(&m_render_targets[index])));
        m_device->CreateRenderTargetView(m_render_targets[index], nullptr, offsetHandle(start, index, m_rtv_descriptor_size));
    }
}

void D3D12Backend::createDepthStencil()
{
    D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};
    heap_desc.NumDescriptors = 1;
    heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    checkHresult("ID3D12Device::CreateDescriptorHeap(DSV)",
                 m_device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&m_dsv_heap)));

    D3D12_HEAP_PROPERTIES heap_properties{};
    heap_properties.Type = D3D12_HEAP_TYPE_DEFAULT;
    heap_properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heap_properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heap_properties.CreationNodeMask = 1;
    heap_properties.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC resource_desc{};
    resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resource_desc.Width = m_width;
    resource_desc.Height = m_height;
    resource_desc.DepthOrArraySize = 1;
    resource_desc.MipLevels = 1;
    resource_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    resource_desc.SampleDesc.Count = 1;
    resource_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resource_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE clear_value{};
    clear_value.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    clear_value.DepthStencil.Depth = 1.0f;
    clear_value.DepthStencil.Stencil = 0;

    checkHresult("ID3D12Device::CreateCommittedResource(depth)",
                 m_device->CreateCommittedResource(
                     &heap_properties,
                     D3D12_HEAP_FLAG_NONE,
                     &resource_desc,
                     D3D12_RESOURCE_STATE_DEPTH_WRITE,
                     &clear_value,
                     IID_PPV_ARGS(&m_depth_stencil)));

    D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc{};
    dsv_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    m_device->CreateDepthStencilView(m_depth_stencil, &dsv_desc, m_dsv_heap->GetCPUDescriptorHandleForHeapStart());
}

void D3D12Backend::createSynchronizationObjects()
{
    checkHresult("ID3D12Device::CreateFence", m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
    m_fence_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (m_fence_event == nullptr)
    {
        throw std::runtime_error("CreateEventW failed for D3D12 fence synchronization");
    }
}

void D3D12Backend::Set_Gamma(float gamma, float bright, float contrast, bool calibrate, bool uselimit)
{
    (void)gamma;
    (void)bright;
    (void)contrast;
    (void)calibrate;
    (void)uselimit;
}

void D3D12Backend::Begin_Scene()
{
    if (m_scene_open)
    {
        return;
    }

    waitForFrame(m_frame_index);
    checkHresult("ID3D12CommandAllocator::Reset", m_command_allocators[m_frame_index]->Reset());
    checkHresult("ID3D12GraphicsCommandList::Reset",
                 m_command_list->Reset(m_command_allocators[m_frame_index], nullptr));

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = m_render_targets[m_frame_index];
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    m_command_list->ResourceBarrier(1, &barrier);

    const D3D12_CPU_DESCRIPTOR_HANDLE rtv = offsetHandle(
        m_rtv_heap->GetCPUDescriptorHandleForHeapStart(),
        m_frame_index,
        m_rtv_descriptor_size);
    const D3D12_CPU_DESCRIPTOR_HANDLE dsv = m_dsv_heap->GetCPUDescriptorHandleForHeapStart();
    m_command_list->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

    D3D12_VIEWPORT viewport{};
    viewport.TopLeftX = static_cast<float>(m_viewport.x);
    viewport.TopLeftY = static_cast<float>(m_viewport.y);
    viewport.Width = static_cast<float>(m_viewport.width);
    viewport.Height = static_cast<float>(m_viewport.height);
    viewport.MinDepth = m_viewport.min_z;
    viewport.MaxDepth = m_viewport.max_z;
    m_command_list->RSSetViewports(1, &viewport);

    D3D12_RECT scissor{};
    scissor.left = static_cast<LONG>(m_viewport.x);
    scissor.top = static_cast<LONG>(m_viewport.y);
    scissor.right = static_cast<LONG>(m_viewport.x + m_viewport.width);
    scissor.bottom = static_cast<LONG>(m_viewport.y + m_viewport.height);
    m_command_list->RSSetScissorRects(1, &scissor);

    m_scene_open = true;
    applyPendingClear();
}

void D3D12Backend::End_Scene(bool flip_frame)
{
    if (!m_scene_open)
    {
        return;
    }
    submitScene(flip_frame);
}

void D3D12Backend::Flip_To_Primary()
{
    presentPendingFrame();
}

void D3D12Backend::Clear(
    bool clear_color,
    bool clear_z_stencil,
    const Vector3 &color,
    float dest_alpha,
    float z,
    unsigned int stencil)
{
    if (clear_color)
    {
        m_clear_color[0] = color.X;
        m_clear_color[1] = color.Y;
        m_clear_color[2] = color.Z;
        m_clear_color[3] = dest_alpha;
        m_clear_color_pending = true;
    }
    if (clear_z_stencil)
    {
        m_clear_depth = z;
        m_clear_stencil = stencil;
        m_clear_depth_pending = true;
    }

    if (m_scene_open)
    {
        applyPendingClear();
    }
}

void D3D12Backend::applyPendingClear()
{
    if (!m_scene_open)
    {
        return;
    }

    if (m_clear_color_pending)
    {
        const D3D12_CPU_DESCRIPTOR_HANDLE rtv = offsetHandle(
            m_rtv_heap->GetCPUDescriptorHandleForHeapStart(),
            m_frame_index,
            m_rtv_descriptor_size);
        m_command_list->ClearRenderTargetView(rtv, m_clear_color, 0, nullptr);
        m_clear_color_pending = false;
    }

    if (m_clear_depth_pending)
    {
        const D3D12_CPU_DESCRIPTOR_HANDLE dsv = m_dsv_heap->GetCPUDescriptorHandleForHeapStart();
        m_command_list->ClearDepthStencilView(
            dsv,
            D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
            m_clear_depth,
            static_cast<UINT8>(m_clear_stencil),
            0,
            nullptr);
        m_clear_depth_pending = false;
    }
}

void D3D12Backend::Set_Viewport(const RenderBackendViewport &viewport)
{
    m_viewport = viewport;
    if (m_scene_open)
    {
        D3D12_VIEWPORT native_viewport{};
        native_viewport.TopLeftX = static_cast<float>(viewport.x);
        native_viewport.TopLeftY = static_cast<float>(viewport.y);
        native_viewport.Width = static_cast<float>(viewport.width);
        native_viewport.Height = static_cast<float>(viewport.height);
        native_viewport.MinDepth = viewport.min_z;
        native_viewport.MaxDepth = viewport.max_z;
        m_command_list->RSSetViewports(1, &native_viewport);

        D3D12_RECT scissor{};
        scissor.left = static_cast<LONG>(viewport.x);
        scissor.top = static_cast<LONG>(viewport.y);
        scissor.right = static_cast<LONG>(viewport.x + viewport.width);
        scissor.bottom = static_cast<LONG>(viewport.y + viewport.height);
        m_command_list->RSSetScissorRects(1, &scissor);
    }
}

void D3D12Backend::Invalidate_Cached_Render_States()
{
    // D3D12 state is explicit and command-list local. There is no DX8-style
    // shadow state cache to invalidate here.
}

void D3D12Backend::Set_Ambient(const Vector3 &color)
{
    (void)color;
    // Lighting becomes pipeline data when the fixed-function material path is
    // migrated. Keeping this call backend-local prevents new DX8 dependencies.
}

void D3D12Backend::Set_Light_Environment(LightEnvironmentClass *light_env)
{
    (void)light_env;
    // The D3D12 lighting path is intentionally not emulated as fixed function.
}

void D3D12Backend::submitScene(bool present)
{
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = m_render_targets[m_frame_index];
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    m_command_list->ResourceBarrier(1, &barrier);

    checkHresult("ID3D12GraphicsCommandList::Close", m_command_list->Close());
    ID3D12CommandList *lists[] = {m_command_list};
    m_command_queue->ExecuteCommandLists(1, lists);

    const std::uint32_t submitted_frame = m_frame_index;
    const std::uint64_t signal_value = m_next_fence_value++;
    checkHresult("ID3D12CommandQueue::Signal", m_command_queue->Signal(m_fence, signal_value));
    m_frame_fence_values[submitted_frame] = signal_value;

    m_scene_open = false;
    m_present_pending = true;
    if (present)
    {
        presentPendingFrame();
    }
}

void D3D12Backend::presentPendingFrame()
{
    if (!m_present_pending)
    {
        return;
    }

    checkHresult("IDXGISwapChain3::Present", m_swap_chain->Present(1, 0));
    m_present_pending = false;
    m_frame_index = m_swap_chain->GetCurrentBackBufferIndex();
}

void D3D12Backend::waitForFrame(std::uint32_t frame_index)
{
    const std::uint64_t fence_value = m_frame_fence_values[frame_index];
    if (fence_value == 0 || m_fence->GetCompletedValue() >= fence_value)
    {
        return;
    }

    checkHresult("ID3D12Fence::SetEventOnCompletion",
                 m_fence->SetEventOnCompletion(fence_value, static_cast<HANDLE>(m_fence_event)));
    if (WaitForSingleObject(static_cast<HANDLE>(m_fence_event), INFINITE) != WAIT_OBJECT_0)
    {
        throw std::runtime_error("WaitForSingleObject failed while waiting for a D3D12 frame");
    }
}

void D3D12Backend::waitForGpu()
{
    if (m_command_queue == nullptr || m_fence == nullptr || m_fence_event == nullptr)
    {
        return;
    }

    if (m_scene_open)
    {
        submitScene(false);
    }

    const std::uint64_t signal_value = m_next_fence_value++;
    checkHresult("ID3D12CommandQueue::Signal", m_command_queue->Signal(m_fence, signal_value));
    if (m_fence->GetCompletedValue() < signal_value)
    {
        checkHresult("ID3D12Fence::SetEventOnCompletion",
                     m_fence->SetEventOnCompletion(signal_value, static_cast<HANDLE>(m_fence_event)));
        if (WaitForSingleObject(static_cast<HANDLE>(m_fence_event), INFINITE) != WAIT_OBJECT_0)
        {
            throw std::runtime_error("WaitForSingleObject failed while waiting for the D3D12 GPU");
        }
    }
}

void D3D12Backend::releaseObjects() noexcept
{
    for (auto &render_target : m_render_targets)
    {
        releaseCom(render_target);
    }
    releaseCom(m_depth_stencil);
    releaseCom(m_fence);
    releaseCom(m_command_list);
    for (auto &allocator : m_command_allocators)
    {
        releaseCom(allocator);
    }
    releaseCom(m_dsv_heap);
    releaseCom(m_rtv_heap);
    releaseCom(m_swap_chain);
    releaseCom(m_command_queue);
    releaseCom(m_device);
    releaseCom(m_factory);

    if (m_fence_event != nullptr)
    {
        CloseHandle(static_cast<HANDLE>(m_fence_event));
        m_fence_event = nullptr;
    }
}
