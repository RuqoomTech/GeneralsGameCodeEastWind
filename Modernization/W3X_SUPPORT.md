# EA SAGE W3X Support

## Scope

In this project, **W3X** means the XML-based evolution of Westwood 3D used by later EA SAGE games such as Command & Conquer 3 / Kane's Wrath and Red Alert 3.

It does **not** mean the unrelated Warcraft III `.w3x` map container format.

## Why W3X belongs in this modernization

W3D must remain for Generals/Zero Hour compatibility, but W3X is a useful Evolution path because it is structured, human/tool-readable, and already part of the broader SAGE asset lineage.

The goal is not merely "open an XML file". The goal is to make W3X a first-class 3D asset input that can participate in:

- high-poly meshes;
- richer materials;
- multiple material/submesh layouts;
- multiple UV sets where represented;
- skeletons and skinned meshes;
- animations;
- LOD/hierarchy data;
- collision/selection metadata;
- modern D3D12 runtime resources.

## Existing baseline

No W3X loader was found in the current GeneralsGameCode baseline.

Legacy W3D loading currently enters through `WW3DAssetManager::Load_3D_Assets`, uses `ChunkLoadClass`, and dispatches to hierarchy/animation/prototype loaders.

W3X should **not** be implemented by pretending XML is a W3D chunk stream.

## Proposed integration

### Format routing

Introduce a shared asset format router at or above the WW3D asset-manager boundary:

```text
asset request
    |
    v
format detection
   / \
 W3D W3X
  |   |
  v   v
W3D  W3X
loader parser
   \ /
    v
normalized import model
    |
    v
runtime prototype/resource creation
```

Detection should validate content, not only extension, because `.w3x` is used by unrelated software ecosystems.

### Shared implementation

New W3X parsing should live in shared/Core code where practical so Generals and Zero Hour do not receive duplicated parsers.

Adapters can bridge into game-specific asset managers during the transition.

### XML parser

The XML parser must:

- be a target-scoped dependency or small isolated component;
- support deterministic, testable parsing independent of graphics;
- produce actionable line/element diagnostics;
- reject malformed or unsupported data safely;
- avoid coupling DOM/node objects to long-lived runtime resources.

Parser-library selection is an implementation decision for W3X Phase A and should consider portability, maintenance, size, and licensing.

Implementation specification for the first parser milestone: [`STEP_05_W3X_IMPORT_FOUNDATION.md`](STEP_05_W3X_IMPORT_FOUNDATION.md).

## Support phases

### W3X-A — Discovery, fixtures, parser foundation

Deliverables:

- collect legally redistributable/minimal synthetic W3X fixtures;
- document supported namespaces/root structures;
- identify C&C3/Kane's Wrath/RA3 dialect differences;
- XML parser abstraction/dependency;
- parse document and includes/references;
- command-line/unit tests;
- clear diagnostics.

Exit gate: parser can load and validate representative W3X documents without renderer/game startup.

### W3X-B — Geometry and hierarchy

Deliverables:

- rigid mesh import;
- vertex streams;
- 16/32-bit indices as represented/normalized;
- submesh/material assignment;
- normals/tangents/UV sets;
- pivots/hierarchy;
- bounds;
- LOD data where available.

Exit gate: neutral imported geometry matches fixture expectations and can be inspected by tooling.

### W3X-C — Materials and textures

Deliverables:

- texture references;
- material parameters;
- normal/specular/emissive-style data where present;
- multiple materials;
- mapping into the modern material model with documented fallback rules.

### W3X-D — Skeletons and animation

Deliverables:

- skeleton/bone import;
- skin weights;
- animation clips/channels;
- visibility/motion channels where relevant;
- test fixtures and validation.

### W3X-E — D3D12 runtime rendering

Deliverables:

- rigid W3X model visible through D3D12;
- skinned/animated W3X model;
- LOD switching;
- material rendering;
- shadow/depth passes;
- performance counters.

### W3X-F — Toolchain and completeness

Deliverables:

- command-line validation/reporting;
- modder documentation;
- compatibility tests against representative later-SAGE W3X assets;
- Blender import/export workflow validation using available community tooling as a reference;
- document unsupported features explicitly.

## Compatibility policy

- W3X support is additive; W3D remains supported.
- A W3X asset must not silently alter simulation behavior.
- Rendering-only metadata stays outside CRC/gameplay state unless explicitly designed otherwise.
- Unsupported features should warn/fail predictably rather than render corrupt data.

## Initial compatibility target

Start with representative C&C3/Kane's Wrath-style W3X assets because they are a direct later-SAGE evolution and have public modding examples/tooling. Expand to Red Alert 3 dialect/features after differences are characterized.

Do not claim "full W3X support" until the support matrix is documented and tested.

## Reference projects/specification sources

Useful external references for behavior and fixtures include:

- OpenSAGE file-format documentation, which identifies W3X as an XML-based evolution of W3D;
- OpenSAGE Blender Plugin, which imports/exports W3D and W3X;
- official/public EA C&C modding-support XML/W3X asset references where legally available.

Use references to understand format behavior; do not copy code without verifying license compatibility and attribution requirements.
