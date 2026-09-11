# Current Source State

This document records verified source facts plus accepted modernization changes through completed Step 03 and Step 04A on 2026-09-11.

## Build system

- Top-level project uses CMake 3.25+; modern project code uses C++20 through `core_config` while the standalone W3X characterization targets remain C++98-compatible.
- VC6 and existing MSVC preset families remain available as historical/comparison paths.
- The canonical i686 MinGW/Ninja names are `mingw32-release`, `mingw32-debug`, `mingw32-profile`, and `mingw32-tests`; older `mingw-w64-i686*` names remain compatibility aliases.
- `mingw32-tests` enables a focused root graph (`RTS_BUILD_TESTS_ONLY`) instead of configuring the full Zero Hour renderer/tool/dependency tree.
- `Core/Tests/CMakeLists.txt` integrates W3X A0/A1/A2 and the Step 01 determinism test with CTest while preserving `z_determinismcheck`.
- C++-only `-Wsuggest-override` and MinGW compatibility/link settings are target-scoped through `core_config` rather than globally leaking into C/vendored targets.
- Direct `FetchContent_Populate()` use in the ReactOS ATL, legacy zlib, and LZHL source-only paths has been removed.
- The i686 toolchain validates the selected GCC triplet, supports `RTS_MINGW_ROOT`, and shares its resolved bin path with WIDL/debug-strip discovery. WIDL supports explicit root/include overrides and is not required for the focused test graph.
- Full MinGW runtime configuration now requires WIDL before populating runtime FetchContent dependencies; native Windows also validates the `oaidl.idl` and `ocidl.idl` imports used by the EABrowser IDLs.
- Generals and Zero Hour install rules use `rts_install_runtime_target()` instead of repeating MSVC-only PDB generator expressions. MSVC keeps optional PDB installation; MinGW Release installs the `.debug` sidecar emitted by the existing strip workflow.
- MinGW toolchain discovery is now shared by tiny i686/x86_64 wrappers. `mingw64-tests` is the first x64 readiness lane and intentionally configures only the focused modernization graph.
- Full MinGW x64 runtime configuration is still blocked by design until runtime/platform/renderer dependencies are migrated subsystem-by-subsystem.
- Local host-native GCC and Clang focused configure/build/CTest validation is green at 7/7 tests, including the Step 02B runtime-install policy, completed Step 03 telemetry regression, and Step 04A architecture-width guard. The user reports the real Step 02B Windows build path working; no Step 02 Windows console transcript is archived in this tree.

## Performance telemetry

- Step 03 is complete. `rts/profile.h` exposes one consolidated `PerformanceTelemetry` seam; no second profiler hierarchy was introduced.
- `mingw32-profile` enables `RTS_BUILD_OPTION_PERF_TELEMETRY`; normal release/debug builds do not compile the engine/client telemetry call sites.
- CSV schema v2 emits one observational row per `GameEngine::update()` with update/client/message/network/logic CPU phases plus the primary WW3D render CPU bracket.
- The same row carries draw/geometry/texture/resource counters and a drawable total/visible/shrouded visibility proxy.
- `scripts/perf-summary.py` reports timing percentiles and mean/max resource counters using only the Python standard library.
- Tracy plots consume the same sample.
- GPU timestamp work is deferred to D3D12 rather than adding temporary D3D8 query infrastructure.
- Telemetry values are never consumed by simulation, frame pacing, CRC, replay, network, or Xfer behavior.

## x64 migration

- Step 04 is active; Step 04A establishes the x64 readiness lane.
- `cmake/toolchains/mingw-w64-common.cmake` centralizes MinGW-w64 discovery; i686 and x86_64 wrappers select architecture/triplet/root/pointer width.
- `mingw64-tests` enables `RTS_BUILD_X64_READINESS` and the focused graph only. This is the first canonical x86_64 Windows build preset.
- Focused x64 readiness does not link legacy D3D8/DirectInput/DirectSound and does not populate ReactOS ATL when there is no consumer.
- `architecture_width_step04a` enforces fixed-width engine/wire primitives and IDs while permitting native pointers/`uintptr_t` to widen.
- The full x64 Zero Hour executable is not enabled yet. Pointer/handle correctness, pools/allocators, serialization/native-layout separation, deterministic core bring-up, and client/platform dependencies are subsequent Step 04 slices.
- The i686 runtime remains the deterministic/replay compatibility reference during migration.

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

Step 01 now has a consolidated characterization harness in `Core/Tests/DeterminismPrimitivesTest.cpp`. The lightweight path directly exercises production CRC, game-logic RNG, and compiler-sensitive float helpers under GCC/Clang optimization variants. The Windows `z_determinismtest` extends that same harness through production Xfer primitives, XferCRC, the real `DamageInfoOutput::xfer()` snapshot method, Win32 ABI/network assumptions, and a replay command-record checkpoint. It is now built as a focused standalone target from six implementation units instead of linking the monolithic `z_gameengine` archive.

One concrete compiler hazard was removed without changing the legacy numeric algorithm: modern/non-VC6 `fast_float_trunc`, `fast_float_floor`, and `fast_float_ceil` now move IEEE-754 bits with `memcpy` rather than aliasing a `float` through an `unsigned *`. A 199,122-input before/after probe produced identical output bits; the VC6/reference assembly branch remains untouched.

The Step 01G Windows gate remains signed off: on 2026-09-10, `z_determinismcheck` passed on MinGW-w64 i686 / GCC 16.2 + Ninja with the complete float-helper, CRC/RNG, Xfer/XferCRC, snapshot, ABI, and replay checkpoint set. Step 02A relocates only the CMake ownership of that focused target into `Core/Tests`; its Windows source set, header-prelude contract, and MinGW IPO isolation are preserved. The new Step 02A Windows run still requires fresh validation.

## Modernization risk areas

- deterministic behavior across compilers;
- x86/32-bit address assumptions while x64 is brought up;
- legacy binary/on-disk layout assumptions;
- 16-bit geometry assumptions in legacy W3D rendering paths;
- DX8 state-machine coupling;
- duplicate Generals / Zero Hour implementation areas;
- high-poly/high-resolution asset pressure;
- legacy tools that may require old Microsoft/MFC components.
