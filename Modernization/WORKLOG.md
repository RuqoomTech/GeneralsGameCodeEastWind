# Modernization Worklog

This is a chronological implementation record. Add an entry whenever a modernization milestone or meaningful architectural slice is merged.

## 2026-09-09 — Authoritative baseline established

Baseline:

- `GeneralsGameCode-main(2).zip`
- SHA-256 `3fa2e6807e842ba51cfa67b5251e4c7f0bdaaedc3dbea5c1f69ddfdde94f08fd`

Actions:

- adopted this source tree as the sole modernization baseline;
- audited current build system, renderer seam, and W3D loader architecture;
- recorded x64 + D3D12 as the Evolution target;
- recorded CMake + Ninja + MinGW-w64 GCC as the primary build direction;
- recorded additive EA SAGE W3X support;
- created roadmap, decisions, guardrails, asset, renderer, performance, and build documentation;
- no gameplay/renderer/build implementation changes were made as part of this documentation step.

### Existing upstream work recognized

The baseline already contains a partial renderer abstraction (`IRenderBackend` + `DX8Backend`) routing a subset of WW3D operations. This work is recorded as `PARTIAL`; it is not claimed as newly implemented by the modernization docs pass.
## 2026-09-09 — W3X pre-step A0: asset-format recognition

- Added dependency-free W3D/W3X path recognition.
- Added conservative XML sniffing for `.w3x` so unrelated binary formats using the same extension are not routed to the future SAGE W3X parser.
- Added a standalone C++98-compatible characterization test.
- No asset-manager, renderer, W3D loader, CRC, simulation, or CMake behavior changed.
- W3X XML parsing remains future work under Step 05.

## 2026-09-09 — W3X pre-step A1: document-envelope probe

- Added a dependency-free, C++98-compatible W3X XML envelope probe.
- Added root qualified/local-name extraction and root namespace resolution.
- Recognizes the SAGE `AssetDeclaration` + `uri:ea.com:eala:asset` envelope used by later-SAGE asset documents.
- Handles UTF-8 BOM, XML declaration, processing instructions, comments, whitespace, and quoted root attributes.
- Explicitly rejects `DOCTYPE` in this narrow probe to avoid accidental entity/parser scope expansion.
- Added standalone synthetic tests; no proprietary W3X asset is committed.
- No asset-manager, W3D loader, renderer, simulation, CRC, or CMake behavior changed.
- Child-element parsing and actual W3X importing remain Step 05 work.

## 2026-09-10 — W3X pre-step A2: top-level child-element discovery

- Added a dependency-free, C++98-compatible direct-child discovery seam for validated SAGE `AssetDeclaration` documents.
- Classifies `W3DMesh`, `W3DHierarchy`, `W3DContainer`, `W3DAnimation`, and `W3DCollisionBox`; unknown direct child types are preserved as `Unknown`.
- Reports child qualified/local names and resolved namespace URI, including default, inherited, alternate-prefix, and child-local namespace bindings.
- Ignores comments/whitespace/processing instructions for discovery and validates nested tag closure without decoding nested asset data.
- Added bounded nesting protection and safe malformed/unsupported-DOCTYPE handling.
- Added standalone synthetic A2 tests; A0 and A1 remain unchanged.
- GCC C++98, Clang C++98, AddressSanitizer, and UBSan standalone validation pass for A0/A1/A2.
- No asset-manager routing, W3D loader, renderer, gameplay, CRC, Xfer, replay, network, or CMake behavior changed.
- Includes/references, child-content decoding, neutral import structures, and runtime W3X loading remain Step 05 work.

## 2026-09-10 — W3X A2R: parser seam consolidation

