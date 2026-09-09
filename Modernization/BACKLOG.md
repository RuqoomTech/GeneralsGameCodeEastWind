# Modernization Backlog

Status values: `DONE`, `ACTIVE`, `VERIFY`, `NEXT`, `PLANNED`, `PARTIAL`, `BLOCKED`.

| ID | Work item | Status | Primary gate |
|---|---|---|---|
| 00 | Repository modernization baseline/docs | DONE | Docs/state stored in authoritative tree |
| F0 | Existing `IRenderBackend` / `DX8Backend` seam | PARTIAL | Already present upstream; incomplete abstraction |
| 00 | Repository modernization baseline/docs | DONE | Docs/state stored in authoritative tree |
| F0 | Existing `IRenderBackend` / `DX8Backend` seam | PARTIAL | Already present upstream; incomplete abstraction |
| 01 | Determinism / CRC / Xfer characterization | DONE | Windows MinGW-w64 i686 / GCC 16.2 + Ninja `z_determinismcheck` passed on 2026-09-10 |
| 02 | MinGW-w64 GCC + Ninja canonical build | DONE* | User reports Step 02B Windows build path working; formal console transcript not archived in this tree |
| 02A | Canonical presets / focused tests / dependency scoping | DONE* | Accepted with Step 02; formal Step 02 Windows transcript not archived |
| 02B | Runtime configure/install hardening | DONE* | User reports real Windows path working; output transcript not archived |
| 03 | HD-mod performance telemetry | DONE | Schema v2 update/render phases + visibility/resource counters + summary tool |
| 03A | Render-frame telemetry / CSV schema v1 | DONE | Folded into completed Step 03 schema v2 |
| 03B | Update-phase / visibility telemetry + capture summary | DONE | Local GCC/Clang focused regression; observational only |
| 04 | x64 engine migration | ACTIVE | Staged port with x86 compatibility/reference retained |
| 04A | x64 readiness build lane | DONE* | `mingw64-tests`, shared MinGW toolchain, architecture-width guard; Windows transcript pending |
| 04B | Pointer/handle correctness | NEXT | Remove/trap address truncation without changing fixed wire IDs |
| 04C | Allocator/pool/container x64 safety | PLANNED | Pointer-sized metadata/alignment with x86 parity |
| 04D | Serialization/network/native-layout separation | PLANNED | Preserve Step 01 byte fixtures on x64 |
| 04E | x64 common + game-logic compile lane | PLANNED | Deterministic core builds without renderer dependencies |
| 04F | x64 client/platform bring-up | PLANNED | Win32/input/audio/file dependencies |
| 04G | Full x64 executable handoff | PLANNED | Evolution runtime executable while x86 remains reference |
| W3X-A0 | W3D/W3X format recognition | DONE | Safe extension/content classification |
| W3X-A1 | W3X document-envelope probe | DONE | Root/namespace/SAGE envelope recognition |
| W3X-A2 | W3X top-level child-element discovery | DONE | Direct-child classification without content decoding |
| W3X-A2R | Consolidate W3X A1/A2 parser seam | DONE | One public API + one shared implementation; no duplicate parser headers |
| 05 | W3X-A parser/import foundation | PLANNED | Parse/validate child content without renderer |
| 06 | HD texture pipeline | PLANNED | Efficient/correct 2K/4K assets |
| 07 | High-poly / 32-bit geometry | PLANNED | >65k-class geometry path where needed |
| 08 | Instancing/batching expansion | PLANNED | Reduce repeated-object CPU/draw cost |
| 09 | LOD/visibility modernization | PLANNED | Scalable dense scenes |
| 06 | HD texture pipeline | PLANNED | Efficient/correct 2K/4K assets |
| 07 | High-poly / 32-bit geometry | PLANNED | >65k-class geometry path where needed |
| 08 | Instancing/batching expansion | PLANNED | Reduce repeated-object CPU/draw cost |
| 09 | LOD/visibility modernization | PLANNED | Scalable dense scenes |
| 10 | x64 runtime stabilization / Evolution runtime | PLANNED | Heavy-mod/replay/network soak after Step 04 bring-up; break 32-bit memory ceiling safely |
| 11 | Renderer architecture boundary completion | PLANNED | D3D12-ready renderer-neutral submission |
| 12 | D3D12 foundation | PLANNED | Device/swapchain/commands/fences |
| 13 | D3D12 resource system | PLANNED | Buffers/textures/descriptors/barriers |
| 11 | Renderer architecture boundary completion | PLANNED | D3D12-ready renderer-neutral submission |
| 12 | D3D12 foundation | PLANNED | Device/swapchain/commands/fences |
| 13 | D3D12 resource system | PLANNED | Buffers/textures/descriptors/barriers |
| 14 | DXC / shader / PSO system | PLANNED | Modern pipeline infrastructure |
| 15 | D3D12 W3D + W3X mesh rendering | PLANNED | Representative model parity |
| 16 | D3D12 terrain | PLANNED | World rendering parity |
| 17 | Modern materials + W3X mapping | PLANNED | Rich materials with legacy fallback |
| 18 | Modern shadows | PLANNED | Scalable shadow maps/LOD |
| 19 | D3D12 particles/effects | PLANNED | Dense-effects scalability |
| 20 | HDR/post-processing | PLANNED | Modern presentation pipeline |
| 21 | Modern lighting | PLANNED | RTS-appropriate dynamic lighting |
| 22 | Texture residency/streaming | PLANNED | Large HD-content VRAM control |
| 23 | W3X completeness/tooling | PLANNED | Documented support matrix/toolchain |
| 24 | Advanced GPU optimization | PLANNED | Only measured bottlenecks |

`DONE*` records user acceptance of the Step 02 Windows path without claiming an archived Windows test transcript.
## Cross-cutting backlog

These are not separate numbered milestones but must be addressed as their owning step touches them:

- Generals / GeneralsMD duplication reduction;
- Generals / GeneralsMD duplication reduction;
- consolidation before expansion: reuse or refactor shared/Core modules instead of adding overlapping headers/helpers;
- test-only configurations that avoid unrelated dependency fetches;
- target-scoped dependencies;
- resource lifetime/ownership clarity;
- test-only configurations that avoid unrelated dependency fetches;
- target-scoped dependencies;
- resource lifetime/ownership clarity;
- crash/device-loss diagnostics;
- benchmark capture automation;
- asset validation/error messages;
- modder-facing documentation.
