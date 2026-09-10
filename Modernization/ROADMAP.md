# Modernization Roadmap

This roadmap is authoritative for the project unless superseded by a dated decision in `DECISIONS.md`.

## Phase 0 — Baseline and safety

### Step 00 — Repository modernization baseline — DONE

- authoritative ZIP identified;
- roadmap stored in repository;
- current renderer/build/asset state audited;
- D3D12-only Evolution direction recorded;
- W3X support added to the program;
- project-state/worklog rules established.

### Step 01 — Determinism / CRC / Xfer Characterization — DONE

The consolidated determinism guard now protects:

- CRC primitive vectors and incremental/carry behavior;
- integer and real game-logic RNG values plus seed-state CRC transitions;
- legacy float trunc/floor/ceil bit behavior without modern strict-aliasing UB;
- Xfer primitive Win32 bytes;
- representative `DamageInfoOutput` snapshot ordering;
- production `XferCRC` folding;
- Win32 replay/network ABI widths and packed packet offsets;
- network/replay enum anchors;
- a known `MSG_LOGIC_CRC` replay-record byte/CRC checkpoint.

The prior GCC strict-aliasing suppression is no longer required. No new determinism test module was added; coverage remains in `Core/Tests/DeterminismPrimitivesTest.cpp`.

Exit/sign-off gate: **PASSED on Windows MinGW-w64 i686 / GCC 16.2 + Ninja on 2026-09-10.** The focused `z_determinismcheck` target compiled and ran successfully and emitted the expected Step 01 success line. The target compiles only the production determinism/Xfer units it characterizes and no longer links the monolithic game archive. Any future compiler/ABI disagreement must be resolved explicitly rather than hidden by relaxing the test.

### Step 02 — Command-Line Build System Foundation — ACTIVE

Primary path:

```text
CMake + Ninja + MinGW-w64 GCC
```

Step 02A implemented the first coherent foundation slice:

- canonical `mingw32-release`, `mingw32-debug`, `mingw32-profile`, and `mingw32-tests` Ninja presets;
- compatibility aliases for the Step 01 `mingw-w64-i686*` names;
- focused `RTS_BUILD_TESTS_ONLY` graph and CTest integration for W3X A0/A1/A2 + Step 01 determinism;
- C/C++ warning separation for `-Wsuggest-override` and target-scoped MinGW compatibility/link settings;
- removal of deprecated direct FetchContent population in source-only ATL/zlib/LZHL paths;
- improved i686 compiler/binutils/WIDL discovery without PATH surgery.

Windows verification of the new presets and the real `z_generals` target is pending. Step 02 remains ACTIVE until concrete MinGW runtime blockers are resolved and the canonical workflow is signed off. Clang remains secondary work rather than a blocker for this slice.

## Phase 1 — Measure and stabilize the current x86 runtime

### Step 03 — HD Mod Performance Foundation

Add CPU/GPU timing where available, draw/geometry counters, memory/resource accounting, visibility data, asset hotspot reports, and repeatable benchmark captures.

### Step 04 — x86 Memory Survival

Improve memory diagnostics, Large Address Aware strategy where appropriate, resource accounting, graceful failure, and short-term HD-mod stability while x64 is prepared.

## Phase 2 — Modern asset foundation

### Step 05 — W3X Phase A: Parser / Import Foundation

**Pre-steps already complete:**

- **A0:** dependency-free `.w3d` / `.w3x` recognition plus conservative XML sniffing;
- **A1:** dependency-free XML document-envelope probe that extracts root name/namespace and recognizes the canonical SAGE `AssetDeclaration` envelope.
- **A2:** dependency-free discovery/classification of direct `AssetDeclaration` child elements, including namespace-aware recognition of the initial W3D element kinds while preserving unknown direct children;
- **A2R:** maintenance consolidation of A1/A2 into one public W3X document API and one shared implementation source, removing the two implementation-heavy parser headers before further feature growth.

Runtime routing, includes/references, child-content decoding, neutral import structures, and actual W3X asset creation remain Step 05 work.

- EA SAGE W3X format discovery and fixtures;
- XML parser component;
- format routing;
- neutral import structures;
- parser/validation tests;
- W3D remains untouched.

This step does not require D3D12 rendering yet.

