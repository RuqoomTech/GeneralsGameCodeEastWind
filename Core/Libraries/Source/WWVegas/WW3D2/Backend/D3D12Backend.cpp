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
#include "WWMath/matrix4.h"

#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstddef>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace
{
static_assert(std::is_trivially_copyable<RenderBackendTexturedVertex>::value,
    "Backend vertex upload must copy values without native ownership");
static_assert(std::is_standard_layout<RenderBackendTexturedVertex>::value &&
    offsetof(RenderBackendTexturedVertex, x)==0 && offsetof(RenderBackendTexturedVertex, r)==12 &&
    offsetof(RenderBackendTexturedVertex, u)==28 && offsetof(RenderBackendTexturedVertex, u2)==40,
    "Textured input layout must match the CPU upload offsets");
static_assert(std::is_trivially_copyable<RenderBackendTerrainVertex>::value &&
    std::is_standard_layout<RenderBackendTerrainVertex>::value && sizeof(RenderBackendTerrainVertex) == 60 &&
    offsetof(RenderBackendTerrainVertex, x) == 0 && offsetof(RenderBackendTerrainVertex, r) == 12 &&
    offsetof(RenderBackendTerrainVertex, u) == 28 && offsetof(RenderBackendTerrainVertex, u2) == 36 &&
    offsetof(RenderBackendTerrainVertex, u3) == 44 && offsetof(RenderBackendTerrainVertex, u4) == 52,
    "Terrain input layout must match the CPU upload offsets");

bool validMaterialSampler(const RenderBackendSamplerState &sampler)
{
    return static_cast<unsigned int>(sampler.min_filter) <= 1 &&
        static_cast<unsigned int>(sampler.mag_filter) <= 1 &&
        static_cast<unsigned int>(sampler.mip_filter) <= 1 &&
        static_cast<unsigned int>(sampler.address_u) <= 1 &&
        static_cast<unsigned int>(sampler.address_v) <= 1 &&
        sampler.max_anisotropy >= 1 && sampler.max_anisotropy <= 16 && sampler.min_mip_level <= 15;
}

bool validMaterialState(const RenderBackendMaterialState &material)
{
    if (material.monochrome && (!std::isfinite(material.monochrome_fade) ||
        material.monochrome_fade < 0.0f || material.monochrome_fade > 1.0f ||
        !std::isfinite(material.monochrome_tint[0]) || !std::isfinite(material.monochrome_tint[1]) ||
        !std::isfinite(material.monochrome_tint[2]))) return false;
    auto valid_face = [](const RenderBackendStencilFace &face) {
        return static_cast<unsigned int>(face.comparison) < 8 &&
            static_cast<unsigned int>(face.stencil_fail) < 8 &&
            static_cast<unsigned int>(face.depth_fail) < 8 &&
            static_cast<unsigned int>(face.pass) < 8;
    };
    return validMaterialSampler(material.sampler) && material.color_write_mask <= 15 &&
        static_cast<unsigned int>(material.depth_test) <= 8 &&
        static_cast<unsigned int>(material.source_blend) <= 6 &&
        static_cast<unsigned int>(material.destination_blend) <= 6 &&
        static_cast<unsigned int>(material.cull) <= 2 &&
        static_cast<unsigned int>(material.texture_combine) <= 3 &&
        static_cast<unsigned int>(material.alpha_test) <= 2 &&
        material.alpha_reference >= 0.0f && material.alpha_reference <= 1.0f &&
        material.stencil.reference <= 255 && material.stencil.read_mask <= 255 && material.stencil.write_mask <= 255 &&
        valid_face(material.stencil.front) && valid_face(material.stencil.back);
}

constexpr float IdentityTransform[16]{
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f};

unsigned int nextResourceGeneration()
{
    // Resource identities remain local renderer values, never deterministic state.
    // Continue across backend recreation so a stale handle cannot alias a new device.
    static std::atomic<unsigned int> generation{0};
    unsigned int result;
    do { result = generation.fetch_add(1, std::memory_order_relaxed) + 1; } while (result == 0);
    return result;
}

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

void createTextureView(ID3D12Device *device, ID3D12Resource *texture, D3D12_CPU_DESCRIPTOR_HANDLE destination)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC view{};
    view.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    view.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    view.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    view.Texture2D.MipLevels = texture->GetDesc().MipLevels;
    device->CreateShaderResourceView(texture, &view, destination);
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

    D3D12_ROOT_PARAMETER parameters[11]{};
    D3D12_ROOT_PARAMETER &texture_parameter = parameters[0];
    texture_parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    texture_parameter.DescriptorTable.NumDescriptorRanges = 1;
    texture_parameter.DescriptorTable.pDescriptorRanges = &texture_range;
    texture_parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_PARAMETER &camera_parameter = parameters[1];
    camera_parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    camera_parameter.Constants.ShaderRegister = 0;
    camera_parameter.Constants.Num32BitValues = 16;
    camera_parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    auto &material_parameter = parameters[2];
    material_parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    material_parameter.Constants.ShaderRegister = 1;
    material_parameter.Constants.Num32BitValues = 12;
    material_parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_DESCRIPTOR_RANGE sampler_range{};
    sampler_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
    sampler_range.NumDescriptors = 1;
    sampler_range.BaseShaderRegister = 2;
    parameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[3].DescriptorTable.NumDescriptorRanges = 1;
    parameters[3].DescriptorTable.pDescriptorRanges = &sampler_range;
    parameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_DESCRIPTOR_RANGE secondary_texture_range = texture_range;
    secondary_texture_range.BaseShaderRegister = 1;
    parameters[4] = parameters[0];
    parameters[4].DescriptorTable.pDescriptorRanges = &secondary_texture_range;
    D3D12_DESCRIPTOR_RANGE secondary_sampler_range = sampler_range;
    secondary_sampler_range.BaseShaderRegister = 3;
    parameters[5] = parameters[3];
    parameters[5].DescriptorTable.pDescriptorRanges = &secondary_sampler_range;

    D3D12_DESCRIPTOR_RANGE cloud_texture_range = texture_range;
    cloud_texture_range.BaseShaderRegister = 2;
    parameters[6] = parameters[0];
    parameters[6].DescriptorTable.pDescriptorRanges = &cloud_texture_range;
    D3D12_DESCRIPTOR_RANGE cloud_sampler_range = sampler_range;
    cloud_sampler_range.BaseShaderRegister = 4;
    parameters[7] = parameters[3];
    parameters[7].DescriptorTable.pDescriptorRanges = &cloud_sampler_range;
    D3D12_DESCRIPTOR_RANGE noise_texture_range = texture_range;
    noise_texture_range.BaseShaderRegister = 3;
    parameters[8] = parameters[0];
    parameters[8].DescriptorTable.pDescriptorRanges = &noise_texture_range;
    D3D12_DESCRIPTOR_RANGE noise_sampler_range = sampler_range;
    noise_sampler_range.BaseShaderRegister = 5;
    parameters[9] = parameters[3];
    parameters[9].DescriptorTable.pDescriptorRanges = &noise_sampler_range;
    parameters[10].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    parameters[10].Constants.ShaderRegister = 2;
    parameters[10].Constants.Num32BitValues = 28;
    parameters[10].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

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
    root_desc.NumParameters = 11;
    root_desc.pParameters = parameters;
    D3D12_STATIC_SAMPLER_DESC samplers[2]{sampler, sampler};
    samplers[1].ShaderRegister = 1;
    samplers[1].AddressU = samplers[1].AddressV = samplers[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplers[1].MaxLOD = 0.0f;
    root_desc.NumStaticSamplers = 2;
    root_desc.pStaticSamplers = samplers;
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

    // Every supported 2D filter/address/anisotropy combination has a stable
    // slot. Existing commands never observe an overwritten sampler descriptor.
    D3D12_DESCRIPTOR_HEAP_DESC sampler_heap{};
    sampler_heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
    sampler_heap.NumDescriptors = MaterialSamplerCount;
    sampler_heap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    checkHresult("CreateDescriptorHeap(material samplers)",
        m_device->CreateDescriptorHeap(&sampler_heap, IID_PPV_ARGS(&m_material_sampler_heap)));

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
        m_material_color_shader = compileShaderFromFile(shader_path.c_str(), "PSMaterialColor", "ps_5_1");
        m_material_texture_shader = compileShaderFromFile(shader_path.c_str(), "PSMaterialTexture", "ps_5_1");
        m_terrain_vertex_shader = compileShaderFromFile(shader_path.c_str(), "VSTerrain", "vs_5_1");
        m_terrain_color_shader = compileShaderFromFile(shader_path.c_str(), "PSTerrainColor", "ps_5_1");
        m_terrain_texture_shader = compileShaderFromFile(shader_path.c_str(), "PSTerrainTexture", "ps_5_1");
        m_material_vertex_shader = textured_vertex_shader;
        m_material_vertex_shader->AddRef();
        m_primitive_vertex_shader = color_vertex_shader;
        m_primitive_color_shader = color_pixel_shader;
        m_primitive_vertex_shader->AddRef();
        m_primitive_color_shader->AddRef();

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

        D3D12_GRAPHICS_PIPELINE_STATE_DESC color_only = screen_pipeline;
        color_only.DSVFormat = DXGI_FORMAT_UNKNOWN;
        checkHresult("CreateGraphicsPipelineState(color-only target)",
            m_device->CreateGraphicsPipelineState(&color_only, IID_PPV_ARGS(&m_color_only_pipeline)));

        screen_pipeline.BlendState.RenderTarget[0].BlendEnable = TRUE;
        screen_pipeline.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
        screen_pipeline.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        screen_pipeline.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
        screen_pipeline.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
        checkHresult(
            "ID3D12Device::CreateGraphicsPipelineState(2D alpha)",
            m_device->CreateGraphicsPipelineState(&screen_pipeline, IID_PPV_ARGS(&m_2d_alpha_pipeline)));
        color_only = screen_pipeline;
        color_only.DSVFormat = DXGI_FORMAT_UNKNOWN;
        checkHresult("CreateGraphicsPipelineState(color-only alpha)",
            m_device->CreateGraphicsPipelineState(&color_only, IID_PPV_ARGS(&m_color_only_alpha_pipeline)));

        screen_pipeline.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
        screen_pipeline.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
        screen_pipeline.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
        screen_pipeline.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
        checkHresult(
            "ID3D12Device::CreateGraphicsPipelineState(2D additive)",
            m_device->CreateGraphicsPipelineState(&screen_pipeline, IID_PPV_ARGS(&m_2d_additive_pipeline)));
        color_only = screen_pipeline;
        color_only.DSVFormat = DXGI_FORMAT_UNKNOWN;
        checkHresult("CreateGraphicsPipelineState(color-only additive)",
            m_device->CreateGraphicsPipelineState(&color_only, IID_PPV_ARGS(&m_color_only_additive_pipeline)));

        const D3D12_INPUT_ELEMENT_DESC textured_elements[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
             D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12,
             D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 28,
             D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, 40,
             D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        };
        pipeline.VS = {textured_vertex_shader->GetBufferPointer(), textured_vertex_shader->GetBufferSize()};
        pipeline.PS = {textured_pixel_shader->GetBufferPointer(), textured_pixel_shader->GetBufferSize()};
        pipeline.InputLayout = {textured_elements, 4};
        checkHresult(
            "ID3D12Device::CreateGraphicsPipelineState(textured)",
            m_device->CreateGraphicsPipelineState(&pipeline, IID_PPV_ARGS(&m_textured_pipeline)));
        pipeline.DepthStencilState = color_only.DepthStencilState;
        pipeline.DSVFormat = DXGI_FORMAT_UNKNOWN;
        checkHresult("CreateGraphicsPipelineState(textured color-only target)",
            m_device->CreateGraphicsPipelineState(&pipeline, IID_PPV_ARGS(&m_textured_color_only_pipeline)));
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
    if (required_capacity > std::numeric_limits<UINT>::max())
    {
        throw std::runtime_error("D3D12 texture descriptor capacity exceeds UINT");
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
        if (m_scene_open)
        {
            // Submitted and currently recorded lists can reference this heap.
            // The current frame's later fence covers both on the same queue.
            auto &retired = m_frame_retired_texture_heaps[m_frame_index];
            retired.reserve(retired.size() + 1);
        }
        else
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
        const D3D12_CPU_DESCRIPTOR_HANDLE new_start = new_heap->GetCPUDescriptorHandleForHeapStart();
        for (std::size_t index = 0; index < m_textures.size(); ++index)
        {
            if (!m_textures[index].occupied)
            {
                continue;
            }
            D3D12_CPU_DESCRIPTOR_HANDLE destination = new_start;
            destination.ptr += index * m_srv_descriptor_size;
            // Shader-visible heaps cannot be descriptor-copy sources. Rebuild
            // the SRV from the resource owned by this same texture slot.
            createTextureView(m_device, m_textures[index].texture, destination);
        }
    }

    if (m_scene_open && m_texture_srv_heap != nullptr)
        m_frame_retired_texture_heaps[m_frame_index].push_back(m_texture_srv_heap);
    else
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

    // Flip-discard does not preserve presented back buffers. Retain the latest
    // output on the GPU before Present so existing synchronous capture callers
    // can read it later without depending on discarded swapchain contents.
    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    heap.CreationNodeMask = heap.VisibleNodeMask = 1;
    D3D12_RESOURCE_DESC capture_desc = m_render_targets[0]->GetDesc();
    capture_desc.Flags = D3D12_RESOURCE_FLAG_NONE;
    checkHresult("CreateCommittedResource(retained output)",
        m_device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &capture_desc,
            D3D12_RESOURCE_STATE_COPY_SOURCE, nullptr, IID_PPV_ARGS(&m_output_capture)));
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
    barrier.Transition.pResource = activeColorTarget();
    barrier.Transition.StateBefore = m_selected_texture.Is_Valid()
        ? D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    m_command_list->ResourceBarrier(1, &barrier);

    m_scene_open = true;
    bindActiveTarget();
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
        const D3D12_CPU_DESCRIPTOR_HANDLE rtv = m_selected_texture.Is_Valid()
            ? m_textures[m_selected_texture.slot - 1].rtv_heap->GetCPUDescriptorHandleForHeapStart()
            : offsetHandle(m_rtv_heap->GetCPUDescriptorHandleForHeapStart(), m_frame_index, m_rtv_descriptor_size);
        m_command_list->ClearRenderTargetView(rtv, m_clear_color, 0, nullptr);
        m_clear_color_pending = false;
    }

    if (m_clear_depth_pending && !activeTargetHasDepth())
        m_clear_depth_pending = false; // Color-only passes have no depth/stencil attachment.
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

