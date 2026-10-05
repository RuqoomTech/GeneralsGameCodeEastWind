# Project state

Updated: 2026-10-05

## Current baseline

The current checked-out baseline is `main` at `2c81f9922beb5c5379c6d695c0e14ea3f15f55b3`; its existing shatter ARGB fix supersedes the older October 3 compiler receipt. A fresh normal Windows MinGW-w64 GCC 16.2 build on October 5 compiled the game graph and reached the executable linker, which failed with 65 unique unresolved legacy renderer symbols.

The `late` branch is fourteen commits ahead of this baseline. Three agents reviewed texture/resource ownership, rendering behavior, and x64 alignment. Its historical game-link log is useful provenance but lacks a pinned source/invocation and boot/frame evidence. Blank procedural text textures, disconnected shader state, lost stencil marking and other behavior gaps make a wholesale merge unsuitable.

The working candidate selectively reuses paired box rendering and DDS FourCC decoding; shared CPU ARGB/format utilities remove device dependencies from mesh material preprocessing. The first candidate game rerun compiled successfully and reached the linker with 64 unresolved symbols, no newly introduced symbols. The detailed reuse/repair queue is recorded in `Modernization/CURRENT_STATE.md`. Additional renderer migration and regression validation remain in progress. No linked/booted current game, visible game frame, sealed milestone or user Windows sign-off is claimed.

## Step 05H1U candidate — D3D12-only build and output ownership

The user's October 1 direction retires the archival DX8 build lane too. `DX8Backend.cpp/.h` and `cmake/dx8.cmake` are deleted. Full-game configuration requires Windows x64 Evolution; game/viewer CMake targets select D3D12/DXGI, and no CMake target links `d3d8`, `d3dx8`, or `d3d8lib`. WW3D owns the shared shader staging command. Its superseded DX8 device lifecycle, registry, and MSAA fallback branches are removed.

Confirmed window mode now belongs to the existing crash-report owner rather than a DX8 global, and the obsolete device-only `-FPUPreserve` switch is removed. Display resize bookkeeping retains bit depth/window mode as well as dimensions. Presentation intervals, coordinate normalization, and stencil capability use the D3D12 output/resource seam. Existing statistics capture real D3D12 draw counts while preserving historical CSV column names.

WW3D TGA/BMP and AVI capture plus the existing threaded PNG/JPEG screenshot caller now read tightly packed top-down RGBA8 through `IRenderBackend`. The backend retains one GPU output image before flip-discard presentation, adding a GPU copy per submitted scene; GPU/CPU synchronization happens only when readback is requested. BMP file size/padding and AVI buffer dimensions/padding are corrected, and AVI handles/buffers are initialized before any failure cleanup. PNG/JPEG encoding and notifications retain their existing worker/main-thread split.

Local GCC 16.2 compilation passes for these changes. All 27 focused tests pass, including GPU checks for interval validation, submitted draw counts, readback before/after presentation, resize, pixel channel order, vertical orientation, and unaligned row pitch. The real `z_generals -j1` build still fails its final link with **98 distinct unresolved legacy renderer symbols**, down from 107. The first is projected-shadow render-target creation, followed by mesh, buffer, material, texture, and effect callers. Legacy headers and implementation reference files still exist; complete DX8 source removal and game boot remain unfinished. PNG/JPEG/TGA/BMP/AVI in-game capture, Generals/viewer builds, and a visible game frame are not validated. No user Windows sign-off or sealed milestone is claimed.

Fresh patch reconstruction from baseline `6d6b94e9e` compares 4555 candidate files with 4555 reconstructed files: zero missing, extra, or byte differences. `git diff --check` passes.

## Step 05H1V candidate - camera transforms and neutral names

