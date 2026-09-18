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

Exit/sign-off gate: **PASSED on Windows MinGW-w64 i686 / GCC 16.2 + Ninja on 2026-09-10.** The focused `check_determinism` target compiled and ran successfully and emitted the expected Step 01 success line. The target compiles only the production determinism/Xfer units it characterizes and no longer links the monolithic game archive. Any future compiler/ABI disagreement must be resolved explicitly rather than hidden by relaxing the test.

### Step 02 — Command-Line Build System Foundation — IMPLEMENTATION COMPLETE / USER ACCEPTED

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

Step 02B advances the real runtime configure/install path without broad source modernization:

- consolidated Generals/Zero Hour runtime installation behind a compiler-aware helper;
- removed GNU/MinGW generation failures caused by unconditional MSVC `$<TARGET_PDB_FILE:...>` install rules;
- preserved MSVC PDB installation and connected MinGW Release installs to the existing `.debug` sidecar workflow;
- moved full-runtime WIDL validation ahead of ReactOS ATL/runtime FetchContent population;
- added native-Windows preflight for the `oaidl.idl` / `ocidl.idl` imports actually used by the EABrowser IDLs.

The user subsequently reported that the Step 02B Windows workflow builds successfully (with expected legacy warnings). No Windows console transcript was supplied for archival, so this tree records user acceptance without claiming a new formally captured Windows test result. Step 03 was subsequently completed and the modernization program has advanced to the staged Step 04 x64 migration; warning cleanup remains opportunistic and must not hide diagnostics from project-owned code.

## Phase 1 — Measure and stabilize the current x86 runtime

### Step 03 — HD Mod Performance Foundation — DONE

Completed measurement foundation:

- versioned per-engine-update CSV capture;
- complete update/client/message/network/logic CPU phase timing;
- WW3D render CPU timing and draw/geometry/texture/resource counters;
- drawable total/visible/shrouded visibility proxy;
- Tracy plots from the same sample;
- dependency-free capture summary tool with percentile output;
- focused schema/regression coverage.

Legacy D3D8 GPU timestamp work is intentionally deferred; native GPU timing belongs to the D3D12 command-queue implementation rather than a temporary renderer path. Memory/resident accounting moves with the x64 migration.

### Step 04 — x64 Engine Migration — DONE

The earlier standalone “x86 memory survival” milestone is folded into the migration: x86 remains the compatibility/reference executable, but new effort now removes the x86 ceiling directly.

**Steps 04A–04F are complete and Windows verified. The supported Evolution modernization lane is x64-only:**

- x64-only MinGW-w64 toolchain discovery after 04F retirement of the former i686 oracle lane;
- `mingw64-tests` x64 readiness preset;
- explicit `RTS_BUILD_EVOLUTION_X64` guard;
- 64-bit focused graph decoupled from legacy D3D8/input/audio link requirements;
- architecture-width test proving pointer widening does not widen fixed wire IDs;
- **04B:** explicit command-packet byte capacity independent of `GameMessage` runtime layout, fixed command enum width, first pointer/userdata truncation fixes, and focused wire/pointer regression guards;
- **04C:** native-width allocator/runtime substrate, pointer/userdata cleanup, and x64-default dependency bootstrap;
- **04D:** centralized deterministic FP policy plus renderer-free production RNG/CRC headless timeline; real Windows i686 and x64 timelines now match at all eight checkpoints through frame 12000;
- **04D3:** selective upstream correctness/runtime alignment, reducing material shared-file divergence from 93 to 63 while retaining EastWind x64/determinism authority.

### Step 04D3 — Upstream Alignment / Divergence Reduction — IMPLEMENTED

Before defining the Evolution protocol, selectively synchronize correctness/runtime work from the supplied upstream snapshot while keeping EastWind as the architecture authority. Imported/adapted areas include Dozer/Worker disabled-task and Xfer-version fixes, production cancellation, corrected neutron-radius behavior behind compatibility gates, GameMemory robustness compatible with the x64 allocator, runtime Bink/Miles loaders, and glyph-buffer safety.

The sync deliberately does **not** replace EastWind's x64 allocator/pointer work, strict-aliasing-safe float helpers, deterministic/wire-width guards, build modernization, or renderer direction. The larger upstream Miles lifecycle refactor remains deferred until its callback/userdata seams are converted to native-width-safe types. Material shared-file divergence against the supplied upstream snapshot fell from 93 to 63 files.

Current/remaining staged slices:

- **04E1 done:** explicit little-endian command codec, command-batch/network v1 framing, replay v1 framing, golden bytes, malformed-input rejection, and migration of live `NetPacketGameCommandData` payloads to the shared codec;
- **04E2 done / Windows signed off:** staged Win64 routed EVN1 gameplay datagrams plus EVR1 Recorder sidecars, retaining legacy ACK/control/session traffic and legacy replay fallback;
- **04E3 done / Windows signed off:** deterministic in-process x64 two-endpoint session harness using production routed EVN1 helpers; real Windows MinGW-w64 GCC 16.2 passed 23/23 plus Step04D/04E1/04E2/04E3 explicit gates;
- **04E4 done / Windows signed off:** exact representative EVN1/EVR1 full-session transcript, deterministic network fault reconstruction, CRC checkpoint equality, version/corruption/truncation rejection, monotonic replay-frame validation, and legacy replay fallback/fail-closed compatibility policy;
- **04F done / Windows signed off:** the active i686 MinGW presets/toolchain/bootstrap/oracle comparator are removed, x64 native pointers are required on the modernization lane, WIDL targets Win64, and the x64 platform policy gate is green. Step 04 is closed.

