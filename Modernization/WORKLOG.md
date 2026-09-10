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
