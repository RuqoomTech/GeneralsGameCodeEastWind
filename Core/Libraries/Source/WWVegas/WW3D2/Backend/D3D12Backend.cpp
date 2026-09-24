/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2026 TheSuperHackers
**
** This program is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
*/

#include "Utility/CppMacros.h"

#include "D3D12Backend.h"
#include "RenderBackend.h"

#include "WWMath/vector3.h"

#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>

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


std::wstring getExecutableDirectory()
{
    std::wstring path(512, L'\0');
    for (;;)
    {
        const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (length == 0)
        {
            throw std::runtime_error("GetModuleFileNameW failed while resolving D3D12 shader assets");
        }
        if (length < path.size())
        {
            path.resize(length);
            break;
        }
        path.resize(path.size() * 2);
    }

    const std::size_t slash = path.find_last_of(L"\\/");
    if (slash == std::wstring::npos)
    {
        return L".";
    }
    path.resize(slash);
    return path;
}

std::wstring getPrimitiveShaderPath()
{
    return getExecutableDirectory() + L"\\Shaders\\PrimitiveColor.hlsl";
}

ID3DBlob *compileShaderFromFile(const wchar_t *path, const char *entry_point, const char *target)
{
    ID3DBlob *shader = nullptr;
    ID3DBlob *errors = nullptr;
    const HRESULT result = D3DCompileFromFile(
        path,
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entry_point,
        target,
        D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3,
        0,
        &shader,
        &errors);

    if (FAILED(result))
    {
        std::string message = "D3DCompileFromFile failed for the WW3D D3D12 primitive shader";
        if (errors != nullptr && errors->GetBufferPointer() != nullptr)
        {
            message.assign(
                static_cast<const char *>(errors->GetBufferPointer()),
                errors->GetBufferSize());
        }
        releaseCom(errors);
        releaseCom(shader);
        throw std::runtime_error(message);
    }

    releaseCom(errors);
    return shader;
}

ID3D12Resource *createDefaultBuffer(ID3D12Device *device, std::size_t byte_count)
{
    D3D12_HEAP_PROPERTIES heap_properties{};
    heap_properties.Type = D3D12_HEAP_TYPE_DEFAULT;
    heap_properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heap_properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heap_properties.CreationNodeMask = 1;
    heap_properties.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC resource_desc{};
    resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resource_desc.Width = static_cast<UINT64>(byte_count);
    resource_desc.Height = 1;
    resource_desc.DepthOrArraySize = 1;
    resource_desc.MipLevels = 1;
    resource_desc.Format = DXGI_FORMAT_UNKNOWN;
    resource_desc.SampleDesc.Count = 1;
    resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    ID3D12Resource *resource = nullptr;
    checkHresult(
        "ID3D12Device::CreateCommittedResource(default buffer)",
        device->CreateCommittedResource(
            &heap_properties,
            D3D12_HEAP_FLAG_NONE,
            &resource_desc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&resource)));
    return resource;
}

ID3D12Resource *createUploadBuffer(ID3D12Device *device, std::size_t byte_count)
{
    D3D12_HEAP_PROPERTIES heap_properties{};
    heap_properties.Type = D3D12_HEAP_TYPE_UPLOAD;
    heap_properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heap_properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heap_properties.CreationNodeMask = 1;
    heap_properties.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC resource_desc{};
    resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resource_desc.Width = static_cast<UINT64>(byte_count);
    resource_desc.Height = 1;
    resource_desc.DepthOrArraySize = 1;
    resource_desc.MipLevels = 1;
    resource_desc.Format = DXGI_FORMAT_UNKNOWN;
    resource_desc.SampleDesc.Count = 1;
    resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    ID3D12Resource *resource = nullptr;
    checkHresult(
        "ID3D12Device::CreateCommittedResource(upload)",
        device->CreateCommittedResource(
            &heap_properties,
            D3D12_HEAP_FLAG_NONE,
            &resource_desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&resource)));
    return resource;
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
    m_windowed = (GetWindowLongPtr(static_cast<HWND>(window), GWL_STYLE) & WS_CAPTION) != 0;

    RECT client_rect{};
    if (!GetClientRect(static_cast<HWND>(window), &client_rect))
    {
        throw std::runtime_error("GetClientRect failed");
    }
    m_width = static_cast<std::uint32_t>(std::max<LONG>(client_rect.right - client_rect.left, 1));
    m_height = static_cast<std::uint32_t>(std::max<LONG>(client_rect.bottom - client_rect.top, 1));
    m_viewport = {0, 0, m_width, m_height, 0.0f, 1.0f};

    createFactoryAndDevice();
    createPrimitivePipeline();
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

