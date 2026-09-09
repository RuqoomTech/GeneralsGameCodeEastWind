# Step 02 — Command-Line Build System Foundation

## Purpose

Make CMake + Ninja + MinGW-w64 GCC the canonical Windows development build without requiring the Visual Studio IDE.

## Starting state

The baseline already has MinGW-w64 i686 support, but its preset uses `Unix Makefiles`. Standard modern Windows presets already demonstrate Ninja infrastructure.

## Deliverables

### Presets

Create/normalize:

- `mingw32-debug`
- `mingw32-release`
- `mingw32-profile`
- `mingw32-tests`

with Ninja and a consistent binary-directory convention.

### Configure independence

Allow runtime/tests to configure without unrelated legacy tools wherever possible.

### Compiler cleanup

Audit:

- MSVC-only options;
- assumptions about multi-config builds;
- calling conventions/packing;
- warnings/diagnostics;
- resource compilation;
- MinGW runtime/link behavior.

### Test integration

Run Step 01 deterministic gates under GCC.

## Non-goals

- no D3D12 renderer implementation yet;
- no x64 port yet;
- no simulation refactor;
- no removal of VC6/reference builds.

## Acceptance criteria

The following workflow succeeds on the documented Windows toolchain without the Visual Studio IDE:

```text
cmake --preset mingw32-debug
cmake --build --preset mingw32-debug
ctest --preset mingw32-debug --output-on-failure
```

Equivalent release/profile workflows must be documented.

Determinism/replay checks from Step 01 must not regress.