void D3D12Backend::Set_View_Projection(const Matrix4x4 &view_projection)
{
    // Pack values explicitly; native WWMath layout never becomes a GPU ABI.
    for (unsigned int row = 0; row < 4; ++row)
        for (unsigned int column = 0; column < 4; ++column)
            m_view_projection[row * 4 + column] = view_projection[row][column];
}

void D3D12Backend::Get_View_Projection(Matrix4x4 &view_projection) const
{
    for (unsigned int row = 0; row < 4; ++row)
        for (unsigned int column = 0; column < 4; ++column)
            view_projection[row][column] = m_view_projection[row * 4 + column];
}

D3D12Backend::TextureResource *D3D12Backend::findTexture(RenderBackendTextureHandle handle)
{
    if (!handle.Is_Valid() || handle.slot > m_textures.size()) return nullptr;
    TextureResource &texture = m_textures[handle.slot - 1];
    return texture.occupied && texture.release_frame == FrameCount &&
        texture.generation == handle.generation ? &texture : nullptr;
}

ID3D12Resource *D3D12Backend::activeColorTarget() const
{
    return m_selected_texture.Is_Valid() ? m_textures[m_selected_texture.slot - 1].texture
                                         : m_render_targets[m_frame_index];
}

bool D3D12Backend::activeTargetHasDepth() const
{
    return !m_selected_texture.Is_Valid() || m_selected_output_depth;
}

void D3D12Backend::bindActiveTarget()
{
    const D3D12_CPU_DESCRIPTOR_HANDLE rtv = m_selected_texture.Is_Valid()
        ? m_textures[m_selected_texture.slot - 1].rtv_heap->GetCPUDescriptorHandleForHeapStart()
        : offsetHandle(m_rtv_heap->GetCPUDescriptorHandleForHeapStart(), m_frame_index, m_rtv_descriptor_size);
    const D3D12_CPU_DESCRIPTOR_HANDLE dsv = m_dsv_heap->GetCPUDescriptorHandleForHeapStart();
    m_command_list->OMSetRenderTargets(1, &rtv, FALSE, activeTargetHasDepth() ? &dsv : nullptr);
    Set_Viewport(m_viewport);
}

RenderBackendTextureHandle D3D12Backend::Create_Render_Texture(unsigned int width, unsigned int height,
    bool use_output_depth)
{
    if ((m_scene_open && !use_output_depth) || !Is_Device_Ready() || width == 0 || height == 0 ||
        (use_output_depth && (width != m_width || height != m_height)) ||
        width > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION || height > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION)
        return {};
    std::size_t slot = 0;
    while (slot < m_textures.size() && m_textures[slot].occupied) ++slot;
    if (slot >= std::numeric_limits<unsigned int>::max()) return {};
    ID3D12Resource *texture = nullptr;
    ID3D12DescriptorHeap *rtv = nullptr;
    try
    {
        ensureTextureDescriptorCapacity(slot + 1);
        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = D3D12_HEAP_TYPE_DEFAULT;
        heap.CreationNodeMask = heap.VisibleNodeMask = 1;
        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width = width;
        desc.Height = height;
        desc.DepthOrArraySize = desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        checkHresult("CreateCommittedResource(render texture)",
            m_device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, IID_PPV_ARGS(&texture)));
        D3D12_DESCRIPTOR_HEAP_DESC rtv_desc{};
        rtv_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtv_desc.NumDescriptors = 1;
        checkHresult("CreateDescriptorHeap(render texture RTV)",
            m_device->CreateDescriptorHeap(&rtv_desc, IID_PPV_ARGS(&rtv)));
        m_device->CreateRenderTargetView(texture, nullptr, rtv->GetCPUDescriptorHandleForHeapStart());
        D3D12_CPU_DESCRIPTOR_HANDLE srv = m_texture_srv_heap->GetCPUDescriptorHandleForHeapStart();
        srv.ptr += slot * m_srv_descriptor_size;
        createTextureView(m_device, texture, srv);
        if (slot == m_textures.size()) m_textures.push_back(TextureResource{});
        TextureResource &stored = m_textures[slot];
        stored.texture = texture;
        stored.rtv_heap = rtv;
        stored.width = width;
        stored.height = height;
        stored.generation = nextResourceGeneration();
        stored.occupied = true;
        return {static_cast<unsigned int>(slot + 1), stored.generation};
    }
    catch (...)
    {
        releaseCom(rtv);
        releaseCom(texture);
        return {};
    }
}