Both game cameras now apply their pixel viewport and zero-to-one-depth view/projection through `WW3D -> IRenderBackend`. `Get_D3D_Projection_Matrix` is renamed `Get_Zero_To_One_Projection_Matrix`; neither camera includes or calls the old wrapper. WWMath still computes the existing perspective/orthographic projection and view matrices. D3D12 packs their values explicitly into vertex-visible root constants and applies them to dynamic, persistent color, and persistent textured geometry. Screen-space UI binds identity independently of the active camera. Per-material/shadow depth bias, object transforms, lighting and offscreen targets remain separate unfinished draw responsibilities.

Renderer statistics functions/macros and telemetry members use responsibility-based names. CSV schema v3 replaces `dx8_triangles`/`dx8_vertices` with `submitted_triangles`/`submitted_vertices`; totals no longer add skin/sorted subsets twice. The summary tool accepts both historical v2 and new v3 captures. CPU mesh diagnostics are renamed from `dx8rendererdebugger.*`/`DX8RendererDebugger` to `meshdebugger.*`/`MeshRendererDebugger`, registered under Core WW3D, and `Enable(false)` now actually disables them. Three old renderer notes move out of the active code directory into `Modernization/History/LEGACY_RENDERER_*.txt`, preserving their historical contents.

All 27 focused tests pass, including production GPU pixel checks for non-symmetric camera transforms, transform changes within an open scene, all three 3D geometry paths, and interleaved 2D isolation. The summary tool accepts v2/v3 samples. The exact `cmake --build --preset mingw64-game --target z_generals -- -j1` command completed all compilation and failed at final link with **95 distinct unresolved legacy renderer symbols**, down from 98, with no added unresolved symbols. The camera removes `DX8Wrapper::Set_Viewport`, `DX8Wrapper::ProjectionMatrix` and `DX8Wrapper::ZBias` dependencies. The first remaining error is `W3DProjectedShadow.cpp:263`, `DX8Wrapper::Create_Render_Target`. Projected shadows still require neutral texture/offscreen-target lifetime and real mesh/material draws. Existing INI/legacy compiler warnings remain; this is not a warning-free whole-game claim. The final build transcript is `build/mingw64-game/h1v-current-build.txt`. Fresh reconstruction from baseline `c5c9ca33d` matches 4555 candidate/reconstructed files with zero missing, extra or byte differences; `git diff --check` passes. This does not establish full source retirement, a linked/booted game, a visible frame, or user Windows sign-off.

## Step 05H1W candidate - projected-shadow texture lifecycle

The next real link failure exposed projected-shadow render-target creation. `WW3D::Create_Render_Texture` now creates color-only RGBA8 render textures through `IRenderBackend`. The existing `TextureBaseClass`/`TextureClass` owns the neutral handle, dimensions, and backend lifetime; these resources bypass the legacy DX8 texture tracker and never manufacture D3D8 objects. D3D12 shares its SRV/resource owner between uploaded and rendered textures. `StaticTextureResource`, `m_static_textures`, and `Release_Static_Texture` become `TextureResource`, `m_textures`, and `Release_Texture`. Texture generations continue across device recreation so surviving owners cannot release or select a replacement device's resource.

The backend owns offscreen RTVs, color-only PSOs for existing colored/textured/2D draws, explicit render/sample/copy transitions, and synchronous GPU-to-GPU copies between matching RGBA8 textures. Offscreen submission never presents or replaces retained output capture; restoring the main target restores its viewport and camera. Current-target dimensions are separate from primary-output dimensions. Target selection and copying require closed scenes; stale handles, incompatible dimensions, self copies, and sampling the selected attachment are rejected.

`TexProjectClass` no longer includes or calls `DX8Wrapper`. Both game shadow managers allocate temporary/permanent textures through WW3D and copy shadows through `TextureClass::Copy_From`, replacing the old surface-copy route. Failures return before publishing new shadow bounds/history. A duplicated buffer-ownership assertion now checks both the vertex and index buffer. These are resource prerequisites: real mesh/material rendering, decal buffers, texture binding, blend/depth bias, and custom depth attachments remain unfinished. Projected shadows are not claimed to work in the game yet. No simulation, wire, replay, CRC, Xfer, RNG, or W3X representations are changed.

