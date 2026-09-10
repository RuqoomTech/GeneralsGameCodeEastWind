# Current Source State

This document records verified facts from the 2026-09-09 authoritative baseline.

## Build system

- Top-level project uses CMake 3.25+.
- Modern builds use C++20 through `core_config`.
- VC6 remains supported as a historical path.
- Standard modern Windows presets use `Ninja Multi-Config`.
- `mingw-w64-i686` and the focused determinism preset use Ninja; the i686 toolchain resolves native MSYS2 MINGW32 tools directly.
- MinGW-specific support exists in `cmake/mingw.cmake`, `cmake/reactos-atl.cmake`, `cmake/widl.cmake`, and the i686 toolchain file.
- The project still contains toolchain/platform assumptions that must be audited before GCC/Ninja becomes the canonical Windows path.

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

The focused Windows gate is now signed off. On 2026-09-10, `z_determinismcheck` passed on MinGW-w64 i686 / GCC 16.2 + Ninja with the complete float-helper, CRC/RNG, Xfer/XferCRC, snapshot, ABI, and replay checkpoint set. The earlier monolithic-link failure was eliminated by keeping the test focused and allowing the MinGW build to remove unused legacy inline/vtable material. Step 02 can now proceed with this gate as a mandatory regression check.

## Modernization risk areas

- deterministic behavior across compilers;
- x86/32-bit memory ceiling;
- legacy binary/on-disk layout assumptions;
- 16-bit geometry assumptions in legacy W3D rendering paths;
- DX8 state-machine coupling;
- duplicate Generals / Zero Hour implementation areas;
- high-poly/high-resolution asset pressure;
- legacy tools that may require old Microsoft/MFC components.