- Performed a maintenance/refactor step before adding any new W3X capability.
- Removed the implementation-heavy `Core/Libraries/Include/rts/w3x_document_probe.h` and `w3x_child_discovery.h`.
- Added one public `Core/Libraries/Include/rts/w3x_document.h` containing the existing A1/A2 contracts.
- Centralized A1/A2 implementation in `Core/Libraries/Source/rts/w3x_document.cpp`, sharing XML name/tag/namespace scanning rather than maintaining parallel helper families.
- Kept `asset_3d_format.h` separate because format routing/sniffing is a distinct responsibility from W3X document parsing.
- Updated A1/A2 tests to consume the consolidated API; no additional test source file was created for the refactor.
- GCC and Clang C++98 standalone validation pass for A0/A1/A2 after the consolidation.
- No runtime asset-manager routing, W3D loader, renderer, gameplay, CRC, Xfer, replay, network, or CMake behavior changed.
- Established a project guardrail to prefer consolidation/refactoring over overlapping shared/Core files as modernization work expands.

## 2026-09-10 — Step 01A: CRC primitive characterization

- Began Step 01 with one isolated characterization slice and no production behavior changes.
- Added `Core/Tests/DeterminismPrimitivesTest.cpp` as the shared home for small determinism primitive vectors, avoiding one test file per sub-step.
- Locked the production `Common/crc.h` primitive's initial/no-op/clear behavior, representative byte vectors, incremental processing, and carry/high-bit behavior.
- Clang 17 passes the vectors at `-O0`, `-O2`, and `-O3`.
- GCC 14.2 passes at `-O0`; optimized `-O2`/`-O3`, ASan, and UBSan runs pass the CRC vectors with `-Wno-strict-aliasing` to suppress pre-existing warnings from unrelated `Lib/BaseType.h` float helpers.
- The strict-aliasing warning is retained as a Step 01 investigation item; no deterministic float behavior was changed in this CRC slice.
- No RNG, Xfer, snapshot, replay, simulation, renderer, W3D/W3X runtime, asset-manager, or build-system implementation changed.
- Step 01 remains `PARTIAL`; the next small target is deterministic game-logic RNG sequence/state characterization.

## 2026-09-10 — Step 01B: deterministic game-logic RNG characterization

- Extended the existing `Core/Tests/DeterminismPrimitivesTest.cpp`; no additional RNG test file or copied RNG implementation was introduced.
- Added a narrow `RTS_STANDALONE_DETERMINISM_TEST` include seam in `Core/GameEngine/Source/Common/RandomValue.cpp` so the production RNG implementation can be linked into the lightweight Linux/GCC/Clang harness without pulling ATL/Win32 through `PreRTS.h`. Normal production builds retain the existing `PreRTS.h` path.
- Locked explicit-seed initialization, replay/base-seed stability, initial/per-draw RNG state CRCs, two deterministic integer sequences, signed-range behavior, same-seed reset reproducibility, equal-range retail state consumption, and retail-compatible `GameLogicRandomValueUnchanged` state advancement.
- GCC 14.2 and Clang 17 pass the combined CRC/RNG characterization at `-O0`, `-O2`, and `-O3`; ASan and UBSan also pass.
- The pre-existing optimized GCC strict-aliasing warning from `Lib/BaseType.h` remains suppressed only for this lightweight gate and is not changed by this step.
- Real-valued RNG, Xfer/snapshot bytes, layout-sensitive structures, and replay fixtures remain future Step 01 work.

## 2026-09-10 — Step 01C: Xfer primitive serialization / byte-order characterization

