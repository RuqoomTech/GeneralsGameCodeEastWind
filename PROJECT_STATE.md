# Project state

Updated: 2026-09-19

## Current baseline

Step 04 is complete and Windows-signed-off. The supported Evolution path is x64-only.

Step 05A is Windows-signed-off. Step 05B's temporary standalone D3D12 shell is also Windows-signed-off: MinGW-w64 GCC 16.2 built it successfully, Intel UHD 770 and WARP both presented frames, the staged executable ran, and the focused graph passed 26/26.

Step 05C2 through Step 05G are Windows-signed-off. The production D3D12 backend lives behind the existing WW3D renderer seam; the standalone `Evolution/` tree is gone, the Windows graph passes the D3D12/determinism/Evolution gates, canonical HLSL is staged beside the executable, persistent indexed geometry uses default-heap buffers, and sampled RGBA8 textures use shader-visible SRVs plus the static sampler. Step 05H is active. Step 05H1 opened the normal `z_generals` x64 build graph and migrated the first real untextured `Render2D` caller. Step 05H1A is the current locally sealed hotfix candidate after the first Windows full-game compile exposed a Win32-only WWSaveLoad pointer-identity assumption.

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
8. **05H — real `z_generals` x64 D3D12 migration — IN PROGRESS.** 05H1 enables the normal Zero Hour game graph and routes the first untextured `Render2D`/`W3DDisplay` screen-space caller through `IRenderBackend`. 05H1A fixes the first real x64 compile blocker in WWSaveLoad using fixed-width persistence tokens rather than serialized addresses. Continue from actual `z_generals` compiler/linker failures.
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
