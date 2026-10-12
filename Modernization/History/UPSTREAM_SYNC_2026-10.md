# Upstream sync — October 2026

Status: **IN PROGRESS**

Branch: `sync/upstream-2026-10-11` (local only). Safety tag: `pre-upstream-sync-2026-10-11`.

## Inputs

- EastWind: `f0021f294` (`main`, 2026-10-11).
- Upstream: `TheSuperHackers/GeneralsGameCode` `main` at `0e6a06c7c` (2026-10-11).
- Common ancestor: `ab0e4fc04` (2026-09-12). EastWind is 117 commits ahead, upstream 161 ahead.

## Method

This sync is a staged `git merge`, not a snapshot comparison as in Step 04D3. Upstream history
is linear, so it is merged at eleven waypoints, each built and tested before the next. EastWind
remains the architecture authority; the rules are:

- DX8-only files EastWind deleted stay deleted. Each upstream change to them is read, and any
  behaviour fix that still applies under D3D12 is ported.
- Gameplay, AI, save/load, INI and network bug fixes are taken, in both game variants.
- x64, fixed-width, allocator and strict-aliasing code: EastWind wins; upstream intent is
  adapted.
- Renderer call sites already migrated to `IRenderBackend`: EastWind wins.
- CMake: keep `mingw64-*`, keep i686 retired, adopt upstream target names.
- Policy tests: follow moved files by updating paths; assertions are not weakened.
- No determinism or protocol fixture is regenerated without the owner's approval.

## Baseline before the first merge

Rebuilt at `f0021f294` on 2026-10-12, Windows 11, MinGW-w64 GCC 16.2.0, AMD Radeon RX 640.

- `cmake --preset mingw64-tests`, build: 90/90 actions, no errors.
- `ctest --preset mingw64-tests`: **31 of 32 pass.** `d3d12_backend_smoke` fails, the same way
  on three consecutive runs:
  `Material sampler pixel failed expected 191,64,0` (`Core/Tests/D3D12BackendSmokeTest.cpp:404`,
  the trilinear mip-blend case). The state documents record 32/32.
- **inferred** The failure is hardware-dependent rather than a regression: the case expects a
  mip blend fraction of exactly 0.25 within +/-2 of 255, and GPUs differ in LOD fraction
  precision. It was not bisected.
- **verified** The smoke test runs its checks in sequence and stops at the first failing group,
  so on this machine every GPU fixture after the sampler group (terrain, water, shroud,
  filters) does not run. Renderer changes cannot be GPU-verified here until that is addressed.
- `cmake --build --preset mingw64-game --target z_generals`: 1077/1077 actions compile; the
  final link fails with **48 unique unresolved symbols / 120 references**, matching
  `PROJECT_STATE.md`. Most frequent: `DynamicVBAccessClass::WriteLockClass`,
  `DX8Wrapper::Draw_Triangles`, `DX8Wrapper::_Create_DX8_Texture`,
  `DX8Wrapper::Apply_Render_State_Changes`, `DX8Wrapper::Clear`.
- `cmake --preset mingw64-game` took about nine minutes to configure (dependency fetches).

## Waypoints

### Waypoint 1 — `ab0e4fc04..efe3d03e6` (46 commits)

Arrived: CI permission fixes; retail-compatibility CMake option (`#2379`); mesh material colour
fix (`#3246`); pathfinder crash fix, retail-compat restore and three perf commits; replay from
the command line (`#3227`); audio, hash table, Miles provider, filesystem, LAN name, control bar,
point group, terrain visual, INI, game info, model draw and recorder null/overflow fixes;
`MessageDelayMS` conversion; GameLogic map files unified into Core (`#3189`); Xfer version-check
fixes (`#3299`, `#3300`); script engine UB fix; particle system refactor and the move of client
and particle updates out of `W3DDisplay::draw` (`#2709`).

Conflicts (4 files):

| File | Resolution |
|---|---|
| `Core/GameEngine/Include/Common/GameDefines.h` | EastWind keeps the retail-compatibility note at the top of the file; upstream's wording (adds `RETAIL_COMPATIBLE_DATA`) applied there. Upstream's new `RETAIL_COMPATIBLE_DATA` macro taken. |
| `Core/Libraries/Source/WWVegas/WW3D2/pointgr.cpp` | Upstream's new `WWASSERT` kept; its local `Matrix4x4 view` dropped because EastWind's migrated code takes `view` from the caller. Upstream's null-safe `pointSize` form taken for `shiftInc` (same behaviour as EastWind's). |
| `Generals/.../W3DDisplay.cpp`, `GeneralsMD/.../W3DDisplay.cpp` | EastWind's backend device-ready check kept; upstream's `if (!viewsUpdated)` body taken. |

