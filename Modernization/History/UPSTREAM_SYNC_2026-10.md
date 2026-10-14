# Upstream sync — October 2026

Status: **DONE to upstream `79e87d082`** (merged 2026-10-15). The planned range ended at
`0e6a06c7c` (waypoints 1-11, 2026-10-14); upstream moved on during the sync, and the 31 commits
after it were merged on the owner's request as waypoint 12 and a combined waypoint 13-15.

Branch: `sync/upstream-2026-10-11` (local only). Safety tag: `pre-upstream-sync-2026-10-11`.

## Inputs

- EastWind: `f0021f294` (`main`, 2026-10-11).
- Upstream: `TheSuperHackers/GeneralsGameCode` `main` at `0e6a06c7c` (2026-10-11), extended to
  `79e87d082` (2026-10-14) during the sync.
- Common ancestor: `ab0e4fc04` (2026-09-12). EastWind is 117 commits ahead, upstream 161 ahead.

## Summary

| # | Upstream range | Commits | Branch commit | Tests | Unresolved symbols |
|---|---|---|---|---|---|
| — | baseline `f0021f294` | — | — | 31 / 32 | 48 |
| 1 | `ab0e4fc04..efe3d03e6` | 46 | `73ebcd0ff` | 31 / 32 | 48 |
| 2 | `..a821f54d7` | 2 | `5d67a0f6e` | 31 / 32 | 48 |
| 3 | `..1747544e0` | 21 | `c94b346ee` | 31 / 32 | 48 |
| 4 | `..7ceb579a7` | 10 | `a9f60207a`, `d7f878d56` | 31 / 32 | 48 |
| 5 | `..a932455f9` | 16 | `dc67d149b` | 31 / 32 | 48 |
| 6 | `..b687c073b` | 2 | `91059570d` | 31 / 32 | 48 |
| 7 | `..edbc165c3` | 9 | `74ba2e0fc` | 31 / 32 | 48 |
| 8 | `..adac468d7` | 22 | `57ba19aca` | 31 / 32 | 48 |
| 9 | `..53fb940c9` | 10 | `b757e2d36` | 31 / 32 | 48 |
| 10 | `..fd62135da` | 6 | `f14a231dd` | 31 / 32 | **47** |
| 11 | `..0e6a06c7c` | 17 | `3baa06edd` | 31 / 32 | 47 |
| 12 | `..4f71d3791` | 9 | `6cdbc6c8b` | not gated alone | — |
| 13-15 | `..79e87d082` | 22 | `d03d1ed96` | 31 / 32 | 47 |

The one failing test is `d3d12_backend_smoke`, failing the same way at every step (see the
baseline). Unresolved symbols are counted at the final `z_generals` link. Waypoint 10's static
`Renderer` bridge resolved `DX8Wrapper::Clear`, and no step added a symbol to the baseline
list. No determinism or protocol fixture changed, and none was regenerated. Waypoints 12 and
13-15 were merged back to back and gated once, at the end, on the owner's request to save
time; the gate result therefore covers both commits together.

What needs the owner's attention:

- **`RETAIL_COMPATIBLE_CRC` is still `(1)`** in `GameDefines.h`. Upstream guards many of its
  fixes with `#if !RETAIL_COMPATIBLE_CRC` (401 `#if`/`#elif` lines naming it, in 158 files, at the end of the sync), so those
  fixes are merged as text but compiled out here. Retail compatibility is otherwise retired in
  EastWind (review finding 5); setting the macro to `0` would enable them, and would change
  CRC behaviour. Left for the owner.
- **GPU verification is partial on this machine.** The smoke test stops at the sampler group,
  so renderer merges (shroud, glyph cache, Renderer bridge) are compile- and link-verified
  only. The bridge is not exercised by any test; the D3D12 smoke test creates its backend
  directly.
- **GNU ld needs archive order that upstream (MSVC) does not.** Upstream's link-list pruning
  removed `z_gameengine` from the executable once (waypoint 8). Every later upstream CMake
  change to link lists should be compared entry by entry, not resolved wholesale.
