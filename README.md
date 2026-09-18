# Generals Game Code

GeneralsGameCode is the open-source C++ codebase for *Command & Conquer: Generals* and *Zero Hour*. The repository contains the original engine lineage, community fixes, development tools, and an ongoing Evolution modernization track.

EA does not endorse or support this project. You still need the original game data to run the game.

## Evolution modernization

The active modernization direction is intentionally narrow:

- x64 runtime development;
- CMake + Ninja + MinGW-w64 GCC as the primary Windows command-line toolchain;
- deterministic simulation, replay, CRC, RNG and Xfer behavior preserved;
- Direct3D 12 as the only new renderer target;
- existing Direct3D 8-era code retained as a temporary compatibility/reference path;
- W3D behavior preserved;
- additive EA SAGE XML W3X support;
- Evolution multiplayer compatibility only between builds produced by this project.

There is no planned Direct3D 9 or Direct3D 11 intermediate renderer.

Step 04 completed the x64 migration foundation and retired the active i686 modernization lane. Step 05 is active: the developer baseline is cleaned and Windows-signed-off, and the first standalone x64 Direct3D 12 Evolution runtime shell now precedes W3X parser/import growth so new assets can be tested in the real future process.

## Windows quick start

The focused Evolution test graph does not require Visual Studio.

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\setup-windows-dev.ps1
cmake --preset mingw64-tests
cmake --build --preset mingw64-tests
ctest --preset mingw64-tests --output-on-failure
```

The preset uses MinGW-w64 GCC, Ninja, x64-only platform checks, deterministic/headless validation, the Evolution network/replay protocol gates, and the current W3X parser tests.

For the detailed test inventory and optional explicit checks, see [`TESTING.md`](TESTING.md).

### First Evolution D3D12 executable

The new Evolution process is separate from the legacy D3D8 game executable. Build the current clear/present shell with:

```powershell
cmake --preset mingw64-d3d12-shell
cmake --build --preset mingw64-d3d12-shell
.\build\mingw64-d3d12-shell\Evolution\GeneralsEvolution.exe --frames 120
```

The shell currently proves Win32 windowing, DXGI hardware-adapter selection, D3D12 device/queue/list creation, flip-model swap chain, render-target views, explicit transitions, fences, clear/present and clean shutdown. Game simulation and asset rendering are attached in later slices; see `TESTING.md` and `Modernization/D3D12_RENDERER.md`.

## Legacy/reference builds

The repository still contains historical VC6/MSVC and Direct3D 8-era build/runtime paths used by the wider project. They are not Evolution runtime targets and should not be used as a reason to reintroduce 32-bit assumptions into new Evolution code.

Linux Docker helpers remain available for the legacy/reference build workflow:

```bash
./scripts/docker-build.sh
./scripts/docker-install.sh --detect
```

## Repository guide

For modernization work, start with:

- [`PROJECT_STATE.md`](PROJECT_STATE.md) — current implementation state and next work;
- [`MODERNIZATION.md`](MODERNIZATION.md) — modernization goals and document index;
- [`Modernization/ROADMAP.md`](Modernization/ROADMAP.md) — ordered milestones;
- [`Modernization/DECISIONS.md`](Modernization/DECISIONS.md) — locked architecture decisions;
- [`Modernization/ARCHITECTURE_GUARDRAILS.md`](Modernization/ARCHITECTURE_GUARDRAILS.md) — invariants new code must preserve;
- [`Modernization/WORKLOG.md`](Modernization/WORKLOG.md) — chronological engineering history.

Historical milestone specifications remain under `Modernization/History/`. They are provenance, not the primary day-to-day developer interface.

## Dependency management

The repository uses a vcpkg manifest (`vcpkg.json`) for supported vcpkg builds. The Evolution focused MinGW graph keeps dependencies target-scoped and intentionally avoids pulling the legacy renderer/input/audio stack into tests that do not need it.

## Profiling

Performance telemetry remains observational and compile-time gated. `scripts/perf-summary.py` summarizes captured telemetry without introducing a runtime dependency. Renderer GPU timing work belongs to the future D3D12 command-queue implementation rather than the temporary D3D8 path.

## Contributing

Read [`CONTRIBUTING.md`](CONTRIBUTING.md) before submitting changes. Prefer consolidation over adding parallel helpers or duplicate subsystem implementations, and keep changes scoped to a clear architectural responsibility.

The upstream community also maintains the project Wiki and Discord resources for broader build and contribution guidance.

## License

This project is licensed under GPL-3.0. See [`LICENSE.md`](LICENSE.md).
