# Direct3D 12 Evolution Renderer

## Mission

Build a clean x64 Direct3D 12 renderer for the Evolution runtime without carrying the entire old DX8 state-machine architecture forward.

## Current starting point

The source already has a partial `IRenderBackend` seam and a `DX8Backend` adapter. This is migration groundwork, not the final D3D12 design.

## Non-goals

- no D3D9 intermediate backend;
- no D3D11 intermediate backend;
- no immediate removal of the DX8 reference path;
- no feature maximalism before basic correctness;
- no simulation changes driven by renderer timing;
- no requirement for cutting-edge GPU features simply because D3D12 exposes them.

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

## D3D12 foundation

### Device/platform

- DXGI adapter selection;
- D3D12 device creation;
- capability query;
- debug layer in development builds;
- robust device-removed diagnostics.

### Frame model

- direct command queue;
- per-frame command allocator(s);
- graphics command list(s);
- swap chain/back buffers;
- fence values per in-flight frame;
- explicit synchronization only where required.

### Resource model

- GPU-local/default resources;
- upload staging strategy;
- descriptor management;
- explicit buffer/texture descriptions;
- resource-state/barrier tracking;
- fence-safe deferred destruction;
- memory accounting from the beginning.

### Shader model

- DXC;
- modern HLSL / DXIL;
- root signatures;
- PSO descriptors/cache;
- controlled shader permutations.

## Migration slices

1. device + clear/present;
2. triangle;
3. indexed textured mesh;
4. representative W3D rigid mesh;
5. camera/depth;
6. representative W3X rigid mesh;
7. multiple materials;
8. skinned mesh/animation;
9. terrain;
10. shadows;
11. particles/effects;
12. full scene coverage.

At each stage compare against deterministic/reference input where practical.

## Heavy-mod design

Organize work around:

- persistent GPU resources;
- sorted render queues;
- repeated-mesh instancing;
- low PSO/state churn;
- batched uploads;
- 32-bit indices for Evolution assets;
- separate shadow LOD;
- compressed HD textures;
- configurable resource budgets;
- asynchronous work only where measurements justify it.

## Advanced features are later decisions

Do not make these initial requirements:

- ray tracing;
- mesh shaders;
- work graphs;
- VRS;
- DirectStorage;
- fully GPU-driven rendering;
- bindless-everything.

Evaluate them only after the basic D3D12 renderer is correct, profiled, and handling the real mod workload.