Adapted outside conflict hunks: upstream's new `W3DDisplay::update` in both game variants
tested the D3D8 device (`DX8Wrapper::_Get_D3D_Device8()->TestCooperativeLevel()`). Replaced
with the same `IRenderBackend::Is_Device_Ready()` check `draw` uses, so no new DX8 dependency
enters the x64 graph.

Not applicable: none in this waypoint.

Result: tests build clean; `ctest` 31 of 32 (the same `d3d12_backend_smoke` sampler failure as
the baseline); `z_generals` compiles, link shows 48 unique unresolved symbols / 120 references,
identical to the baseline list.

### Waypoint 2 — `efe3d03e6..a821f54d7` (2 commits)

Arrived: `W3DDisplay`, `W3DDisplayString`, `W3DDisplayStringManager`, `W3DDebugDisplay` and
`W3DDebugIcons` unified and moved from the game trees into `Core/GameEngineDevice` (`#3260`).

Conflicts (1 file):

| File | Resolution |
|---|---|
| `Generals/.../W3DDisplay.cpp` (deleted upstream, modified by EastWind) | Deleted. EastWind's Zero Hour edits were a superset of its Generals edits, and git carried them through the rename; the resulting `Core/.../W3DDisplay.cpp` is byte-identical to EastWind's previous Zero Hour file. |

Not applicable: none. No policy test or CMake script referenced the moved paths.

Result: unchanged from the baseline (31 of 32 tests, 48 unique unresolved symbols).

### Waypoint 3 — `a821f54d7..1747544e0` (21 commits)

Arrived: Visual Studio natvis visualizers; the new `Precompiled/precompiled.h`, force-included
in every target (`#3327`); null-dereference fixes in the control bar and window transitions;
`std::swap` compat fix; drawable and terrain decal fade decoupled from render update (`#3303`);
damage type flag validation on INI parse (`#3344`); `RefCountPtr` raw-pointer construction;
debug pointer removed from `AsciiString` / `UnicodeString`; save/load fixes for
`DockUpdate::xfer` (`#3117`) and `TintEnvelope::xfer` (`#3127`); `SystemAllocator.h`,
`STLUtils.h`, `stringex.h`, `DbgHelpLoader` and `Usp10Loader` moved from WWLib to
`Dependencies/`, with the dbghelp and Uniscribe entry points turned into free functions
(`#3342`).

Conflicts (9 files):

| File | Resolution |
|---|---|
| `CMakeLists.txt` | Upstream's `Dependencies/Precompiled` subdirectory and global force-include taken. EastWind's test-graph ordering kept (`Dependencies/Utility`, then `Core/Tests`). The VC6 `/FIUtility/CppMacros.h` block is gone; `Dependencies/DbgHelp` and `Dependencies/Usp10` are added in the runtime section only, so the focused test graph does not build them. |
| `Core/Libraries/Source/WWVegas/CMakeLists.txt` | `core_wwcommon` links upstream's `dbghelploader` and `usp10loader`; EastWind's removal of `d3d8lib` kept. |
| `GeneralsMD/.../WW3D2/CMakeLists.txt` | EastWind's PCH list structure kept, with upstream's `Precompiled/precompiled.h` as the first header. |
| `Core/.../WWLib/DbgHelpLoader.{cpp,h}` (moved upstream, modified by EastWind) | Deleted at the old path. EastWind's native-width work re-applied to `Dependencies/DbgHelp/DbgHelpLoader.{cpp,h}` (below). |
| `Core/.../WWLib/Except.cpp`, `GeneralsMD/.../System/StackDump.cpp` | EastWind's 64-bit stack walk kept; calls renamed from `DbgHelpLoader::symFromAddr` style to upstream's `DbgHelp::SymFromAddr` free functions. |
| `Core/Tests/CMakeLists.txt`, policy test scripts | Path-only: `Utility/CppMacros.h` became `Precompiled/CppMacros.h`. |

Adapted outside conflict hunks:

- **DbgHelp on x64.** Upstream's new loader exposes only the 32-bit-address API (`StackWalk`,
  `SymGetSymFromAddr`, `SymGetModuleBase`, ...). On Win64 the Windows headers macro-rename
  those names to their `64` variants, so they cannot coexist with explicit 64-bit entry points.
  The seven legacy functions are now compiled only when `_WIN64` is not defined, and
  `SymFromAddr`, `SymGetLineFromAddr64`, `SymFunctionTableAccess64`, `SymGetModuleBase64` and
  `StackWalk64` are added to `namespace DbgHelp` for x64. `load()` fails on x64 when a required
  native entry point is missing, as EastWind's loader did.
- **`core_utility_no_pch` removed upstream.** `core_utility` no longer carries a precompiled
  header, so the 14 test targets that linked the no-PCH variant now link `core_utility`. The
  first gate run caught this: three test objects failed with `Utility/compat.h: No such file`.
- `Utility/CppMacros.h` includes in `EvolutionGameMessageAdapter.cpp`, `RandomValue.cpp`,
  `D3D12Backend.cpp`, `FastAllocator.cpp` and five test sources follow the header to
  `Precompiled/CppMacros.h`.
- `cmake/tests/X64PlatformPolicyTest.cmake` reads `Dependencies/DbgHelp/DbgHelpLoader.cpp` and
  asserts the new spellings (`DbgHelp::StackWalk64`, `DbgHelp::SymFromAddr`,
  `DBGHELP_RESOLVE(...)` for the four native entries). Same assertions, new names; none removed.

Not applicable: none.

Save format note: `#3127` changes `TintEnvelope` version 2 from a 4-byte float to an 8-byte
double (new `Xfer::xferDouble`), so a save written by a build between upstream's two versions
of that tweak would misread. EastWind has produced no such saves (the game does not link). The
`DockUpdate` fix is load-side only. No EastWind determinism or protocol fixture covers either
path, and none was regenerated.

Result: tests build clean after the `core_utility` fix; `ctest` 31 of 32 (same sampler failure);
`z_generals` compiles, 48 unique unresolved symbols / 120 references, identical to the baseline.

### Waypoint 4 — `1747544e0..7ceb579a7` (10 commits)

Arrived: `WeaponSet::xfer` pitch-limit flag fix (`#3145`); pathfinder classes split out of
`AIPathfind.h` into separate headers (`#3334`); a build-warning sweep (`#3358`: debug guards
around `WWASSERT` / `WWDEBUG_SAY` / `SNAPSHOT_SAY` / `DEBUG_LOG`, unused variables,
assignments in conditions, conversion types); `stdint_adapter.h` moved under `precompiled.h`;
`BaseType.h` split into one header per type plus `BaseFunctions.h`; base macros, pragmas and
scalar typedefs moved to a new force-included `Precompiled/BasePrecomp.h`, delivered through a
new `core_libraries` interface target; `BaseType.h` includes removed tree-wide.

Conflicts (23 files):

| File | Resolution |
|---|---|
| `CMakeLists.txt` | Upstream's target-scoped force include taken (the waypoint 3 global `add_compile_options` is gone). |
| `Core/Libraries/Include/Lib/BaseType.h` | Upstream's split taken whole. EastWind's only change, the strict-aliasing-safe `base_type_float_bits` / `base_type_float_from_bits` helpers, re-applied in `Lib/BaseFunctions.h`, where `fast_float_trunc` / `floor` / `ceil` now live. |
| `GameNetwork/NetworkDefs.h`, `NetCommandMsg.h` | EastWind's kept. Upstream moved `Common/MessageStream.h` into `NetworkDefs.h`; EastWind removed it from there on purpose in Step 04B so wire sizes cannot depend on the `GameMessage` layout (`DeterminismPolicyTest.cmake` asserts this). The explicit `Lib/BaseType.h` includes stay. |
| `W3DWater.h`, `W3DBibBuffer.h`, `W3DBridgeBuffer.h` (both games) | EastWind's kept: upstream only dropped `Lib/BaseType.h` next to DX8 buffer includes EastWind had already removed. |
| `W3DTerrainTracks.h` | Both: EastWind's backend includes plus upstream's explicit `WW3D2/vertmaterial.h`. |
| `WWSaveLoad/pointerremap.{cpp,h}`, `saveload.h` | EastWind's fixed-width persist-token signatures kept; upstream's guard rename `WWDEBUG` to `DEBUG_LOGGING` applied to them, and its move of `WWASSERT(0)` outside the guard taken. |
| `WW3D2/meshdebugger.{cpp,h}` (upstream `dx8rendererdebugger`) | EastWind's class name kept; upstream's shape taken (`Add_Mesh` always declared, body guarded). |
| `WW3D2/dx8wrapper.h`, `missingtexture.cpp`, `pointgr.cpp`, `sortingrenderer.cpp`, `streakRender.cpp` | EastWind's migrated code kept. Upstream's hunks here are unused-variable and debug-guard fixes inside DX8 draw paths EastWind already replaced. |
| `WW3D2/dx8renderer.cpp` (deleted by EastWind) | Stays deleted. Upstream change: one `(void)res` and two `DEBUG_CRASHING` guards. |
| `Core/Tools/WW3D/max2w3d/CMakeLists.txt` | Upstream's removal of the direct `maxsdk` link and EastWind's removal of `d3d8lib` both applied. Not built on x64. |
| `GeneralsMD/.../WW3D2/CMakeLists.txt` | EastWind's PCH list kept; first entry is now `Precompiled/BasePrecomp.h`. |