All 27 focused tests pass. Production GPU pixel checks cover all three existing world-geometry paths into an unaligned 61x37 render texture, offscreen 2D blend modes, GPU copies and subsequent sampling, preserved deferred output/capture, restored camera/viewport, selected-target release, rejected stale handles, and device recreation. The strict GPU test build passes. The exact `cmake --build --preset mingw64-game --target z_generals -- -j1` command completed all compilation (158 actions including the final link attempt) and failed at final link with **95 distinct unresolved legacy renderer symbols**, unchanged from H1V, with none added. Other callers still reference the old render-target APIs, so their global symbols remain. The first error is `W3DProjectedShadow.cpp:704`, `DX8Wrapper::Apply_Render_State_Changes`, in `W3DProjectedShadowManager::flushDecals()`. The full transcript is `build/mingw64-game/h1w-current-build.txt`.

Fresh patch reconstruction from baseline `e6f36e322` matches 4555 candidate/reconstructed files with zero missing, extra, or byte differences; `git diff --check` passes. Existing whole-game warnings remain. This is local validation, not a sealed milestone, complete DX8 source removal, working shadows, a linked/booted game, a visible game frame, or user Windows sign-off. The next draw migration must bring decal geometry, real texture data, multiplicative/alpha/additive blending and depth behavior across the seam together.

## Step 05H1X candidate - neutral decal batches and asset textures

Both game projected-shadow managers now submit decals through `WW3D -> IRenderBackend::Draw_Indexed_Decal_Triangles -> D3D12Backend`. Their D3D8 decal vertex/index buffers, FVF, locks, stream bindings and state setup are removed. The existing terrain/UV calculations, clipping, winding and position offsets remain. CPU batches reserve vertex and index capacity together before writing; overflow flushes the complete previous batch. The simple decal path initializes its diffuse color too. Texture/type switches start from the first eligible shadow, and failed submissions return failure rather than publishing a successful draw count.

D3D12 uses the existing transient upload/fence owner with textured vertices and three decal PSOs. `PSDecal` in canonical `PrimitiveColor.hlsl` multiplies the real texture by vertex diffuse. A separate static linear-clamp sampler uses level zero. Multiply, source-alpha and additive blending preserve the legacy equations, including alpha; clockwise faces are culled, depth comparison is LEQUAL, and depth writes are disabled. Decals require the main depth attachment; color-only offscreen targets are explicitly rejected. Other primitive samplers and draw behavior remain intact.

The existing texture loader reads DDS or TGA through the game file factory and converts to RGBA8 before upload. CPU DXT1-5 block decoding is consolidated in Core `BitmapHandlerClass`, shared by both DDS loaders, with transparent BC1, explicit/interpolated alpha and unchanged premultiplied DXT2/4 samples. The DDS disk header now uses a fixed 32-bit reserved field instead of a native pointer and is asserted to be 124 bytes. Header/read bounds, mip reduction, rounded block extents and level sizes are checked. TGA uses the established orientation and color conversion; the existing missing-texture owner supplies its magenta fallback.

File-backed level-zero `TextureClass` resources use the existing neutral texture handle, owner and missing flag. Generation queries reject handles surviving device recreation. Decal assets request one mip and prepare uploads before scene submission. Renderer thread ownership becomes `TextureLoader::Is_Render_Thread`; WW3D initializes and stops the loader around its asset/backend lifetime, removing dependency on the retired wrapper's thread ID. Background mip-chain/native-surface loading and other texture callers remain unfinished migration work.