- **Unbuilt targets**: base Generals, the tools (GUIEdit, WorldBuilder, W3DView) and
  upstream's GoogleTest/benchmark targets took upstream CMake changes without being built by
  any gate.
- **New DX8 caller from upstream.** Terrain-conforming particles (#3245, waypoint 12) add
  `W3DTerrainParticles.cpp`, written against `DX8Wrapper`. It adds no new unresolved symbol,
  but 15 of the link's references (116 to 131); it needs a D3D12 port like the other callers.
- **Header names that collide under GCC.** Upstream's debug library now exports
  `debug/debug.h` through an `Include` directory, which shadows libstdc++'s own
  `<debug/debug.h>` in any target that has that directory on its path. Today that is only
  `core_debug` and `core_profile_legacy`; MSVC has no such header, so upstream will not notice
  if it spreads further.
- **Disk**: D: had about 5 GB free throughout. The C++ module-scanning incident in waypoint 7
  filled it once.

## Upstream after the target

During the sync, `upstream/main` was fetched forward to `79e87d082` (2026-10-14), 31 commits past
the planned target. They were merged as waypoint 12 and a combined waypoint 13-15 (below), so
`sync/upstream-2026-10-11` contains all of upstream as of that fetch. A later sync should start
from `79e87d082` and use the same staged method.

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

### Waypoint 6 — `a932455f9..b687c073b` (2 commits)

Arrived: two tree-wide formatting commits, 287 files: trailing comments moved off opening
brace lines (`#3351`), and leading `&&` / `||` in multi-line conditions moved to the end of
the previous line (`#3352`). No behaviour change.

Conflicts (20 files, 69 blocks):

| File | Resolution |
|---|---|
| `BaseHeightMap.cpp`, `FlatHeightMap.cpp`, `HeightMap.cpp`, `W3DDisplay.cpp`, `W3DShaderManager.cpp`, `Water/W3DWater.cpp`, `Water/W3DWaterTracks.cpp` (Core device); `WW3D2/dynamesh.cpp`; `WWLib/registry.cpp`; `Shadow/W3DProjectedShadow.cpp`, `Shadow/W3DVolumetricShadow.cpp`, `W3DBridgeBuffer.cpp`, `WW3D2/ddsfile.cpp`, `WW3D2/mesh.cpp` (both games) | EastWind's side kept in every block. Each block is code EastWind rewrote for the D3D12 backend or x64, overlapping a line upstream only reformatted. |
| `WW3D2/dx8renderer.cpp` (deleted by EastWind) | Stays deleted; upstream change is formatting. |

Consequence: the 69 kept blocks still use the old formatting (trailing brace comments, leading
operators). Everything outside them took upstream's style. A later formatting pass over these
19 files would close the gap; it is cosmetic.

Adapted outside conflict hunks: none. Not applicable: the `dx8renderer.cpp` formatting.

Result: tests build clean; `ctest` 31 of 32 (same sampler failure); `z_generals` compiles with no
errors, 48 unique unresolved symbols / 120 references, identical to the baseline.

### Waypoint 7 — `b687c073b..edbc165c3` (9 commits)

Arrived: Generals crash and manual-control fixes for a scripted Particle Cannon attack on an
object (`#3355`); a bounds assert in `GarrisonContain::findClosestFreeGarrisonPointIndex`
(`#3357`); the `deps_` prefix on every target under `Dependencies/` (`#3367`); file path
tokenization fixes in `ArchiveFile`, `ArchiveFileSystem` and `Win32LocalFileSystem`, and token
clearing in `AsciiString::nextToken` / `UnicodeString::nextToken` (`#3374`); STL utility
polish (`#3375`); Tracy development files excluded from the install target (`#3382`); CMake
comment and whitespace cleanup (`#3386`). The CMake minimum rises from 3.25 to 3.28 (needed for
`EXCLUDE_FROM_ALL` in `FetchContent_Declare`); the installed CMake is 4.4.3.

Target renames adopted everywhere, including EastWind's own additions (`Core/Tests`, the root
tests-only graph):

