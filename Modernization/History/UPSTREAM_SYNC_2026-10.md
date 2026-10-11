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

