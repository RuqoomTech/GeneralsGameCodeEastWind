# Current Source State

This document records verified source facts plus accepted modernization changes through Step 02B on 2026-09-10.

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
- Local host-native GCC and Clang configure/build/CTest validation is green at 5/5 tests, including `buildsystem_runtime_install_policy`. GNU configure/build/install probes validate both the generic runtime install helper and the MinGW-style `.debug` sidecar branch. Windows MinGW validation of Step 02B and the real `z_generals` target is still pending; no Windows success is claimed for Step 02B.

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
- x86/32-bit memory ceiling;
- legacy binary/on-disk layout assumptions;
- 16-bit geometry assumptions in legacy W3D rendering paths;
- DX8 state-machine coupling;
- duplicate Generals / Zero Hour implementation areas;
- high-poly/high-resolution asset pressure;
- legacy tools that may require old Microsoft/MFC components.