| Old | New |
|---|---|
| `core_precompiled` | `deps_precompiled` |
| `core_utility` | `deps_utility` |
| `binkloader` | `deps_bink` |
| `milesloader` | `deps_miles` |
| `dbghelploader` | `deps_dbghelp` |
| `usp10loader` | `deps_usp10` |
| `maxsdk` | `deps_maxsdk` |

Conflicts (16 files, all build files):

| File | Resolution |
|---|---|
| `CMakeLists.txt` | EastWind's layout kept (tests-only graph adds `Core/Libraries/Include`; Bink, Miles, DbgHelp and Usp10 stay inside the full-runtime branch). The non-Windows placeholder targets renamed to `deps_dbghelp` / `deps_usp10`. Minimum version 3.28 taken. |
| `CMakePresets.json` | EastWind's presets kept; `cmakeMinimumRequired` raised to 3.28 to match. |
| `Core/GameEngine/CMakeLists.txt`, `Core/GameEngineDevice/CMakeLists.txt`, `Core/Libraries/Source/WWVegas/CMakeLists.txt`, `Generals/.../WWVegas/CMakeLists.txt`, `GeneralsMD/.../WWVegas/CMakeLists.txt`, `Generals/Code/Tools/W3DView/CMakeLists.txt` | EastWind's link lists kept (no `d3d8lib`, `d3d8`, `d3dx8`), with the new target names. Upstream also re-sorted these lists, so a plain "ours" resolution silently dropped four entries that had only moved; `deps_dbghelp`, `deps_bink`, `deps_usp10` and `deps_miles` were put back by hand and every file's reference count was checked against the pre-merge tree. |
| `Core/Tools/WW3D/max2w3d/CMakeLists.txt` | Ours: no `d3d8lib` (upstream only reworded the comment above it). |
| `cmake/debug_strip.cmake`, `cmake/mingw.cmake`, `cmake/reactos-atl.cmake`, `cmake/widl.cmake`, `cmake/zlib.cmake` | Ours. Upstream's change is comment and whitespace cleanup on the 32-bit MinGW versions of these files, which EastWind rewrote for the x64 lane. |
| `Core/Libraries/Source/EABrowserEngine/CMakeLists.txt`, `cmake/toolchains/mingw-w64-i686.cmake` (deleted by EastWind) | Stay deleted; upstream's changes are the target rename and a removed comment. |

Adapted outside conflict hunks: `Core/Tests/CMakeLists.txt` (14 link lines to `deps_utility`).
Brought forward from a later upstream commit: `set(CMAKE_CXX_SCAN_FOR_MODULES OFF)` in
`cmake/compilers.cmake` (upstream `917613099`, `#3442`, which belongs to waypoint 11). With the
minimum at 3.28 CMake turns on C++20 module scanning (policy CMP0155). The first gate run
added a scan step per source, wrote 5.2 GB of `.ddi` files into `build/mingw64-game` and
filled drive D: (106 GB, 30 MB left), so 208 compile steps failed with "No space left on
device". The scan files were deleted and the gate re-run with scanning off. D: has about 5 GB
free after that, which is little headroom for an 8 GB build tree.
Not applicable: the comment cleanup in the five rewritten `cmake/` files and the two deleted
files.

Finding: upstream guards its Particle Cannon fix with `#if !RETAIL_COMPATIBLE_CRC`, as it does
for 373 preprocessor directives in 156 files. `GameDefines.h` still defines
`RETAIL_COMPATIBLE_CRC` as `(1)` in EastWind, so every upstream fix written this way arrives
compiled out, although the project has dropped retail compatibility. This is review finding 5
with a concrete cost attached. Changing the macro changes simulation results and CRCs, so it
is a decision for the owner, not a merge resolution; nothing was changed here.

Result (second gate run, scanning off): tests build clean; `ctest` 31 of 32 (same sampler
failure); `z_generals` compiles with no errors, 48 unique unresolved symbols / 120 references,
identical to the baseline.

### Waypoint 8 — `edbc165c3..adac468d7` (22 commits)