- Extended the existing `Core/Tests/DeterminismPrimitivesTest.cpp`; no new determinism test source was created.
- Added an engine-linked Xfer mode that derives a capture sink from `Xfer` and calls the production base-class primitive wrappers rather than copying their behavior.
- Locked the legacy Windows primitive widths and exact little-endian byte stream for version, byte/bool, 16/32/64-bit integers, `Real`, and `xferUser` raw bytes.
- Added `z_determinismtest` directly to the existing Zero Hour extras CMake path instead of creating another CMake subdirectory/module.
- The target is opt-in through the existing `RTS_BUILD_ZEROHOUR_EXTRAS` switch; normal game/build behavior is unchanged when extras are off.
- The lightweight Linux CRC/RNG characterization remains green. The engine-linked Xfer test requires Windows because the production Xfer/GameEngine header graph is tied to legacy Win32/ATL infrastructure.
- Windows MinGW-w64 i686 and MSVC reference execution commands are documented in `STEP_01_DETERMINISM_GUARD.md`; reference execution remains pending user validation.
- No Xfer serialization implementation, snapshot behavior, replay format, gameplay, renderer, asset manager, or W3D/W3X runtime behavior changed.

## 2026-09-10 — Step 01D: representative snapshot field-order characterization

- Reused `Core/Tests/DeterminismPrimitivesTest.cpp` and the existing `z_determinismtest` target; no new source/test module was created.
- Added an in-memory `XferSave` capture subclass so the production `XferSave::xferSnapshot()` dispatch can be exercised without writing a test file to disk.
- Characterized the real production `DamageInfoOutput::xfer()` snapshot path with representative non-zero values.
- Locked the exact 10-byte Windows stream and field order: version, actual damage dealt, actual damage clipped, then no-effect flag.
- No Xfer, XferSave, Damage, Snapshot, save-game, replay, gameplay, renderer, W3D, or W3X production behavior changed.
- Lightweight Linux CRC/RNG and W3X regressions remain part of the local gate; the engine-linked primitive/snapshot test remains a Windows execution gate for MinGW-w64 i686 and MSVC reference builds.

## 2026-09-10 — Step 01E: determinism guard completion

- Completed the remaining Step 01 characterization in the existing `Core/Tests/DeterminismPrimitivesTest.cpp`; no new determinism test file/module was created.
- Added exact IEEE-754 game-logic real-RNG vectors and per-draw RNG-state CRCs.
- Characterized the legacy fast float trunc/floor/ceil bit behavior, including historical edge behavior.
- Removed modern C++ strict-aliasing UB from the non-VC6 float bit-conversion path in `Lib/BaseType.h` using `memcpy`; the VC6 assembly branch and legacy bit-mask/arithmetic algorithm are unchanged.
- Compared 199,122 deterministic finite float inputs before/after the alias-safe change; the trunc/floor/ceil output stream/hash was identical.
- GCC 14.2 now passes the lightweight determinism test at `-O0`, `-O2`, and `-O3` with `-Werror` and no `-Wno-strict-aliasing`; Clang 17 passes the same matrix; ASan and UBSan pass.
- Added production `XferCRC` checkpoints for full-word and partial-tail folding.
- Added strict Win32 ABI guards for replay/network primitive widths, packed `TransportMessageHeader`, native `GameMessage` size, command-packet capacity, and `CommandPacket` offsets/size.
- Locked replay/network enum anchors, including `MSG_BEGIN_NETWORK_MESSAGES = 1000`, `MSG_LOGIC_CRC = 1095`, and `MSG_END_NETWORK_MESSAGES = 1999`.
- Added a known 19-byte replay `MSG_LOGIC_CRC` command-record fixture with production CRC and XferCRC checkpoints.
- W3X A0/A1/A2 regressions remain green.
- Step 01 characterization implementation was complete at this point; final Win32 sign-off was still pending and was completed after the focused-target stabilization recorded below.

## 2026-09-10 — Step 01F: Windows sign-off harness decoupling