void D3D12Backend::createPrimitivePipeline()
{
    D3D12_DESCRIPTOR_RANGE texture_range{};
    texture_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    texture_range.NumDescriptors = 1;
    texture_range.BaseShaderRegister = 0;
    texture_range.RegisterSpace = 0;
    texture_range.OffsetInDescriptorsFromTableStart = 0;

    D3D12_ROOT_PARAMETER texture_parameter{};
    texture_parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    texture_parameter.DescriptorTable.NumDescriptorRanges = 1;
    texture_parameter.DescriptorTable.pDescriptorRanges = &texture_range;
    texture_parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_STATIC_SAMPLER_DESC sampler{};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.MipLODBias = 0.0f;
    sampler.MaxAnisotropy = 1;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
    sampler.MinLOD = 0.0f;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0;
    sampler.RegisterSpace = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC root_desc{};
    root_desc.NumParameters = 1;
    root_desc.pParameters = &texture_parameter;
    root_desc.NumStaticSamplers = 1;
    root_desc.pStaticSamplers = &sampler;
    root_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ID3DBlob *serialized_root = nullptr;
    ID3DBlob *root_errors = nullptr;
    const HRESULT serialize_result = D3D12SerializeRootSignature(
        &root_desc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &serialized_root,
        &root_errors);
    if (FAILED(serialize_result))
    {
        std::string message = "D3D12SerializeRootSignature failed";
        if (root_errors != nullptr && root_errors->GetBufferPointer() != nullptr)
        {
            message.assign(
                static_cast<const char *>(root_errors->GetBufferPointer()),
                root_errors->GetBufferSize());
        }
        releaseCom(root_errors);
        releaseCom(serialized_root);
        throw std::runtime_error(message);
    }
    releaseCom(root_errors);

    checkHresult(
        "ID3D12Device::CreateRootSignature",
        m_device->CreateRootSignature(
            0,
            serialized_root->GetBufferPointer(),
            serialized_root->GetBufferSize(),
            IID_PPV_ARGS(&m_primitive_root_signature)));
    releaseCom(serialized_root);

    const std::wstring shader_path = getPrimitiveShaderPath();
    ID3DBlob *color_vertex_shader = compileShaderFromFile(shader_path.c_str(), "VSMain", "vs_5_1");
    ID3DBlob *color_pixel_shader = nullptr;
    ID3DBlob *textured_vertex_shader = nullptr;
    ID3DBlob *textured_pixel_shader = nullptr;
    try
    {
        color_pixel_shader = compileShaderFromFile(shader_path.c_str(), "PSMain", "ps_5_1");
        textured_vertex_shader = compileShaderFromFile(shader_path.c_str(), "VSTextured", "vs_5_1");
        textured_pixel_shader = compileShaderFromFile(shader_path.c_str(), "PSTextured", "ps_5_1");

        D3D12_BLEND_DESC blend{};
        blend.RenderTarget[0].BlendEnable = FALSE;
        blend.RenderTarget[0].LogicOpEnable = FALSE;
        blend.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
        blend.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
        blend.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        blend.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
        blend.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
        blend.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        blend.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
        blend.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

        D3D12_RASTERIZER_DESC rasterizer{};
        rasterizer.FillMode = D3D12_FILL_MODE_SOLID;
        rasterizer.CullMode = D3D12_CULL_MODE_NONE;
        rasterizer.FrontCounterClockwise = FALSE;
        rasterizer.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
        rasterizer.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
        rasterizer.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        rasterizer.DepthClipEnable = TRUE;
        rasterizer.MultisampleEnable = FALSE;
        rasterizer.AntialiasedLineEnable = FALSE;
        rasterizer.ForcedSampleCount = 0;
        rasterizer.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

        D3D12_DEPTH_STENCIL_DESC depth_stencil{};
        depth_stencil.DepthEnable = TRUE;
        depth_stencil.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        depth_stencil.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
        depth_stencil.StencilEnable = FALSE;
        depth_stencil.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
        depth_stencil.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
        depth_stencil.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
        depth_stencil.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
        depth_stencil.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
        depth_stencil.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        depth_stencil.BackFace = depth_stencil.FrontFace;

        D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline{};
        pipeline.pRootSignature = m_primitive_root_signature;
        pipeline.BlendState = blend;
        pipeline.SampleMask = std::numeric_limits<UINT>::max();
        pipeline.RasterizerState = rasterizer;
        pipeline.DepthStencilState = depth_stencil;
        pipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pipeline.NumRenderTargets = 1;
        pipeline.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        pipeline.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
        pipeline.SampleDesc.Count = 1;

        const D3D12_INPUT_ELEMENT_DESC color_elements[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
             D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12,
             D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        };
        pipeline.VS = {color_vertex_shader->GetBufferPointer(), color_vertex_shader->GetBufferSize()};
        pipeline.PS = {color_pixel_shader->GetBufferPointer(), color_pixel_shader->GetBufferSize()};
        pipeline.InputLayout = {color_elements, 2};
        checkHresult(
            "ID3D12Device::CreateGraphicsPipelineState(color)",
            m_device->CreateGraphicsPipelineState(&pipeline, IID_PPV_ARGS(&m_primitive_pipeline)));

        D3D12_GRAPHICS_PIPELINE_STATE_DESC screen_pipeline = pipeline;
        screen_pipeline.DepthStencilState.DepthEnable = FALSE;
        screen_pipeline.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        screen_pipeline.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        checkHresult(
            "ID3D12Device::CreateGraphicsPipelineState(2D opaque)",
            m_device->CreateGraphicsPipelineState(&screen_pipeline, IID_PPV_ARGS(&m_2d_opaque_pipeline)));

        screen_pipeline.BlendState.RenderTarget[0].BlendEnable = TRUE;
        screen_pipeline.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
        screen_pipeline.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        screen_pipeline.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
        screen_pipeline.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
        checkHresult(
            "ID3D12Device::CreateGraphicsPipelineState(2D alpha)",
            m_device->CreateGraphicsPipelineState(&screen_pipeline, IID_PPV_ARGS(&m_2d_alpha_pipeline)));

        screen_pipeline.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
        screen_pipeline.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
        screen_pipeline.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
        screen_pipeline.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
        checkHresult(
            "ID3D12Device::CreateGraphicsPipelineState(2D additive)",
            m_device->CreateGraphicsPipelineState(&screen_pipeline, IID_PPV_ARGS(&m_2d_additive_pipeline)));

        const D3D12_INPUT_ELEMENT_DESC textured_elements[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
             D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12,
             D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 28,
             D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        };
        pipeline.VS = {textured_vertex_shader->GetBufferPointer(), textured_vertex_shader->GetBufferSize()};
        pipeline.PS = {textured_pixel_shader->GetBufferPointer(), textured_pixel_shader->GetBufferSize()};
        pipeline.InputLayout = {textured_elements, 3};
        checkHresult(
            "ID3D12Device::CreateGraphicsPipelineState(textured)",
            m_device->CreateGraphicsPipelineState(&pipeline, IID_PPV_ARGS(&m_textured_pipeline)));
    }
    catch (...)
    {
        releaseCom(textured_pixel_shader);
        releaseCom(textured_vertex_shader);
        releaseCom(color_pixel_shader);
        releaseCom(color_vertex_shader);
        throw;
    }
    releaseCom(textured_pixel_shader);
    releaseCom(textured_vertex_shader);
    releaseCom(color_pixel_shader);
    releaseCom(color_vertex_shader);
}