bool D3D12Backend::Set_Render_Texture(RenderBackendTextureHandle handle, bool use_output_depth)
{
    TextureResource *texture = nullptr;
    if (handle.Is_Valid())
    {
        texture = findTexture(handle);
        if (texture == nullptr || texture->rtv_heap == nullptr ||
            (use_output_depth && (texture->width != m_width || texture->height != m_height))) return false;
    }
    else if (handle.slot != 0 || handle.generation != 0) return false;
    const bool output_depth = texture != nullptr && use_output_depth;
    if (m_scene_open && !(output_depth || (texture == nullptr && m_selected_output_depth))) return false;
    if (handle.slot == m_selected_texture.slot && handle.generation == m_selected_texture.generation &&
        output_depth == m_selected_output_depth) return true;
    const bool target_changed = handle.slot != m_selected_texture.slot ||
        handle.generation != m_selected_texture.generation;
    if (m_scene_open && target_changed)
    {
        D3D12_RESOURCE_BARRIER barriers[2]{};
        for (auto &barrier : barriers)
        {
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        }
        barriers[0].Transition.pResource = activeColorTarget();
        barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barriers[0].Transition.StateAfter = m_selected_texture.Is_Valid()
            ? D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_PRESENT;
        barriers[1].Transition.pResource = texture != nullptr ? texture->texture : m_render_targets[m_frame_index];
        barriers[1].Transition.StateBefore = texture != nullptr
            ? D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_PRESENT;
        barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        m_command_list->ResourceBarrier(2, barriers);
    }
    if (!m_selected_texture.Is_Valid())
    {
        m_output_viewport = m_viewport;
        std::memcpy(m_output_view_projection, m_view_projection, sizeof(m_view_projection));
    }
    m_selected_texture = handle;
    m_selected_output_depth = output_depth;
    if (texture != nullptr)
        m_viewport = {0, 0, texture->width, texture->height, 0.0f, 1.0f};
    else
    {
        m_viewport = m_output_viewport;
        std::memcpy(m_view_projection, m_output_view_projection, sizeof(m_view_projection));
    }
    if (m_scene_open) bindActiveTarget();
    return true;
}

bool D3D12Backend::Get_Render_Target_Size(int &width, int &height) const
{
    if (!Is_Device_Ready()) return false;
    width = static_cast<int>(m_selected_texture.Is_Valid() ? m_textures[m_selected_texture.slot - 1].width : m_width);
    height = static_cast<int>(m_selected_texture.Is_Valid() ? m_textures[m_selected_texture.slot - 1].height : m_height);
    return true;
}

bool D3D12Backend::Copy_Texture(RenderBackendTextureHandle destination, RenderBackendTextureHandle source)
{
    TextureResource *dest = findTexture(destination);
    TextureResource *src = findTexture(source);
    if (m_scene_open || dest == nullptr || src == nullptr || dest == src ||
        dest->width != src->width || dest->height != src->height) return false;
    ID3D12CommandAllocator *allocator = nullptr;
    ID3D12GraphicsCommandList *list = nullptr;
    try
    {
        checkHresult("CreateCommandAllocator(texture copy)",
            m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)));
        checkHresult("CreateCommandList(texture copy)",
            m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator, nullptr, IID_PPV_ARGS(&list)));
        D3D12_RESOURCE_BARRIER barriers[2]{};
        for (auto &barrier : barriers)
        {
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        }
        barriers[0].Transition.pResource = src->texture;
        barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
        barriers[1].Transition.pResource = dest->texture;
        barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
        list->ResourceBarrier(2, barriers);
        list->CopyResource(dest->texture, src->texture);
        for (auto &barrier : barriers) std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
        list->ResourceBarrier(2, barriers);
        checkHresult("Close(texture copy)", list->Close());
        ID3D12CommandList *lists[] = {list};
        m_command_queue->ExecuteCommandLists(1, lists);
        waitForGpu();
        releaseCom(list);
        releaseCom(allocator);
        return true;
    }
    catch (...)
    {
        releaseCom(list);
        releaseCom(allocator);
        return false;
    }
}

void D3D12Backend::Invalidate_Cached_Render_States()
{
    // D3D12 state is explicit and command-list local. There is no DX8-style
    // shadow state cache to invalidate here.
}

bool D3D12Backend::Is_Device_Ready() const
{
    if (m_device == nullptr || m_swap_chain == nullptr || m_rtv_heap == nullptr ||
        m_dsv_heap == nullptr || m_depth_stencil == nullptr ||
        FAILED(m_device->GetDeviceRemovedReason()))
    {
        return false;
    }
    for (const ID3D12Resource *render_target : m_render_targets)
    {
        if (render_target == nullptr)
        {
            return false;
        }
    }
    return true;
}

bool D3D12Backend::Set_Swap_Interval(unsigned int interval)
{
    if (interval > 4)
    {
        return false;
    }
    m_sync_interval = interval;
    return true;
}

bool D3D12Backend::Has_Stencil() const
{
    return m_depth_stencil != nullptr &&
        m_depth_stencil->GetDesc().Format == DXGI_FORMAT_D24_UNORM_S8_UINT;
}

void D3D12Backend::Reset_Frame_Statistics()
{
    m_frame_statistics = {};
}

RenderBackendFrameStatistics D3D12Backend::Get_Frame_Statistics() const
{
    return m_frame_statistics;
}

unsigned int D3D12Backend::Get_Swap_Interval() const
{
    return m_sync_interval;
}

bool D3D12Backend::Read_Output_RGBA8(
    unsigned int &width, unsigned int &height, std::vector<unsigned char> &pixels)
{
    width = height = 0;
    pixels.clear();
    if (m_scene_open || !m_capture_available || m_output_capture == nullptr || !Is_Device_Ready())
    {
        return false;
    }

    ID3D12Resource *readback = nullptr;
    ID3D12CommandAllocator *allocator = nullptr;
    ID3D12GraphicsCommandList *list = nullptr;
    try
    {
        ID3D12Resource *source = m_output_capture;
        const D3D12_RESOURCE_DESC source_desc = source->GetDesc();
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
        UINT rows = 0;
        UINT64 row_bytes = 0, total_bytes = 0;
        m_device->GetCopyableFootprints(&source_desc, 0, 1, 0,
                                       &footprint, &rows, &row_bytes, &total_bytes);
        if (source_desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM || rows != m_height ||
            row_bytes != static_cast<UINT64>(m_width) * 4 ||
            total_bytes > std::numeric_limits<std::size_t>::max())
        {
            return false;
        }
        std::vector<unsigned char> captured(static_cast<std::size_t>(m_width) * m_height * 4);
        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = D3D12_HEAP_TYPE_READBACK;
        heap.CreationNodeMask = heap.VisibleNodeMask = 1;
        D3D12_RESOURCE_DESC buffer{};
        buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        buffer.Width = total_bytes;
        buffer.Height = 1;
        buffer.DepthOrArraySize = buffer.MipLevels = 1;
        buffer.SampleDesc.Count = 1;
        buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        checkHresult("CreateCommittedResource(output readback)",
            m_device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &buffer,
                D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readback)));
        checkHresult("CreateCommandAllocator(output readback)",
            m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)));
        checkHresult("CreateCommandList(output readback)",
            m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator,
                                       nullptr, IID_PPV_ARGS(&list)));
        D3D12_TEXTURE_COPY_LOCATION destination{};
        destination.pResource = readback;
        destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        destination.PlacedFootprint = footprint;
        D3D12_TEXTURE_COPY_LOCATION origin{};
        origin.pResource = source;
        origin.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        list->CopyTextureRegion(&destination, 0, 0, 0, &origin, nullptr);
        checkHresult("Close(output readback)", list->Close());
        ID3D12CommandList *lists[] = {list};
        m_command_queue->ExecuteCommandLists(1, lists);
        waitForGpu();
        const D3D12_RANGE read_range{0, static_cast<std::size_t>(total_bytes)};
        void *mapped = nullptr;
        checkHresult("Map(output readback)", readback->Map(0, &read_range, &mapped));
        const auto *data = static_cast<const unsigned char *>(mapped) + footprint.Offset;
        for (unsigned int row = 0; row < m_height; ++row)
        {
            std::memcpy(captured.data() + static_cast<std::size_t>(row) * m_width * 4,
                        data + static_cast<std::size_t>(row) * footprint.Footprint.RowPitch,
                        static_cast<std::size_t>(row_bytes));
        }
        const D3D12_RANGE no_write{0, 0};
        readback->Unmap(0, &no_write);
        width = m_width;
        height = m_height;
        pixels.swap(captured);
        releaseCom(list);
        releaseCom(allocator);
        releaseCom(readback);
        return true;
    }
    catch (...)
    {
        releaseCom(list);
        releaseCom(allocator);
        releaseCom(readback);
        return false;
    }
}

