# Modernization Backlog

Status values: `DONE`, `ACTIVE`, `VERIFY`, `NEXT`, `PLANNED`, `PARTIAL`, `BLOCKED`.

| ID | Work item | Status | Primary gate |
|---|---|---|---|
| 00 | Repository modernization baseline/docs | DONE | Docs/state stored in authoritative tree |
| F0 | Existing `IRenderBackend` / `DX8Backend` seam | PARTIAL | Already present upstream; incomplete abstraction |
| 01 | Determinism / CRC / Xfer characterization | DONE | Windows MinGW-w64 i686 / GCC 16.2 + Ninja `check_determinism` passed on 2026-09-10 |
| 02 | MinGW-w64 GCC + Ninja canonical build | DONE* | User reports Step 02B Windows build path working; formal console transcript not archived in this tree |
| 02A | Canonical presets / focused tests / dependency scoping | DONE* | Accepted with Step 02; formal Step 02 Windows transcript not archived |
| 02B | Runtime configure/install hardening | DONE* | User reports real Windows path working; output transcript not archived |
| 03 | HD-mod performance telemetry | DONE | Schema v2 update/render phases + visibility/resource counters + summary tool |
| 03A | Render-frame telemetry / CSV schema v1 | DONE | Folded into completed Step 03 schema v2 |
| 03B | Update-phase / visibility telemetry + capture summary | DONE | Local GCC/Clang focused regression; observational only |
| 04 | x64 engine migration | DONE | Final Step 04F Windows MinGW x64 run passed 25/25 plus all explicit deterministic/Evolution/x64-platform checks |
| 04A | x64 readiness build lane | DONE | `mingw64-tests` verified on real Windows x64 as part of the Step 04D/04D3 17-test gate |
| 04B | Wire/replay ABI freeze + pointer/handle audit | DONE | Explicit fixed-width wire contract; first native pointer/handle truncations removed |
| 04C | Native-width runtime substrate + dependency bootstrap | DONE | Native allocator/pointer tests verified in the real Windows x64 and frozen i686 focused graphs |
| 04D | x64 deterministic/headless core bring-up | DONE | Windows x64 17/17, i686 17/17, Step 01 oracle pass, and identical 8-checkpoint i686/x64 timeline through frame 12000 |
| 04D3 | Upstream alignment / divergence reduction | DONE | Selected gameplay/runtime fixes imported/adapted; Windows x64 17/17 plus unchanged Step 04D fixture |
| 04E | Evolution network/replay protocol + x64 validation | DONE | 04E1-04E4 complete; 04E4 Windows MinGW x64 passed 25/25 plus all explicit gates |
| 04E1 | Protocol v1 foundation + game-command payload migration | DONE | Golden byte fixtures + malformed-input gates; covered by the Step04E2B Windows 21/21 sign-off |
| 04E2 | Staged EVN1 transport + EVR1 Recorder integration | DONE | Win64-only gameplay datagrams + dual-format replay runtime bridge; real Windows MinGW x64 21/21 + explicit gates passed |
| 04E3 | Evolution x64-to-x64 multiplayer session validation | DONE | Real Windows MinGW-w64 GCC 16.2 passed 23/23 plus all Step04D/04E1/04E2/04E3 explicit gates |
| 04E4 | Replay/network golden session gates | DONE | Real Windows GCC 16.2 passed 25/25 + explicit full-session gate; representative full-client run moved to x64 stabilization |
| 04F | Retire x86 modernization/oracle lane | DONE | i686 MinGW modernization surface removed; real Windows GCC 16.2 focused graph and x64 platform policy passed |
| W3X-A0 | W3D/W3X format recognition | DONE | Safe extension/content classification |
| W3X-A1 | W3X document-envelope probe | DONE | Root/namespace/SAGE envelope recognition |
| W3X-A2 | W3X top-level child-element discovery | DONE | Direct-child classification without content decoding |
| W3X-A2R | Consolidate W3X A1/A2 parser seam | DONE | One public API + one shared implementation; no duplicate parser headers |
| 05 | Evolution runtime + W3X import foundation | ACTIVE | Clean x64/D3D12 process root plus parser/import/routing gates |
| 05A | Developer baseline cleanup | DONE | Real Windows GCC 16.2 passed 25/25 cleaned-name graph plus all explicit deterministic/Evolution/x64 checks |
| 05B | D3D12 proof shell | DONE | Windows-signed-off hardware/WARP clear-present proof; temporary architecture superseded by 05C |
| 05C | In-place D3D12 backend | DONE | Real Windows x64 production backend passes 27/27 plus explicit GPU gate |
| 05D | D3D12 indexed primitive foundation | DONE | Real Windows backend submits indexed geometry through `IRenderBackend` and passes 27/27 |
| 05E | D3D12 shader asset foundation | ACTIVE | Canonical HLSL replaces embedded shader source and is staged for smoke/game executables |
| 05F | D3D12 persistent buffers + first WW3D caller migration | NEXT | First real `DX8Wrapper` draw/buffer responsibility crosses the backend seam |
| 05W1 | W3X XML parser component | PLANNED | Real XML parser behind shared project-owned interface |
| 05W2 | W3X neutral rigid-mesh import | PLANNED | Representative rigid mesh reaches renderer-neutral import model |
| 05W3 | W3D/W3X routing and validation | PLANNED | Existing W3D path unchanged; W3X routed to validated importer |
| 06 | HD texture pipeline | PLANNED | Efficient/correct 2K/4K assets |
| 07 | High-poly / 32-bit geometry | PLANNED | >65k-class geometry path where needed |
| 08 | Instancing/batching expansion | PLANNED | Reduce repeated-object CPU/draw cost |
| 09 | LOD/visibility modernization | PLANNED | Scalable dense scenes |
| 10 | x64 runtime stabilization | PLANNED | Heavy-mod/replay/network soak after Step 04 bring-up |
| 11 | Renderer architecture boundary completion | PLANNED | D3D12-ready renderer-neutral submission |
| 12 | D3D12 foundation completion | PARTIAL | 05B establishes device/swapchain/commands/fences; Step 12 hardens/integrates the remaining foundation |
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

`DONE*` records implementation where a relevant Windows gate remains outstanding. Step 04 is fully Windows-signed-off. Representative full-client session/replay execution is tracked as later x64 stabilization, not as an i686-retention gate.

## Cross-cutting backlog

These are not separate numbered milestones but must be addressed as their owning step touches them:

- Generals / GeneralsMD duplication reduction;
- consolidation before expansion: reuse or refactor shared/Core modules instead of adding overlapping headers/helpers;
- test-only configurations that avoid unrelated dependency fetches;
- target-scoped dependencies;
- resource lifetime/ownership clarity;
- crash/device-loss diagnostics;
- benchmark capture automation;
- asset validation/error messages;
- modder-facing documentation.
