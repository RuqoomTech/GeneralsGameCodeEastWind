# Step 05 — W3X Phase A: Parser / Import Foundation

## Purpose

Add the first real EA SAGE W3X capability without coupling XML parsing to Direct3D or destabilizing the existing W3D loader.

## Preconditions

- Step 01 deterministic guardrails are available;
- Step 02 canonical build/test path is working;
- Step 03 telemetry may proceed independently, but W3X parser tests must be runnable without the game renderer.

## Completed pre-work entering Step 05

- A0 format recognition is already implemented and tested.
- A1 document-envelope probing is already implemented and tested.
- A2 top-level child-element discovery is already implemented and tested for the initial W3D child kinds plus unknown preservation.
- A2R consolidated A1/A2 behind `rts/w3x_document.h` with implementation in `Core/Libraries/Source/rts/w3x_document.cpp`; future parser work must extend or deliberately replace this seam rather than create parallel XML scanners.
- Step 05 therefore starts **after** file classification, root-envelope recognition, direct-child discovery, and parser-seam cleanup; it must not duplicate those primitives.
## Deliverables

### 1. Format fixtures

Create minimal synthetic/legal fixtures covering:

- valid W3X root/document structure;
- include/reference handling;
- one rigid mesh;
- malformed XML;
- unsupported namespace/version;
- missing required data.

Do not commit proprietary game assets simply for tests.

### 2. XML component

Choose or implement an XML parser behind a narrow project-owned interface. Requirements:

- CMake target-scoped dependency;
- MinGW/GCC and Clang compatibility;
- clear parse errors with file and location;
- unit-testable without renderer initialization;
- no long-lived XML DOM types in runtime asset structures.

### 3. Format router

Introduce format detection/routing without rewriting the W3D chunk loader.

Conceptually:

```text
Load 3D asset
   |
   +-- W3D -> existing ChunkLoadClass path
   |
   +-- W3X -> new XML importer path
```

Content validation must supplement extension checks.

### 4. Neutral import model

Define only the structures needed by the first W3X fixture, with room to expand deliberately. Initial concepts likely include:

- asset/document identity;
- mesh name;
- positions;
- indices;
- normals;
- UVs;
- submesh/material references;
- bounds;
- hierarchy references where needed.

Do not add D3D8/D3D12 GPU handles here.

### 5. Validation/reporting

Provide diagnostics for:

- unsupported elements/attributes;
- broken references;
- invalid indices;
- empty/degenerate geometry;
- unsupported data types/versions.

### 6. Tests

Tests must verify:

- known fixture parses;
- malformed fixture fails predictably;
- normalized geometry data matches expected values;
- format router chooses W3D vs W3X correctly;
- W3D loading remains behaviorally unchanged for covered fixtures.

## Non-goals

This step does **not** need to:

- render W3X through DX8;
- render W3X through D3D12;
- support every later-SAGE element;
- implement all animations/materials;
- convert W3X into W3D;
- create a proprietary replacement format.

## Acceptance criteria

- W3X parser/import tests run from the command line;
- at least one representative rigid W3X fixture reaches the neutral import model;
- malformed/unsupported files produce useful diagnostics;
- existing W3D path remains intact;
- parser code is shared/Core-side where practical rather than duplicated between Generals and Zero Hour;
- W3X support matrix is updated with exactly what is supported;
- `PROJECT_STATE.md`, `BACKLOG.md`, and `WORKLOG.md` are updated.
