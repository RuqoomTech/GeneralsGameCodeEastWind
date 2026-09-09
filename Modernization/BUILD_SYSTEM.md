# Build System Modernization

## Goal

The project must configure, build, test, and eventually package from the command line without requiring the Visual Studio IDE.

## Primary Windows path

```text
CMake
  -> CMake Presets
  -> Ninja
  -> MinGW-w64 GCC
```

Secondary path:

```text
CMake + Ninja + Clang
```

Optional comparison path:

```text
CMake + Ninja + standalone MSVC Build Tools
```

The Visual Studio IDE is not a project dependency.

## Existing groundwork

This baseline already contains:

- MinGW-w64 i686 toolchain support;
- MinGW presets;
- ReactOS ATL compatibility support;
- WIDL support;
- MinGW release debug-symbol handling;
- Ninja-based standard/VC6 preset infrastructure.

The main gap is making MinGW + Ninja the canonical and reliably tested Windows path.

## Planned preset families

Initial x86/reference development:

- `mingw32-debug`
- `mingw32-release`
- `mingw32-profile`
- `mingw32-tests`
- later `clang32-*`

Future Evolution runtime:

- `mingw64-*`
- `clang64-*`

## Required work

### 1. Canonical Ninja presets

Move the MinGW path away from `Unix Makefiles` and standardize cache/build/test presets.

### 2. Generator-neutral CMake

Audit assumptions tied to multi-config generators, Visual Studio, and MSVC-only flags.

### 3. Runtime/tool separation

The game runtime and automated tests must configure without requiring legacy GUI/MFC tools. Unsupported old tools should be optional, not blockers.

### 4. Target-scoped dependencies

Windows/DirectX/third-party include and library paths should be target-scoped instead of globally injected where practical.

### 5. Actionable dependency discovery

Avoid one-machine absolute SDK paths. Support explicit cache/environment roots and clear errors.

### 6. Test presets

`ctest --preset ...` should work for deterministic/asset/build tests without needing to launch the full game.

## Determinism gate

Compiler migration is not complete because an executable launches.

Before GCC becomes the accepted reference development compiler, verify:

- deterministic RNG behavior;
- CRC vectors/state;
- Xfer/snapshot representation where relevant;
- critical layout assumptions;
- replay CRC/command behavior.

Any mismatch must be understood before acceptance.

## Relationship to D3D12 and W3X

The build system must prepare for:

- x64 toolchains;
- D3D12/DXGI and DXC dependencies;
- asset-parser tests independent of the renderer;
- W3X XML parsing as a target-scoped dependency/component;
- offline asset tools and validation utilities.