void D3D12Backend::ensureTextureDescriptorCapacity(std::size_t required_capacity)
{
    if (required_capacity <= m_texture_descriptor_capacity)
    {
        return;
    }
    if (m_scene_open || required_capacity > std::numeric_limits<UINT>::max())
    {
        throw std::runtime_error("D3D12 texture descriptor heap cannot grow during an open scene");
    }

    std::size_t new_capacity = m_texture_descriptor_capacity == 0 ? 64u : m_texture_descriptor_capacity;
    while (new_capacity < required_capacity)
    {
        if (new_capacity > (std::numeric_limits<UINT>::max() / 2u))
        {
            new_capacity = required_capacity;
            break;
        }
        new_capacity *= 2u;
    }

    if (m_texture_srv_heap != nullptr)
    {
        waitForGpu();
    }

    D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};
    heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heap_desc.NumDescriptors = static_cast<UINT>(new_capacity);
    heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    ID3D12DescriptorHeap *new_heap = nullptr;
    checkHresult(
        "ID3D12Device::CreateDescriptorHeap(texture SRV)",
        m_device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&new_heap)));

    if (m_srv_descriptor_size == 0)
    {
        m_srv_descriptor_size = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }

    if (m_texture_srv_heap != nullptr)
    {
        const D3D12_CPU_DESCRIPTOR_HANDLE old_start = m_texture_srv_heap->GetCPUDescriptorHandleForHeapStart();
        const D3D12_CPU_DESCRIPTOR_HANDLE new_start = new_heap->GetCPUDescriptorHandleForHeapStart();
        for (std::size_t index = 0; index < m_static_textures.size(); ++index)
        {
            if (!m_static_textures[index].occupied)
            {
                continue;
            }
            D3D12_CPU_DESCRIPTOR_HANDLE source = old_start;
            source.ptr += index * m_srv_descriptor_size;
            D3D12_CPU_DESCRIPTOR_HANDLE destination = new_start;
            destination.ptr += index * m_srv_descriptor_size;
            m_device->CopyDescriptorsSimple(
                1,
                destination,
                source,
                D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        }
    }

    releaseCom(m_texture_srv_heap);
    m_texture_srv_heap = new_heap;
    m_texture_descriptor_capacity = static_cast<std::uint32_t>(new_capacity);
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

bool D3D12Backend::Configure_Output(unsigned int width, unsigned int height, bool windowed)
{
    if (m_scene_open || m_present_pending || m_swap_chain == nullptr ||
        m_rtv_heap == nullptr || m_dsv_heap == nullptr || m_depth_stencil == nullptr ||
        width == 0 || height == 0 ||
        width > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION ||
        height > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION)
    {
        return false;
    }

    if (width == m_width && height == m_height)
    {
        m_windowed = windowed;
        return true;
    }

    try
    {
        waitForGpu();
        for (std::uint32_t index = 0; index < FrameCount; ++index)
        {
            releaseFrameUploads(index);
        }

        // Reset the closed list so it no longer holds references to the old
        // back buffers before DXGI resizes the flip-discard swapchain.
        checkHresult("ID3D12CommandAllocator::Reset(resize)", m_command_allocators[m_frame_index]->Reset());
        checkHresult("ID3D12GraphicsCommandList::Reset(resize)",
            m_command_list->Reset(m_command_allocators[m_frame_index], nullptr));
        checkHresult("ID3D12GraphicsCommandList::Close(resize)", m_command_list->Close());

        for (auto &render_target : m_render_targets)
        {
            releaseCom(render_target);
        }
        releaseCom(m_depth_stencil);
        releaseCom(m_rtv_heap);
        releaseCom(m_dsv_heap);

        checkHresult("IDXGISwapChain3::ResizeBuffers",
            m_swap_chain->ResizeBuffers(FrameCount, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0));
        m_width = width;
        m_height = height;
        m_frame_index = m_swap_chain->GetCurrentBackBufferIndex();
        m_viewport = {0, 0, width, height, 0.0f, 1.0f};
        createRenderTargets();
        createDepthStencil();
        m_windowed = windowed;
        return true;
    }
    catch (const std::exception &error)
    {
        OutputDebugStringA("D3D12 output resize failed: ");
        OutputDebugStringA(error.what());
        OutputDebugStringA("\n");
        return false;
    }
    catch (...)
    {
        OutputDebugStringA("D3D12 output resize failed with an unknown exception.\n");
        return false;
    }
}

bool D3D12Backend::Get_Output_Description(
    int &width, int &height, int &bits, bool &windowed) const
{
    if (m_swap_chain == nullptr || m_rtv_heap == nullptr ||
        m_dsv_heap == nullptr || m_depth_stencil == nullptr)
    {
        return false;
    }
    width = static_cast<int>(m_width);
    height = static_cast<int>(m_height);
    bits = 32;
    windowed = m_windowed;
    return true;
}

bool D3D12Backend::Draw_Indexed_Triangles(
    const RenderBackendColorVertex *vertices,
    unsigned int vertex_count,
    const unsigned short *indices,
    unsigned int index_count)
{
    return drawDynamicColorGeometry(vertices, vertex_count, indices, index_count, m_primitive_pipeline);
}

bool D3D12Backend::Draw_2D_Indexed_Triangles(
    const RenderBackendColorVertex *vertices,
    unsigned int vertex_count,
    const unsigned short *indices,
    unsigned int index_count,
    RenderBackend2DBlendMode blend_mode)
{
    ID3D12PipelineState *pipeline = m_2d_opaque_pipeline;
    switch (blend_mode)
    {
        case RenderBackend2DBlendMode::Alpha:
            pipeline = m_2d_alpha_pipeline;
            break;
        case RenderBackend2DBlendMode::Additive:
            pipeline = m_2d_additive_pipeline;
            break;
        case RenderBackend2DBlendMode::Opaque:
        default:
            break;
    }

    return drawDynamicColorGeometry(vertices, vertex_count, indices, index_count, pipeline);
}

bool D3D12Backend::drawDynamicColorGeometry(
    const RenderBackendColorVertex *vertices,
    unsigned int vertex_count,
    const unsigned short *indices,
    unsigned int index_count,
    ID3D12PipelineState *pipeline)
{
    if (!m_scene_open || pipeline == nullptr || vertices == nullptr || indices == nullptr ||
        vertex_count == 0 || index_count < 3 || (index_count % 3) != 0)
    {
        return false;
    }

    if (vertex_count > (std::numeric_limits<std::size_t>::max() / sizeof(RenderBackendColorVertex)) ||
        index_count > (std::numeric_limits<std::size_t>::max() / sizeof(unsigned short)))
    {
        return false;
    }

    const std::size_t vertex_bytes = static_cast<std::size_t>(vertex_count) * sizeof(RenderBackendColorVertex);
    const std::size_t index_bytes = static_cast<std::size_t>(index_count) * sizeof(unsigned short);
    if (vertex_bytes > std::numeric_limits<UINT>::max() || index_bytes > std::numeric_limits<UINT>::max())
    {
        return false;
    }

    ID3D12Resource *vertex_upload = nullptr;
    ID3D12Resource *index_upload = nullptr;
    try
    {
        auto &frame_uploads = m_frame_uploads[m_frame_index];
        frame_uploads.reserve(frame_uploads.size() + 2);

        vertex_upload = createUploadBuffer(m_device, vertex_bytes);
        index_upload = createUploadBuffer(m_device, index_bytes);

        void *mapped = nullptr;
        D3D12_RANGE no_read{0, 0};
        checkHresult("ID3D12Resource::Map(dynamic color vertex)", vertex_upload->Map(0, &no_read, &mapped));
        std::memcpy(mapped, vertices, vertex_bytes);
        vertex_upload->Unmap(0, nullptr);

        mapped = nullptr;
        checkHresult("ID3D12Resource::Map(dynamic color index)", index_upload->Map(0, &no_read, &mapped));
        std::memcpy(mapped, indices, index_bytes);
        index_upload->Unmap(0, nullptr);

        D3D12_VERTEX_BUFFER_VIEW vertex_view{};
        vertex_view.BufferLocation = vertex_upload->GetGPUVirtualAddress();
        vertex_view.SizeInBytes = static_cast<UINT>(vertex_bytes);
        vertex_view.StrideInBytes = sizeof(RenderBackendColorVertex);

        D3D12_INDEX_BUFFER_VIEW index_view{};
        index_view.BufferLocation = index_upload->GetGPUVirtualAddress();
        index_view.SizeInBytes = static_cast<UINT>(index_bytes);
        index_view.Format = DXGI_FORMAT_R16_UINT;

        frame_uploads.push_back(vertex_upload);
        frame_uploads.push_back(index_upload);
        vertex_upload = nullptr;
        index_upload = nullptr;

        m_command_list->SetGraphicsRootSignature(m_primitive_root_signature);
        m_command_list->SetPipelineState(pipeline);
        m_command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_command_list->IASetVertexBuffers(0, 1, &vertex_view);
        m_command_list->IASetIndexBuffer(&index_view);
        m_command_list->DrawIndexedInstanced(index_count, 1, 0, 0, 0);
        return true;
    }
    catch (...)
    {
        releaseCom(index_upload);
        releaseCom(vertex_upload);
        return false;
    }
}

RenderBackendGeometryHandle D3D12Backend::Create_Static_Indexed_Color_Geometry(
    const RenderBackendColorVertex *vertices,
    unsigned int vertex_count,
    const unsigned short *indices,
    unsigned int index_count)
{
    return createStaticGeometry(
        vertices,
        vertex_count,
        sizeof(RenderBackendColorVertex),
        indices,
        index_count,
        false);
}

RenderBackendGeometryHandle D3D12Backend::Create_Static_Indexed_Textured_Geometry(
    const RenderBackendTexturedVertex *vertices,
    unsigned int vertex_count,
    const unsigned short *indices,
    unsigned int index_count)
{
    return createStaticGeometry(
        vertices,
        vertex_count,
        sizeof(RenderBackendTexturedVertex),
        indices,
        index_count,
        true);
}

RenderBackendGeometryHandle D3D12Backend::createStaticGeometry(
    const void *vertices,
    unsigned int vertex_count,
    unsigned int vertex_stride,
    const unsigned short *indices,
    unsigned int index_count,
    bool textured)
{
    if (m_scene_open || vertices == nullptr || indices == nullptr || vertex_stride == 0 ||
        vertex_count == 0 || index_count < 3 || (index_count % 3) != 0)
    {
        return RenderBackendGeometryHandle();
    }

    if (vertex_count > (std::numeric_limits<std::size_t>::max() / vertex_stride) ||
        index_count > (std::numeric_limits<std::size_t>::max() / sizeof(unsigned short)))
    {
        return RenderBackendGeometryHandle();
    }

    const std::size_t vertex_bytes = static_cast<std::size_t>(vertex_count) * vertex_stride;
    const std::size_t index_bytes = static_cast<std::size_t>(index_count) * sizeof(unsigned short);
    if (vertex_bytes > std::numeric_limits<UINT>::max() || index_bytes > std::numeric_limits<UINT>::max())
    {
        return RenderBackendGeometryHandle();
    }

    ID3D12Resource *vertex_buffer = nullptr;
    ID3D12Resource *index_buffer = nullptr;
    ID3D12Resource *vertex_upload = nullptr;
    ID3D12Resource *index_upload = nullptr;
    ID3D12CommandAllocator *upload_allocator = nullptr;
    ID3D12GraphicsCommandList *upload_list = nullptr;

    try
    {
        vertex_buffer = createDefaultBuffer(m_device, vertex_bytes);
        index_buffer = createDefaultBuffer(m_device, index_bytes);
        vertex_upload = createUploadBuffer(m_device, vertex_bytes);
        index_upload = createUploadBuffer(m_device, index_bytes);

        D3D12_RANGE no_read{0, 0};
        void *mapped = nullptr;
        checkHresult("ID3D12Resource::Map(static vertex upload)", vertex_upload->Map(0, &no_read, &mapped));
        std::memcpy(mapped, vertices, vertex_bytes);
        vertex_upload->Unmap(0, nullptr);

        mapped = nullptr;
        checkHresult("ID3D12Resource::Map(static index upload)", index_upload->Map(0, &no_read, &mapped));
        std::memcpy(mapped, indices, index_bytes);
        index_upload->Unmap(0, nullptr);

        checkHresult(
            "ID3D12Device::CreateCommandAllocator(static geometry)",
            m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&upload_allocator)));
        checkHresult(
            "ID3D12Device::CreateCommandList(static geometry)",
            m_device->CreateCommandList(
                0,
                D3D12_COMMAND_LIST_TYPE_DIRECT,
                upload_allocator,
                nullptr,
                IID_PPV_ARGS(&upload_list)));

        upload_list->CopyBufferRegion(vertex_buffer, 0, vertex_upload, 0, vertex_bytes);
        upload_list->CopyBufferRegion(index_buffer, 0, index_upload, 0, index_bytes);

        D3D12_RESOURCE_BARRIER barriers[2]{};
        barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[0].Transition.pResource = vertex_buffer;
        barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[1].Transition.pResource = index_buffer;
        barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_INDEX_BUFFER;
        barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        upload_list->ResourceBarrier(2, barriers);

        checkHresult("ID3D12GraphicsCommandList::Close(static geometry)", upload_list->Close());
        ID3D12CommandList *lists[] = {upload_list};
        m_command_queue->ExecuteCommandLists(1, lists);

        const std::uint64_t signal_value = m_next_fence_value++;
        checkHresult("ID3D12CommandQueue::Signal(static geometry)", m_command_queue->Signal(m_fence, signal_value));
        if (m_fence->GetCompletedValue() < signal_value)
        {
            checkHresult(
                "ID3D12Fence::SetEventOnCompletion(static geometry)",
                m_fence->SetEventOnCompletion(signal_value, static_cast<HANDLE>(m_fence_event)));
            if (WaitForSingleObject(static_cast<HANDLE>(m_fence_event), INFINITE) != WAIT_OBJECT_0)
            {
                throw std::runtime_error("WaitForSingleObject failed while uploading static D3D12 geometry");
            }
        }

        releaseCom(upload_list);
        releaseCom(upload_allocator);
        releaseCom(index_upload);
        releaseCom(vertex_upload);

        std::size_t slot = 0;
        while (slot < m_static_geometry.size() && m_static_geometry[slot].occupied)
        {
            ++slot;
        }
        if (slot >= std::numeric_limits<unsigned int>::max())
        {
            throw std::runtime_error("D3D12 static geometry handle space exhausted");
        }
        if (slot == m_static_geometry.size())
        {
            m_static_geometry.push_back(StaticGeometryResource{});
        }

        StaticGeometryResource &geometry = m_static_geometry[slot];
        ++geometry.generation;
        if (geometry.generation == 0)
        {
            ++geometry.generation;
        }
        geometry.vertex_buffer = vertex_buffer;
        geometry.index_buffer = index_buffer;
        geometry.vertex_bytes = static_cast<unsigned int>(vertex_bytes);
        geometry.index_bytes = static_cast<unsigned int>(index_bytes);
        geometry.vertex_stride = vertex_stride;
        geometry.index_count = index_count;
        geometry.textured = textured;
        geometry.occupied = true;
        vertex_buffer = nullptr;
        index_buffer = nullptr;
        return RenderBackendGeometryHandle(static_cast<unsigned int>(slot + 1), geometry.generation);
    }
    catch (...)
    {
        releaseCom(upload_list);
        releaseCom(upload_allocator);
        releaseCom(index_upload);
        releaseCom(vertex_upload);
        releaseCom(index_buffer);
        releaseCom(vertex_buffer);
        return RenderBackendGeometryHandle();
    }
}

