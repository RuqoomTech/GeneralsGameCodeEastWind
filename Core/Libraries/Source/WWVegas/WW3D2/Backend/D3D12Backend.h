/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2026 TheSuperHackers
**
** This program is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
*/

#pragma once

#include "WW3D2/IRenderBackend.h"

#include <cstdint>
#include <vector>

struct IDXGIFactory4;
struct IDXGISwapChain3;
struct ID3D12CommandAllocator;
struct ID3D12CommandQueue;
struct ID3D12DescriptorHeap;
struct ID3D12Device;
struct ID3D12Fence;
struct ID3D12GraphicsCommandList;
struct ID3D12PipelineState;
struct ID3D12Resource;
struct ID3D12RootSignature;

// Direct3D 12 implementation of the existing WW3D render-backend seam.
// This is the only renderer backend supported by the x64 Evolution runtime.
class D3D12Backend final : public IRenderBackend
{
public:
    static D3D12Backend *Create(void *window, bool lite);

    D3D12Backend(const D3D12Backend &) = delete;
    D3D12Backend &operator=(const D3D12Backend &) = delete;
    ~D3D12Backend() override;

    void Set_Gamma(float gamma, float bright, float contrast, bool calibrate, bool uselimit) override;

    void Begin_Scene() override;
    void End_Scene(bool flip_frame) override;
    void Flip_To_Primary() override;
    void Clear(bool clear_color, bool clear_z_stencil,
               const Vector3 &color,
               float dest_alpha, float z, unsigned int stencil) override;
    void Set_Viewport(const RenderBackendViewport &viewport) override;
    void Invalidate_Cached_Render_States() override;
    bool Draw_Indexed_Triangles(
        const RenderBackendColorVertex *vertices,
        unsigned int vertex_count,
        const unsigned short *indices,
        unsigned int index_count) override;

    void Set_Ambient(const Vector3 &color) override;
    void Set_Light_Environment(LightEnvironmentClass *light_env) override;

private:
    static constexpr std::uint32_t FrameCount = 2;

    D3D12Backend() = default;

    void initialize(void *window);
    void createFactoryAndDevice();
    void createCommandObjects();
    void createSwapChain();
    void createRenderTargets();
    void createDepthStencil();
    void createSynchronizationObjects();
    void createPrimitivePipeline();
    void applyPendingClear();
    void submitScene(bool present);
    void presentPendingFrame();
    void waitForFrame(std::uint32_t frame_index);
    void releaseFrameUploads(std::uint32_t frame_index) noexcept;
    void waitForGpu();
    void releaseObjects() noexcept;

    void *m_window = nullptr;
    std::uint32_t m_width = 0;
    std::uint32_t m_height = 0;

    IDXGIFactory4 *m_factory = nullptr;
    ID3D12Device *m_device = nullptr;
    ID3D12CommandQueue *m_command_queue = nullptr;
    IDXGISwapChain3 *m_swap_chain = nullptr;
    ID3D12DescriptorHeap *m_rtv_heap = nullptr;
    ID3D12DescriptorHeap *m_dsv_heap = nullptr;
    ID3D12Resource *m_render_targets[FrameCount]{};
    ID3D12Resource *m_depth_stencil = nullptr;
    ID3D12CommandAllocator *m_command_allocators[FrameCount]{};
    ID3D12GraphicsCommandList *m_command_list = nullptr;
    ID3D12RootSignature *m_primitive_root_signature = nullptr;
    ID3D12PipelineState *m_primitive_pipeline = nullptr;
    ID3D12Fence *m_fence = nullptr;
    void *m_fence_event = nullptr;

    RenderBackendViewport m_viewport{};
    float m_clear_color[4]{0.0f, 0.0f, 0.0f, 1.0f};
    float m_clear_depth = 1.0f;
    unsigned int m_clear_stencil = 0;
    bool m_clear_color_pending = false;
    bool m_clear_depth_pending = false;
    bool m_scene_open = false;
    bool m_present_pending = false;

    std::uint32_t m_rtv_descriptor_size = 0;
    std::uint32_t m_frame_index = 0;
    std::uint64_t m_next_fence_value = 1;
    std::uint64_t m_frame_fence_values[FrameCount]{};
    std::vector<ID3D12Resource *> m_frame_uploads[FrameCount];
};