bool D3D12Backend::Configure_Output(unsigned int width, unsigned int height, bool windowed)
{
    if (m_scene_open || m_selected_texture.Is_Valid() || m_present_pending || m_swap_chain == nullptr ||
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
        m_capture_available = false;
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
        releaseCom(m_output_capture);
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
    if (Get_Pass_Color_Write_Mask() != 15)
    {
        RenderBackendMaterialState material;
        material.cull = RenderBackendCullMode::None;
        if (!activeTargetHasDepth()) {
            material.depth_test = RenderBackendDepthTest::Disabled;
            material.depth_write = false;
        }
        try {
            return drawDynamicGeometry(vertices, vertex_count, sizeof(RenderBackendColorVertex), indices, index_count,
                materialPipeline(material, false, false, true), false);
        } catch (...) { return false; }
    }
    return drawDynamicGeometry(vertices, vertex_count, sizeof(RenderBackendColorVertex), indices, index_count,
        activeTargetHasDepth() ? m_primitive_pipeline : m_color_only_pipeline, false);
}

bool D3D12Backend::Draw_2D_Indexed_Triangles(
    const RenderBackendColorVertex *vertices,
    unsigned int vertex_count,
    const unsigned short *indices,
    unsigned int index_count,
    RenderBackend2DBlendMode blend_mode)
{
    if (Get_Pass_Color_Write_Mask() != 15)
    {
        RenderBackendMaterialState material;
        material.depth_test = RenderBackendDepthTest::Disabled;
        material.depth_write = false;
        material.cull = RenderBackendCullMode::None;
        if (blend_mode == RenderBackend2DBlendMode::Alpha) {
            material.source_blend = RenderBackendBlendFactor::SourceAlpha;
            material.destination_blend = RenderBackendBlendFactor::InverseSourceAlpha;
        } else if (blend_mode == RenderBackend2DBlendMode::Additive) {
            material.source_blend = material.destination_blend = RenderBackendBlendFactor::One;
        }
        try {
            return drawDynamicGeometry(vertices, vertex_count, sizeof(RenderBackendColorVertex), indices, index_count,
                materialPipeline(material, false, false, true), true);
        } catch (...) { return false; }
    }
    const bool offscreen = !activeTargetHasDepth();
    ID3D12PipelineState *pipeline = offscreen ? m_color_only_pipeline : m_2d_opaque_pipeline;
    switch (blend_mode)
    {
        case RenderBackend2DBlendMode::Alpha:
            pipeline = offscreen ? m_color_only_alpha_pipeline : m_2d_alpha_pipeline;
            break;
        case RenderBackend2DBlendMode::Additive:
            pipeline = offscreen ? m_color_only_additive_pipeline : m_2d_additive_pipeline;
            break;
        case RenderBackend2DBlendMode::Opaque:
        default:
            break;
    }

    return drawDynamicGeometry(vertices, vertex_count, sizeof(RenderBackendColorVertex), indices, index_count, pipeline, true);
}

bool D3D12Backend::drawDynamicGeometry(
    const void *vertices,
    unsigned int vertex_count,
    unsigned int vertex_stride,
    const unsigned short *indices,
    unsigned int index_count,
    ID3D12PipelineState *pipeline,
    bool screen_space,
    RenderBackendTextureHandle texture_handle,
    const RenderBackendMaterialState *material,
    RenderBackendTextureHandle secondary_texture,
    const RenderBackendTerrainState *terrain)
{
    if (!m_scene_open || pipeline == nullptr || vertices == nullptr || indices == nullptr ||
        vertex_count == 0 || vertex_stride == 0 || index_count < 3 || (index_count % 3) != 0)
    {
        return false;
    }

    if (vertex_count > (std::numeric_limits<std::size_t>::max() / vertex_stride) ||
        index_count > (std::numeric_limits<std::size_t>::max() / sizeof(unsigned short)))
    {
        return false;
    }

    const std::size_t vertex_bytes = static_cast<std::size_t>(vertex_count) * vertex_stride;
    const std::size_t index_bytes = static_cast<std::size_t>(index_count) * sizeof(unsigned short);
    if (vertex_bytes > std::numeric_limits<UINT>::max() || index_bytes > std::numeric_limits<UINT>::max())
    {
        return false;
    }

    for (unsigned int index = 0; index < index_count; ++index)
        if (indices[index] >= vertex_count) return false;

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
        checkHresult("ID3D12Resource::Map(dynamic vertex)", vertex_upload->Map(0, &no_read, &mapped));
        std::memcpy(mapped, vertices, vertex_bytes);
        vertex_upload->Unmap(0, nullptr);

        mapped = nullptr;
        checkHresult("ID3D12Resource::Map(dynamic index)", index_upload->Map(0, &no_read, &mapped));
        std::memcpy(mapped, indices, index_bytes);
        index_upload->Unmap(0, nullptr);

        D3D12_VERTEX_BUFFER_VIEW vertex_view{};
        vertex_view.BufferLocation = vertex_upload->GetGPUVirtualAddress();
        vertex_view.SizeInBytes = static_cast<UINT>(vertex_bytes);
        vertex_view.StrideInBytes = vertex_stride;

        D3D12_INDEX_BUFFER_VIEW index_view{};
        index_view.BufferLocation = index_upload->GetGPUVirtualAddress();
        index_view.SizeInBytes = static_cast<UINT>(index_bytes);
        index_view.Format = DXGI_FORMAT_R16_UINT;

        frame_uploads.push_back(vertex_upload);
        frame_uploads.push_back(index_upload);
        vertex_upload = nullptr;
        index_upload = nullptr;

        bindDrawState(pipeline, screen_space, texture_handle, material, secondary_texture, terrain);
        issueGeometryDraw(vertex_view, index_view, index_count, vertex_count, material);
        return true;
    }
    catch (...)
    {
        releaseCom(index_upload);
        releaseCom(vertex_upload);
        return false;
    }
}