bool D3D12Backend::drawStaticGeometry(RenderBackendGeometryHandle geometry_handle, bool textured)
{
    if (!m_scene_open || !geometry_handle.Is_Valid())
    {
        return false;
    }

    const std::size_t slot = static_cast<std::size_t>(geometry_handle.slot - 1);
    if (slot >= m_static_geometry.size())
    {
        return false;
    }

    const StaticGeometryResource &geometry = m_static_geometry[slot];
    if (!geometry.occupied || geometry.generation != geometry_handle.generation ||
        geometry.vertex_buffer == nullptr || geometry.index_buffer == nullptr ||
        geometry.textured != textured)
    {
        return false;
    }

    D3D12_VERTEX_BUFFER_VIEW vertex_view{};
    vertex_view.BufferLocation = geometry.vertex_buffer->GetGPUVirtualAddress();
    vertex_view.SizeInBytes = geometry.vertex_bytes;
    vertex_view.StrideInBytes = geometry.vertex_stride;

    D3D12_INDEX_BUFFER_VIEW index_view{};
    index_view.BufferLocation = geometry.index_buffer->GetGPUVirtualAddress();
    index_view.SizeInBytes = geometry.index_bytes;
    index_view.Format = DXGI_FORMAT_R16_UINT;

    m_command_list->SetGraphicsRootSignature(m_primitive_root_signature);
    m_command_list->SetPipelineState(m_primitive_pipeline);
    m_command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_command_list->IASetVertexBuffers(0, 1, &vertex_view);
    m_command_list->IASetIndexBuffer(&index_view);
    m_command_list->DrawIndexedInstanced(geometry.index_count, 1, 0, 0, 0);
    return true;
}

