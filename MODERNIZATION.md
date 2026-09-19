# Evolution modernization

This repository carries a staged modernization of the Generals / Zero Hour engine. The goal is to preserve proven simulation and content behavior while removing the major runtime limits that block large modern mods.

## Locked direction

- Evolution runtime: x64 only.
- Primary Windows toolchain: CMake + Ninja + MinGW-w64 GCC.
- Renderer: Direct3D 12 only for new Evolution rendering work.
- Legacy Direct3D 8 code: compatibility/reference only during migration.
- Determinism: simulation, CRC, replay, RNG, Xfer and snapshots remain stable.
- Wire/replay data: explicit fixed-width serialization; never native object layout or pointer state.
- Assets: W3D remains supported; W3X is additive EA SAGE XML support.
- Multiplayer: future compatibility is required between Evolution builds, not retail 32-bit clients.
- Code organization: consolidate shared/Core functionality before adding another helper/module.

## Current phase

Steps 00-04 are complete. Step 04 established the x64 deterministic/network/replay foundation and retired the active i686 modernization lane after real Windows MinGW-w64 validation.

**Step 05 is active: renderer-first modernization + W3X asset foundation.**

Step 05A normalized the repository and is Windows-signed-off. Step 05B proved the required Win32/DXGI/D3D12 fundamentals on real Windows hardware. Step 05C2 is Windows-signed-off with D3D12 integrated behind the existing WW3D backend seam. Step 05D now adds the first indexed primitive path on that production backend. W3X parser/import work resumes after the D3D12 indexed-mesh foundation so W3D and W3X can converge on one renderer rather than two runtime architectures.

## Primary documents

Day-to-day development should normally need only these files:

1. [`PROJECT_STATE.md`](PROJECT_STATE.md) — current state, baseline and next work.
2. [`Modernization/ROADMAP.md`](Modernization/ROADMAP.md) — milestone order.
3. [`Modernization/DECISIONS.md`](Modernization/DECISIONS.md) — locked decisions.
4. [`Modernization/ARCHITECTURE_GUARDRAILS.md`](Modernization/ARCHITECTURE_GUARDRAILS.md) — invariants.
5. [`TESTING.md`](TESTING.md) — current validation commands and test inventory.
6. [`Modernization/WORKLOG.md`](Modernization/WORKLOG.md) — chronological history.

Subsystem references remain available when needed:

- `Modernization/W3X_IMPORT.md`
- `Modernization/W3X_SUPPORT.md`
- `Modernization/ASSET_PIPELINE.md`
- `Modernization/D3D12_RENDERER.md`
- `Modernization/PERFORMANCE_TARGETS.md`
- `Modernization/BUILD_SYSTEM.md`

Completed milestone specifications are retained under `Modernization/History/` as historical design/sign-off records. They should not be treated as the current command reference when `TESTING.md` or `PROJECT_STATE.md` says otherwise.

## Definition of done

A modernization milestone is complete only when:

- implementation is present in the authoritative tree;
- deterministic/network/replay invariants relevant to the change still pass;
- command-line tests are reproducible;
- current documentation is updated without duplicating the full history everywhere;
- no unrecorded architectural deviation is introduced.

## Step 05H1A — x64 WWSaveLoad persistence-token hotfix

The first real `mingw64-game` build reached `core_wwsaveload` and exposed a Win32-only pointer cast in `SimplePersistFactoryClass`. The hotfix keeps persisted identity fixed at 32 bits without serializing native addresses: WWSaveLoad assigns `PersistPointerToken` values per save context and performs token-to-native-pointer remapping only in memory. SimplePersistFactory, render-object/dazzle factories, audible-sound identity, and remapped SoundSceneObj attachments use that token contract; runtime-only SoundSceneObj user-object state persists as null. Legacy 4-byte identity fields remain loadable as opaque tokens. Windows full-game compilation must be rerun before sign-off.

## Step 05H1B — native-width x64 crash diagnostics

The second real `mingw64-game` blocker was the old WWLib exception reporter, which still assumed 32-bit registers and DbgHelp addresses. The Evolution Win64 path now uses `uintptr_t`/`DWORD64`, `Rip/Rsp/Rbp`, `STACKFRAME64`, `IMAGE_FILE_MACHINE_AMD64`, `SymFromAddr`, and `StackWalk64`. The required 64-bit DbgHelp entry points live in the existing `DbgHelpLoader`, preserving one loader/resource responsibility rather than adding another helper. The legacy x86 implementation is preprocessor-isolated and does not compile into the active x64 game path. Windows full-game compilation must be rerun before sign-off.

