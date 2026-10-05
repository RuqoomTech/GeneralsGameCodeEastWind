# Current Source State

## Tree and shroud continuation - 2026-10-05

The current checkout remains user commit `87ac65056fd6fa20e3247ab71bb96682b7b96344` plus local changes. The user prohibits GitHub pushes.

The real tree caller now submits CPU batches through `IRenderBackend` material draws. Native vertex/index buffers, assembly shader handles, declarations and tree device calls are removed from the owner and its header. Atlas pixels use retained CPU surfaces, the existing mip generator, explicit BGRA packing and backend mip selection. The original lighting, UV layout, winding, visibility, shadows, push/topple calculations and fixed-width Xfer fields remain. The `Trees.nvv` position and diffuse operations run on the CPU before the ordinary backend vertex shader: sway is applied every render, RGB darkening preserves alpha, the world transform is explicit, and shroud UVs use the unswayed position. Trees that do not fit the geometry capacity are no longer published to the update buffer. Enum-to-float conversions are explicit without changing arithmetic order. The optional atlas debug draw uses the same backend seam; the disabled archival DX8 statistics gate is retired from the tree dispatch.

The existing material path now binds a second texture and independent sampler for RGB-only shroud modulation in canonical HLSL. Primary texture alpha, alpha testing, blend/depth state and subsequent unlayered draws remain explicit. Secondary resources receive the same generation and self-sampling checks as primary resources. Sampler descriptors retain stable slots in a bounded cache; minimum mip selection is owned by the existing `TextureFilterClass` and uses sampler LOD rather than native texture state. This also handles the plain `TextureClass` atlas-overflow fallback without a derived-object cast. GPU pixel checks cover distinct UV sets, wrap/clamp and point/linear filtering, zero shroud alpha, alpha-test thresholds, mip selection, interleaved unlayered drawing and invalid/stale/self-sampled resources. Both production shader-state tests also exercise filter default/copy/secondary-stage mip state.

Paired Generals/Zero Hour shroud owners now retain `SurfaceClass` CPU storage and copy visible rectangles/borders to their actual destination texture surfaces. No pointer survives a native unlock. Existing fog values, interpolation, timing, projection bounds and input-to-simulation behavior are unchanged. Native shroud surface allocation/copy calls and obsolete native member types are removed. This prepares actual tree shroud sampling; other terrain/material callers still need their full rendering migrations.

Validation: the focused graph builds; 31/31 non-browser tests pass (GPU smoke 3.56 seconds), and the native browser test passes separately in 1.89 seconds with approved profile/controller access. Focused source checks pass for the tree (including the optional atlas debug path), terrain dispatch and both shroud variants. A read-only agent audit found no introduced ownership, shader or determinism defect. The latest exact `cmake --build --preset mingw64-game --target z_generals -- -j1` compiles the updated game sources and reaches the executable linker, failing with 59 unique unresolved legacy symbols, down from 61, with none added. Removed symbols are the old vertex/pixel shader handle globals. Transcript: `build/mingw64-game/tree-shroud-game-build.txt`; comparison: `build/tree-shroud-link-summary.json`. Fresh reconstruction from `2c81f9922beb5c5379c6d695c0e14ea3f15f55b3` matches all 4554 candidate files byte-for-byte, with zero missing, extra or different files (`build/tree-shroud-patchcheck.txt`). The first current blockers are `FlatHeightMap` lighting/render state and actual terrain draws. Their migration must preserve terrain texture layers, cloud/noise/shroud projection and material masks with real draw consumers. No current linked game, boot, visible frame, complete DX8 removal, sealed milestone or user Windows sign-off is claimed.

## CPU texture ownership and shoreline continuation - 2026-10-05

Current checkout is user commit `87ac65056fd6fa20e3247ab71bb96682b7b96344` plus local changes. All work remains local; the user prohibits GitHub pushes.

`SurfaceClass` now owns CPU pixels. File surfaces reuse the existing RGBA decoder, preserving Surface/BitmapHandler BGRA storage. Copies, scaling and locks update a local revision; overlapping copies read a snapshot and invalid rectangles are rejected. Backend upload expands packed 16-bit components to their normalized range, forces XRGB alpha opaque, and preserves alpha-only RGB as zero. The one-byte R3G3B2 writer no longer overwrites its neighbor. A native import bridge remains for staged callers, without creating native surfaces.