bool D3D12Backend::Draw_Static_Indexed_Color_Geometry(RenderBackendGeometryHandle geometry_handle)
{
    return drawStaticGeometry(geometry_handle, false);
}

void D3D12Backend::Release_Static_Geometry(RenderBackendGeometryHandle geometry_handle)
{
    if (!geometry_handle.Is_Valid() || m_scene_open)
    {
        return;
    }

    const std::size_t slot = static_cast<std::size_t>(geometry_handle.slot - 1);
    if (slot >= m_static_geometry.size() || !m_static_geometry[slot].occupied ||
        m_static_geometry[slot].generation != geometry_handle.generation)
    {
        return;
    }

    try
    {
        waitForGpu();
    }
    catch (...)
    {
        return;
    }
    releaseStaticGeometry(m_static_geometry[slot]);
}

RenderBackendTextureHandle D3D12Backend::Create_Static_RGBA8_Texture(
    unsigned int width,
    unsigned int height,
    const unsigned char *pixels,
    unsigned int row_pitch)
{
    if (m_scene_open || width == 0 || height == 0 || pixels == nullptr ||
        width > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION ||
        height > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION ||
        width > (std::numeric_limits<unsigned int>::max() / 4u) ||
        row_pitch < width * 4u)
    {
        return RenderBackendTextureHandle();
    }

    std::size_t slot = 0;
    while (slot < m_static_textures.size() && m_static_textures[slot].occupied)
    {
        ++slot;
    }
    if (slot >= std::numeric_limits<unsigned int>::max())
    {
        return RenderBackendTextureHandle();
    }

    ID3D12Resource *texture = nullptr;
    ID3D12Resource *upload = nullptr;
    ID3D12CommandAllocator *upload_allocator = nullptr;
    ID3D12GraphicsCommandList *upload_list = nullptr;

    try
    {
        ensureTextureDescriptorCapacity(slot + 1);

        D3D12_HEAP_PROPERTIES heap_properties{};
        heap_properties.Type = D3D12_HEAP_TYPE_DEFAULT;
        heap_properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heap_properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heap_properties.CreationNodeMask = 1;
        heap_properties.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC texture_desc{};
        texture_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        texture_desc.Width = width;
        texture_desc.Height = height;
        texture_desc.DepthOrArraySize = 1;
        texture_desc.MipLevels = 1;
        texture_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        texture_desc.SampleDesc.Count = 1;
        texture_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

        checkHresult(
            "ID3D12Device::CreateCommittedResource(texture)",
            m_device->CreateCommittedResource(
                &heap_properties,
                D3D12_HEAP_FLAG_NONE,
                &texture_desc,
                D3D12_RESOURCE_STATE_COPY_DEST,
                nullptr,
                IID_PPV_ARGS(&texture)));

        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
        UINT row_count = 0;
        UINT64 row_size = 0;
        UINT64 upload_size = 0;
        m_device->GetCopyableFootprints(
            &texture_desc,
            0,
            1,
            0,
            &footprint,
            &row_count,
            &row_size,
            &upload_size);
        if (row_count != height || row_size < static_cast<UINT64>(width) * 4u ||
            upload_size > std::numeric_limits<std::size_t>::max())
        {
            throw std::runtime_error("unexpected D3D12 RGBA8 upload footprint");
        }

        upload = createUploadBuffer(m_device, static_cast<std::size_t>(upload_size));
        D3D12_RANGE no_read{0, 0};
        void *mapped = nullptr;
        checkHresult("ID3D12Resource::Map(texture upload)", upload->Map(0, &no_read, &mapped));
        auto *destination = static_cast<unsigned char *>(mapped) + static_cast<std::size_t>(footprint.Offset);
        const std::size_t copy_bytes = static_cast<std::size_t>(width) * 4u;
        for (unsigned int row = 0; row < height; ++row)
        {
            std::memcpy(
                destination + static_cast<std::size_t>(row) * footprint.Footprint.RowPitch,
                pixels + static_cast<std::size_t>(row) * row_pitch,
                copy_bytes);
        }
        upload->Unmap(0, nullptr);

        checkHresult(
            "ID3D12Device::CreateCommandAllocator(texture upload)",
            m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&upload_allocator)));
        checkHresult(
            "ID3D12Device::CreateCommandList(texture upload)",
            m_device->CreateCommandList(
                0,
                D3D12_COMMAND_LIST_TYPE_DIRECT,
                upload_allocator,
                nullptr,
                IID_PPV_ARGS(&upload_list)));

        D3D12_TEXTURE_COPY_LOCATION destination_location{};
        destination_location.pResource = texture;
        destination_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        destination_location.SubresourceIndex = 0;
        D3D12_TEXTURE_COPY_LOCATION source_location{};
        source_location.pResource = upload;
        source_location.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        source_location.PlacedFootprint = footprint;
        upload_list->CopyTextureRegion(
            &destination_location,
            0,
            0,
            0,
            &source_location,
            nullptr);

        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = texture;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        upload_list->ResourceBarrier(1, &barrier);

        checkHresult("ID3D12GraphicsCommandList::Close(texture upload)", upload_list->Close());
        ID3D12CommandList *lists[] = {upload_list};
        m_command_queue->ExecuteCommandLists(1, lists);
        const std::uint64_t signal_value = m_next_fence_value++;
        checkHresult("ID3D12CommandQueue::Signal(texture upload)", m_command_queue->Signal(m_fence, signal_value));
        if (m_fence->GetCompletedValue() < signal_value)
        {
            checkHresult(
                "ID3D12Fence::SetEventOnCompletion(texture upload)",
                m_fence->SetEventOnCompletion(signal_value, static_cast<HANDLE>(m_fence_event)));
            if (WaitForSingleObject(static_cast<HANDLE>(m_fence_event), INFINITE) != WAIT_OBJECT_0)
            {
                throw std::runtime_error("WaitForSingleObject failed while uploading D3D12 texture");
            }
        }

        D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
        srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srv_desc.Texture2D.MostDetailedMip = 0;
        srv_desc.Texture2D.MipLevels = 1;
        srv_desc.Texture2D.ResourceMinLODClamp = 0.0f;
        D3D12_CPU_DESCRIPTOR_HANDLE srv = m_texture_srv_heap->GetCPUDescriptorHandleForHeapStart();
        srv.ptr += slot * m_srv_descriptor_size;
        m_device->CreateShaderResourceView(texture, &srv_desc, srv);

        releaseCom(upload_list);
        releaseCom(upload_allocator);
        releaseCom(upload);

        if (slot == m_static_textures.size())
        {
            m_static_textures.push_back(StaticTextureResource{});
        }
        StaticTextureResource &stored = m_static_textures[slot];
        ++stored.generation;
        if (stored.generation == 0)
        {
            ++stored.generation;
        }
        stored.texture = texture;
        stored.width = width;
        stored.height = height;
        stored.occupied = true;
        texture = nullptr;
        return RenderBackendTextureHandle(static_cast<unsigned int>(slot + 1), stored.generation);
    }
    catch (...)
    {
        releaseCom(upload_list);
        releaseCom(upload_allocator);
        releaseCom(upload);
        releaseCom(texture);
        return RenderBackendTextureHandle();
    }
}