Local validation: **28/28 focused tests pass**, including production GPU pixels for all three blend equations, texture/diffuse modulation, clamp sampling, opposite-winding culling, LEQUAL comparison without depth writes, rejected invalid indices/modes/handles and offscreen decal submissions. A CPU test covers the DDS header size/offsets and DXT1-5 color/alpha decoding. Existing determinism, replay, protocol, session and render-texture tests pass. The exact `cmake --build --preset mingw64-game --target z_generals -- -j1` command compiles the modified real game graph, then fails at final link with **94 distinct legacy renderer symbols**, down from 95, with none added. `DX8Wrapper::_MainThreadID` is removed from that list. The first failure is `W3DProjectedShadow.cpp:1231`, `DX8MeshRendererClass::Flush()`, in the projected-shadow mesh path. Transcripts: `build/mingw64-game/h1x-current-build.txt`, `build/h1x-test-build.txt`, `build/h1x-tests.txt`; symbol comparison: `build/h1x-link-summary.json`.

Fresh patch reconstruction from baseline `005631f94` matches 4556 candidate/reconstructed files with zero missing, extra or byte differences; `git diff --check` passes. This validates the local candidate, not a sealed milestone or user Windows sign-off. Actual asset decoding through the running game, complete projected shadows, remaining mesh/material/terrain/effect migration, full DX8 source retirement and a visible game frame are still unvalidated. No simulation, wire, replay, CRC, Xfer, RNG, SDL or W3X representation changes are included.

## Locked architecture

- Evolution runtime is x64-only.
- Direct3D 12 is the only Evolution renderer target; no D3D9/D3D11 intermediate.
- `IRenderBackend` is the current WW3D migration seam. It should grow only for real migrated callers.
- D3D12 is the only selectable renderer. The DX8 backend adapter and SDK build dependency are retired; remaining DX8 caller code is unfinished migration work and provides no supported renderer lane.
- CMake + Ninja + MinGW-w64 GCC is the primary Windows toolchain; Clang is secondary.
- Simulation, replay, networking, CRC, RNG, Xfer and snapshot behavior remain deterministic.
- Fixed-width game/wire/replay fields do not widen on x64.
- W3D remains supported. W3X is additive EA SAGE XML and remains renderer-neutral.
- Future multiplayer compatibility is Evolution-to-Evolution only.
- Prefer consolidation over duplicate helpers, executables, or subsystem trees.

## Completed foundation

- Determinism characterization and fixed replay/CRC anchors.
- Command-line CMake/Ninja build foundation.
- Performance telemetry.
- x64 native-width runtime substrate.
- 12,000-frame deterministic timeline.
- EVN1/EVR1 fixed-width network/replay formats and staged runtime integration.
- Deterministic two-endpoint and full-session network/replay gates.
- Retirement of the active i686 modernization lane.
- Developer-facing repository cleanup.
- Proven Win32/DXGI/D3D12 device, swap-chain, clear/present and fence implementation on real Windows hardware.

Historical milestone detail is kept in `Modernization/WORKLOG.md` and `Modernization/History/`.

## Step 05 — renderer-first modernization + asset foundation

Current order:

