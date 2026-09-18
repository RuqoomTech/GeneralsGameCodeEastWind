# Current Source State

This document records verified source facts through the Step 05F persistent-geometry candidate on 2026-09-18. Step 05D is Windows-signed-off: the production D3D12 backend passed 27/27 on Windows, submitted indexed geometry through `IRenderBackend`, and passed every deterministic/Evolution/x64 gate. Step 05E moves the bootstrap shader into a canonical HLSL asset staged beside the executable and establishes the migration convention for the existing D3D8-era shader sources.

## Build system

- Top-level project uses CMake 3.25+; modern project code uses C++20 through `core_config` while the standalone W3X characterization targets remain C++98-compatible.
- VC6 and existing MSVC preset families remain available as historical/comparison paths.
- The supported MinGW/Ninja modernization lane is x86_64-only and uses `mingw64-tests`; Step 04F removes the former `mingw32-*` and `mingw-w64-i686*` presets rather than preserving dormant aliases.
- `Core/Tests/CMakeLists.txt` integrates W3X, determinism, ABI, runtime and Evolution protocol/session coverage using permanent subsystem-oriented test names.
- C++-only `-Wsuggest-override` and MinGW compatibility/link settings are target-scoped through `core_config` rather than globally leaking into C/vendored targets.
- Direct `FetchContent_Populate()` use in the ReactOS ATL, legacy zlib, and LZHL source-only paths has been removed.
- The MinGW toolchain validates the `x86_64-w64-mingw32` triplet, supports `RTS_MINGW_ROOT`, and shares its resolved bin path with WIDL/debug-strip discovery. WIDL supports explicit root/include overrides and is not required for the focused test graph.
- Legacy full-game MinGW runtime configuration requires WIDL before populating runtime FetchContent dependencies; the focused `mingw64-tests` graph intentionally avoids those unrelated full-runtime dependencies.
- Generals and Zero Hour install rules use `rts_install_runtime_target()` instead of repeating MSVC-only PDB generator expressions. MSVC keeps optional PDB installation; MinGW Release installs the `.debug` sidecar emitted by the existing strip workflow.
- MinGW toolchain discovery is x86_64-only through `mingw-w64-common.cmake` plus the canonical x86_64 wrapper. `mingw64-tests` configures the focused regression graph, including the Windows x64 D3D12 backend smoke test.
- The monolithic game runtime remains blocked from the x64 MinGW lane until direct renderer/platform dependencies are migrated explicitly. There is no parallel Evolution application tree; the normal game executable remains the convergence target.
- Step 04, Step 05A, Step 05C2, and Step 05D are Windows-signed-off. The temporary Step 05B proof shell was Windows-verified and then removed. The host-portable graph remains 26 tests; Windows x64 adds the real `d3d12_backend_smoke` GPU test. Step 05E keeps that test path but stages/loads the canonical HLSL shader asset instead of compiling C++-embedded shader text. Step 05F extends the same backend with renderer-neutral persistent geometry handles backed by D3D12 default-heap vertex/index resources; Windows GPU verification is pending.

## Performance telemetry

- Step 03 is complete. `rts/profile.h` exposes one consolidated `PerformanceTelemetry` seam; no second profiler hierarchy was introduced.
- The historical `mingw32-profile` capture preset is retired by Step 04F. Telemetry remains compile-time gated and observational; a new real gameplay capture waits for the full Win64 profile/runtime target.
- CSV schema v2 emits one observational row per `GameEngine::update()` with update/client/message/network/logic CPU phases plus the primary WW3D render CPU bracket.
- The same row carries draw/geometry/texture/resource counters and a drawable total/visible/shrouded visibility proxy.
- `scripts/perf-summary.py` reports timing percentiles and mean/max resource counters using only the Python standard library.
- Tracy plots consume the same sample.
- GPU timestamp work is deferred to D3D12 rather than adding temporary D3D8 query infrastructure.
- Telemetry values are never consumed by simulation, frame pacing, CRC, replay, network, or Xfer behavior.

## x64 migration

