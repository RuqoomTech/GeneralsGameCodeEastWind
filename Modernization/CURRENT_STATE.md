# Current Source State

This document records verified source facts through the locally validated Step 05A developer-baseline cleanup candidate on 2026-09-18. Windows Step 05A sign-off remains pending.

## Build system

- Top-level project uses CMake 3.25+; modern project code uses C++20 through `core_config` while the standalone W3X characterization targets remain C++98-compatible.
- VC6 and existing MSVC preset families remain available as historical/comparison paths.
- The supported MinGW/Ninja modernization lane is x86_64-only and uses `mingw64-tests`; Step 04F removes the former `mingw32-*` and `mingw-w64-i686*` presets rather than preserving dormant aliases.
- `Core/Tests/CMakeLists.txt` integrates W3X, determinism, ABI, runtime and Evolution protocol/session coverage using permanent subsystem-oriented test names.
- C++-only `-Wsuggest-override` and MinGW compatibility/link settings are target-scoped through `core_config` rather than globally leaking into C/vendored targets.
- Direct `FetchContent_Populate()` use in the ReactOS ATL, legacy zlib, and LZHL source-only paths has been removed.
- The MinGW toolchain validates the `x86_64-w64-mingw32` triplet, supports `RTS_MINGW_ROOT`, and shares its resolved bin path with WIDL/debug-strip discovery. WIDL supports explicit root/include overrides and is not required for the focused test graph.
- Full MinGW runtime configuration now requires WIDL before populating runtime FetchContent dependencies; native Windows also validates the `oaidl.idl` and `ocidl.idl` imports used by the EABrowser IDLs.
- Generals and Zero Hour install rules use `rts_install_runtime_target()` instead of repeating MSVC-only PDB generator expressions. MSVC keeps optional PDB installation; MinGW Release installs the `.debug` sidecar emitted by the existing strip workflow.
- MinGW toolchain discovery is now x86_64-only through `mingw-w64-common.cmake` plus the canonical x86_64 wrapper. `mingw64-tests` intentionally configures only the focused modernization graph.
- Full MinGW x64 runtime configuration is still blocked by design until runtime/platform/renderer dependencies are migrated subsystem-by-subsystem.
- Step 04 is fully Windows-signed-off. The user-supplied final 04F run passed 25/25 plus every explicit deterministic/Evolution/x64-platform gate. The focused graph remains 25 tests after repository normalization.

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
- `mingw64-tests` enables `RTS_BUILD_EVOLUTION_X64` and `RTS_BUILD_HEADLESS_CORE`; it remains renderer-free and is the canonical x86_64 Evolution focused preset.
- Focused x64 readiness does not link legacy D3D8/DirectInput/DirectSound and does not populate ReactOS ATL when there is no consumer.
- `architecture_abi` enforces fixed-width engine/wire primitives and IDs while permitting native pointers/`uintptr_t` to widen.
- Step 04D centralized `setFPMode()` in Core and established the x64 round-to-nearest deterministic timeline; the historical i686 x87 result remains recorded as provenance, not as an active build lane.
- Step 04D3 selectively aligns with the supplied upstream snapshot: Dozer/Worker Xfer/task fixes, production cancellation, neutron radius behavior, adapted GameMemory robustness, runtime Bink/Miles loading, and glyph-buffer safety. Material shared-file divergence fell from 93 to 63 without replacing EastWind x64/determinism infrastructure.
- The full x64 Zero Hour executable is not enabled yet. 04E2 supplies staged Win64 EVN1/EVR1 runtime integration; 04E3 validates the deterministic two-endpoint session contract; 04E4 freezes the representative command/CRC transcript; 04F removes the frozen i686 modernization/oracle machinery. Representative full-client multiplayer/replay execution remains a later x64 stabilization gate.
- The i686 modernization/oracle lane is retired. Retail x86 multiplayer interoperability is not required; supported Evolution development proceeds on x64.

## Renderer

The renderer is still fundamentally the legacy Direct3D 8-era WW3D implementation.

A partial abstraction has already started:

```text
WW3D callers
    |
    v
IRenderBackend
    |
    v
DX8Backend
    |
    v
DX8Wrapper / D3D8
```

Verified routed operations include scene begin/end, present/flip, clear, viewport, gamma, ambient/light environment, and cached-state invalidation.

This is useful groundwork for Step 11 but does not yet abstract meshes, textures, buffers, pipelines, materials, descriptors, or command submission.

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