## Step 05H1C — native-width Win32 registry handles

The next real `mingw64-game` build advanced beyond the x64 crash-diagnostics blocker and failed in `WWLib/registry.cpp` because `RegistryClass` still stored an opaque `HKEY` in a 32-bit `int`. The Evolution path now keeps the registry key as `HKEY` for its full native lifetime: construction initializes it to `nullptr`, successful open/create assigns the handle directly, registry APIs receive the native handle directly, and destruction clears it after `RegCloseKey`. The x64 platform policy forbids reintroducing the old `HKEY`-to-`int` truncation or reconstructing a handle from integer storage. This is runtime/native-handle correctness only; registry values and deterministic/wire contracts are unchanged. The next Windows full-game build advanced past this blocker to the WWMath/D3DX8 dependency.

## Step 05H1D — renderer-neutral WWMath / D3D8 matrix isolation

The next real `mingw64-game` blocker was `WWMath/matrix3d.cpp` including `d3dx8math.h`. The dependency was not part of Matrix3D/Matrix4x4 math; it existed only for legacy Direct3D matrix-conversion helpers. D3D matrix conversion is now owned by the archival `DX8Wrapper`, where its remaining DX8 callers already belong. Renderer-neutral `matrix3d`/`matrix4` headers and implementations no longer include D3D8/D3DX8 headers or declare D3D matrix conversion, and unused `D3DXMATRIX` conversion overloads are removed rather than preserved behind an x64 shim. The D3D12 policy locks this ownership boundary. No gameplay/deterministic data or renderer-backend contract changes are involved. Windows full-game compilation must be rerun before sign-off.

## Step 05H1E — pointer-independent legacy profiler thread identity

The next real `mingw64-game` build advanced through the WWMath/D3D8 isolation and stopped while compiling the legacy profile library PCH because `ProfileFuncLevel::Thread::GetId()` converted its internal `ProfileFuncLevelTracer*` directly to 32-bit `unsigned`. That value was only a diagnostic pseudo-ID, not a Windows thread ID or a deterministic contract. The profiler now assigns each tracer a stable logical 32-bit ID under its existing `ProfileFastCS` lock, keeps the tracer pointer internal only for runtime access, and returns the logical ID through `GetId()`. Existing `%08x` CSV result naming therefore remains compatible without truncating or exposing native addresses. The x64 platform policy forbids restoring pointer-derived profiler IDs. No simulation, replay, network, CRC, RNG, Xfer, snapshot, renderer, or persisted game format changes are involved. Windows full-game compilation must be rerun before sign-off.

## Step 05H1F — x64 debug frame identity + exposed warning cleanup

The next real `mingw64-game` build advanced through H1E to `core_debug` and stopped in `debug_debug.cpp`: the debug frame hash and log-group keys still used 32-bit `unsigned` addresses, pointer formatting truncated through `unsigned long`, memory-dump absolute addresses truncated through `unsigned`, and `SkipNext()` only supported 32-bit inline assembly. H1F makes those diagnostic identities native-width with `std::uintptr_t` and uses compiler return-address capture on the active MinGW x64 path. The same slice also fixes the compiler warnings already exposed by the real game build: `ThreadClass` now retains `_beginthread()` handles at native width, `Buffer` and `CriticalSectionClass` delete owned `char[]` storage through the allocated type instead of `void*`, and executable-version comparison carries the module instance as `HINSTANCE` rather than `int`. These are runtime/diagnostic correctness changes only; no deterministic/wire/replay/rendering contract is widened. Windows full-game compilation must be rerun before sign-off.
## Step 05H1G/H1H — native x64 debug exception path and declaration hotfix

H1G moved the remaining `core_debug` exception/stack-walk path to native Win64 registers, pointer-width addresses, DbgHelp64 stack walking, XMM save-state diagnostics, and the correct `INT_PTR` dialog callback. The subsequent real Windows compile exposed one narrow source-declaration omission before that path could finish compiling: `debug_except.cpp` used `sprintf` and `strcpy` without directly owning their standard headers. H1H adds `<cstdio>` and `<cstring>` only; it does not alter the H1G runtime design or any simulation/network/replay/rendering contract. Windows full-game compilation must be rerun before sign-off.
