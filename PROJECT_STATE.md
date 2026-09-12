# Project State — Authoritative Modernization Working State

This file is the authoritative state marker for modernization work. Read it before applying patches or beginning a new milestone.

## Baseline identity

- **Step 04D3 authoritative local input:** `GeneralsGameCodeEastWind.zip`, SHA-256 `e290c9bb51c4fc271ed89428b531d33298e498336f61fa4a8db6abcab5ad8502`.
- **Step 04D3 upstream comparison snapshot:** `GeneralsGameCode-main (1).zip`, SHA-256 `c5c561ca47ffe874c31732c3cb86bcc0016f246f5ee36cc3435bae50e26427d1`.
- EastWind remains the architecture baseline; upstream is used selectively for correctness/gameplay/runtime fixes.
- **Step 04D2 authoritative input:** `GeneralsGameCode-Step04D1-Windows-WIDL-Bootstrap-Hotfix-full.zip`.
- SHA-256 of the exact Step 04D1 archive used for this hotfix: `e88521da197d004012672076e71a11e63e0e4c19c6596760425f080cde81dae5`.
- Step 04D2 was produced strictly from those bytes after the first successful Windows `mingw64-tests` configure exposed GCC 16.2 runtime-ABI compile blockers.
- **Step 04D authoritative input:** `GeneralsGameCode-Step04C-Native-Width-Runtime-Dependencies-full.zip`.
- SHA-256 of the exact Step 04C archive used for this work: `e49b157c93e9fced03c61bb76519f4262bddc4b0682d08a35499700e1c47e1d9`.
- Step 04D was produced strictly from those bytes; no GitHub state, remembered repository, older patch, or alternate ZIP was substituted.
- Historical Step 04C authoritative input: `GeneralsGameCode-Step04B-Wire-Pointer-Audit-full.zip`, SHA-256 `b6e983715e74c32ececcbe820964697496a9df25dd5e36cb86d88e6c981a09f9`.
- Step 04B itself was produced from `GeneralsGameCode-Step03-Complete-Step04A-x64-Readiness-full.zip`, SHA-256 `e87de5e1d2f6bd2e916030b5beb487fb4a71eb2beb4e57b14ccca3d22e1d7eba`.

- Step 02A was based on the sealed Step 01G Windows-signoff repository supplied on 2026-09-10.
- Step 01G user-declared archive/hash: `GeneralsGameCode-Step01G-Windows-Signoff-Baseline-Seal-full.zip` / `bb179526f5a093397375220025e265ffc66ad223e8569a30b4d933562ac8718a`.
- SHA-256 of the Step 01G archive bytes actually received for Step 02A: `796c7e5642d655bebdf0ca079d7a099a5af46d82cb11a9b27282021f800fce07`; that mismatch remains recorded rather than silently substituting another tree.
- Historical Step 03 completion + Step 04A implementation base: `GeneralsGameCode-Step03A-Performance-Telemetry-Foundation-full.zip`.
- SHA-256 of the exact Step 03A archive bytes used here: `e1166a1d1834530eea4fc740b00802f27a85269e8831d25c8ff9e61c81c0c5c3`.
- Step 03 completion/04A were produced strictly from those received Step 03A bytes; later steps use the chained authoritative ZIPs recorded above.
- Historical Step 03A base: Step 02B SHA-256 `c0bac9981f10ec82bdeb391fa7e33811be7a611d3368c31c1f5e73ef15ba1ab5`.
- Historical upstream provenance remains recorded in `Modernization/BASELINE_MANIFEST.md`.

## What was changed when establishing this baseline

The original Step 00 baseline pass changed documentation only. Subsequent signed-off modernization steps now include C/C++, CMake, tests, scripts, telemetry, network ABI guards, and x64-readiness/runtime work; the historical baseline statement must not be read as describing the current tree.

## Verified implementation already present in the uploaded source

### Modern C++ / build groundwork

- CMake 3.25 minimum.
- Modern non-VC6 builds request C++20.
- Visual Studio 6 remains a historical build path.
- Modern Windows presets already use Ninja Multi-Config for the standard configuration family.
- The sealed Step 01G MinGW-w64 i686 path uses Ninja and passed the focused Windows determinism gate.
- Step 02A adds canonical `mingw32-*` Ninja presets, a focused CTest graph, target-scoped MinGW compatibility flags, and improved ATL/WIDL/binutils discovery.

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
11. Evolution multiplayer compatibility is required only between our own Evolution/game editions; retail 32-bit multiplayer interoperability is not a target.
12. The i686 build receives no new features and remains only until x64 deterministic/replay/network golden gates replace it as the oracle.


