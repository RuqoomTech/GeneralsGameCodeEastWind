# Direct3D 12 Evolution Renderer

## Mission

Replace the DX8-era renderer in the existing game architecture with a clean x64 Direct3D 12 backend. Do not build a parallel engine or preserve a second Evolution application tree.

## Current implementation status

The initial D3D12 proof was Windows-verified in Step 05B on Intel UHD 770 and WARP. Step 05C folds that proven implementation into the pre-existing WW3D backend seam:

- `Core/Libraries/Source/WWVegas/WW3D2/Backend/D3D12Backend.*` owns the x64 device/frame lifecycle;
- `Create_Render_Backend()` selects D3D12 on the x64 path;
- DX8 backend/implementation sources are excluded from the x64 WW3D source graph;
- the normal Zero Hour executable selects `d3d12` + `dxgi` on x64 instead of `d3d8` + `d3dx8`;
- color and depth/stencil clears, viewport/scissor, present and fence synchronization are implemented through D3D12;
- a Windows smoke target exercises the production backend through the same factory used by WW3D;
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

1. **device/frame backend — active:** D3D12 device, swap chain, render/depth targets, viewport, clear, present, fences;
2. **draw foundation:** root signature, shader compilation, PSO and triangle draw;
3. **buffer foundation:** static vertex/index buffers and upload path;
4. **WW3D primitive migration:** move the first direct `DX8Wrapper` draw/state callers;
5. **representative W3D rigid mesh:** render existing W3D geometry through the D3D12 path;
6. **W3X rigid mesh:** feed the same renderer-neutral mesh path from W3X;
7. materials/textures;
8. camera/depth completeness;
9. skinned mesh/animation;
10. terrain, shadows, particles/effects and full scene coverage.

## Non-goals

- no D3D9 or D3D11 intermediate;
- no permanent DX8 compatibility renderer in the x64 runtime;
- no second Evolution executable/process architecture;
- no simulation changes driven by render timing;
- no advanced GPU feature maximalism before correctness and profiling.

## Later decisions

Ray tracing, mesh shaders, VRS, DirectStorage, GPU-driven rendering and broad bindless designs remain optional future evaluations after the real game is stable on the basic D3D12 renderer.