1. **05A — developer baseline cleanup — DONE / Windows signed off.**
2. **05B — D3D12 proof shell — DONE / Windows signed off / architecture superseded.** It proved the Windows x64 D3D12 fundamentals and is now being removed rather than retained as a second runtime.
3. **05C — in-place D3D12 backend integration — DONE / Windows signed off.** The WW3D backend owns the x64 device/frame lifecycle and the temporary standalone shell is removed.
4. **05D — indexed primitive foundation — DONE / Windows signed off.** The first renderer-neutral indexed position/color draw contract, D3D12 root signature/PSO, shader compilation, transient upload lifetime, and production GPU smoke proof are green.
5. **05E — shader asset foundation — DONE / Windows signed off.** D3D12 shader source lives in canonical HLSL staged beside the executable; legacy `.nvp/.nvv` terrain/filter/tree/water sources remain behavior references until their real paths migrate.
6. **05F — persistent indexed geometry — DONE / Windows signed off.** Renderer-neutral create/draw/release handles own persistent D3D12 default-heap vertex/index buffers with explicit upload/copy transitions and real GPU reuse/release validation.
7. **05G — texture/SRV/sampler foundation — DONE / Windows signed off.** Default-heap RGBA8 upload, shader-visible SRVs, static sampler binding and textured indexed drawing are verified on the production backend.
8. **05H — real `z_generals` x64 D3D12 migration — IN PROGRESS.** 05H1 enables the normal Zero Hour game graph and routes the first untextured `Render2D`/`W3DDisplay` screen-space caller through `IRenderBackend`. 05H1A fixes the WWSaveLoad pointer-identity blocker with fixed-width persistence tokens. 05H1B fixes the crash-diagnostics blocker by moving active crash diagnostics from Win32 `Eip/Esp`/I386 assumptions to native x64 `CONTEXT`, `uintptr_t`, `SymFromAddr`, and `StackWalk64`. 05H1C fixes the next blocker by retaining `RegistryClass` keys as native `HKEY` values rather than 32-bit integers. 05H1D removes the WWMath build blocker by moving legacy D3DMATRIX conversion out of renderer-neutral WWMath and into the archival `DX8Wrapper`, deleting the unused D3DXMATRIX conversion surface. 05H1E removes the profiler blocker by replacing the tracer-pointer pseudo thread ID with a lock-assigned logical ID. 05H1F removes the first `core_debug` blocker by making debug frame/hash identities native-width and also clears the already-exposed thread-handle, module-handle, and delete-through-`void*` warnings. 05H1G upgrades the remaining core_debug exception/stack-walk responsibility to native x64 `CONTEXT`, `uintptr_t`, `StackWalk64`/`SymFromAddr`, XMM save-state diagnostics, and a correct `INT_PTR` dialog callback. 05H1H adds the missing direct C-library declarations in `debug_except.cpp`. H1I follows the next real Windows blocker in `debug_stack.cpp`: it adds that translation unit's own C-library declarations and renames the engine method from macro-colliding `StackWalk` to `Capture`, while retaining DbgHelp `StackWalk64`. Continue from the next actual `z_generals` compiler/linker failure and clean newly exposed x64 warnings in the same owning responsibility.
9. **W3X parser/import:** resume real XML parsing and renderer-neutral W3D/W3X mesh convergence once the normal D3D12 game path is visibly rendering.

The full x64 Zero Hour build is intentionally enabled through `mingw64-game`; remaining `DX8Wrapper` compile/link failures are the migration queue. No parallel replacement executable or giant DX8-on-D3D12 facade will be maintained.

## Current developer commands

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\setup-windows-dev.ps1
cmake --preset mingw64-tests
cmake --build --preset mingw64-tests
ctest --preset mingw64-tests --output-on-failure