## Step 04B implementation state — 2026-09-11

The authoritative input for Step 04B is `GeneralsGameCode-Step03-Complete-Step04A-x64-Readiness-full.zip`, SHA-256 `e87de5e1d2f6bd2e916030b5beb487fb4a71eb2beb4e57b14ccca3d22e1d7eba`. Step 04B was implemented directly from those bytes.

Key invariants now encoded in source/tests:

- future Evolution runtime is x64-only, while i686 remains a frozen temporary determinism oracle;
- `GameMessage::Type` has explicit 32-bit `Int` underlying width;
- legacy command-packet payload capacity is a frozen 1008-byte protocol constant and is no longer derived from `sizeof(GameMessage)`;
- native GUI callback payload `WindowMsgData` follows `uintptr_t`;
- wave-output userdata/callback and IME candidate pointer arithmetic no longer truncate through 32-bit integers;
- focused test graph includes `wire_replay_abi_step04b` and `pointer_wire_source_audit_step04b`;
- no Windows/Win64 result is claimed until supplied by a Windows console run.

## Step 04C implementation state — 2026-09-11

Authoritative input: `GeneralsGameCode-Step04B-Wire-Pointer-Audit-full.zip`, SHA-256 `b6e983715e74c32ececcbe820964697496a9df25dd5e36cb86d88e6c981a09f9`.

Implemented in this slice:

- repaired `WWLib/ObjectPoolClass` backing-block metadata so its chain is a native pointer rather than a `uint32*` surrogate;
- moved `GameMemory` raw size/stride calculations to `size_t`, native pointer alignment, checked blob multiplication, and `uintptr_t` address checks;
- made `FastFixedAllocator` stride/chunk storage pointer-aligned while preserving four-byte behavior on x86 and widening naturally on x64;
- replaced `FastAllocatorGeneral`'s hard-coded four-byte prefix with a pointer-aligned header and corrected realloc copy bounds;
- converted native pointer hashing and keyboard-layout handle extraction away from direct 32-bit truncation;
- fixed list-box multi-selection pointer return to use an explicit `Int**` native pointer contract;
- added `scripts/setup-windows-dev.ps1` as the x64-default Windows dependency bootstrap, with optional `-IncludeLegacyX86`;
- expanded the focused graph to 12 tests with allocator runtime, pointer source-audit, and dependency-bootstrap policy regressions.

Local validation is green with GCC 14.2 and Clang 17 at `-O0`, `-O2`, and `-O3`, plus GCC ASan and UBSan: 12/12 tests in every configuration. No Windows/Win64/bootstrap pass is claimed because this environment has neither PowerShell nor an x86_64 MinGW-w64 compiler.

## Step 04D implementation state — 2026-09-12

Authoritative input: `GeneralsGameCode-Step04C-Native-Width-Runtime-Dependencies-full.zip`, SHA-256 `e49b157c93e9fced03c61bb76519f4262bddc4b0682d08a35499700e1c47e1d9`.

Implemented in this slice:

- centralized the duplicated Generals/Zero Hour `setFPMode()` implementation in shared Core GameEngine code;
- preserved the signed-off i686 x87 round-to-nearest / 24-bit precision oracle behavior while defining the x64 contract as round-to-nearest with no legacy x87 precision-width dependency;
- added the `RTS_BUILD_X64_HEADLESS_CORE` lane to `mingw64-tests`;
- added a renderer-free 12,000-frame deterministic/headless executable using the production GameLogic RNG, production CRC primitive, and shared production FPU policy;
- the timeline hashes only explicit fixed-width simulation fields plus RNG state and never raw object layout, pointers, padding, `size_t`, or allocator state;
- checked-in checkpoints are frame 0, 1, 10, 100, 1000, 5000, 10000, and 12000/end;
- added `scripts/compare-determinism-timelines.py` so the frozen i686 executable and x64 executable can emit and compare the same timeline;
- deterministic focused compile policy explicitly disables fast-math and FP contraction (`-fno-fast-math`, `-ffp-contract=off`; `/fp:strict` on MSVC);
- expanded the focused graph from 12 to 15 tests.