bool D3D12Backend::Draw_Static_Indexed_Textured_Geometry(
    RenderBackendGeometryHandle geometry_handle,
    RenderBackendTextureHandle texture_handle)
{
    if (!m_scene_open || !geometry_handle.Is_Valid() || !texture_handle.Is_Valid() ||
        m_texture_srv_heap == nullptr || m_textured_pipeline == nullptr)
    {
        return false;
    }

    const std::size_t geometry_slot = static_cast<std::size_t>(geometry_handle.slot - 1);
    const std::size_t texture_slot = static_cast<std::size_t>(texture_handle.slot - 1);
    if (geometry_slot >= m_static_geometry.size() || texture_slot >= m_static_textures.size())
    {
        return false;
    }

    const StaticGeometryResource &geometry = m_static_geometry[geometry_slot];
    const StaticTextureResource &texture = m_static_textures[texture_slot];
    if (!geometry.occupied || geometry.generation != geometry_handle.generation || !geometry.textured ||
        geometry.vertex_buffer == nullptr || geometry.index_buffer == nullptr ||
        !texture.occupied || texture.generation != texture_handle.generation || texture.texture == nullptr)
    {
        return false;
    }

    D3D12_VERTEX_BUFFER_VIEW vertex_view{};
    vertex_view.BufferLocation = geometry.vertex_buffer->GetGPUVirtualAddress();
    vertex_view.SizeInBytes = geometry.vertex_bytes;
    vertex_view.StrideInBytes = geometry.vertex_stride;
    D3D12_INDEX_BUFFER_VIEW index_view{};
    index_view.BufferLocation = geometry.index_buffer->GetGPUVirtualAddress();
    index_view.SizeInBytes = geometry.index_bytes;
    index_view.Format = DXGI_FORMAT_R16_UINT;

    ID3D12DescriptorHeap *heaps[] = {m_texture_srv_heap};
    m_command_list->SetDescriptorHeaps(1, heaps);
    m_command_list->SetGraphicsRootSignature(m_primitive_root_signature);
    m_command_list->SetPipelineState(m_textured_pipeline);
    D3D12_GPU_DESCRIPTOR_HANDLE srv = m_texture_srv_heap->GetGPUDescriptorHandleForHeapStart();
    srv.ptr += texture_slot * m_srv_descriptor_size;
    m_command_list->SetGraphicsRootDescriptorTable(0, srv);
    m_command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_command_list->IASetVertexBuffers(0, 1, &vertex_view);
    m_command_list->IASetIndexBuffer(&index_view);
    m_command_list->DrawIndexedInstanced(geometry.index_count, 1, 0, 0, 0);
    return true;
}