- Step 04 is complete and Windows verified. The final 04F baseline passed 25/25 on real Windows MinGW-w64 GCC 16.2 plus every explicit deterministic/Evolution/x64-platform gate.
- `cmake/toolchains/mingw-w64-common.cmake` now describes only x86_64. The former i686 wrapper/preset/bootstrap path is retired by 04F.
- `mingw64-tests` enables `RTS_BUILD_EVOLUTION_X64` and `RTS_BUILD_HEADLESS_CORE`; it is the canonical x86_64 focused preset and additionally builds the real D3D12 backend smoke executable on Windows.
- Focused x64 readiness does not link legacy D3D8/DirectInput/DirectSound and does not populate ReactOS ATL when there is no consumer.
- `architecture_abi` enforces fixed-width engine/wire primitives and IDs while permitting native pointers/`uintptr_t` to widen.
- Step 04D centralized `setFPMode()` in Core and established the x64 round-to-nearest deterministic timeline; the historical i686 x87 result remains recorded as provenance, not as an active build lane.
- Step 04D3 selectively aligns with the supplied upstream snapshot: Dozer/Worker Xfer/task fixes, production cancellation, neutron radius behavior, adapted GameMemory robustness, runtime Bink/Miles loading, and glyph-buffer safety. Material shared-file divergence fell from 93 to 63 without replacing EastWind x64/determinism infrastructure.
- The full x64 Zero Hour executable remains gated while direct `DX8Wrapper` callers are migrated. Step 05C removes the temporary standalone process and makes the existing WW3D backend seam the only D3D12 migration path; the normal game executable is the eventual x64 runtime target.
- The i686 modernization/oracle lane is retired. Retail x86 multiplayer interoperability is not required; supported Evolution development proceeds on x64.

## Renderer

The x64 renderer is now migrating in-place behind WW3D `IRenderBackend`. The production D3D12 backend owns DXGI adapter/device creation, command submission, flip-model swap chain, render/depth targets, viewport/scissor, clears, present and fences. Step 05D adds an immutable root signature/PSO, shader compilation, transient upload-buffer lifetime tracking, and `DrawIndexedInstanced` for the first renderer-neutral indexed position/color primitive. Step 05E externalizes that shader into canonical HLSL. Step 05F adds persistent default-heap vertex/index resources with explicit create/draw/release lifetime and upload-to-default copy transitions. The DX8 backend is excluded from the x64 WW3D source selection; remaining direct `DX8Wrapper` callers are the explicit migration backlog before the full game target is enabled.

The backend direction is now:

```text
WW3D callers
    |
    v
IRenderBackend
    |
    +-- D3D12Backend   (Windows x64 / Evolution)
    `-- DX8Backend     (archival 32-bit/reference path only)
```

Verified routed operations include scene begin/end, deferred present/flip, clear, viewport, gamma, ambient/light environment, and cached-state invalidation. The D3D12 backend does not emulate the full `DX8Wrapper` API. Meshes, textures, buffers, pipelines, materials, render targets and the remaining renderer state/draw call sites must move across the existing seam or into renderer-neutral WW3D structures deliberately.

## Asset system

Legacy W3D loading uses the WW3D asset manager and chunk/prototype loader system.

Representative Zero Hour path:

```text
WW3DAssetManager::Load_3D_Assets
    -> FileClass
    -> ChunkLoadClass
    -> hierarchy / animation managers
    -> PrototypeLoaderClass implementations
    -> RenderObj prototypes
```

A corresponding Generals copy exists. This duplication is one reason new W3X parsing should be implemented in shared/Core code where practical.

## W3X

No W3X runtime loader was found in this baseline.

The modernization program defines W3X as the EA SAGE XML-based evolution of W3D used by later SAGE games. W3X support therefore starts as a new additive asset path.

## Determinism guard

Step 01 now has a consolidated characterization harness in `Core/Tests/DeterminismPrimitivesTest.cpp`. The lightweight path directly exercises production CRC, game-logic RNG, and compiler-sensitive float helpers under GCC/Clang optimization variants. Historically, the retired Windows/i686 `determinism_test` extended that same harness through production Xfer primitives, XferCRC, the real `DamageInfoOutput::xfer()` snapshot method, Win32 ABI/network assumptions, and a replay command-record checkpoint. Step 04F no longer builds that x86-only branch; active x64 coverage is supplied by the portable Step 01 primitives plus the 04A-04E4 fixed-width, deterministic timeline, replay/network, session, and CRC gates.

One concrete compiler hazard was removed without changing the legacy numeric algorithm: modern/non-VC6 `fast_float_trunc`, `fast_float_floor`, and `fast_float_ceil` now move IEEE-754 bits with `memcpy` rather than aliasing a `float` through an `unsigned *`. A 199,122-input before/after probe produced identical output bits; the VC6/reference assembly branch remains untouched.

The Step 01G Windows gate remains signed-off historical provenance: on 2026-09-10, the retired MinGW-w64 i686 / GCC 16.2 + Ninja `check_determinism` passed the complete float-helper, CRC/RNG, Xfer/XferCRC, snapshot, ABI, and replay checkpoint set. Step 04F deliberately does not preserve an active i686 compiler lane merely to rerun that historical gate.

## Modernization risk areas

- deterministic behavior across compilers;
- x86/32-bit address assumptions while x64 is brought up;
- legacy binary/on-disk layout assumptions;
- 16-bit geometry assumptions in legacy W3D rendering paths;
- DX8 state-machine coupling;
- duplicate Generals / Zero Hour implementation areas;
- high-poly/high-resolution asset pressure;
- legacy tools that may require old Microsoft/MFC components.
