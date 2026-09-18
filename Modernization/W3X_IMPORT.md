# W3X import foundation

This document defines the active parser/import work for EA SAGE XML W3X assets. W3X support is additive beside the existing W3D chunk loader; it is not a replacement format and is not connected to the runtime asset manager yet.

## Existing parser seam

The repository already provides:

- W3D/W3X format recognition;
- W3X document-envelope probing;
- direct `AssetDeclaration` child discovery and classification;
- one shared public seam in `rts/w3x_document.h`, implemented by `Core/Libraries/Source/rts/w3x_document.cpp`.

New parser work must extend or deliberately replace that seam rather than introducing another XML scanner. The Step 05A repository cleanup is complete locally before parser growth begins.

## Parser component — 05B

Integrate a real XML parser behind a narrow project-owned interface. The parser layer must:

- be target-scoped in CMake;
- build with MinGW-w64 GCC and Clang;
- report parse failures with source/file location where available;
- run in unit tests without renderer or game initialization;
- keep XML-library DOM types out of long-lived runtime asset structures.

Fixtures should cover a valid document, include/reference handling, malformed XML, unsupported namespace/version, and missing required data. Tests must use synthetic or legally redistributable data rather than proprietary game assets.

## Neutral import model — 05C

Decode the first supported rigid-mesh W3X content into renderer-neutral structures. The initial model should contain only what the first supported fixture requires, such as:

- asset/document identity;
- mesh name;
- positions, normals and UVs;
- indices;
- submesh/material references;
- bounds;
- hierarchy references where required by the fixture.

Do not put D3D8/D3D12 resources, handles or renderer ownership in the import model.

Validation should reject or clearly diagnose broken references, invalid indices, empty/degenerate geometry, unsupported data types/versions, and unsupported required elements/attributes.

## Format routing and validation — 05D

Asset routing must preserve the existing W3D loader and add W3X beside it:

```text
Load 3D asset
   |
   +-- W3D -> existing ChunkLoadClass path
   |
   +-- W3X -> W3X parser/import path
```

Extension checks are not sufficient on their own; the selected loader must validate the actual content format.

Tests must prove that:

- valid W3X reaches the neutral model;
- malformed/unsupported W3X fails predictably with useful diagnostics;
- normalized rigid-mesh data matches expected values;
- routing selects W3D vs W3X correctly;
- covered W3D behavior remains unchanged.

## Out of scope for this foundation

This work does not need to render W3X through D3D8 or D3D12, implement every later-SAGE element, complete animation/material support, convert W3X to W3D, or invent another proprietary asset format.

## Completion criteria

The W3X import foundation is complete when at least one representative rigid-mesh fixture reaches the neutral model through command-line tests, malformed/unsupported input has useful diagnostics, the existing W3D path remains intact, shared/Core code owns the parser/import implementation where practical, and the support matrix/current project documentation accurately describes the supported subset.