void D3D12Backend::Release_Static_Texture(RenderBackendTextureHandle texture_handle)
{
    if (!texture_handle.Is_Valid() || m_scene_open)
    {
        return;
    }
    const std::size_t slot = static_cast<std::size_t>(texture_handle.slot - 1);
    if (slot >= m_static_textures.size() || !m_static_textures[slot].occupied ||
        m_static_textures[slot].generation != texture_handle.generation)
    {
        return;
    }
    try
    {
        waitForGpu();
    }
    catch (...)
    {
        return;
    }
    releaseStaticTexture(m_static_textures[slot]);
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
    if (fence_value != 0 && m_fence->GetCompletedValue() < fence_value)
    {
        checkHresult("ID3D12Fence::SetEventOnCompletion",
                     m_fence->SetEventOnCompletion(fence_value, static_cast<HANDLE>(m_fence_event)));
        if (WaitForSingleObject(static_cast<HANDLE>(m_fence_event), INFINITE) != WAIT_OBJECT_0)
        {
            throw std::runtime_error("WaitForSingleObject failed while waiting for a D3D12 frame");
        }
    }
    releaseFrameUploads(frame_index);
}

void D3D12Backend::releaseFrameUploads(std::uint32_t frame_index) noexcept
{
    for (ID3D12Resource *resource : m_frame_uploads[frame_index])
    {
        if (resource != nullptr)
        {
            resource->Release();
        }
    }
    m_frame_uploads[frame_index].clear();
}