Arrived: missing null terminators and wrong terminator types in `FieldParse` arrays (`#3361`,
`#3388`); uninitialized variable in `AIPlayer::onUnitProduced` (`#3379`); dock approach bone
count restored after load (`#3151`); terrain deformation scan bounds (`#3258`); Generals retail
fixes for `AIAttackState::onEnter` and save games (`#3398`, `#3406`); WorldBuilder
`WHeightMapEdit` unified (`#3368`); drawable physics transforms interpolated between logic
frames (`#2055`); long arrays reflowed one element per line (`#3407`); supply attack timing
and unrecorded damage/healing timestamps (`#3380`); `isInRegion` made consistent (`#3428`);
BIG file timestamps taken from cached info (`#3415`); `WIN32_LEAN_AND_MEAN` and `NOMINMAX`
moved into `precompiled.h` (`#3393`); link libraries a target already inherits removed
(`#3432`); `canAnyForceAttack` checks every selected unit (`#3433`); and `#3405` in four parts:
`deps_config` / `depsi_always` targets with the dependency setup moved into
`Dependencies/CMakeLists.txt`, `DebugSetCrashHandler`, Google Test unit tests, Google Benchmark
benchmarks, plus the CI jobs that run them.

Conflicts (12 files, all build files and one document):

| File | Resolution |
|---|---|
| `CMakeLists.txt` | EastWind's two-mode layout kept, but the dependency list now comes from upstream's `Dependencies/CMakeLists.txt` through one `add_subdirectory(Dependencies)`. Upstream's Google Test / Benchmark includes and its `Tests` / `Benchmarks` subdirectories are wired inside the full-runtime branch only (they link the whole game). An auto-merged second copy of those two blocks outside the branch was removed. |
| `cmake/config-build.cmake` | Both option sets kept: EastWind's `RTS_BUILD_TESTS`, `RTS_BUILD_TESTS_ONLY`, `RTS_BUILD_EVOLUTION_X64`, `RTS_BUILD_HEADLESS_CORE`, and upstream's `RTS_BUILD_OPTION_TESTS`, `RTS_BUILD_OPTION_BENCHMARKS`. `cxx_std_20` moves to `deps_config` as upstream has it; EastWind's `-Wsuggest-override` and `RTS_PERF_TELEMETRY` stay on `core_config`. The Tracy placeholder stays in this file (upstream moved it to the root file) so the focused graph still gets it. |
| `CMakePresets.json` | EastWind's presets kept. Upstream's seven `win32*` test presets not added (see below). |
| `Generals/.../WWVegas/CMakeLists.txt`, `GeneralsMD/.../WWVegas/CMakeLists.txt` | Upstream: `deps_miles` and `stlport` dropped, both inherited from `core_wwcommon`. |
| `Generals/Code/Main/CMakeLists.txt`, `GeneralsMD/Code/Main/CMakeLists.txt` | Upstream's pruning applied to what EastWind links: `deps_bink`, `deps_miles` and the game engine library dropped (inherited through the device library). `dxguid`, and for Generals `d3d12` / `dxgi`, kept in place of upstream's removed DX8 entries. |
| `Generals/Code/Tools/GUIEdit/CMakeLists.txt`, `Generals/Code/Tools/WorldBuilder/CMakeLists.txt` | Upstream's pruning taken. |
| `Generals/Code/Tools/W3DView/CMakeLists.txt`, `GeneralsMD/Code/Tools/W3DView/CMakeLists.txt` | Ours: `d3d12` / `dxgi`, no `d3d8lib`. |
| `TESTING.md` | EastWind's document kept; upstream's "Unit Tests" and "Benchmarks" sections appended under a new heading that states their status in this fork. Upstream's replay-check sentence dropped with the section it belonged to, which EastWind had already replaced. |

Adapted outside conflict hunks:

- `Dependencies/CMakeLists.txt` (new from upstream): an early `return()` under
  `RTS_BUILD_TESTS_ONLY` after the two header targets, so the focused graph still configures
  without Bink, Miles, DbgHelp and Usp10.
- `cmake/tests/RuntimeCompatibilityPolicyTest.cmake`: the two assertions that the root file
  adds the source-built Bink and Miles loaders now check `add_subdirectory(Dependencies)` in
  the root file and `add_subdirectory(Bink)` / `add_subdirectory(Miles)` in
  `Dependencies/CMakeLists.txt`. Same requirement, new location; the two "must not include
  `cmake/bink.cmake` / `cmake/miles.cmake`" assertions are unchanged.

