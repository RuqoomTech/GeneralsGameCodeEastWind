# Project state

Updated: 2026-09-19

## Current baseline

Step 04 is complete and Windows-signed-off. The supported Evolution path is x64-only.

Step 05A is Windows-signed-off. Step 05B's temporary standalone D3D12 shell is also Windows-signed-off: MinGW-w64 GCC 16.2 built it successfully, Intel UHD 770 and WARP both presented frames, the staged executable ran, and the focused graph passed 26/26.

Step 05C2 through Step 05G are Windows-signed-off. The production D3D12 backend lives behind the existing WW3D renderer seam; the standalone `Evolution/` tree is gone, the Windows graph passes the D3D12/determinism/Evolution gates, canonical HLSL is staged beside the executable, persistent indexed geometry uses default-heap buffers, and sampled RGBA8 textures use shader-visible SRVs plus the static sampler. Step 05H is active. Step 05H1 opened the normal `z_generals` x64 build graph and migrated the first real untextured `Render2D` caller. Step 05H1A passed its intended Windows compile blocker and the real `z_generals` build advanced to `WWLib/Except.cpp`. Step 05H1B passed its intended Windows compile blocker and the real `z_generals` build advanced to `WWLib/registry.cpp`. Step 05H1C passed its intended Windows compile blocker and the real build advanced to `WWMath/matrix3d.cpp`. Step 05H1D passed its intended Windows compile blocker and the real build advanced to the legacy profile PCH at 120/1094. Step 05H1E passed its intended Windows compile blocker and the real build advanced to `core_debug/debug_debug.cpp` at 49/1022. Step 05H1F passed its intended Windows compile blocker and the real build advanced to `core_debug/debug_except.cpp` at 105/1077. Step 05H1G reached the intended native x64 exception/stack-walk implementation but Windows GCC exposed one source-declaration defect in `debug_except.cpp`; H1H fixed that and the real build advanced to `core_debug/debug_stack.cpp` at 112/1077. Step 05H1I is the current locally sealed corrective candidate: `debug_stack.cpp` explicitly owns its C-library declarations and the engine-owned stack-capture method is renamed from `StackWalk` to macro-safe `Capture`, while the actual DbgHelp64 entry remains `StackWalk64`.

## Locked architecture

- Evolution runtime is x64-only.
- Direct3D 12 is the only Evolution renderer target; no D3D9/D3D11 intermediate.
- `IRenderBackend` is the current WW3D migration seam. It should grow only for real migrated callers.
- The x64 backend is D3D12. The DX8 backend/source remains 32-bit historical/reference material only while direct legacy call sites are removed.
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
