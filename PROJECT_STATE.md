# Project State — Authoritative Modernization Working State

This file is the authoritative state marker for modernization work. Read it before applying patches or beginning a new milestone.

## Baseline identity

- Step 02A was based on the sealed Step 01G Windows-signoff repository supplied on 2026-09-10.
- Step 01G user-declared archive/hash: `GeneralsGameCode-Step01G-Windows-Signoff-Baseline-Seal-full.zip` / `bb179526f5a093397375220025e265ffc66ad223e8569a30b4d933562ac8718a`.
- SHA-256 of the Step 01G archive bytes actually received for Step 02A: `796c7e5642d655bebdf0ca079d7a099a5af46d82cb11a9b27282021f800fce07`; that mismatch remains recorded rather than silently substituting another tree.
- **Step 03 completion + Step 04A authoritative implementation base:** `GeneralsGameCode-Step03A-Performance-Telemetry-Foundation-full.zip`.
- SHA-256 of the exact Step 03A archive bytes used here: `e1166a1d1834530eea4fc740b00802f27a85269e8831d25c8ff9e61c81c0c5c3`.
- This work was produced strictly from those received Step 03A bytes; no remembered repository, older patch, or alternate ZIP was used.
- Historical Step 03A base: Step 02B SHA-256 `c0bac9981f10ec82bdeb391fa7e33811be7a611d3368c31c1f5e73ef15ba1ab5`.
- Historical upstream provenance remains recorded in `Modernization/BASELINE_MANIFEST.md`.

## What was changed when establishing this baseline

Only documentation files were added/updated to establish the roadmap and project state. No C/C++, CMake, shader, renderer, gameplay, CRC, replay, network, asset-loader, or build-script implementation was intentionally changed as part of this baseline documentation pass.

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

## Current modernization status

| Area | State |
|---|---|
| Baseline documentation and roadmap | **Done** |
| Existing upstream renderer backend seam | **Partial / already present** |
| Determinism/CRC/Xfer characterization | **DONE — Step 01 signed off on MinGW-w64 i686 / GCC 16.2 + Ninja** |
| MinGW-w64 GCC + Ninja canonical build | **Implementation complete / user accepted; Windows transcript not archived** |
| HD performance telemetry | **DONE — Step 03 schema v2 + phase/visibility/resource capture + summary tool** |
| x64 engine migration | **ACTIVE — Step 04A readiness lane implemented** |
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

Step 01 remains the deterministic compatibility contract. The i686 MinGW/Ninja runtime is the reference executable and must keep its replay/network/Xfer byte behavior while the x64 lane is brought up. Step 02 remains user-accepted on Windows.

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

The next implementation slice is **Step 04B — pointer/handle correctness**: inventory and remove address truncation (`pointer -> Int/DWORD/LONG`), distinguish numeric IDs from native addresses/handles, and add focused regressions while preserving the i686 determinism gate.