Local GCC 14.2 and Clang 17 O0/O2/O3 builds, GCC ASan, and GCC UBSan all pass 15/15 tests and reproduce the same eight timeline checkpoints. This is a locally stable candidate golden fixture, not yet a signed-off x86-vs-Win64 oracle: no Windows/MinGW x64 result is claimed until the user supplies console output.

Windows bootstrap follow-up (2026-09-12): a user-supplied run successfully updated/installed the MSYS2 x64 dependencies and discovered GCC 16.2.0, CMake 4.4.3, Ninja 1.13.2, and WIDL. The run then exposed a repository bootstrap bug: WIDL was queried with unsupported `--version`. The script now uses WIDL's supported `-V` option for both x64 and optional i686 verification, with a source-policy regression guard. The interrupted run is not counted as a passing Windows test gate; rerun/sign-off remains pending.

Windows x64 sign-off follow-up (2026-09-12): after Step 04D2, the user rebuilt `mingw64-tests` with MSYS2 MinGW-w64 GCC 16.2.0. The build completed, `ctest --preset mingw64-tests --output-on-failure` passed **15/15**, and `z_headlessdeterminismcheck` matched the checked-in Step 04D fixture. This is a real Windows x64 focused-lane pass for the pre-04D3 baseline. The explicit i686-vs-x64 timeline comparison remains pending.

## Step 04D3 implementation state — 2026-09-12

Authoritative local input: `GeneralsGameCodeEastWind.zip`, SHA-256 `e290c9bb51c4fc271ed89428b531d33298e498336f61fa4a8db6abcab5ad8502`. Upstream comparison snapshot: `GeneralsGameCode-main (1).zip`, SHA-256 `c5c561ca47ffe874c31732c3cb86bcc0016f246f5ee36cc3435bae50e26427d1`.

Implemented selectively rather than by wholesale merge:

- imported Dozer/Worker disabled-task resumption in both editions and corrected Xfer version gating from `currentVersion` to the serialized/read `version`;
- imported the newer production cancellation/refund flow for both editions, including the non-refundable started-batch guard;
- imported the neutron outer-radius search/damage corrections behind explicit compatibility switches and added only the required coordinate unary operators;
- retained EastWind's strict-aliasing-safe float helpers, native-width GameMemory sizing/alignment and Step 04D2 `noexcept` delete contract;
- adapted upstream GameMemory robustness: portable `NOINLINE`, noinline pre-main init, debug-only link-tester accounting, null-safe delete/free paths and sized global delete overloads;
- replaced Bink/Miles import-stub linkage with the source-built runtime loaders, which fail neutrally on x64 when the legacy 32-bit DLLs cannot load;
- imported the 512-point font clamp and dynamically sized glyph buffers for compatibility-renderer safety;
- added `upstream_coordinate_ops_step04d3` and `upstream_alignment_source_policy_step04d3`, increasing the focused graph from 15 to 17 tests;
- fixed Step 04D fixture-header reading to accept LF or CRLF without changing any checkpoint value.

Shared-tree material divergence against the supplied upstream snapshot fell from **93 files to 63**. Full details and intentional non-imports are recorded in `Modernization/STEP_04D3_UPSTREAM_ALIGNMENT.md`.

Local validation: GCC 14.2 O0/O2/O3, Clang 17 O0/O2/O3, GCC ASan and GCC UBSan all pass **17/17** and verify the unchanged Step 04D timeline. No Windows Step 04D3 pass is claimed yet; rerun `mingw64-tests` on Windows after this sync.

## Current modernization status

| Area | State |
|---|---|
| Baseline documentation and roadmap | **Done** |
| Existing upstream renderer backend seam | **Partial / already present** |
| Determinism/CRC/Xfer characterization | **DONE — Step 01 signed off on MinGW-w64 i686 / GCC 16.2 + Ninja** |
| MinGW-w64 GCC + Ninja canonical build | **Implementation complete / user accepted; Windows transcript not archived** |
| HD performance telemetry | **DONE — Step 03 schema v2 + phase/visibility/resource capture + summary tool** |
| x64 engine migration | **ACTIVE — Step 04D Windows x64 focused lane verified; Step 04D3 upstream alignment implemented locally; i686-vs-x64 certification pending** |
| W3X format-recognition pre-step | **Done — A0** |
| W3X document-envelope probe | **Done — A1** |
| W3X top-level child-element discovery | **Done — A2** |
| W3X parser seam consolidation | **Done — A2R** |
| W3X XML parser/import foundation | Planned — Step 05 |
| HD texture pipeline | Planned — Step 06 |
| 32-bit/high-poly geometry path | Planned — Step 07 |
| Expanded instancing/batching | Planned — Step 08 |
| Modern LOD/visibility | Planned — Step 09 |
| x64 runtime stabilization | Planned — Step 10 after Step 04 bring-up |
| Renderer boundary completion | Planned — Step 11 |
| D3D12 renderer | Planned — Steps 12+ |