Regular 2D `TextureClass` retains the exact CPU surfaces returned to procedural writers and all decoded authored mip levels. Changed revisions trigger a new generation-safe backend upload; GPU render targets remain GPU-owned, with real dimensions and mip memory accounting. Cube, volume, depth, native loader/fallback and legacy Apply paths still require their complete caller migrations. CPU ownership alone does not complete shroud, mouse or profiler paths that still request native surfaces.

Both original shoreline loops now submit CPU geometry through the material backend, preserving sorted continuation, visibility, winding, depth comparison, LUT UVs and alpha-only coverage writes. D3D12 material PSOs support per-channel write masks. The font atlas copy now uses CPU SurfaceClass rather than native DX8 rectangles. Water's consumption of shoreline alpha remains staged with its full material migration.

Terrain no longer inherits or registers the retired native device-reset cleanup hook. Its explicit resource methods and destructor ownership remain. D3D12 output resize retains asset resources; GPU checks confirm an uploaded texture survives resize and is sampled correctly, with a render texture remaining valid too. Terrain readiness now queries the actual backend.

Local validation: 31/31 non-browser regressions pass, including production GPU pixel checks for all sixteen channel masks and rejected invalid masks; the real native browser regression passes separately with approved controller/profile access. CPU tests cover BGRA/RGBA conversion, glyph alpha, overlapping copies, scaling, revision updates and the packed-pixel sentinel. The latest exact serial `z_generals` command compiles the changed sources and fails at the executable link with 61 unique unresolved legacy symbols, down from 64, with no newly introduced symbols. Transcript: `build/mingw64-game/terrain-lifecycle-game-build.txt`. The remaining queue begins with terrain/tree DX8 draw-state ownership, then actual terrain drawing. Fresh reconstruction from `2c81f9922beb5c5379c6d695c0e14ea3f15f55b3` matches all 4554 candidate files byte-for-byte, with zero missing, extra or different files. No current game link, boot, visible frame, sealed milestone or user Windows sign-off is claimed.

## Step05H continuation and late branch reuse - 2026-10-05

Current reviewed baseline is `2c81f9922beb5c5379c6d695c0e14ea3f15f55b3`. The normal `cmake --build --preset mingw64-game --target z_generals -- -j1` invocation compiled the current game graph and reached the executable link; it failed with 65 unique unresolved legacy renderer symbols. This is compile evidence only. No current game boot, visible frame, or user Windows sign-off is recorded here.

`late` is fourteen commits ahead of that baseline. Its historical `build/zlink16.log` records a normal game link, but does not pin an invocation, source SHA, or boot/frame result. Review found behavior losses, so the branch is being reused by responsibility rather than merged wholesale.

Selected reuse in the working candidate:

- Paired box render objects submit the original eight vertices and twelve triangles through the current material backend. Axis-aligned boxes retain translation-only positioning; oriented boxes retain full transforms. The original white-emissive material is preserved as prelit vertex RGBA. Generals retains its CPU triangle sorting queue; Zero Hour retains immediate submission.
- Paired DDS constructors decode fixed DXT1-5 FourCC identifiers on the CPU; file layouts and native overloads still needed by staged callers remain intact.
- Packed ARGB conversion is consolidated in the existing `ww3dformat` owner for mesh material preprocessing and shatter interpolation. It retains channel shifts and the established WWMath quantization.
- Texture format selection describes actual CPU conversion support rather than querying the retired native device. Unsupported signed bump formats are rejected for their future complete material migration.
- Terrain tracks retain generated edges, time/distance fade, module order, authored textures/sampler, per-module transforms and shared strip topology. Native buffers and their obsolete member names are retired; rejected draws are reported.
- The native browser now observes actual navigation completion. Request supersession, redirect IDs, weak callback ownership and handler removal preserve lifetime safety. Real missing-file failures retain Failed/HRESULT instead of reporting success when the request is merely accepted.

Local verification: the focused graph builds successfully; 31/31 non-browser tests pass including the actual D3D12 GPU smoke, and the native browser smoke passes separately with approved access to its profile/controller. Both game variants' changed WW3D source files pass focused MinGW syntax checks. The normal `z_generals` rerun compiles the candidate and fails at the executable linker with 64 unique unresolved legacy renderer symbols, no new symbols relative to the 65-symbol baseline. Game boot/frame and Windows user sign-off remain unproven.

Outstanding repairs before larger `late` imports:

| Owner | Evidence requiring repair |
| --- | --- |
| Texture/surface | Detached procedural surfaces never upload font/video/radar writes; RGBA/BGRA is inconsistent; mip queries and generation report incomplete results. Cube/volume/depth placeholders do not preserve resource behavior. |
| Shader manager/scene | Stored shader state has no draw consumers; cloud/shroud/noise/monochrome logic is omitted; scene stencil marking is removed and masks exceed the backend's eight-bit contract. |
| Smudge/tree/water | Smudge indices are rebased twice and background capture is unpopulated; ordinary tree breeze can freeze; water effects and matrix conventions need complete migration. |
| Terrain/roads/bridges | CPU topology can be reused, but secondary UVs, blend stages, cloud/shroud and lighting must reach actual submissions. |

Scorch and bibs are promising subsequent CPU geometry ports after their texture, transform and failure-propagation contracts are verified. SDL3 remains Step06 after normal D3D12 game boot/frame; W3X runtime work remains deferred to the same gate.

## Historical baseline through Step05H1N

The remaining baseline notes record verified source facts through the Step 05H1N point-orientation/runtime-pointer/ownership candidate on 2026-09-19. Steps 05D through 05G are Windows-signed-off. Step 05H is the active normal-game x64 migration: successive real `mingw64-game` builds have cleared WWSaveLoad pointer identity, crash/debug diagnostics, registry/native-handle width, WWMath D3D8 leakage, profiler/debug pointer identities, compression pointer arithmetic, and now the first active D3DX8 dependency cluster. H1L is locally sealed only; Windows sign-off requires the real `z_generals` build to advance past the updated WW3D asset/name paths.

## Build system

- Top-level project uses CMake 3.25+; modern project code uses C++20 through `core_config` while the standalone W3X characterization targets remain C++98-compatible.
- VC6 and existing MSVC preset families remain available as historical/comparison paths.
- The supported MinGW/Ninja modernization lane is x86_64-only. `mingw64-tests` remains the focused regression preset; Step 05H1 adds `mingw64-game` for the normal Zero Hour `z_generals` migration graph. Step 04F removed the former `mingw32-*` and `mingw-w64-i686*` presets rather than preserving dormant aliases.
- `Core/Tests/CMakeLists.txt` integrates W3X, determinism, ABI, runtime and Evolution protocol/session coverage using permanent subsystem-oriented test names.
- C++-only `-Wsuggest-override` and MinGW compatibility/link settings are target-scoped through `core_config` rather than globally leaking into C/vendored targets.
- Direct `FetchContent_Populate()` use in the ReactOS ATL, legacy zlib, and LZHL source-only paths has been removed.
- The MinGW toolchain validates the `x86_64-w64-mingw32` triplet, supports `RTS_MINGW_ROOT`, and shares its resolved bin path with WIDL/debug-strip discovery. WIDL supports explicit root/include overrides and is not required for the focused test graph.
- Full-game MinGW runtime configuration requires WIDL before populating runtime FetchContent dependencies; the focused `mingw64-tests` graph intentionally avoids those unrelated full-runtime dependencies. Step 05H1 deliberately exposes the full `z_generals` graph through `mingw64-game` so remaining renderer blockers are discovered from the real executable rather than a synthetic app.
- Generals and Zero Hour install rules use `rts_install_runtime_target()` instead of repeating MSVC-only PDB generator expressions. MSVC keeps optional PDB installation; MinGW Release installs the `.debug` sidecar emitted by the existing strip workflow.
- MinGW toolchain discovery is x86_64-only through `mingw-w64-common.cmake` plus the canonical x86_64 wrapper. `mingw64-tests` configures the focused regression graph, including the Windows x64 D3D12 backend smoke test; `mingw64-game` configures the normal Zero Hour executable with tools disabled and the same Evolution x64 policy.
- The former CMake hard gate that blocked the monolithic x64 game runtime has been removed in Step 05H1. This does not imply the game already links: remaining direct renderer/platform dependencies are now intentionally exposed by building the normal `z_generals` target. There is no parallel Evolution application tree.
- Step 04, Step 05A and Steps 05C2 through 05G are Windows-signed-off. The temporary Step 05B proof shell was Windows-verified and then removed. The host-portable graph remains 26 tests; Windows x64 adds the real `d3d12_backend_smoke` GPU test. Step 05H1 passes the host 26/26 graph and all source-policy probes locally; Windows `mingw64-game` configure/build is still required before any game-runtime sign-off.

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
- The full x64 Zero Hour executable is no longer configuration-gated. Step 05H1 adds the `mingw64-game` preset specifically to let the normal target reveal the remaining direct `DX8Wrapper` compile/link blockers. The existing WW3D backend seam remains the only D3D12 migration path.
- The first `mingw64-game` compile reached WWSaveLoad before renderer blockers and exposed raw 32-bit pointer identity persistence. Step 05H1A replaces serialized addresses/native pointer sizes with explicit 32-bit `PersistPointerToken` values and keeps native pointers only in the in-memory remap table; this does not alter simulation/network/replay formats.
- Steps 05H1B through 05H1J then cleared native-width crash/debug/registry/profiler/compression blockers and warning classes exposed by the same real game graph. Step 05H1K removes D3DX8 utility/math dependencies from the active x64 graph: unused D3DX headers are removed across WW3D and game-device/client callers, point/sorting and shared Bezier math use WWMath, missing-texture mip generation reuses `BitmapHandler`, and Evolution `SurfaceClass` copy/scale uses the existing CPU bitmap path. Genuine D3DX-bound shader/water/terrain/tree renderer responsibilities are intentionally left for coherent D3D12 migration; archival DX8-only source remains untouched.
- The i686 modernization/oracle lane is retired. Retail x86 multiplayer interoperability is not required; supported Evolution development proceeds on x64.