cmake --preset mingw64-game
cmake --build --preset mingw64-game --target z_generals -- -j1
```

See `TESTING.md` for the D3D12 backend smoke gate and explicit deterministic/network checks.

## Known non-blockers

Legacy `GameMemory` warnings such as custom `operator new` returning null remain known technical debt unless they become errors or affect active work.

## Step 05H1A — x64 WWSaveLoad persistence-token hotfix

The first real `mingw64-game` build reached `core_wwsaveload` and exposed a Win32-only pointer cast in `SimplePersistFactoryClass`. The hotfix keeps persisted identity fixed at 32 bits without serializing native addresses: WWSaveLoad now assigns `PersistPointerToken` values per save context and performs token-to-native-pointer remapping only in memory. SimplePersistFactory, render-object/dazzle factories, audible-sound identity, and remapped SoundSceneObj attachments use that token contract; the historical runtime-only `m_UserObj` field now persists as null instead of an address. Legacy 4-byte identity fields remain loadable as opaque tokens. This is a compile/runtime correctness hotfix for the real x64 game graph; it does not claim Windows sign-off until the Windows build is rerun.

## Step 05H1B — x64 WWLib exception/DbgHelp hotfix

The next Windows `mingw64-game` build advanced to `core_wwlib` and failed because `Except.cpp` still compiled a 32-bit-only crash reporter: pointer-sized `GetProcAddress` results were stored through `unsigned long`, `CONTEXT` was read through `Eip/Esp/Ebp`, x87-only fields were assumed, and stack walking selected `IMAGE_FILE_MACHINE_I386`. The active Win64 branch now keeps diagnostic addresses native-width, dumps x64 registers, resolves symbols through `SymFromAddr`, and walks the captured exception context through `STACKFRAME64`/`StackWalk64` with AMD64 callbacks. These DbgHelp64 entry points were consolidated into the existing `DbgHelpLoader`; no parallel crash subsystem was added. The historical 32-bit branch remains isolated behind the non-Win64 preprocessor path and is not part of Evolution x64. Windows compilation must be rerun before sign-off.

## Step 05H1C — x64 RegistryClass handle hotfix

The next Windows `mingw64-game` build advanced through the H1B crash-diagnostics changes and stopped in `WWLib/registry.cpp`: `RegistryClass` asserted that `HKEY` matched `int`, cast the opened key to `int`, then reconstructed `HKEY` at each Win32 API call. That is invalid on Win64 because registry keys are opaque pointer-sized handles. `RegistryClass` now owns an `HKEY` directly, initializes it to null, assigns open/create results without truncation, passes it directly to registry APIs, and clears it after close. The x64 platform policy locks this native-handle contract. No registry value layout, deterministic state, renderer behavior, networking, replay, CRC, RNG, Xfer, or snapshot format changes are involved. The next Windows build advanced past this blocker to `WWMath/matrix3d.cpp`.

## Step 05H1D — renderer-neutral WWMath / D3D8 matrix isolation

The next real `mingw64-game` build stopped because `WWMath/matrix3d.cpp` directly included the retired `d3dx8math.h`. Inspection showed that neither Matrix3D nor Matrix4x4 math requires D3DX8; the dependency existed only for legacy D3D matrix conversion helpers. Those helpers no longer live in WWMath. The required `D3DMATRIX` transpose/conversion implementation is consolidated with the archival `DX8Wrapper`, while the unused `D3DXMATRIX` conversion overloads are deleted. `matrix3d.cpp`, `matrix4.cpp`, and their public headers are now renderer-neutral and carry no D3D8/D3DX8 matrix API surface. This does not convert or extend the DX8 backend; it narrows its ownership until remaining callers migrate. Windows compilation must be rerun before sign-off.

## Step 05H1E — legacy profiler logical thread-ID hotfix

The next real `mingw64-game` build advanced through H1D to `core_profile_legacy` and stopped while generating its PCH: `ProfileFuncLevel::Thread::GetId()` cast the internal `ProfileFuncLevelTracer*` to 32-bit `unsigned`, which loses precision on Win64. That value is diagnostic-only and was never a Windows thread ID or a gameplay/wire field. `ProfileFuncLevelTracer` now owns a monotonically assigned logical `unsigned` ID allocated under the existing profiler critical section; `Thread::GetId()` returns that ID and no longer derives identity from a native address. The existing CSV file naming contract remains `%08x`, while native allocator addresses are neither truncated nor exposed. The x64 platform policy locks this separation. Windows compilation must be rerun before sign-off.

## Step 05H1F — x64 debug identity and warning-cleanup hotfix

The next real `mingw64-game` build advanced through H1E and stopped in `core_debug/debug_debug.cpp`. `Debug::SkipNext()` only knew x86 inline assembly, `curStackFrame`/`FrameHashEntry::frameAddr` were 32-bit, static log strings were hashed by truncating their pointers to `unsigned`, and pointer/memory-dump formatting also narrowed addresses. The active path now captures the return address through the compiler builtin and stores diagnostic identities in `std::uintptr_t`. In the same requested cleanup slice, the warnings already exposed by the real build are fixed at their owning types: `_beginthread()` handles stay `uintptr_t`, owned `char[]` allocations are deleted through `char*`, and `Compare_EXE_Version` carries `HINSTANCE` natively. The x64 policy locks these contracts. `#pragma message` diagnostics and optional CMake status messages are intentionally unchanged because they are not compiler warnings. Windows compilation must be rerun before sign-off.
## Step 05H1G — native-width core_debug exception/stack-walk hotfix