Not applicable: upstream's `win32*` test presets and the CI workflow changes. Their configure
presets stop at EastWind's Windows x64 / D3D12 `FATAL_ERROR`, so the test presets could never
run. `.github/workflows` took upstream's edits by auto-merge, but those jobs do not run for
this fork (review finding 4).

Not verified here: upstream's Google tests. `z_googletest` links `z_gameenginedevice`, the
whole Zero Hour runtime, so it cannot link while `z_generals` has unresolved symbols. The
options exist and default to off. The four tool link lists (GUIEdit, WorldBuilder, W3DView x2)
are also not exercised by any gate.

Link-order fix (found by the gate): upstream dropped `z_gameengine` / `g_gameengine` from the
executables' link lists because they arrive transitively through the device library. MSVC does
not care about archive order; GNU ld does. With the game engine archive placed after the device
archive, the first gate run reported 53 unique unresolved symbols, 5 of them new:
`MOTDSystem`, `OSDisplaySetBusyState`, `ReloadAllTextures` (engine-to-device callbacks) and
`__imp_accept` / `__imp_listen` (winsock, now resolved too late). `z_gameengine` and
`g_gameengine` are listed again ahead of the device library in `GeneralsMD/Code/Main` and
`Generals/Code/Main`, with a comment saying why. Upstream's removal of `deps_bink` / `deps_miles`
from those lists stays: they still arrive transitively and in a working order.

Result (second gate run): tests build clean; `ctest` 31 of 32 (same sampler failure);
`z_generals` compiles with no errors and the link reports 48 unique unresolved symbols, the same
list as the baseline.

### Waypoint 9 — `adac468d7..53fb940c9` (10 commits)

Upstream's #3429 series splits `AIPathfind.cpp` into one file per class: `PathNode`, `Path`,
`PathfindCellInfo`, the shared constants, `PathfindCell`, `PathfindCellList`, the cell helpers,
`ZoneBlock` with the global helpers, `PathfindZoneManager` and `PathfindLayer`. Code moves only.

Conflicts: none. `Core/GameEngine/CMakeLists.txt` auto-merged the new source list. EastWind's
edits to `AIPathfind.cpp` sit outside the moved blocks, so they stay in place, and no policy
test names the pathfinder files.

Result: tests build clean; `ctest` 31 of 32 (same sampler failure); `z_generals` compiles with
no errors and the link reports 48 unique unresolved symbols, the same list as the baseline.

### Waypoint 10 — `53fb940c9..fd62135da` (6 commits)

Taken: `917613099` (#3442, C++ modules scanning off; already brought forward in waypoint 7, so
`cmake/compilers.cmake` merged to the same line), `1b1212f0d` (#3426, uninitialized variables
in Generals `AIPlayer::getPlayerStructureBounds`), `2119cc9b6` (#3438, do not exit a temporary
AI state that was reset during its own update), `2f023dec7` (#3443, rebuild static display
strings only when their hotkey setting changes), `ab568af44` (#3453, the render backend object
becomes a static `Renderer` class) and `fd62135da` (#3417, `Clear` / `Set_Viewport` /
`Set_Gamma` callers routed through `Renderer`). The four non-renderer commits merged cleanly.

The Renderer bridge (the decision taken at planning, "bridge both"):

- Git saw upstream's `IRenderBackend.h -> Renderer.h` as a rename and merged EastWind's
  485-line interface into `Renderer.h`. Undone: `IRenderBackend.h` is restored from EastWind
  unchanged, and `Renderer.h` is upstream's file, with one added comment paragraph saying where
  EastWind defines the methods. Upstream's separate `RenderViewport` struct stays as it is.
- New `Core/Libraries/Source/WWVegas/WW3D2/Backend/Renderer.cpp` defines the eleven
  `Renderer::` statics. `Init` creates the backend through `Create_Render_Backend` and
  `Shutdown` deletes it, the same lifetime `WW3D::Init` / `WW3D::Shutdown` gave it before. The
  other nine forward to that `IRenderBackend` and do nothing while there is no backend, matching
  the null checks EastWind's callers made. `Set_Viewport` copies `RenderViewport` into
  `RenderBackendViewport` (same fields).
