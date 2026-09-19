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
- Step 05G adds renderer-neutral RGBA8 texture handles, default-heap texture upload, shader-visible SRVs, a static sampler and textured indexed drawing;
- Step 05H1 adds the first real normal-game screen-space caller: untextured `Render2DClass` geometry routes through D3D12 depth-disabled opaque/alpha/additive PSOs;
- the temporary standalone `Evolution/` runtime tree and shell preset are removed.

Step 05H1 enables configuration of the normal x64 Zero Hour game target through `mingw64-game`. Many legacy render callers still invoke `DX8Wrapper` directly, so the executable is expected to expose additional compile/link blockers until those responsibilities migrate. Those call sites are the renderer migration backlog; they must move behind D3D12-capable interfaces rather than being hidden behind a permanent DX8 emulation layer.

### Step 05H1 real-caller bootstrap

The first normal-game draw responsibility has crossed the seam: untextured `Render2DClass` geometry used by `W3DDisplay` for lines, outlines, filled rectangles and rectangle clocks now converts its existing screen-space vertices/colors into `RenderBackendColorVertex` data and submits through `IRenderBackend::Draw_2D_Indexed_Triangles()`. D3D12 owns dedicated depth-disabled opaque, source-alpha and additive PSOs for this path. The old DX8 dynamic VB/IB implementation remains compiled only outside `RTS_EVOLUTION_X64`.

This intentionally does **not** treat `TextureClass` as a D3D8 texture on x64. Textured and grayscale `Render2D` batches remain the next coherent resource-lifetime migration, where real texture data must map to `RenderBackendTextureHandle` rather than a DX8 compatibility facade.

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
3. **shader asset foundation — done / Windows signed off:** canonical HLSL assets staged beside the x64 executable; translate the existing D3D8-era terrain/filter/tree/water assembly only when each real rendering path is migrated;
4. **buffer foundation — done / Windows signed off:** persistent/default-heap indexed position/color geometry is available beside the transient path;
5. **texture binding — done / Windows signed off:** default-heap RGBA8 upload, shader-visible SRV descriptors, static sampler ownership and persistent textured indexed drawing;
6. **first normal-game caller — active:** untextured `W3DDisplay`/`Render2D` primitives now cross `IRenderBackend`; next connect real `TextureClass` lifetime/data to renderer-neutral texture handles, then continue from actual `z_generals` blockers;
7. **complete legacy shader-backed caller / first shader translation:** translate one real `.nvp/.nvv` behavior together with its texture/constants/state and route its normal-game WW3D caller off `DX8Wrapper`;
8. **representative W3D rigid mesh:** render existing W3D geometry through the D3D12 path;
9. **W3X rigid mesh:** feed the same renderer-neutral mesh path from W3X;
10. materials/textures;
11. camera/depth completeness;
12. skinned mesh/animation;
13. terrain, shadows, particles/effects and full scene coverage.

## Non-goals

- no D3D9 or D3D11 intermediate;
- no permanent DX8 compatibility renderer in the x64 runtime;
- no second Evolution executable/process architecture;
- no simulation changes driven by render timing;
- no advanced GPU feature maximalism before correctness and profiling.

## Later decisions

Ray tracing, mesh shaders, VRS, DirectStorage, GPU-driven rendering and broad bindless designs remain optional future evaluations after the real game is stable on the basic D3D12 renderer.


## Step 05H1K — D3DX8 utility isolation in the active game graph

The normal x64 game build reached an unavailable `d3dx8core.h` include in the Zero Hour asset manager. H1K removes D3DX8 from the active Evolution utility/math surface without adding emulation: WWMath now covers point/sorting and shared Bezier transforms, existing CPU `BitmapHandler` routines cover missing-texture mip generation plus Evolution surface copy/scale conversion, and unused D3DX headers are removed from active game-device/client callers. Archival DX8-only implementation files may still reference D3DX8, while genuine shader/water/terrain/tree D3DX responsibilities remain queued for coherent D3D12 migration. This is compile-graph isolation, not a substitute for migrating real texture/material/draw ownership through `IRenderBackend`.