## Renderer

The x64 renderer is now migrating in-place behind WW3D `IRenderBackend`. The production D3D12 backend owns DXGI adapter/device creation, command submission, flip-model swap chain, render/depth targets, viewport/scissor, clears, present and fences. Step 05D adds an immutable root signature/PSO, shader compilation, transient upload-buffer lifetime tracking, and `DrawIndexedInstanced` for the first renderer-neutral indexed position/color primitive. Step 05E externalizes that shader into canonical HLSL. Step 05F adds persistent default-heap vertex/index resources with explicit create/draw/release lifetime and upload-to-default copy transitions. Step 05G adds the first sampled-texture path: default-heap RGBA8 upload, shader-visible SRVs, a D3D12 static sampler and persistent textured indexed drawing. Step 05H1 moves the first normal-game caller responsibility: untextured `Render2DClass` screen-space geometry used by `W3DDisplay` lines/rectangles now emits renderer-neutral color vertices/indices and uses explicit D3D12 opaque/alpha/additive 2D PSOs with depth disabled. The DX8 backend remains excluded from the x64 WW3D source selection; remaining direct `DX8Wrapper` callers are now the real `z_generals` migration queue. Step 05H1K removes active D3DX8 utility dependencies without creating a D3DX compatibility layer: WWMath handles point/sorting/Bezier transforms, `BitmapHandler` handles CPU image conversion/mips, and remaining D3DX8 use is confined to archival/non-Evolution code or genuine renderer responsibilities that must cross the D3D12 seam coherently.

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

Verified routed operations include scene begin/end, deferred present/flip, clear, viewport, gamma, ambient/light environment, cached-state invalidation, backend proof geometry/textures, and the first real untextured `Render2D` draw path. `W3DDisplay::setGamma()` also now reaches the existing backend seam instead of `DX8Wrapper`. The D3D12 backend does not emulate the full `DX8Wrapper` API. Textured `Render2D` is intentionally not faked in 05H1: it remains the next coherent resource-lifetime migration, where real `TextureClass` data must map to renderer-neutral texture handles. Meshes, materials, transforms, render targets and the remaining state/draw call sites must continue moving responsibility-by-responsibility.

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

- Step 05H1L follows the H1K Windows build to `assetmgr.cpp` at 125/940 and removes four active Zero Hour asset-name address-to-`int` subtraction sites. String positions are now computed as pointer differences before the small Win32/API length conversion, and `PrimitiveAnimationChannelClass::KeyClass::Set_Time` is corrected to its side-effect-only `void` contract to remove the exposed non-void/no-return warning.
- Step 05H1M follows the H1L Windows build to the real Evolution `Render2D` caller at 154/942 and gives both renderer-neutral backend vertex PODs explicit equality operators required by Westwood `DynamicVectorClass`; both remain trivially copyable with unchanged D3D12 upload layout.
- Step 05H1N follows the H1M Windows build to `pointgr.cpp` at 188/942: point orientation uses `Matrix3D::Rotate_Vector`, logical-audio runtime pointers cross `On_Event` as `std::uintptr_t`, and `PivotMapClass`/`SnapPointsClass` delete from complete-object overrides instead of their offset secondary `RefCountClass` base. Host validation is 26/26; Windows sign-off is pending.