- `Backend/RenderBackend.h` gains `Get_Renderer_Backend()`. The auto-merge had also removed
  `WW3D::Get_Render_Backend()` and `WW3D::RenderBackend` (about 30 EastWind callers use them);
  both are back, and `WW3D::Init` now sets `RenderBackend` from `Get_Renderer_Backend()` after
  `Renderer::Init` succeeds and clears it after `Renderer::Shutdown`.
- `Backend/DX8Backend.cpp` (modified upstream, deleted here) stays deleted. `Backend/DX8Backend.h`
  and the `dx8renderer.h` include upstream kept in `ww3d.cpp` are not brought back.

Call-site conflicts:

| File | Resolution |
|---|---|
| `W3DDisplay.cpp` (`Set_Gamma`), `W3DView.cpp` (`Clear` depth only) | Upstream's `Renderer::` call; EastWind's includes kept beside `Renderer.h`, and EastWind's clearer comment kept on the `W3DView` clear. |
| `W3DShaderManager.cpp` | Both includes. The auto-merged `Renderer::Clear` replaces `DX8Wrapper::Clear`, which was one of the 48 unresolved symbols. |
| `W3DScene.cpp` (both games) | Both includes; the auto-merged `Renderer::Clear` calls stay. |
| `camera.cpp` (both games) | Ours: EastWind validates the normalized bounds and skips an empty viewport before calling the backend. |
| `Water/W3DWater.cpp` | Ours: the include list and the retired `WATER_TYPE_1_FB_REFLECTION` path, which upstream still renders through D3DX8 math and `Renderer::Clear`. |
| `ww3d.h`, `ww3d.cpp`, `WW3D2/CMakeLists.txt` | Ours, plus `Renderer.h` and `Backend/Renderer.cpp`, as above. |

Policy tests: no assertion changed. The `D3D12BackendPolicyTest` checks that `DX8Backend.cpp` /
`.h` stay absent and that `IRenderBackend.h` keeps its contents; both still hold.

Result: tests build clean; `ctest` 31 of 32 (same sampler failure); `z_generals` compiles with
no errors and the link reports **47** unique unresolved symbols: the baseline list minus
`DX8Wrapper::Clear`, nothing new. The D3D12 smoke test creates its backend directly, so it does
not exercise the bridge; only the game link does.

### Waypoint 11 — `fd62135da..0e6a06c7c` (17 commits, upstream tip)

