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

## 2026-09-11 — Step 04B: wire/replay ABI freeze + pointer/handle audit

Authoritative input: `GeneralsGameCode-Step03-Complete-Step04A-x64-Readiness-full.zip` (`e87de5e1d2f6bd2e916030b5beb487fb4a71eb2beb4e57b14ccca3d22e1d7eba`).

Implemented:

- made `GameMessage::Type` explicitly `Int`-backed;
- removed the `NetworkDefs.h` dependency on runtime `GameMessage` layout and froze legacy command payload capacity at 1008 bytes / 28 compatibility commands;
- removed the now-unnecessary `MessageStream.h` include from `NetworkDefs.h`;
- widened native-only `WindowMsgData` to `uintptr_t`;
- converted `waveOutOpen` callback/instance userdata to `DWORD_PTR`;
- replaced IME pointer-through-`UnsignedInt` arithmetic with typed byte-pointer arithmetic;
- added `wire_replay_abi_step04b`;
- added `pointer_wire_source_audit_step04b`;
- retained fixed-width IDs/protocol fields and did not open the x64 D3D8 runtime graph.

Validation completed in the Linux container with GCC 14.2.0 and Clang 17.0.0. The focused graph contains 9 tests and passed in all of these configurations: GCC `-O0`, `-O2`, `-O3`; Clang `-O0`, `-O2`, `-O3`; GCC AddressSanitizer; GCC UndefinedBehaviorSanitizer. `git diff --check` and clean patch-application verification are packaging gates for the final deliverables. No Windows or Win64 execution is claimed by this environment.

## 2026-09-11 — Step 04C: native-width runtime substrate + quick Windows dependency bootstrap

Authoritative input: `GeneralsGameCode-Step04B-Wire-Pointer-Audit-full.zip` (`b6e983715e74c32ececcbe820964697496a9df25dd5e36cb86d88e6c981a09f9`).

Locked compatibility clarification for this slice: Evolution is x64-only and multiplayer compatibility is required only between our own Evolution/game editions. Retail 32-bit multiplayer interoperability is not a target. The frozen i686 build receives no new features and remains temporarily only as a deterministic/replay/network oracle.

Implemented:

- converted `ObjectPoolClass` backing-block chaining from the Win32-accidental `uint32*` layout to a native one-pointer `BlockHeader`, preventing x64 overlap between the eight-byte block link and the first pooled object/free-list entry;
- converted `GameMemory` raw byte arithmetic/strides to `size_t`, made pool byte alignment follow `sizeof(void*)`, added blob multiplication overflow checking, and removed the pointer-through-`unsigned` alignment check;
- retained the separate `GameMemoryInit.cpp` four-count rounding rule because it rounds pool **counts**, not addresses;
- made `FastFixedAllocator` chunk storage/stride pointer-aligned and replaced `FastAllocatorGeneral`'s fixed four-byte prefix with a pointer-aligned header that remains four bytes on x86 and becomes eight bytes on x64;
- corrected `FastAllocatorGeneral::Realloc` so grow/shrink copies only the valid payload prefix instead of the old augmented allocation size;
- converted `WindowVideoManager` pointer hashing to `uintptr_t` and `HKL` low-word extraction to an explicit native-width handle bridge;
- fixed list-box multi-select retrieval so the native `Int*` selection array is returned through `Int**`, not truncated into `Int`; updated the modern Core GameSpy chat caller to use that contract;
- added guarded `HAVE_WCSLCPY` / `HAVE_WCSLCAT` compatibility seams used only by the Clang/Linux focused allocator regression, avoiding a new duplicate string implementation;
- added `scripts/setup-windows-dev.ps1`: x64/MSYS2 GCC + WIDL + CMake + Ninja + Python + Git by default, with the frozen i686 toolchain only behind `-IncludeLegacyX86`; `-VerifyOnly`, `-SkipMsysUpdate`, and `-SkipPathUpdate` are available for existing installations;
- added `runtime_native_width_step04c`, `runtime_pointer_source_audit_step04c`, and `windows_dependency_bootstrap_step04c`, increasing the focused graph from 9 to 12 tests.

Validation completed in the Linux container:

- GCC 14.2 `-O0`, `-O2`, `-O3`: 12/12 each;
- Clang 17 `-O0`, `-O2`, `-O3`: 12/12 each;
- GCC AddressSanitizer: 12/12;
- GCC UndefinedBehaviorSanitizer: 12/12;
- production Step 04C allocator guard reports 64-bit native pointers on this host.

