# Asset Pipeline Modernization

## Goal

Support legacy Generals/Zero Hour assets while introducing a modern asset path suitable for high-poly models, richer materials, large textures, D3D12, and offline validation.

## Formats

### W3D — compatibility format

W3D remains fully supported. Existing mods must not be forced to convert assets merely to run on the modern engine.

### W3X — Evolution asset format

Add the EA SAGE W3X XML-based model/asset format used by later SAGE games. W3X provides a natural path for richer structured data and a modern content toolchain.

See [`W3X_SUPPORT.md`](W3X_SUPPORT.md).

## Target architecture

Do not make GPU objects part of file parsing.

```text
W3D binary loader ----\
                      \
                       -> Asset Import Model -> Validation -> Runtime Asset -> GPU upload
                      /
W3X XML loader -------/
```

The `Asset Import Model` should be renderer-neutral and suitable for tests/tools.

Candidate concepts:

- mesh/submesh;
- index format;
- vertex streams/attributes;
- skeleton/bones/weights;
- animation clips/channels;
- hierarchy/pivots;
- materials/textures;
- UV sets/tangents/normals;
- LOD groups;
- collision/selection geometry;
- bounds;
- metadata/user properties.

The exact C++ type design belongs to the implementation milestone, not this planning document.

## Runtime vs offline processing

Long-term, expensive validation/transformation should happen offline where possible:

```text
source W3D/W3X
      |
      v
 importer / validator
      |
      v
 normalized/cached runtime representation (optional future step)
      |
      v
 game runtime
```

Direct loading of original W3D/W3X should remain useful for development/modding even if an optional optimized cache format is introduced later.

## High-poly requirements

The modern asset path must allow:

- explicit 32-bit index data;
- large vertex counts;
- multiple submeshes/materials;
- robust tangent/normal data;
- multiple UV sets where W3X content provides them;
- efficient LOD definitions;
- separate simplified collision/shadow geometry where available;
- bounds suitable for aggressive culling.

## Texture requirements

Plan for:

- DDS and modern block compression;
- sRGB correctness;
- normal-map handling;
- mip chains;
- future residency/streaming;
- material texture roles instead of filename heuristics where possible.

## Tooling

The modernization program should eventually provide:

- command-line asset inspector;
- W3D/W3X validation;
- statistics report (poly count, materials, textures, bones, LODs);
- conversion/caching tools if needed;
- Blender workflow validation using available community W3D/W3X tooling as a compatibility reference, without making a Blender plugin a runtime dependency.