- User MinGW-w64 GCC 16.2 + Ninja validation reached all 653 compilation steps and failed only at final `z_determinismtest` linkage because the target linked the monolithic `z_gameengine` archive.
- The unresolved symbols were executable/device responsibilities (`TheKey_*`, `MapObject` storage, UI/renderer hooks, language-file globals), not failures in CRC/RNG/Xfer determinism.
- Reworked `z_determinismtest` as a focused standalone target built from the existing test plus production RandomValue, Snapshot, Xfer, XferCRC, and Damage implementation units; removed the `z_gameengine` link dependency.
- Kept characterized Xfer primitive/XferCRC/DamageInfoOutput code on the production implementations; only out-of-scope Xfer APIs and identifier/string-allocation plumbing use standalone-test seams.
- Removed the `XferSave` dependency from the snapshot capture while continuing to test the real `DamageInfoOutput::xfer()` field order.
- Changed the MinGW i686 configure preset to Ninja and added `mingw-w64-i686-determinism` plus `z_determinismcheck` for a short build-and-run workflow.
- Made the i686 toolchain discover native MSYS2 MINGW32 `gcc/g++/ar/ranlib/windres/dlltool` as well as conventional cross-prefixed tool names.
- Removed duplicate `_com_util` conversion implementations from `comsupp_compat.h` because modern MinGW-w64 already provides them.
- Local GCC/Clang lightweight determinism and W3X A0/A1/A2 regressions remained green; final focused Win32 execution was completed successfully after the final linker-isolation correction.

## 2026-09-10 — Step 01G: Windows sign-off and baseline seal

- User validation on Windows passed `cmake --build --preset mingw-w64-i686-determinism --target z_determinismcheck` with MinGW-w64 i686 / GCC 16.2 + Ninja.
- Final output: `Step 01 determinism guard passed: float helpers, CRC/RNG, Xfer/XferCRC, snapshot, ABI, and replay checkpoints.`
- Corrected the provisional replay enum checkpoint from `MSG_LOGIC_CRC = 1093` to the source-accurate `1095`; updated the 19-byte replay fixture and CRC/XferCRC expected values accordingly.
- The focused MinGW determinism target uses interprocedural optimization when supported to eliminate unused legacy inline/vtable material without relinking the monolithic GameEngine.
- Step 01 is now **DONE**. Its CRC/RNG/Xfer/snapshot/ABI/replay contracts are the mandatory regression gate for Step 02 and all later modernization work.
- Step 02 — Command-Line Build System Foundation is now the active milestone.

## 2026-09-10 — Step 02A: canonical MinGW/Ninja build foundation

- Started from the exact Step 01G archive bytes supplied for this chat; the received archive SHA-256 (`796c7e5642d655bebdf0ca079d7a099a5af46d82cb11a9b27282021f800fce07`) does not match the user-declared handoff hash (`bb179526f5a093397375220025e265ffc66ad223e8569a30b4d933562ac8718a`), so the mismatch is recorded rather than silently falling back to another repository.
- Added canonical `mingw32-release`, `mingw32-debug`, `mingw32-profile`, and `mingw32-tests` Ninja presets while retaining the Step 01 `mingw-w64-i686*` names as compatibility aliases.
- Added a focused `RTS_BUILD_TESTS_ONLY` CMake graph and centralized modernization test wiring in `Core/Tests/CMakeLists.txt`.
- Moved only the CMake ownership of `z_determinismtest` out of the Zero Hour tools subtree; the signed-off Win32 production source set, `CppMacros` prelude, Zero Hour header precedence, ReactOS ATL use, and MinGW IPO isolation remain intact.
- Integrated W3X A0/A1/A2 and the Step 01 lightweight determinism path with CTest without adding new test source files.
- Scoped `-Wsuggest-override` to C++ project compilation instead of C/vendored targets; moved MinGW compatibility flags/defines/libraries from global injection to `core_config`; removed the global `-mwindows` injection.
- Replaced direct deprecated `FetchContent_Populate()` use for ReactOS ATL, legacy zlib, and LZHL with source-only `FetchContent_MakeAvailable()` flows; marked appropriate vendor include boundaries as `SYSTEM`.
- Hardened i686 toolchain selection with an explicit `RTS_MINGW_ROOT`, selected-bin discovery, target-triplet validation, and selected-bin debug-strip lookup.
- Reworked WIDL discovery around the selected MinGW/MSYS2 environment plus explicit `RTS_WIDL_ROOT`, `WIDL_ROOT`, and `RTS_WIDL_INCLUDE_DIR` overrides. Native MINGW32 now resolves both `widl.exe` and `${RTS_MINGW_ROOT}/include/oaidl.idl` without PATH surgery; Linux Wine include layouts remain fallbacks. The focused test graph no longer requires WIDL.
- Local GCC 14.2 CMake/Ninja/CTest passed all four focused tests; Clang 17 CMake/Ninja/CTest passed the same four tests.
- Manual Step 01 lightweight determinism passed GCC and Clang at `-O0`, `-O2`, and `-O3`; GCC ASan and UBSan passed; standalone W3X A0/A1/A2 passed with both GCC and Clang.
- Windows Step 02A verification and the real `z_generals` MinGW build remain pending. No Step 02A Windows pass is claimed.