void D3D12Backend::releaseStaticGeometry(StaticGeometryResource &geometry) noexcept
{
    releaseCom(geometry.index_buffer);
    releaseCom(geometry.vertex_buffer);
    geometry.vertex_bytes = 0;
    geometry.index_bytes = 0;
    geometry.vertex_stride = 0;
    geometry.index_count = 0;
    geometry.textured = false;
    geometry.occupied = false;
}

void D3D12Backend::releaseStaticTexture(StaticTextureResource &texture) noexcept
{
    releaseCom(texture.texture);
    texture.width = 0;
    texture.height = 0;
    texture.occupied = false;
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
    for (std::uint32_t index = 0; index < FrameCount; ++index)
    {
        releaseFrameUploads(index);
    }
    for (StaticGeometryResource &geometry : m_static_geometry)
    {
        releaseStaticGeometry(geometry);
    }
    m_static_geometry.clear();
    for (StaticTextureResource &texture : m_static_textures)
    {
        releaseStaticTexture(texture);
    }
    m_static_textures.clear();
    releaseCom(m_textured_pipeline);
    releaseCom(m_2d_additive_pipeline);
    releaseCom(m_2d_alpha_pipeline);
    releaseCom(m_2d_opaque_pipeline);
    releaseCom(m_primitive_pipeline);
    releaseCom(m_primitive_root_signature);
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
    releaseCom(m_texture_srv_heap);
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