The next real `mingw64-game` build advanced through H1F to `core_debug/debug_except.cpp` at 105/1077 and exposed the remainder of the debug subsystem's Win32 assumptions: x86-only `CONTEXT` register names, `FLOATING_SAVE_AREA`, EIP-based instruction dumps, a 32-bit stack-signature address type, 32-bit DbgHelp APIs, and a `BOOL` dialog callback incompatible with Win64 `DLGPROC`. H1G upgrades that existing subsystem in place. Win64 exception logging uses `Rip/Rsp/Rbp` and the full x64 register set; FP/SIMD diagnostics use `XMM_SAVE_AREA32`; stack signatures, module/symbol offsets, and instruction addresses use `std::uintptr_t`; dynamic DbgHelp binding uses the 64-bit stack/symbol APIs and AMD64 machine type; and the dialog callback returns `INT_PTR`. `SymInitialize` now receives `GetCurrentProcess()` instead of a process ID cast to a handle. No parallel crash helper is added, and deterministic/gameplay/wire/rendering contracts are unchanged. Windows compilation must be rerun before sign-off.
## Step 05H1H — core_debug explicit C-library declarations

The Windows `mingw64-game` validation of H1G reached `core_debug/debug_except.cpp` again at 105/1077 and failed before exercising the new x64 exception code because `sprintf` was no longer declared after the header cleanup. The translation unit now explicitly includes `<cstdio>` for `sprintf` and `<cstring>` for its existing `strcpy` calls rather than relying on unrelated transitive includes. This is a minimal compile-correctness hotfix only: the H1G x64 `CONTEXT`, stack-walk, DbgHelp, dialog-callback, and deterministic/runtime boundaries are unchanged. Windows full-game compilation must be rerun before sign-off.

## Step 05H1I — core_debug macro-safe stack capture

The Windows H1H build advanced past `debug_except.cpp` and stopped at `core_debug/debug_stack.cpp` at 112/1077. `sprintf` lacked its direct `<cstdio>` declaration, and MinGW's DbgHelp headers define `StackWalk` as a macro alias on Win64, rewriting the engine method definition `DebugStackwalk::StackWalk` into an undeclared `DebugStackwalk::StackWalk64`. H1I makes the translation unit self-contained with `<cstdio>`/`<cstring>` and renames the engine-owned operation to `Capture` at its declaration, definition, and four callers. The dynamically loaded DbgHelp API remains `_StackWalk64`; no compatibility facade or extra stack-walk subsystem is introduced. Windows full-game compilation must be rerun before sign-off.

## Step 05H1J — active native-address width sweep

The H1I Windows build cleared `core_debug` and reached the EAC Huffman encoder at 4/826, where progress tracking subtracted pointers only after truncating both to Win32 `long`. H1J fixes that blocker with native pointer-difference arithmetic and proactively removes the same unambiguous truncation class from active WW3D/debug code: locked-surface pixel addressing no longer casts `pBits` through `unsigned int`, sphere scratch-buffer validation compares pointers directly, debug buffer-length checks keep native difference width, and legacy alignment assertions use native `size_t` addresses. Fixed-width compressed-stream fields, gameplay types, wire/replay/Xfer data, and deterministic state are unchanged. Continue the Windows `z_generals -j1` build to expose the next real responsibility blocker.


## Step 05H1K — active D3DX8 dependency isolation