Adapted outside conflict hunks:

- **Focused test graph.** `Int`, `Real` and the base macros are no longer defined by
  `Lib/BaseType.h`; they come from the force-included `BasePrecomp.h`. The tests-only graph
  does not add `Core/`, so the root `CMakeLists.txt` now adds `Core/Libraries/Include` when
  `RTS_BUILD_TESTS_ONLY` is set, and the 14 test targets link `core_libraries`.
- `Core/Tests/DeterminismPrimitivesTest.cpp` includes `Lib/BaseType.h` directly; it reached the
  fast-float functions through `RandomValue.h`, which no longer includes it.

- `cmake/tests/RuntimeCompatibilityPolicyTest.cmake` follows the code: the `memcpy` float-bits
  assertions now read `Lib/BaseFunctions.h` and the `Coord3D operator-()` assertion reads
  `Lib/Coord3D.h`. Same three assertions. The first gate run failed on this until updated.

Not applicable: the `dx8renderer.cpp` warning fixes (file deleted).

Save format note: `#3145` bumps `WeaponSet::xfer` to version 3 (writes `m_hasPitchLimit` where
version 2 wrote `m_hasDamageWeapon` twice); older saves still load. No EastWind fixture covers
it; none was regenerated.

Result: tests build clean; `ctest` 31 of 32 (same sampler failure); full rebuild of `z_generals`
compiles with no errors, 48 unique unresolved symbols / 120 references, identical to the
baseline. Observation: `d3d12_backend_smoke` took about 4 s in the gate run and about 2 minutes
in two later runs of the same binary, failing identically each time; not investigated.

### Waypoint 5 — `7ceb579a7..a932455f9` (16 commits)

Arrived: stack-buffer-overflow fix in `FirewallHelperClass::getManglerName` (`#3346`); path
separator helpers; `ThingTemplate` merged and moved to Core (`#3324`); logging for a bad
primary damage radius on projectile weapons; `INI::loadFileDirectory` no-throw option
(`#3373`); debug cheat code unified from Zero Hour (`#3377`); `ARRAY_SIZE`, the base algorithm
templates (`sqr`, `clamp`, `sign`, ...) and the WWLib pragmas moved under `BasePrecomp.h`
(`#3369`); Borland and Watcom code removed; `GameMode` types moved to their own file (`#3349`);
filesystem and archive simplifications (`#3376`); Generals retail-compatibility fix for
`ParticleUplinkCannonUpdate::update` float ordering (`#3365`).

Conflicts (3 files):

| File | Resolution |
|---|---|
| `Core/Libraries/Include/Lib/BaseFunctions.h` | Upstream's removal of the algorithm templates taken (they moved to the precompiled base). EastWind's `<string.h>` include, needed by the `memcpy` float helpers further down, kept. |
| `Generals/.../GameLogic.cpp`, `GeneralsMD/.../GameLogic.cpp` | Both removals applied: EastWind had moved `setFPMode` to `Core/.../FPUControl.cpp`, upstream moved `toString(GameMode)` to the new GameMode file. Neither stays in `GameLogic.cpp`. |

Adapted outside conflict hunks: none. Not applicable: none.

Result: tests build clean; `ctest` 31 of 32 (same sampler failure, back to about 5 s); full
rebuild of `z_generals` compiles with no errors, 48 unique unresolved symbols / 120 references,
identical to the baseline.