## Small W3X pre-steps completed after baseline adoption

- **W3X-A0** adds only a dependency-free W3D/W3X format-recognition primitive and standalone test.
- **W3X-A1** adds a dependency-free document-envelope probe that recognizes the XML root name/namespace, XML declaration presence, and the canonical SAGE `AssetDeclaration` envelope. It deliberately stops before child-element parsing.
- **W3X-A2** adds dependency-free discovery of direct child elements under a validated SAGE `AssetDeclaration`. It classifies `W3DMesh`, `W3DHierarchy`, `W3DContainer`, `W3DAnimation`, and `W3DCollisionBox`, preserves other direct children as `Unknown`, resolves direct-child namespace prefixes/default namespaces, and validates nesting without decoding asset contents.
- **W3X-A2R** consolidates the A1/A2 parser seam into one public `rts/w3x_document.h` API and one shared `Core/Libraries/Source/rts/w3x_document.cpp` implementation. The previous implementation-heavy `w3x_document_probe.h` and `w3x_child_discovery.h` headers were removed before runtime integration, eliminating duplicated XML/name/namespace scanning and reducing public-header surface.

None of these pre-steps is wired into the runtime asset manager. They do not change the main milestone order.

## Next implementation milestone

**Step 03 is complete. Step 04 — x64 Engine Migration — is active.**

Step 01 remains the deterministic behavior contract. The frozen i686 MinGW/Ninja runtime is retained only as a temporary simulation/replay/CRC oracle while the x64 lane is brought up; it is not a future retail-multiplayer compatibility target. Step 02 remains user-accepted on Windows.

### Step 03 completion

Step 03 extends the Step 03A renderer sample into stable CSV schema v2: one row per `GameEngine::update()` containing complete update CPU time, client/message/network/logic phase timing, primary WW3D render timing, drawable total/visible/shrouded visibility proxies, and the existing draw/geometry/texture/resource counters. `scripts/perf-summary.py` reports p50/p95/p99/max phase timings plus mean/max resource counters. The path is compile-time limited to telemetry/profile builds and remains observational only. Legacy D3D8 GPU timestamp work is intentionally deferred to D3D12 rather than creating instrumentation that would be discarded with the compatibility renderer.

### Step 04A — x64 readiness lane

Step 04A begins the engine migration without pretending the full runtime is already 64-bit:

- MinGW-w64 discovery is consolidated into one common toolchain implementation used by i686 and x86_64 wrappers;
- `mingw64-tests` is the canonical x86_64 focused preset and enables `RTS_BUILD_X64_READINESS`;
- a full x64 runtime configure is intentionally rejected until runtime boundaries are ported;
- the x64 focused graph does not depend on legacy D3D8/DirectInput/DirectSound libraries or an unused ATL population;
- `architecture_width_step04a` locks fixed 32-bit wire/game IDs while validating pointer-sized `uintptr_t` round trips.

Local host-native GCC 14.2 and Clang 17 focused configure/build/CTest runs pass 7/7 tests, including `-O0`/`-O2`/`-O3` coverage; GCC ASan and UBSan are also green. The validation container has no MinGW-w64 x86_64 compiler, so no Windows `mingw64-tests` pass is claimed until actual output is provided.

Before Step 04E, rerun the 17-test `mingw64-tests` graph on Windows and complete the explicit i686-vs-x64 Step 04D timeline comparison. Then the next implementation slice is **Step 04E — Evolution network/replay protocol + x64 validation**: define an explicit versioned fixed-width Evolution wire protocol, validate x64-to-x64 command streams/CRCs, and use the Step 04D timeline machinery plus representative replay fixtures as compatibility gates. The frozen i686 build remains only until the deterministic/replay oracle is fully replaced.