## 2026-09-10 — Step 02B: runtime configure/install hardening

- Continued strictly from `GeneralsGameCode-Step02A-Command-Line-Build-Foundation-full.zip` as supplied in this chat; SHA-256 of the exact Step 02A bytes used as the comparison base is `d0ecee0b8dc4810a18eee53b9ddcc278862422f725aa516756ac00090b6bc2ee`.
- Reproduced a real CMake generation blocker under GNU: unconditional `install(FILES $<TARGET_PDB_FILE:...>)` fails because `TARGET_PDB_FILE` is unsupported by the GNU linker, even when the install file is `OPTIONAL`. The prior Generals/Zero Hour install rules used this expression whenever an install prefix was present.
- Consolidated runtime installation in `cmake/debug_strip.cmake` through `rts_install_runtime_target()`. MSVC retains optional PDB installation; MinGW Release installs the existing `$<TARGET_FILE>.debug` sidecar when symbol stripping is available; non-MSVC/GNU generation no longer evaluates a PDB expression.
- Replaced the duplicated Generals and Zero Hour target/PDB install blocks with the shared helper and compact target lists.
- Moved full-runtime WIDL discovery/requirement ahead of ReactOS ATL and the other runtime dependency graph. A missing WIDL now fails before external runtime dependencies are populated.
- Added native-Windows validation for `oaidl.idl` and `ocidl.idl`, matching the imports in `BrowserEngine.idl` and `BrowserDispatch.idl`; focused `mingw32-tests` remains WIDL-independent.
- Added `cmake/tests/RuntimeInstallPolicyTest.cmake` and registered `buildsystem_runtime_install_policy` in the existing `Core/Tests` graph; it uses a tiny nested target to catch linker-specific install-generator-expression regressions and requires the `.debug` sidecar on MinGW.
- GCC 14.2 and Clang 17 focused CMake/Ninja graphs each passed 5/5 CTest tests.
- The lightweight Step 01 determinism harness was rerun under GCC 14.2 and Clang 17 at `-O0`, `-O2`, and `-O3`; GCC ASan and UBSan also passed.
- The repository-owned runtime-install policy test configured, built, and installed successfully through `rts_install_runtime_target()` under both GCC and Clang; a separate host-GNU probe also validated the helper directly. A second synthetic GNU probe exercising the MinGW debug-sidecar branch produced and installed both the executable and `.debug` file.
- A real i686 MinGW compiler is not available in this execution environment and package-network access is unavailable, so no Windows/MinGW `z_generals` build pass is claimed. The next Step 02 slice should use the first concrete compiler/linker failure from `cmake --build --preset mingw32-release --target z_generals`.

## 2026-09-11 — Step 03A: render-frame performance telemetry foundation

