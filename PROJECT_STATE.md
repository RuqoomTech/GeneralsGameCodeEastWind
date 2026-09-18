# Project state

Updated: 2026-09-18

## Current baseline

Step 04 is complete and Windows-signed-off. The supported Evolution path is x64-only.

Step 05A is Windows-signed-off. Step 05B's temporary standalone D3D12 shell is also Windows-signed-off: MinGW-w64 GCC 16.2 built it successfully, Intel UHD 770 and WARP both presented frames, the staged executable ran, and the focused graph passed 26/26.

Step 05C is implemented locally and supersedes the standalone-shell architecture. The proven D3D12 device/frame code now lives behind the existing WW3D renderer backend seam, the standalone `Evolution/` tree is removed, and the host-portable regression graph passes 26/26 across GCC/Clang/sanitizer configurations. Windows production-backend smoke validation remains pending.

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
3. **05C — in-place D3D12 backend integration — ACTIVE.** Move the proven D3D12 device/frame lifecycle into `WW3D2/Backend`, select it for x64, remove the standalone `Evolution/` tree, and stop linking D3D8/D3DX8 from the x64 game target.
4. **Next renderer slices:** migrate the remaining direct `DX8Wrapper` state/resource/draw call sites into D3D12-capable renderer abstractions until the normal Zero Hour executable links and renders on D3D12.
5. **W3X parser/import:** resume real XML parsing and renderer-neutral W3D/W3X mesh convergence once the D3D12 indexed-mesh path exists to exercise both formats through the same renderer.

The current full x64 game build remains deliberately gated while direct `DX8Wrapper` callers still exist. The gate is removed only when the normal runtime can compile/link without the DX8 renderer implementation; no parallel replacement executable will be maintained.

## Current developer commands

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\setup-windows-dev.ps1
cmake --preset mingw64-tests
cmake --build --preset mingw64-tests
ctest --preset mingw64-tests --output-on-failure
```

See `TESTING.md` for the D3D12 backend smoke gate and explicit deterministic/network checks.

## Known non-blockers

Legacy `GameMemory` warnings such as custom `operator new` returning null remain known technical debt unless they become errors or affect active work.