Taken as upstream wrote them (clean merges): #3308 (stranded TunnelTracker occupants,
`destroyAllObjectsImmediate` lookup), #3414 (ControlBar / ReplayControl windows deleted on
resolution change and shutdown), #3430 (`memcchr`), #3462 (LANAPI `GameInfo` leaks), #3449's
tree-buffer refill (EastWind's `allocateTreeBuffers` already set `m_anythingChanged`, so the
conflict resolved to ours with the same effect), #3282 (Helix experience from attached-gun
kills), #3471 (`Region` `contains()` / `containsWithin()` rename), #3476 (`fullviewport.dat`
handle closed), the #3288 glyph-cache series (`render2dsentence`, `assetmgr`, glyph caches
discarded on map load and resolution change, stale memory-pool entry removed) and #3475
(`dx8wrapper.cpp`; merged as text, but that file is not in EastWind's build).

Conflicts and adaptations:

| File | Resolution |
|---|---|
| `W3DShroud.h` / `.cpp` (both games) | EastWind's files, plus upstream's row-fill fix `8b0430b9e` (#3440) re-applied on its own: `fillShroudData` advanced the wrong pointer, so only the first row of `m_finalFogData` was filled. Upstream's `08ce89694` (#3440, a manual copy in place of `CopyRects` with a 16-bit array) is not applicable. It works around the NVIDIA D3D8 driver exhausting address space on device reset; EastWind's shroud already copies through the CPU-side `SurfaceClass::Copy`, which clamps to both surfaces. |
| `W3DDisplay.cpp` | Both: EastWind's `DebugSetWindowedMode` call and upstream's glyph-cache discard on resolution change. |
| `W3DTreeBuffer.cpp` | Ours (EastWind's CPU geometry buffers; the refill flag was already set). |
| `render2dsentence.cpp` (no conflict hunk) | Upstream's new glyph-texture sizing asked the D3D8 device for `MaxTextureWidth/Height`. Removed with a comment: Direct3D 12 guarantees 16384 px, above the 2048 px cap upstream applies, so the clamp never applies. Upstream's new buffer sizing only asserts `char_len <= length`, and the assert compiles out of release builds, so `char_len` is kept in the `max` (EastWind's earlier oversized-glyph fix). |
| `cmake/tests/RuntimeCompatibilityPolicyTest.cmake` | The assertion pinned the old `max( (int)CHAR_BUFFER_LEN, char_len )` line that #3288 replaced. It now pins the new line, which still includes `char_len`: same guarantee, not weakened. |

Mistake caught by the gate: the first resolution of `W3DShroud.cpp` took "ours" only for the
conflict blocks, which left upstream's non-conflicting hunks of `08ce89694` in place and broke
the compile (`Byte *` vs `UnsignedShort *`, missing `srcRect` / `dstPoint`). The files were
restored from EastWind's side and only `8b0430b9e` was re-applied, as described above.

Result (third gate run): tests build clean; `ctest` 31 of 32 (same sampler failure);
`z_generals` compiles with no errors and the link reports 47 unique unresolved symbols, the
same list as after waypoint 10.

### Waypoint 12 — `0e6a06c7c..4f71d3791` (9 commits)

Taken as upstream wrote them: #3325 (interlocked functions and atomic adapter for VC6), #3468
(obsolete `RECOIL_FROM_BEING_DAMAGED` code removed), #3148 (`AssaultTransportAIUpdate::xfer()`
serializes all member flags), #3003 (2D diagonal locomotor speed; guarded by the new
`PRESERVE_RETAIL_PHYSICS_FORWARD_SPEED_*` macros, all `(1)`, so behaviour is unchanged here),
#3467 (GameMemory CMake simplified), #3488 (sockets that fail to bind are closed) and #3479
(`FlatHeightMap::ReleaseResources()` releases its tiles on device reset; EastWind keeps
`releaseTiles()`).

Conflicts and adaptations:

| File | Resolution |
|---|---|
| `GameDefines.h` | Upstream's three `PRESERVE_RETAIL_PHYSICS_FORWARD_SPEED_*` macros added; its duplicate of an existing retail-compatibility comment dropped. `ENABLE_TERRAIN_CONFORMING_PARTICLES (1)` merged cleanly. |
| `statistics.h` | EastWind's renderer-neutral names (`RENDER_RECORD_*`), plus upstream's terrain-particle batch counter under the same naming: `RENDER_RECORD_TERRAIN_PARTICLE_BATCH`. |
| `W3DTerrainParticles.cpp` (new, #3245) | Taken, with its statistics macro renamed to match. It draws through `DX8Wrapper` and is a new caller to port (see the owner items). |
| `W3DScreenshot.cpp`, `ww3d.cpp` | EastWind's files. #3477 fixes a leaked D3D8 surface copy; EastWind reads the frame through `IRenderBackend::Read_Output_RGBA8`, so the fix does not apply. |
| `GameMemoryNull.h` → `.inl` (#3467) | Taken. `cmake/tests/RuntimePointerPolicyTest.cmake` follows the rename; its assertions are unchanged and still pass. |

Result: committed without a gate of its own (see waypoint 13-15).

### Waypoint 13-15 — `4f71d3791..79e87d082` (22 commits, upstream tip)

The owner asked for the remaining commits to be merged in one step and tested once at the end,
so the three planned waypoints were merged as one.

Taken as upstream wrote them: #3482 (legacy cliff state buffer freed when loading blend tile
data), #3497 (handles closed in `GameWindowManager`, `ImageCollection`, `MainMenuUtils`), #3500
(portable `memcchr` without SSE2), #3481 (`checkForCrateToPickup()`; behind
`RETAIL_COMPATIBLE_CRC`, so compiled out here), #3436 (INI out-of-range numbers parse as the
legacy `strto*` path did; live here because EastWind builds the C++17 `from_chars` path), #3504
and #3518 (IABS and min/max consolidated), #3505 (`Win32BIGFile`/`StdBIGFile` merged into
`ArchiveFile`, the file systems into `BIGFileSystem`) and #3506 (MiniDumper decoupled from
GameEngine into its own library). EastWind had not changed the ArchiveFile, BIG file or
MiniDumper sources since the fork, so those restructurings lost nothing.