void D3D12Backend::bindDrawState(ID3D12PipelineState *pipeline, bool screen_space,
    RenderBackendTextureHandle texture_handle, const RenderBackendMaterialState *material,
    RenderBackendTextureHandle secondary_texture, const RenderBackendTerrainState *terrain)
{
    m_command_list->SetGraphicsRootSignature(m_primitive_root_signature);
    m_command_list->SetGraphicsRoot32BitConstants(
        1, 16, screen_space ? IdentityTransform : m_view_projection, 0);
    m_command_list->SetPipelineState(pipeline);
    if (material != nullptr)
    {
        struct Constants {
            unsigned int combine, alpha_test; float alpha_reference; unsigned int clamp;
            unsigned int effects, terrain_layers; unsigned int padding[2];
            float monochrome_tint[3], monochrome_fade;
        };
        const Constants constants{static_cast<unsigned int>(material->texture_combine),
            static_cast<unsigned int>(material->alpha_test), material->alpha_reference,
            material->clamp_texture ? 1u : 0u,
            (material->secondary_rgb_modulate ? 1u : 0u) | (material->monochrome ? 2u : 0u) |
                (material->vertex_alpha ? 4u : 0u),
            terrain ? (terrain->shroud_texture.Is_Valid() ? 1u : 0u) |
                (terrain->cloud_texture.Is_Valid() ? 2u : 0u) |
                (terrain->noise_texture.Is_Valid() ? 4u : 0u) |
                (terrain->diffuse_after_layers ? 8u : 0u) |
                (terrain->blend_secondary_by_vertex_alpha ? 16u : 0u) : 0u, {0,0},
            {material->monochrome_tint[0], material->monochrome_tint[1], material->monochrome_tint[2]},
            material->monochrome_fade};
        static_assert(sizeof(Constants) == 12 * sizeof(unsigned int), "Material root constants");
        m_command_list->SetGraphicsRoot32BitConstants(2, 12, &constants, 0);
        m_command_list->OMSetStencilRef(material->stencil.reference);
    }
    if (texture_handle.Is_Valid())
    {
        ID3D12DescriptorHeap *heaps[] = {m_texture_srv_heap, m_material_sampler_heap};
        m_command_list->SetDescriptorHeaps(material ? 2 : 1, heaps);
        D3D12_GPU_DESCRIPTOR_HANDLE srv = m_texture_srv_heap->GetGPUDescriptorHandleForHeapStart();
        srv.ptr += static_cast<std::size_t>(texture_handle.slot - 1) * m_srv_descriptor_size;
        m_command_list->SetGraphicsRootDescriptorTable(0, srv);
        if (material)
        {
            auto sampler = material->sampler;
            if (material->clamp_texture)
            {
                sampler.address_u = sampler.address_v = RenderBackendTextureAddress::Clamp;
                sampler.mipmaps = false;
            }
            const auto slot = materialSampler(sampler);
            auto descriptor = m_material_sampler_heap->GetGPUDescriptorHandleForHeapStart();
            descriptor.ptr += static_cast<std::size_t>(slot) *
                m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
            m_command_list->SetGraphicsRootDescriptorTable(3, descriptor);
            auto bind_layer = [&](unsigned int root_index, RenderBackendTextureHandle layer,
                                  const RenderBackendSamplerState &layer_sampler) {
                auto srv = m_texture_srv_heap->GetGPUDescriptorHandleForHeapStart();
                srv.ptr += static_cast<std::size_t>(layer.slot - 1) * m_srv_descriptor_size;
                m_command_list->SetGraphicsRootDescriptorTable(root_index, srv);
                const auto layer_slot = materialSampler(layer_sampler);
                auto descriptor = m_material_sampler_heap->GetGPUDescriptorHandleForHeapStart();
                descriptor.ptr += static_cast<std::size_t>(layer_slot) *
                    m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
                m_command_list->SetGraphicsRootDescriptorTable(root_index + 1, descriptor);
            };
            // Bind every declared texture, including optional disabled layers.
            if (terrain)
            {
                bind_layer(4, terrain->shroud_texture.Is_Valid() ? terrain->shroud_texture : texture_handle,
                    terrain->shroud_texture.Is_Valid() ? terrain->shroud_sampler : sampler);
                bind_layer(6, terrain->cloud_texture.Is_Valid() ? terrain->cloud_texture : texture_handle,
                    terrain->cloud_texture.Is_Valid() ? terrain->cloud_sampler : sampler);
                bind_layer(8, terrain->noise_texture.Is_Valid() ? terrain->noise_texture : texture_handle,
                    terrain->noise_texture.Is_Valid() ? terrain->noise_sampler : sampler);
            }
            else
                bind_layer(4, material->secondary_rgb_modulate ? secondary_texture : texture_handle,
                    material->secondary_rgb_modulate ? material->secondary_sampler : sampler);
        }
    }
    if (terrain)
    {
        struct ProjectionConstants {
            float world[12], shroud[4], cloud_noise[4];
            unsigned int project, padding[3];
            float color[4];
        } constants{};
        static_assert(sizeof(ProjectionConstants) == 28 * sizeof(unsigned int), "Terrain root constants");
        std::memcpy(constants.world, terrain->world_transform, sizeof(constants.world));
        std::memcpy(constants.shroud, terrain->shroud_projection, sizeof(constants.shroud));
        std::memcpy(constants.cloud_noise, terrain->cloud_noise_projection, sizeof(constants.cloud_noise));
        std::memcpy(constants.color, terrain->constant_color, sizeof(constants.color));
        constants.project = (terrain->project_world_coordinates ? 1u : 0u) |
            (terrain->project_base_to_shroud ? 2u : 0u) |
            (terrain->blend_secondary_by_vertex_alpha ? 4u : 0u) |
            (terrain->use_constant_color ? 8u : 0u);
        m_command_list->SetGraphicsRoot32BitConstants(10, 28, &constants, 0);
    }
}

void D3D12Backend::issueGeometryDraw(const D3D12_VERTEX_BUFFER_VIEW &vertex_view,
    const D3D12_INDEX_BUFFER_VIEW &index_view, unsigned int index_count, unsigned int vertex_count,
    const RenderBackendMaterialState *material)
{
    m_command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_command_list->IASetVertexBuffers(0, 1, &vertex_view);
    m_command_list->IASetIndexBuffer(&index_view);
    const RenderBackendViewport saved_viewport = m_viewport;
    if (material != nullptr && material->screen_space)
    {
        const auto target = activeColorTarget()->GetDesc();
        Set_Viewport({0, 0, static_cast<unsigned int>(target.Width), target.Height, 0.0f, 1.0f});
    }
    m_command_list->DrawIndexedInstanced(index_count, 1, 0, 0, 0);
    if (material != nullptr && material->screen_space) Set_Viewport(saved_viewport);
    ++m_frame_statistics.draw_calls;
    m_frame_statistics.triangles += index_count / 3;
    m_frame_statistics.vertices += vertex_count;
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
    if (!Is_Device_Ready() || vertices == nullptr || indices == nullptr || vertex_stride == 0 ||
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

    for (unsigned int index = 0; index < index_count; ++index)
        if (indices[index] >= vertex_count) return {};

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
        geometry.generation = nextResourceGeneration();
        geometry.release_frame = FrameCount;
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
        ++m_frame_statistics.static_geometry_uploads;
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
    if (!m_scene_open || !Is_Geometry_Valid(geometry_handle))
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
        geometry.textured != textured || geometry.vertex_stride !=
            (textured ? sizeof(RenderBackendTexturedVertex) : sizeof(RenderBackendColorVertex)))
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
    m_command_list->SetGraphicsRoot32BitConstants(1, 16, m_view_projection, 0);
    ID3D12PipelineState *pipeline = activeTargetHasDepth() ? m_primitive_pipeline : m_color_only_pipeline;
    if (Get_Pass_Color_Write_Mask() != 15) {
        RenderBackendMaterialState material;
        material.cull = RenderBackendCullMode::None;
        if (!activeTargetHasDepth()) {
            material.depth_test = RenderBackendDepthTest::Disabled;
            material.depth_write = false;
        }
        try { pipeline = materialPipeline(material, false, false, true); }
        catch (...) { return false; }
    }
    m_command_list->SetPipelineState(pipeline);
    m_command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_command_list->IASetVertexBuffers(0, 1, &vertex_view);
    m_command_list->IASetIndexBuffer(&index_view);
    m_command_list->DrawIndexedInstanced(geometry.index_count, 1, 0, 0, 0);
    ++m_frame_statistics.draw_calls;
    m_frame_statistics.triangles += geometry.index_count / 3;
    m_frame_statistics.vertices += geometry.vertex_bytes / geometry.vertex_stride;
    return true;
}

bool D3D12Backend::Draw_Static_Indexed_Color_Geometry(RenderBackendGeometryHandle geometry_handle)
{
    return drawStaticGeometry(geometry_handle, false);
}