- Advanced from Step 02 after the user reported the Windows canonical build path working; no Step 02 Windows console transcript was supplied, so the repository records user acceptance without inventing formal output.
- Reused `rts/profile.h`, the existing profile module, WW3D `Debug_Statistics`, and the current Tracy seam rather than introducing a parallel profiler/statistics hierarchy.
- Added `RTS_BUILD_OPTION_PERF_TELEMETRY`; canonical `mingw32-profile` enables it while normal release/debug render paths compile the hook out.
- Added production `performance_telemetry.cpp` with runtime-opt-in `RTS_PERF_CAPTURE` CSV output, buffered periodic flush, stable schema version 1, and optional Tracy plots from the same frame sample.
- Bracketed successful primary WW3D render frames only; telemetry reads render/sync identity, CPU microseconds, draw/geometry counters, texture/resource counters, and WW3D allocation/free counts. No telemetry value feeds back into simulation or frame pacing.
- Reused existing texture statistics. CSV capture temporarily promotes `RECORD_TEXTURE_NONE` to `RECORD_TEXTURE_SIMPLE` only for the primary frame when needed, then restores it.
- Added `performance_telemetry_step03a` to the focused CTest graph. GCC 14.2 and Clang 17 each pass 6/6 focused tests.
- Final telemetry sealing matrix passed with GCC and Clang at `-O0`, `-O2`, and `-O3`, plus GCC ASan and UBSan. A telemetry-enabled focused configure confirmed `RTS_PERF_TELEMETRY` propagation; the normal focused Release graph confirmed the define stays absent.
- Step 03 remains active: GPU timing, full client/update phases, visibility/culling, process memory, asset hotspots, and benchmark automation remain.

## 2026-09-11 — Step 03 completion + Step 04A x64 readiness lane

- Completed Step 03 by extending the Step 03A render sample into versioned CSV schema v2 with one row per `GameEngine::update()`.
- Added complete engine update, GameClient, message-stream, network, GameLogic, and primary WW3D render CPU timing while keeping all telemetry observational.
- Added a profile-build drawable visibility snapshot: total drawables, DrawModule-visible drawables, and fully shrouded drawables. This is explicitly a client visibility proxy, not a frustum/GPU occlusion result.
- Preserved existing WW3D draw/geometry/texture/resource counters and Tracy plots; no duplicate renderer-statistics subsystem was introduced.
- Added `scripts/perf-summary.py` for standard-library-only p50/p95/p99/max timing and mean/max counter summaries.
- Deliberately deferred legacy D3D8 GPU timestamp work to D3D12, where native queue timestamp infrastructure will survive the renderer transition.
- Began Step 04 immediately after Step 03 by consolidating MinGW toolchain discovery into one architecture-parameterized implementation.
- Kept `mingw-w64-i686.cmake` as the compatibility/reference wrapper and added `mingw-w64-x86_64.cmake` for the staged x64 lane.
- Added `mingw64-tests` and `RTS_BUILD_X64_READINESS`; the x64 lane is initially focused-tests-only, and full x64 runtime configuration fails intentionally with an explanatory diagnostic.
- Removed legacy D3D8/DirectInput/DirectSound link requirements from the focused MinGW test graph and avoided ReactOS ATL population in the x64 focused graph when there is no consumer.
- Added `architecture_width_step04a`, which validates pointer/`uintptr_t` width behavior while locking fixed-width engine/wire primitives and ObjectID/DrawableID to 32 bits.
- Local host-native GCC 14.2 and Clang 17 focused CMake/Ninja/CTest runs pass 7/7 tests.
- No Windows x86_64 test pass is claimed; `mingw64-tests` remains the Step 04A Windows validation gate.
- Final local validation also passed GCC/Clang `-O0`/`-O2`/`-O3` focused suites (7/7 each), GCC ASan and UBSan (7/7 each), schema-v2 summary text/JSON checks, and telemetry compile-definition isolation.
- The validation container did not provide an x86_64 MinGW-w64 compiler, so no Win64 execution/build result was inferred from the host-native 64-bit pass.