Conflicts and adaptations:

| File | Resolution |
|---|---|
| Header moves (#3456, #3512) | Upstream deleted `Core/Libraries/Include`. `Lib/*` now lives in `Core/Libraries/Source/Lib/Include/Lib`, `rts/profile.h` became `Lib/Profile.h` (EastWind's PerformanceTelemetry declarations came along) and `rts/debug.h` became `Lib/Debug.h`. EastWind's own `rts/asset_3d_format.h` and `rts/w3x_document.h` moved to `Core/Libraries/Source/rts/Include/rts`, next to `w3x_document.cpp`; their include spelling is unchanged. Five EastWind files that included `rts/profile.h` now include `Lib/Profile.h`. |
| Root `CMakeLists.txt` (tests-only graph) | That graph never added `Core/`, and upstream's `core_libraries` is now a static library that builds `Debug.cpp` and `Trig.cpp` and links the profile and debug libraries. The tests graph gets a header-only `core_libraries` interface target instead: the `Lib/Include` directory and the forced `Precompiled/BasePrecomp.h`. |
| `Core/Tests/CMakeLists.txt` | Each `Core/Libraries/Include` path replaced by `Source/Lib/Include` and `Source/rts/Include`. |
| `profile/CMakeLists.txt`, `Lib/CMakeLists.txt` | `performance_telemetry.cpp` stays in `profile/` but builds in `core_libraries`, where its API (`Lib/Profile.h`) lives. In `core_profile_legacy` it failed under GCC: that target has `core_debug`'s `Include` directory on its path, so `<chrono>`'s `#include <debug/debug.h>` found the game's debug header instead of libstdc++'s. `profile/CMakeLists.txt` is now identical to upstream. |
| `Common/Debug.h`, `Debug.cpp` | Upstream's slimmed header plus EastWind's `DebugSetWindowedMode`. `Debug.cpp` keeps EastWind's `s_debugWindowedMode`; upstream's new `extern bool DX8Wrapper_IsWindowed` was not taken, since nothing in EastWind defines it. |
| `wwdebug.h` | Upstream's `Lib/Debug.h` include, plus EastWind's `<cstdint>`. |
| `dx8wrapper.h` (#3516) | Upstream consolidates `F2DW` here as a pointer cast; EastWind's version copies through `memcpy` (strict aliasing, as in waypoint 4). Upstream's `DX8_ErrorCode` / `Log_DX8_ErrorCode` were not taken: EastWind checks results with `WW_CHECK_RESULT` and nothing calls them. |
| `W3DWater.cpp` (#3489) | Ours. Upstream skips rivers whose indices overflow the 16-bit dynamic index buffer; EastWind's river path already rejects `count > 65535` and keeps its D3D12 geometry. |
| Policy tests | `D3D12BackendPolicyTest`, `RuntimeCompatibilityPolicyTest` and `X64PlatformPolicyTest` point at the moved files (`Lib/Include`, `profile/Include/profile/`, `debug/Include/debug/`). No assertion was changed. |

Mistakes caught by the gate: the first run failed `x64_platform_policy` on the three moved
profile and debug headers, and stopped the game build at `performance_telemetry.cpp` (first
`Lib/Profile.h` not found, then the libstdc++ header collision above). Each was fixed as
described.

Result (third gate run, covering waypoints 12 and 13-15): tests build clean; `ctest` 31 of 32
(same sampler failure); `z_generals` compiles with no errors and the link reports 47 unique
unresolved symbols, identical to waypoint 11. References rose from 116 to 131, all from
`W3DTerrainParticles.cpp`. `git rev-list --count HEAD..upstream/main` is 0.