The H1J Windows build cleared the native-address sweep and advanced to Zero Hour WW3D `assetmgr.cpp` at 132/947, where the active x64 graph still directly included `d3dx8core.h`. H1K performs a bounded active-graph D3DX8 sweep rather than fixing only that include. Unused D3DX8 includes are removed from `assetmgr.cpp`, texture loading, height-map/view/client files and the Zero Hour shadow/web-browser callers; point orientation and sorting transforms now preserve the old D3D transform convention through WWMath; shared Bezier math now uses `Matrix4x4`/`Vector4`; missing-texture mip generation reuses `BitmapHandlerClass::Create_Mipmap_B8G8R8A8`; and Evolution `SurfaceClass` copy/scale uses the existing CPU bitmap conversion path while the archival non-Evolution branch retains its historical D3DX behavior. No D3DX compatibility layer is introduced, no serialized asset/game/network format changes, and genuine shader/water/terrain/tree renderer responsibilities remain for coherent D3D12 migration instead of mechanical substitution. Continue the Windows `z_generals -j1` build to expose the next real renderer/resource responsibility blocker.

## Step 05H1L candidate — active WW3D pointer-difference/warning cleanup
- Windows H1K validation advanced to `GeneralsMD/.../WW3D2/assetmgr.cpp` at 125/940.
- Replaced four active Zero Hour asset-name pointer-to-`int` subtraction sites with pointer-difference arithmetic before the required bounded `int` length conversion.
- Corrected `PrimitiveAnimationChannelClass::KeyClass::Set_Time` from `float` to `void`; its only caller ignores a result and the old body returned nothing.
- Extended the x64 platform policy to forbid those address-truncating forms and the non-void/no-return setter regression.
- No deterministic/wire/W3D serialized widths changed. Windows sign-off remains pending the next `z_generals -j1` build.

## Step 05H1M candidate — Render2D backend-vertex container contract
- Windows H1L validation cleared the asset-name/warning slice and advanced to `GeneralsMD/.../WW3D2/render2d.cpp` at 154/942.
- The first real Evolution Render2D path uses `DynamicVectorClass<RenderBackendColorVertex>`; Westwood's generic vector vtable instantiates value equality/ID operations and therefore requires element equality operators.
- `RenderBackendColorVertex` and `RenderBackendTexturedVertex` now provide explicit `operator==`/`operator!=` without changing their storage layout or D3D12 upload representation.
- The D3D12 policy locks this renderer-neutral value contract so later textured caller migration cannot regress into the same template failure.
- No deterministic, wire, W3D/W3X asset, replay, CRC, RNG, Xfer, or gameplay state changes. Windows sign-off remains pending the next `z_generals -j1` build.


## Step 05H1N candidate — point orientation / runtime pointer / intrusive-delete cleanup
- Windows H1M validation cleared the real Evolution Render2D blocker and advanced the normal `z_generals` graph to `Core/.../WW3D2/pointgr.cpp` at 188/942.
- `pointgr.cpp` now rotates the two ground-multiplier vectors with `Matrix3D::Rotate_Vector()`, preserving the H1K negative-Z rotation without depending on the disabled `ALLOW_TEMPORARIES` operator overload.
- `SoundSceneObjClass::On_Event` now carries its runtime-only callback parameters as `std::uintptr_t`; `EVENT_LOGICAL_HEARD` no longer truncates listener/sound pointers through `uint32`. Deterministic/audio object IDs remain `uint32`.
- `PivotMapClass` and `SnapPointsClass` now override `Delete_This()` at the complete-object class, avoiding deletion from their nonzero-offset secondary `RefCountClass` subobject.
- x64/D3D12 policy guards cover all three fixes. Clean host validation rebuilt 46/46 actions and passed 26/26 tests.
- H1N is locally validated only until the next real Windows `cmake --build --preset mingw64-game --target z_generals -- -j1` run.

### Step 05H1AB candidate — BrowserHost target dependency

The real Windows `mingw64-game` continuation after H1AA advanced to 55/107 and then stopped in `W3DDisplay.cpp` because `browserhost.h` was not on the `z_gameenginedevice` include path. H1AB makes `core_browserhost` a direct private dependency of `z_gameenginedevice`, which propagates BrowserHost's public include directory and records the implementation ownership of the browser calls in `W3DDisplay.cpp` / `W3DWebBrowser.cpp`. No renderer behavior or deterministic state is changed. Windows sign-off remains pending the next real `z_generals` build.