The environment has no PowerShell runtime and no `x86_64-w64-mingw32` compiler, so no Windows/Win64 dependency-bootstrap or `mingw64-tests` pass is claimed. Step 04D is next: bring real deterministic/headless Common/GameLogic units into the x64 lane and start golden x86-versus-x64 CRC timeline validation.


## 2026-09-12 — Step 04D: deterministic/headless x64 core + CRC timeline gate

Authoritative input: `GeneralsGameCode-Step04C-Native-Width-Runtime-Dependencies-full.zip` (`e49b157c93e9fced03c61bb76519f4262bddc4b0682d08a35499700e1c47e1d9`).

Implemented:

- centralized the duplicated Generals/Zero Hour `setFPMode()` implementation in shared Core GameEngine code;
- retained the frozen i686 x87 round-to-nearest/24-bit precision behavior while giving x64 a round-to-nearest contract without legacy x87 precision emulation;
- enabled `RTS_BUILD_X64_HEADLESS_CORE` in `mingw64-tests`;
- added `headless_determinism_step04d`, a renderer-free 12,000-frame deterministic executable using production GameLogic RNG, production CRC, and production FP reset;
- hashes only explicit fixed-width fields and RNG state, never native pointers/padding/allocator state;
- added the versioned candidate timeline fixture at frames 0, 1, 10, 100, 1000, 5000, 10000 and 12000/end;
- added `scripts/compare-determinism-timelines.py` for direct i686-vs-x64 oracle comparison;
- added deterministic compiler policy guards (`-fno-fast-math`, `-ffp-contract=off`, `/fp:strict` on MSVC) and source-policy regression coverage;
- focused graph increased from 12 to 15 tests.

Local validation: GCC 14.2 O0/O2/O3, Clang 17 O0/O2/O3, GCC ASan and GCC UBSan all passed 15/15 tests. All configurations emitted identical eight-checkpoint timelines through frame 12000.

No Windows/Win64 execution is claimed. The candidate fixture becomes a signed-off cross-architecture oracle only after the same revision matches under the frozen Windows i686 lane and Windows x64 `mingw64-tests` lane. Step 04E is next: explicit Evolution network/replay protocol and x64-to-x64 validation.

## 2026-09-12 — Step 04D Windows dependency-bootstrap WIDL verification hotfix

- Windows bootstrap transcript supplied by the user confirmed MSYS2 package update/install completed and discovered the x64 GCC 16.2.0, CMake 4.4.3, Ninja 1.13.2, and `widl.exe` tools.
- The bootstrap then failed during WIDL verification because `scripts/setup-windows-dev.ps1` used the generic `--version` argument. The installed MSYS2/Wine WIDL accepts `-V` instead; `cmake/widl.cmake` already used the correct flag.
- Changed both x64 and optional i686 bootstrap WIDL probes to `-V` rather than weakening or skipping WIDL verification.
- Extended `WindowsDependencyBootstrapTest.cmake` to require the `-V` probes and reject a regression to `--version` for WIDL.
- This transcript proves dependency installation/tool discovery up to the WIDL probe; it does not yet constitute a passing Windows `mingw64-tests` or i686-vs-x64 timeline gate. Those remain pending a rerun after this hotfix.


## 2026-09-12 — Step 04D2 Windows GCC 16 runtime ABI compile hotfix

- Windows `mingw64-tests` configure succeeded under MSYS2 MinGW-w64 GCC 16.2.0 after the WIDL bootstrap correction.
- The build exposed two real focused-lane compiler barriers: pre-C++11 global delete declarations disagreed with the standard `noexcept` declaration from `<new>`, and a `WWASSERT`-only object-pool counter became unused in release builds under `-Werror`.
- Updated WWLib, GameMemory and GameMemoryNull standard unsized global delete/delete[] declarations and definitions to the modern non-throwing contract; project-specific debug placement overloads were not changed.
- Scoped the object-pool block-count verification to `DEBUG_CRASHING`, preserving the debug invariant without release warning noise.
- Extended the existing Step 04C runtime source audit rather than creating another overlapping audit module.
- Focused GCC and Clang allocator/runtime tests pass locally; Windows build/CTest remains pending user rerun and is not claimed here.
