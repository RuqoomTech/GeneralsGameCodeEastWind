# Project State — Authoritative Modernization Baseline

This file is the authoritative state marker for modernization work. Read it before applying patches or beginning a new milestone.

## Baseline identity

- Authoritative source upload: `GeneralsGameCode-main(2).zip`
- Baseline SHA-256: `3fa2e6807e842ba51cfa67b5251e4c7f0bdaaedc3dbea5c1f69ddfdde94f08fd`
- Baseline adoption date: 2026-09-09
- This repository replaces older modernization planning snapshots as the source of truth.

## What was changed when establishing this baseline

Only documentation files were added/updated to establish the roadmap and project state. No C/C++, CMake, shader, renderer, gameplay, CRC, replay, network, asset-loader, or build-script implementation was intentionally changed as part of this baseline documentation pass.

## Verified implementation already present in the uploaded source

### Modern C++ / build groundwork

- CMake 3.25 minimum.
- Modern non-VC6 builds request C++20.
- Visual Studio 6 remains a historical build path.
- Modern Windows presets already use Ninja Multi-Config for the standard configuration family.
- A MinGW-w64 i686 toolchain and presets already exist, but currently use `Unix Makefiles` rather than the target Ninja workflow.
- MinGW-specific ATL/WIDL/debug-symbol support exists.

### Renderer separation groundwork

A partial renderer backend seam already exists in `Core/Libraries/Source/WWVegas/WW3D2`:

- `IRenderBackend.h`;
- `Backend/RenderBackend.h`;
- `Backend/DX8Backend.h/.cpp`;
- `WW3D::Get_Render_Backend()` routing for a small but real set of operations.

The current concrete backend remains the legacy Direct3D 8 / `DX8Wrapper` path. The abstraction is **partial**, not a complete renderer-neutral API.

### W3D asset path

- Legacy W3D loading is implemented through `WW3DAssetManager::Load_3D_Assets` and the chunk/prototype-loader system.
- The Zero Hour copy currently lives under `GeneralsMD/Code/Libraries/Source/WWVegas/WW3D2/assetmgr.*`, with a corresponding Generals copy.
- There are currently no W3X loader references in this baseline.

## Locked direction

1. Preserve deterministic simulation, CRC, Xfer, replay, and network behavior unless a change is explicit and tested.
2. Make command-line CMake + Ninja builds the primary development workflow; Visual Studio IDE must not be required.
3. Use MinGW-w64 GCC as the primary Windows compiler path, with Clang as a secondary path.
4. Optimize for a large high-poly / HD-texture Zero Hour mod.
5. Keep the existing DX8-era renderer only as the temporary compatibility/reference path.
6. Build the new Evolution renderer directly on **Direct3D 12**.
7. Do **not** build D3D9 or D3D11 intermediate backends.
8. The long-term Evolution runtime is **x64 + D3D12**.
9. Add native support for the EA SAGE **W3X** asset format alongside W3D.
10. Rendering must remain downstream of simulation; renderer/GPU timing must never influence deterministic game state.

## Current modernization status

| Area | State |
|---|---|
| Baseline documentation and roadmap | **Done** |
| Existing upstream renderer backend seam | **Partial / already present** |
| Determinism/CRC/Xfer characterization | Planned — Step 01 |
| MinGW-w64 GCC + Ninja canonical build | Planned — Step 02 |
| HD performance telemetry | Planned — Step 03 |
| x86 memory-survival work | Planned — Step 04 |
| W3X format-recognition pre-step | **Done — A0** |
| W3X XML parser/import foundation | Planned — Step 05 |
| HD texture pipeline | Planned — Step 06 |
| 32-bit/high-poly geometry path | Planned — Step 07 |
| Expanded instancing/batching | Planned — Step 08 |
| Modern LOD/visibility | Planned — Step 09 |
| x64 Evolution runtime | Planned — Step 10 |
| Renderer boundary completion | Planned — Step 11 |
| D3D12 renderer | Planned — Steps 12+ |

## Small W3X pre-step completed after baseline adoption

W3X-A0 adds only a dependency-free W3D/W3X format-recognition primitive and standalone test. It is intentionally not wired into the runtime asset manager yet. This does not change the main milestone order.

## Next implementation milestone

**Step 01 — Determinism / CRC / Xfer Characterization**.

This is deliberately before the primary GCC/Ninja compiler migration acceptance gate. Compiler changes must not silently change simulation bytes or replay CRC behavior.