### Step 06 — HD Texture Pipeline

- DDS/compression policy;
- mip quality;
- sRGB correctness;
- anisotropic filtering;
- texture diagnostics;
- material-role preparation;
- future streaming-compatible resource metadata.

### Step 07 — High-Poly Geometry Foundation

- explicit 16/32-bit index capability;
- large meshes;
- robust bounds/statistics;
- neutral geometry representation shared by W3D/W3X where practical;
- asset validation.

### Step 08 — Aggressive Instancing and Batching

Expand repeated-asset batching/instancing for units, buildings, props, vegetation, shadow/depth passes, and other suitable categories.

### Step 09 — Modern LOD and Visibility

Strengthen geometry LOD, shadow LOD, distance/material choices, frustum/occlusion strategy as appropriate, and profiler warnings for expensive assets.

## Phase 3 — Break the x86 ceiling

### Step 10 — x64 Evolution Runtime

Audit and port deliberately:

- pointers/handles;
- pools/allocators;
- serialization widths;
- replay/network layouts;
- file structures;
- Win32 assumptions;
- external libraries/tools.

The compatibility/reference x86 build remains available while this stabilizes.

## Phase 4 — Complete renderer separation

### Step 11 — Renderer Architecture Boundary

Build on the existing partial `IRenderBackend` seam, but introduce the higher-level concepts D3D12 actually needs:

- render-world extraction;
- render items/queues;
- resource descriptions;
- materials;
- passes;
- explicit submission/lifetime boundaries.

Do not expose D3D12 objects to gameplay systems.

## Phase 5 — Direct3D 12 Evolution Renderer

### Step 12 — D3D12 Foundation

- DXGI adapter/device;
- swap chain;
- direct queue;
- command allocators/lists;
- frame resources/fences;
- resize/present/device-loss diagnostics;
- debug layer in development builds.

Proof: clear -> triangle -> indexed mesh.

### Step 13 — D3D12 Resource System

- GPU/upload/readback resources;
- vertex/index buffers;
- textures;
- descriptor heaps;
- SRV/RTV/DSV management;
- barriers/state tracking;
- fence-safe destruction;
- memory accounting.

### Step 14 — Shader and Pipeline System

- DXC;
- Shader Model 6.x baseline appropriate to target hardware;
- root signatures;
- PSOs/cache;
- shader permutation diagnostics.

### Step 15 — W3D + W3X Mesh Rendering

Bring representative legacy W3D and imported W3X geometry into D3D12:

- rigid meshes first;
- 16/32-bit indices;
- multiple submeshes/materials;
- skinned/animated models after rigid correctness;
- LOD integration.

### Step 16 — Terrain Rendering

Port terrain/depth/material integration and establish large-scene correctness.

### Step 17 — Modern Materials / W3X Material Mapping

- compatibility shaders for legacy W3D;
- modern material model for Evolution assets;
- W3X material mapping/fallbacks;
- normal/specular/emissive and later PBR-like parameters where appropriate.

### Step 18 — Modern Shadows

Use scalable shadow maps/cascades and separate shadow LOD. Avoid high-poly legacy shadow-volume behavior as the high-end path.

### Step 19 — Particles and Effects

Port/redesign batching, transparency, particles, trails, distortions, and effect submission for D3D12.

### Step 20 — HDR and Post Processing

Tone mapping, bloom, color grading, anti-aliasing, optional AO, and other quality-scalable effects.

### Step 21 — Modern Lighting

Directional sun/environment lighting plus controlled local/effect lighting appropriate for an RTS.

### Step 22 — Texture Residency and Streaming

Configurable VRAM budgets, mip residency, async uploads, and large-content behavior suitable for the heavy mod.

## Phase 6 — Asset/tooling completion and advanced optimization

### Step 23 — W3X Completeness / Tooling

Complete the documented support matrix:

- skeletons/animations;
- collision/selection data;
- LOD/hierarchy edge cases;
- command-line validation/statistics;
- modder documentation;
- compatibility fixtures;
- Blender workflow validation.

### Step 24 — Advanced GPU Optimization

Only after profiling proves a need. Candidates include GPU culling/indirect submission, more bindless-style access, mesh shaders, VRS, or other D3D12 features.

Do not make advanced hardware features prerequisites without measured benefit.