void D3D12Backend::Release_Static_Geometry(RenderBackendGeometryHandle geometry_handle)
{
    if (!Is_Geometry_Valid(geometry_handle))
    {
        return;
    }

    const std::size_t slot = static_cast<std::size_t>(geometry_handle.slot - 1);

    if (m_scene_open || m_present_pending)
    {
        // The current command list may still refer to this resource. Retire the
        // public handle now and free its buffers only after this frame's fence.
        m_static_geometry[slot].release_frame = m_frame_index;
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
    unsigned int width, unsigned int height, const unsigned char *pixels, unsigned int row_pitch)
{
    const RenderBackendTextureMipLevel level{width,height,row_pitch,pixels};
    return Create_Static_RGBA8_Texture(&level,1);
}

RenderBackendTextureHandle D3D12Backend::Create_Static_RGBA8_Texture(
    const RenderBackendTextureMipLevel *levels, unsigned int level_count)
{
    if (levels == nullptr || level_count == 0 || level_count > 15) return {};
    const unsigned int width = levels[0].width, height = levels[0].height;
    if (width == 0 || height == 0 || width > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION ||
        height > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION) return {};
    unsigned int expected_width = width, expected_height = height;
    for (unsigned int index=0; index<level_count; ++index)
    {
        const auto &level = levels[index];
        if (level.width != expected_width || level.height != expected_height || level.pixels == nullptr ||
            level.row_pitch < expected_width*4u) return {};
        if (index+1 < level_count && expected_width == 1 && expected_height == 1) return {};
        expected_width = std::max(1u,expected_width/2);
        expected_height = std::max(1u,expected_height/2);
    }

    std::size_t slot = 0;
    while (slot < m_textures.size() && m_textures[slot].occupied)
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
        texture_desc.MipLevels = static_cast<UINT16>(level_count);
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

        std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints(level_count);
        std::vector<UINT> row_counts(level_count);
        std::vector<UINT64> row_sizes(level_count);
        UINT64 upload_size = 0;
        m_device->GetCopyableFootprints(&texture_desc,0,level_count,0,
            footprints.data(),row_counts.data(),row_sizes.data(),&upload_size);
        if (upload_size > std::numeric_limits<std::size_t>::max())
            throw std::runtime_error("D3D12 mip-chain upload exceeds addressable memory");
        for (unsigned int index=0; index<level_count; ++index)
            if (row_counts[index] != levels[index].height || row_sizes[index] < levels[index].width*4u)
                throw std::runtime_error("unexpected D3D12 RGBA8 mip footprint");

        upload = createUploadBuffer(m_device,static_cast<std::size_t>(upload_size));
        D3D12_RANGE no_read{0,0};
        void *mapped = nullptr;
        checkHresult("ID3D12Resource::Map(texture upload)",upload->Map(0,&no_read,&mapped));
        for (unsigned int index=0; index<level_count; ++index)
        {
            const auto &level = levels[index];
            const auto &footprint = footprints[index];
            auto *destination = static_cast<unsigned char *>(mapped) + static_cast<std::size_t>(footprint.Offset);
            for (unsigned int row=0; row<level.height; ++row)
                std::memcpy(destination + static_cast<std::size_t>(row)*footprint.Footprint.RowPitch,
                    level.pixels + static_cast<std::size_t>(row)*level.row_pitch,static_cast<std::size_t>(level.width)*4);
        }
        upload->Unmap(0,nullptr);

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

        for (unsigned int index=0; index<level_count; ++index)
        {
            D3D12_TEXTURE_COPY_LOCATION destination_location{};
            destination_location.pResource = texture;
            destination_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            destination_location.SubresourceIndex = index;
            D3D12_TEXTURE_COPY_LOCATION source_location{};
            source_location.pResource = upload;
            source_location.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            source_location.PlacedFootprint = footprints[index];
            upload_list->CopyTextureRegion(&destination_location,0,0,0,&source_location,nullptr);
        }

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

        D3D12_CPU_DESCRIPTOR_HANDLE srv = m_texture_srv_heap->GetCPUDescriptorHandleForHeapStart();
        srv.ptr += slot * m_srv_descriptor_size;
        createTextureView(m_device, texture, srv);

        releaseCom(upload_list);
        releaseCom(upload_allocator);
        releaseCom(upload);

        if (slot == m_textures.size())
        {
            m_textures.push_back(TextureResource{});
        }
        TextureResource &stored = m_textures[slot];
        stored.generation = nextResourceGeneration();
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
    if (Get_Pass_Color_Write_Mask() != 15) {
        RenderBackendMaterialState material;
        material.cull = RenderBackendCullMode::None;
        if (!activeTargetHasDepth()) {
            material.depth_test = RenderBackendDepthTest::Disabled;
            material.depth_write = false;
        }
        return Draw_Static_Indexed_Material_Geometry(geometry_handle, texture_handle, material);
    }
    if ((texture_handle.slot == m_selected_texture.slot && texture_handle.generation == m_selected_texture.generation) ||
        !m_scene_open || !Is_Geometry_Valid(geometry_handle) || !texture_handle.Is_Valid() ||
        m_texture_srv_heap == nullptr || m_textured_pipeline == nullptr)
    {
        return false;
    }

    const std::size_t geometry_slot = static_cast<std::size_t>(geometry_handle.slot - 1);
    const std::size_t texture_slot = static_cast<std::size_t>(texture_handle.slot - 1);
    if (geometry_slot >= m_static_geometry.size() || texture_slot >= m_textures.size())
    {
        return false;
    }

    const StaticGeometryResource &geometry = m_static_geometry[geometry_slot];
    const TextureResource &texture = m_textures[texture_slot];
    if (!geometry.occupied || geometry.generation != geometry_handle.generation || !geometry.textured ||
        geometry.vertex_stride != sizeof(RenderBackendTexturedVertex) ||
        geometry.vertex_buffer == nullptr || geometry.index_buffer == nullptr ||
        !texture.occupied || texture.release_frame != FrameCount ||
        texture.generation != texture_handle.generation || texture.texture == nullptr)
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
    m_command_list->SetPipelineState(activeTargetHasDepth() ? m_textured_pipeline : m_textured_color_only_pipeline);
    m_command_list->SetGraphicsRoot32BitConstants(1, 16, m_view_projection, 0);
    D3D12_GPU_DESCRIPTOR_HANDLE srv = m_texture_srv_heap->GetGPUDescriptorHandleForHeapStart();
    srv.ptr += texture_slot * m_srv_descriptor_size;
    m_command_list->SetGraphicsRootDescriptorTable(0, srv);
    m_command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_command_list->IASetVertexBuffers(0, 1, &vertex_view);
    m_command_list->IASetIndexBuffer(&index_view);
    m_command_list->DrawIndexedInstanced(geometry.index_count, 1, 0, 0, 0);
    ++m_frame_statistics.draw_calls;
    m_frame_statistics.triangles += geometry.index_count / 3;
    m_frame_statistics.vertices += geometry.vertex_bytes / geometry.vertex_stride;
    return true;
}

bool D3D12Backend::Is_Texture_Valid(RenderBackendTextureHandle handle) const
{
    if (!handle.Is_Valid() || handle.slot > m_textures.size()) return false;
    const auto &texture = m_textures[handle.slot - 1];
    return texture.occupied && texture.release_frame == FrameCount && texture.generation == handle.generation;
}

unsigned int D3D12Backend::materialSampler(const RenderBackendSamplerState &state)
{
    const unsigned int key = static_cast<unsigned int>(state.min_filter) |
        (static_cast<unsigned int>(state.mag_filter) << 1) |
        (static_cast<unsigned int>(state.mip_filter) << 2) |
        (static_cast<unsigned int>(state.address_u) << 3) |
        (static_cast<unsigned int>(state.address_v) << 4) |
        (static_cast<unsigned int>(state.mipmaps) << 5) | ((state.max_anisotropy-1) << 6) | (state.min_mip_level << 10);
    for (unsigned int slot = 0; slot < m_material_sampler_keys.size(); ++slot)
        if (m_material_sampler_keys[slot] == key) return slot;
    if (m_material_sampler_keys.size() >= MaterialSamplerCount)
        throw std::runtime_error("Material sampler heap exhausted");
    const unsigned int slot = static_cast<unsigned int>(m_material_sampler_keys.size());
    {
        D3D12_SAMPLER_DESC sampler{};
        sampler.Filter = state.max_anisotropy > 1 ? D3D12_FILTER_ANISOTROPIC :
            static_cast<D3D12_FILTER>((static_cast<unsigned int>(state.min_filter) << 4) |
                (static_cast<unsigned int>(state.mag_filter) << 2) | static_cast<unsigned int>(state.mip_filter));
        sampler.AddressU = state.address_u == RenderBackendTextureAddress::Wrap ?
            D3D12_TEXTURE_ADDRESS_MODE_WRAP : D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.AddressV = state.address_v == RenderBackendTextureAddress::Wrap ?
            D3D12_TEXTURE_ADDRESS_MODE_WRAP : D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler.MaxAnisotropy = state.max_anisotropy;
        sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        sampler.MinLOD = state.mipmaps ? static_cast<float>(state.min_mip_level) : 0.0f;
        sampler.MaxLOD = state.mipmaps ? D3D12_FLOAT32_MAX : 0.0f;
        auto descriptor = m_material_sampler_heap->GetCPUDescriptorHandleForHeapStart();
        descriptor.ptr += static_cast<std::size_t>(slot) *
            m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
        m_device->CreateSampler(&sampler, descriptor);
        m_material_sampler_keys.push_back(key);
    }
    return slot;
}

ID3D12PipelineState *D3D12Backend::materialPipeline(const RenderBackendMaterialState &material, bool textured,
    bool terrain, bool primitive_color)
{
    const unsigned int write_mask = material.override_pass_color_write_mask ? material.color_write_mask :
        material.color_write_mask & Get_Pass_Color_Write_Mask();
    const unsigned int depth = static_cast<unsigned int>(material.depth_test);
    const unsigned int source = static_cast<unsigned int>(material.source_blend);
    const unsigned int destination = static_cast<unsigned int>(material.destination_blend);
    const unsigned int cull = static_cast<unsigned int>(material.cull);
    const bool color_only = !activeTargetHasDepth();
    auto face_key = [](const RenderBackendStencilFace &face) {
        return static_cast<std::uint64_t>(face.comparison) |
            (static_cast<std::uint64_t>(face.stencil_fail) << 3) |
            (static_cast<std::uint64_t>(face.depth_fail) << 6) |
            (static_cast<std::uint64_t>(face.pass) << 9);
    };
    const std::uint64_t key = depth | (source << 4) | (destination << 7) | (cull << 10) |
        (static_cast<unsigned int>(material.depth_write) << 12) |
        (static_cast<unsigned int>(material.color_write) << 13) |
        (static_cast<unsigned int>(textured) << 14) | (static_cast<unsigned int>(color_only) << 15) |
        (static_cast<std::uint64_t>(material.stencil.enabled) << 16) |
        (static_cast<std::uint64_t>(material.stencil.read_mask) << 17) |
        (static_cast<std::uint64_t>(material.stencil.write_mask) << 25) |
        (face_key(material.stencil.front) << 33) | (face_key(material.stencil.back) << 45) |
        (static_cast<std::uint64_t>(write_mask) << 57) |
        (static_cast<std::uint64_t>(terrain) << 61) |
        (static_cast<std::uint64_t>(material.wireframe) << 62) |
        (static_cast<std::uint64_t>(primitive_color) << 63);
    for (const auto &entry : m_material_pipelines)
        if (entry.key == key && entry.depth_bias == material.depth_bias) return entry.pipeline;

    static const D3D12_BLEND factors[] = {D3D12_BLEND_ZERO, D3D12_BLEND_ONE,
        D3D12_BLEND_SRC_COLOR, D3D12_BLEND_INV_SRC_COLOR, D3D12_BLEND_SRC_ALPHA, D3D12_BLEND_INV_SRC_ALPHA, D3D12_BLEND_DEST_COLOR};
    static const D3D12_BLEND alpha_factors[] = {D3D12_BLEND_ZERO, D3D12_BLEND_ONE,
        D3D12_BLEND_SRC_ALPHA, D3D12_BLEND_INV_SRC_ALPHA, D3D12_BLEND_SRC_ALPHA, D3D12_BLEND_INV_SRC_ALPHA, D3D12_BLEND_DEST_ALPHA};
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline{};
    pipeline.pRootSignature = m_primitive_root_signature;
    auto *vertex_shader = primitive_color ? m_primitive_vertex_shader :
        terrain ? m_terrain_vertex_shader : m_material_vertex_shader;
    pipeline.VS = {vertex_shader->GetBufferPointer(), vertex_shader->GetBufferSize()};
    auto *pixel_shader = primitive_color ? m_primitive_color_shader :
        terrain ? (textured ? m_terrain_texture_shader : m_terrain_color_shader) :
        (textured ? m_material_texture_shader : m_material_color_shader);
    pipeline.PS = {pixel_shader->GetBufferPointer(), pixel_shader->GetBufferSize()};
    const D3D12_INPUT_ELEMENT_DESC elements[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, 40, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};
    pipeline.InputLayout = {elements, 4};
    if (primitive_color) pipeline.InputLayout.NumElements = 2;
    const D3D12_INPUT_ELEMENT_DESC terrain_elements[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 2, DXGI_FORMAT_R32G32_FLOAT, 0, 44, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 3, DXGI_FORMAT_R32G32_FLOAT, 0, 52, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};
    if (terrain) pipeline.InputLayout = {terrain_elements, 6};
    auto &blend = pipeline.BlendState.RenderTarget[0];
    blend.BlendEnable = source != 1 || destination != 0;
    blend.SrcBlend = factors[source];
    blend.DestBlend = factors[destination];
    blend.SrcBlendAlpha = alpha_factors[source];
    blend.DestBlendAlpha = alpha_factors[destination];
    blend.BlendOp = blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blend.LogicOp = D3D12_LOGIC_OP_NOOP;
    blend.RenderTargetWriteMask = material.color_write ? static_cast<UINT8>(write_mask) : 0;
    pipeline.SampleMask = std::numeric_limits<UINT>::max();
    pipeline.RasterizerState.FillMode = material.wireframe ? D3D12_FILL_MODE_WIREFRAME : D3D12_FILL_MODE_SOLID;
    pipeline.RasterizerState.CullMode = cull == 0 ? D3D12_CULL_MODE_NONE : D3D12_CULL_MODE_BACK;
    pipeline.RasterizerState.FrontCounterClockwise = cull == 1;
    pipeline.RasterizerState.DepthBias = material.depth_bias;
    pipeline.RasterizerState.DepthClipEnable = TRUE;
    pipeline.DepthStencilState.DepthEnable = material.depth_test != RenderBackendDepthTest::Disabled;
    pipeline.DepthStencilState.DepthWriteMask = material.depth_write ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
    pipeline.DepthStencilState.DepthFunc = depth < 8 ? static_cast<D3D12_COMPARISON_FUNC>(depth + 1) : D3D12_COMPARISON_FUNC_ALWAYS;
    pipeline.DepthStencilState.StencilEnable = material.stencil.enabled;
    pipeline.DepthStencilState.StencilReadMask = static_cast<UINT8>(material.stencil.read_mask);
    pipeline.DepthStencilState.StencilWriteMask = static_cast<UINT8>(material.stencil.write_mask);
    auto stencil_face = [](const RenderBackendStencilFace &face) {
        return D3D12_DEPTH_STENCILOP_DESC{
            static_cast<D3D12_STENCIL_OP>(static_cast<unsigned int>(face.stencil_fail)+1),
            static_cast<D3D12_STENCIL_OP>(static_cast<unsigned int>(face.depth_fail)+1),
            static_cast<D3D12_STENCIL_OP>(static_cast<unsigned int>(face.pass)+1),
            static_cast<D3D12_COMPARISON_FUNC>(static_cast<unsigned int>(face.comparison)+1)};
    };
    pipeline.DepthStencilState.FrontFace = stencil_face(material.stencil.front);
    pipeline.DepthStencilState.BackFace = stencil_face(material.stencil.back);
    pipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipeline.NumRenderTargets = 1;
    pipeline.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pipeline.DSVFormat = color_only ? DXGI_FORMAT_UNKNOWN : DXGI_FORMAT_D24_UNORM_S8_UINT;
    pipeline.SampleDesc.Count = 1;
    m_material_pipelines.reserve(m_material_pipelines.size() + 1);
    ID3D12PipelineState *created = nullptr;
    checkHresult("CreateGraphicsPipelineState(W3D material)",
        m_device->CreateGraphicsPipelineState(&pipeline, IID_PPV_ARGS(&created)));
    m_material_pipelines.push_back({key, material.depth_bias, created});
    return created;
}

bool D3D12Backend::Draw_Indexed_Material_Triangles(
    const RenderBackendTexturedVertex *vertices, unsigned int vertex_count,
    const unsigned short *indices, unsigned int index_count,
    RenderBackendTextureHandle texture, const RenderBackendMaterialState &material,
    RenderBackendTextureHandle secondary_texture)
{
    if (!validMaterialDraw(texture, material, secondary_texture)) return false;
    try
    {
        return drawDynamicGeometry(vertices, vertex_count, sizeof(RenderBackendTexturedVertex),
            indices, index_count, materialPipeline(material, texture.Is_Valid()), material.screen_space, texture, &material, secondary_texture);
    }
    catch (...) { return false; }
}

bool D3D12Backend::validMaterialDraw(RenderBackendTextureHandle texture, const RenderBackendMaterialState &material,
    RenderBackendTextureHandle secondary_texture) const
{
    auto valid_texture = [&](RenderBackendTextureHandle handle) {
        return Is_Texture_Valid(handle) &&
            !(handle.slot == m_selected_texture.slot && handle.generation == m_selected_texture.generation);
    };
    if ((secondary_texture.slot != 0 || secondary_texture.generation != 0) &&
        (!material.secondary_rgb_modulate || !valid_texture(secondary_texture))) return false;
    if (material.secondary_rgb_modulate &&
        (!valid_texture(texture) || !valid_texture(secondary_texture) || !validMaterialSampler(material.secondary_sampler))) return false;
    if (material.monochrome && !valid_texture(texture)) return false;
    if (!m_scene_open || !validMaterialState(material) ||
        ((texture.slot != 0 || texture.generation != 0) && !texture.Is_Valid()) ||
        (!activeTargetHasDepth() && (material.depth_test != RenderBackendDepthTest::Disabled || material.stencil.enabled)) ||
        (texture.Is_Valid() && (!Is_Texture_Valid(texture) ||
            (texture.slot == m_selected_texture.slot && texture.generation == m_selected_texture.generation))))
        return false;
    return true;
}

bool D3D12Backend::Draw_Indexed_Terrain_Triangles(
    const RenderBackendTerrainVertex *vertices, unsigned int vertex_count,
    const unsigned short *indices, unsigned int index_count,
    RenderBackendTextureHandle base_texture, const RenderBackendMaterialState &material,
    const RenderBackendTerrainState &terrain)
{
    if (!validTerrainDraw(base_texture, material, terrain)) return false;
    try
    {
        return drawDynamicGeometry(vertices, vertex_count, sizeof(RenderBackendTerrainVertex), indices, index_count,
            materialPipeline(material, base_texture.Is_Valid(), true), material.screen_space, base_texture, &material, {}, &terrain);
    }
    catch (...) { return false; }
}

bool D3D12Backend::validTerrainDraw(RenderBackendTextureHandle base_texture,
    const RenderBackendMaterialState &material, const RenderBackendTerrainState &terrain) const
{
    auto valid_texture = [&](RenderBackendTextureHandle texture) {
        return Is_Texture_Valid(texture) &&
            !(texture.slot == m_selected_texture.slot && texture.generation == m_selected_texture.generation);
    };
    auto valid_layer = [&](RenderBackendTextureHandle texture, const RenderBackendSamplerState &sampler) {
        return validMaterialSampler(sampler) &&
            ((texture.slot == 0 && texture.generation == 0) || valid_texture(texture));
    };
    const bool textured = base_texture.Is_Valid();
    if (terrain.blend_secondary_by_vertex_alpha && (!textured || !terrain.shroud_texture.Is_Valid() ||
        terrain.project_base_to_shroud || terrain.diffuse_after_layers ||
        material.texture_combine != RenderBackendTextureCombine::Modulate)) return false;
    if ((textured && material.texture_combine != RenderBackendTextureCombine::Replace &&
         material.texture_combine != RenderBackendTextureCombine::Modulate) ||
        (terrain.project_base_to_shroud && (!terrain.project_world_coordinates || !textured ||
            material.texture_combine != RenderBackendTextureCombine::Replace ||
            terrain.shroud_texture.Is_Valid() || terrain.cloud_texture.Is_Valid() || terrain.noise_texture.Is_Valid())))
        return false;
    if (!m_scene_open || !validMaterialState(material) || material.secondary_rgb_modulate || material.monochrome ||
        ((base_texture.slot != 0 || base_texture.generation != 0) && !valid_texture(base_texture)) ||
        !valid_layer(terrain.shroud_texture, terrain.shroud_sampler) ||
        !valid_layer(terrain.cloud_texture, terrain.cloud_sampler) ||
        !valid_layer(terrain.noise_texture, terrain.noise_sampler) ||
        (!textured && (terrain.shroud_texture.Is_Valid() || terrain.cloud_texture.Is_Valid() || terrain.noise_texture.Is_Valid())) ||
        (!activeTargetHasDepth() && (material.depth_test != RenderBackendDepthTest::Disabled || material.stencil.enabled)))
        return false;
    return true;
}

RenderBackendGeometryHandle D3D12Backend::Create_Static_Indexed_Terrain_Geometry(
    const RenderBackendTerrainVertex *vertices, unsigned int vertex_count,
    const unsigned short *indices, unsigned int index_count)
{
    return createStaticGeometry(vertices, vertex_count, sizeof(RenderBackendTerrainVertex), indices, index_count, true);
}

bool D3D12Backend::Is_Geometry_Valid(RenderBackendGeometryHandle handle) const
{
    if (!handle.Is_Valid() || handle.slot > m_static_geometry.size()) return false;
    const auto &geometry = m_static_geometry[handle.slot - 1];
    return geometry.occupied && geometry.release_frame == FrameCount && geometry.generation == handle.generation &&
        geometry.vertex_buffer != nullptr && geometry.index_buffer != nullptr;
}

bool D3D12Backend::Draw_Static_Indexed_Terrain_Geometry(
    RenderBackendGeometryHandle handle, RenderBackendTextureHandle base_texture,
    const RenderBackendMaterialState &material, const RenderBackendTerrainState &terrain)
{
    if (!Is_Geometry_Valid(handle) || !validTerrainDraw(base_texture, material, terrain)) return false;
    return drawStaticMaterialGeometry(handle, base_texture, material, &terrain);
}

bool D3D12Backend::Draw_Static_Indexed_Material_Geometry(
    RenderBackendGeometryHandle handle, RenderBackendTextureHandle texture,
    const RenderBackendMaterialState &material)
{
    if (!Is_Geometry_Valid(handle) || material.secondary_rgb_modulate || !validMaterialDraw(texture, material)) return false;
    return drawStaticMaterialGeometry(handle, texture, material, nullptr);
}

bool D3D12Backend::drawStaticMaterialGeometry(
    RenderBackendGeometryHandle handle, RenderBackendTextureHandle texture,
    const RenderBackendMaterialState &material, const RenderBackendTerrainState *terrain)
{
    const auto &geometry = m_static_geometry[handle.slot - 1];
    if (geometry.vertex_stride != (terrain ? sizeof(RenderBackendTerrainVertex) : sizeof(RenderBackendTexturedVertex)) ||
        !geometry.textured) return false;
    try
    {
        D3D12_VERTEX_BUFFER_VIEW vertex_view{};
        vertex_view.BufferLocation = geometry.vertex_buffer->GetGPUVirtualAddress();
        vertex_view.SizeInBytes = geometry.vertex_bytes;
        vertex_view.StrideInBytes = geometry.vertex_stride;
        D3D12_INDEX_BUFFER_VIEW index_view{};
        index_view.BufferLocation = geometry.index_buffer->GetGPUVirtualAddress();
        index_view.SizeInBytes = geometry.index_bytes;
        index_view.Format = DXGI_FORMAT_R16_UINT;
        bindDrawState(materialPipeline(material, texture.Is_Valid(), terrain != nullptr), material.screen_space,
            texture, &material, {}, terrain);
        issueGeometryDraw(vertex_view, index_view, geometry.index_count,
            geometry.vertex_bytes / geometry.vertex_stride, &material);
        return true;
    }
    catch (...) { return false; }
}

bool D3D12Backend::Draw_Indexed_Decal_Triangles(
    const RenderBackendTexturedVertex *vertices, unsigned int vertex_count,
    const unsigned short *indices, unsigned int index_count,
    RenderBackendTextureHandle texture, RenderBackendDecalBlendMode blend_mode)
{
    const auto mode = static_cast<unsigned int>(blend_mode);
    if (!activeTargetHasDepth() || !Is_Texture_Valid(texture) || mode >= 3) return false;
    RenderBackendMaterialState material;
    material.depth_write = false;
    material.clamp_texture = true;
    material.source_blend = mode == 0 ? RenderBackendBlendFactor::Zero :
        mode == 1 ? RenderBackendBlendFactor::SourceAlpha : RenderBackendBlendFactor::One;
    material.destination_blend = mode == 0 ? RenderBackendBlendFactor::SourceColor :
        mode == 1 ? RenderBackendBlendFactor::InverseSourceAlpha : RenderBackendBlendFactor::One;
    return Draw_Indexed_Material_Triangles(vertices, vertex_count, indices, index_count, texture, material);
}

void D3D12Backend::Release_Texture(RenderBackendTextureHandle texture_handle)
{
    if (!texture_handle.Is_Valid())
    {
        return;
    }
    const std::size_t slot = static_cast<std::size_t>(texture_handle.slot - 1);
    if (slot >= m_textures.size() || !m_textures[slot].occupied ||
        m_textures[slot].generation != texture_handle.generation || m_textures[slot].release_frame != FrameCount)
    {
        return;
    }
    if (m_scene_open)
    {
        // Keep the resource and its occupied descriptor slot alive until every
        // command recorded in this frame has passed its fence.
        m_textures[slot].release_frame = m_frame_index;
        return;
    }
    if (texture_handle.slot == m_selected_texture.slot && texture_handle.generation == m_selected_texture.generation)
        Set_Render_Texture(RenderBackendTextureHandle{});
    try
    {
        waitForGpu();
    }
    catch (...)
    {
        return;
    }
    releaseTexture(m_textures[slot]);
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
    const bool offscreen = m_selected_texture.Is_Valid();
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = activeColorTarget();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = offscreen ? D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_COPY_SOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    m_command_list->ResourceBarrier(1, &barrier);

    if (!offscreen)
    {
        D3D12_RESOURCE_BARRIER capture_barrier{};
        capture_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        capture_barrier.Transition.pResource = m_output_capture;
        capture_barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
        capture_barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
        capture_barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        m_command_list->ResourceBarrier(1, &capture_barrier);
        m_command_list->CopyResource(m_output_capture, m_render_targets[m_frame_index]);
        capture_barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        capture_barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
        m_command_list->ResourceBarrier(1, &capture_barrier);
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        m_command_list->ResourceBarrier(1, &barrier);

    }

    checkHresult("ID3D12GraphicsCommandList::Close", m_command_list->Close());
    ID3D12CommandList *lists[] = {m_command_list};
    m_command_queue->ExecuteCommandLists(1, lists);

    const std::uint32_t submitted_frame = m_frame_index;
    const std::uint64_t signal_value = m_next_fence_value++;
    checkHresult("ID3D12CommandQueue::Signal", m_command_queue->Signal(m_fence, signal_value));
    m_frame_fence_values[submitted_frame] = signal_value;

    m_scene_open = false;
    if (offscreen && m_textures[m_selected_texture.slot - 1].release_frame != FrameCount)
        Set_Render_Texture({});
    if (!offscreen)
    {
        m_present_pending = true;
        m_capture_available = true;
    }
    if (present && !offscreen)
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

    checkHresult("IDXGISwapChain3::Present", m_swap_chain->Present(m_sync_interval, 0));
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
    for (auto *heap : m_frame_retired_texture_heaps[frame_index]) heap->Release();
    m_frame_retired_texture_heaps[frame_index].clear();
    for (auto &texture : m_textures)
        if (texture.occupied && texture.release_frame == frame_index) releaseTexture(texture);
    for (auto &geometry : m_static_geometry)
        if (geometry.occupied && geometry.release_frame == frame_index) releaseStaticGeometry(geometry);
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
    geometry.release_frame = FrameCount;
}

void D3D12Backend::releaseTexture(TextureResource &texture) noexcept
{
    releaseCom(texture.rtv_heap);
    releaseCom(texture.texture);
    texture.width = 0;
    texture.height = 0;
    texture.occupied = false;
    texture.release_frame = FrameCount;
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
    for (TextureResource &texture : m_textures)
    {
        releaseTexture(texture);
    }
    m_textures.clear();
    releaseCom(m_textured_color_only_pipeline);
    releaseCom(m_color_only_pipeline);
    releaseCom(m_color_only_alpha_pipeline);
    releaseCom(m_color_only_additive_pipeline);
    for (auto &entry : m_material_pipelines) releaseCom(entry.pipeline);
    m_material_pipelines.clear();
    releaseCom(m_terrain_texture_shader);
    releaseCom(m_terrain_color_shader);
    releaseCom(m_terrain_vertex_shader);
    releaseCom(m_material_texture_shader);
    releaseCom(m_material_color_shader);
    releaseCom(m_material_vertex_shader);
    releaseCom(m_primitive_vertex_shader);
    releaseCom(m_primitive_color_shader);
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
    releaseCom(m_output_capture);
    releaseCom(m_fence);
    releaseCom(m_command_list);
    for (auto &allocator : m_command_allocators)
    {
        releaseCom(allocator);
    }
    releaseCom(m_texture_srv_heap);
    releaseCom(m_material_sampler_heap);
    m_material_sampler_keys.clear();
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
