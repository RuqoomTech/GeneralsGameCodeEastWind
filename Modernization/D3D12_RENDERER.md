# Direct3D 12 Evolution Renderer

## Mission

Replace the DX8-era renderer in the existing game architecture with a clean x64 Direct3D 12 backend. Do not build a parallel engine or preserve a second Evolution application tree.

## Current implementation status

The initial D3D12 proof was Windows-verified in Step 05B on Intel UHD 770 and WARP. Step 05C/C2 folded that proven implementation into the pre-existing WW3D backend seam and is Windows-signed-off:

- `Core/Libraries/Source/WWVegas/WW3D2/Backend/D3D12Backend.*` owns the x64 device/frame lifecycle;
- `Create_Render_Backend()` selects D3D12 on the x64 path;
- DX8 backend/implementation sources are excluded from the x64 WW3D source graph;
- the normal Zero Hour executable selects `d3d12` + `dxgi` on x64 instead of `d3d8` + `d3dx8`;
- color and depth/stencil clears, viewport/scissor, present and fence synchronization are implemented through D3D12;
- a Windows smoke target exercises the production backend through the same factory used by WW3D;
- Step 05D adds the first indexed position/color primitive contract, root signature, PSO, shader compilation and upload-buffer lifetime tracking without introducing a second renderer abstraction;
- Step 05E moves the bootstrap shader out of C++ into the first canonical HLSL asset (`Shaders/PrimitiveColor.hlsl`) and stages the same asset for the Windows smoke binary and normal x64 game target;
- Step 05F adds the first persistent renderer-neutral geometry lifetime: create/draw/release handles backed by D3D12 default-heap vertex/index buffers, uploaded through explicit copy commands and state transitions;
- the temporary standalone `Evolution/` runtime tree and shell preset are removed.

The full x64 game is not yet enabled because many legacy render callers still invoke `DX8Wrapper` directly. Those call sites are now the renderer migration backlog; they must move behind D3D12-capable interfaces rather than being hidden behind a permanent DX8 emulation layer.

## Migration rule

Prefer the smallest existing abstraction that matches the real responsibility:

- frame/device lifecycle -> `IRenderBackend`;
- GPU buffers/textures/descriptors -> renderer resource abstractions;
- draw/material state -> renderer-neutral draw/pipeline descriptions;
- asset parsing -> CPU-side W3D/W3X structures, never D3D12 objects.

Do not mechanically add hundreds of DX8-style methods to `IRenderBackend`. Where DX8's fixed-function/state-machine model is the wrong abstraction, replace the caller with a higher-level D3D12-oriented concept.

## Target architecture

```text
Simulation / Game State
          |
          v
     Client View State
          |
          v
      RenderWorld
          |
          v
      RenderQueue
     /    |      \
 meshes terrain effects
     \    |      /
          v
 renderer-neutral assets/materials/passes
          |
          v
      D3D12 Backend
   /       |        \
resources PSOs   command submission
          |
          v
          GPU
```

## Immediate migration slices

1. **device/frame backend — done / Windows signed off:** D3D12 device, swap chain, render/depth targets, viewport, clear, present, fences;
2. **draw foundation — done / Windows signed off:** root signature, PSO and indexed triangle draw through `IRenderBackend`;
3. **shader asset foundation — locally done / Windows verification pending:** canonical HLSL assets staged beside the x64 executable; translate the existing D3D8-era terrain/filter/tree/water assembly only when each real rendering path is migrated;
4. **buffer foundation — active:** persistent/default-heap indexed position/color geometry is now available beside the transient path;
5. **texture binding:** establish D3D12 texture upload, SRV descriptor and sampler ownership before translating the legacy texture shaders;
6. **complete legacy shader-backed caller:** translate one real `.nvp/.nvv` behavior together with its texture/constants/state and route its WW3D caller off `DX8Wrapper`;
7. **representative W3D rigid mesh:** render existing W3D geometry through the D3D12 path;
8. **W3X rigid mesh:** feed the same renderer-neutral mesh path from W3X;
9. materials/textures;
10. camera/depth completeness;
11. skinned mesh/animation;
12. terrain, shadows, particles/effects and full scene coverage.

## Non-goals

- no D3D9 or D3D11 intermediate;
- no permanent DX8 compatibility renderer in the x64 runtime;
- no second Evolution executable/process architecture;
- no simulation changes driven by render timing;
- no advanced GPU feature maximalism before correctness and profiling.

## Later decisions

Ray tracing, mesh shaders, VRS, DirectStorage, GPU-driven rendering and broad bindless designs remain optional future evaluations after the real game is stable on the basic D3D12 renderer.
