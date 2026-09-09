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