See `History/STEP_04_X64_MIGRATION.md`.

## Phase 2 — Modern asset foundation

### Step 05 — W3X Phase A: Parser / Import Foundation — ACTIVE

**Pre-steps already complete:**

- **A0:** dependency-free `.w3d` / `.w3x` recognition plus conservative XML sniffing;
- **A1:** dependency-free XML document-envelope probe that extracts root name/namespace and recognizes the canonical SAGE `AssetDeclaration` envelope.
- **A2:** dependency-free discovery/classification of direct `AssetDeclaration` child elements, including namespace-aware recognition of the initial W3D element kinds while preserving unknown direct children;
- **A2R:** maintenance consolidation of A1/A2 into one public W3X document API and one shared implementation source, removing the two implementation-heavy parser headers before further feature growth.

Runtime routing, includes/references, child-content decoding, neutral import structures, and actual W3X asset creation remain Step 05 work.

Current Step 05 slices:

- **05A — developer baseline cleanup (DONE / Windows signed off):** normalized permanent test/build names, consolidated policy-test boilerplate, removed stale wrappers, separated historical milestone documents, and simplified current documentation;
- **05B — D3D12 proof shell (DONE / Windows signed off):** validated Win32/DXGI/D3D12 device, queue/list, flip-model swap chain, clear/present and fences on hardware and WARP; the temporary standalone process is not retained as architecture;
- **05C — in-place D3D12 backend integration (DONE / Windows signed off):** the proven device/frame implementation now lives in the existing WW3D backend seam and the standalone `Evolution/` tree is removed;
- **05D — indexed primitive foundation (DONE / Windows signed off):** first renderer-neutral indexed draw contract, D3D12 root signature/PSO, shader compilation and fence-safe transient upload lifetime through the production backend;
- **05E — shader asset foundation (DONE / Windows signed off):** canonical HLSL staging/loading for D3D12, with legacy `.nvp/.nvv` sources retained only as behavior references until their actual terrain/filter/tree/water paths migrate;
- **05F — persistent indexed geometry (DONE / Windows signed off):** renderer-neutral geometry handles own persistent D3D12 default-heap vertex/index resources uploaded through explicit copy transitions;
- **05G — texture/SRV/sampler foundation:** default-heap RGBA8 upload, shader-visible descriptors, explicit sampler binding and textured indexed geometry;
- **05H+ — visible real-caller migration:** move complete `DX8Wrapper` shader/state/draw callers to D3D12-capable abstractions, beginning with one normal-game path that can be visually verified, then continue until the normal Zero Hour x64 executable links/renders without the DX8 renderer;
- **05W1 — XML parser component (after D3D12 indexed-mesh foundation):** integrate a real XML parser behind the existing shared W3X document seam;
- **05W2 — neutral import model + rigid mesh:** decode the first representative mesh fixture into renderer-neutral data;
- **05W3 — format routing/validation:** route W3D to the existing loader and W3X to the importer with useful diagnostics and no W3D behavior regression.

The first D3D12 foundation slice was intentionally pulled forward from Step 12 so W3D/W3X work can be exercised in the real future x64 process. This does not change the renderer-isolation rule: asset parsing/import remains CPU-side and renderer-neutral.

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

## Phase 3 — x64 stabilization / modern client convergence

### Step 10 — x64 Runtime Stabilization

Step 10 no longer begins the x64 port; that work moved forward into Step 04 by project decision on 2026-09-11. Step 10 is reserved for stabilization after the intervening asset/geometry/visibility work has exercised the 64-bit engine:

- sustained heavy-mod memory/load testing;
- replay/network compatibility soak;
- allocator/resource lifetime diagnostics;
- removal of temporary x64 migration guards;
- readiness gate before the renderer boundary becomes the primary runtime path.

Step 10 is an x64-only stabilization gate. Historical Win32 project material may remain for provenance, but no supported i686 Evolution runtime/oracle lane is restored.

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

The device/clear/present runtime shell was pulled forward into Step 05B to establish the real Evolution executable early. Step 12 completes and hardens that foundation rather than creating a second renderer process.

Already started in 05B:

- DXGI hardware-adapter/device selection;
- flip-model swap chain;
- direct queue;
- command allocators/list;
- back-buffer RTVs;
- basic frame fences;
- clear/present;
- optional debug layer.

Step 12 remaining foundation work includes resize handling, stronger device-removed diagnostics, frame-resource ownership, presentation policy and integration with the renderer architecture boundary.

Proof progression: clear (05B) -> triangle -> indexed mesh.

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
