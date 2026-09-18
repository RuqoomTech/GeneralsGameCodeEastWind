# Build system

## Supported Evolution command-line lane

The primary Windows modernization path is:

```text
CMake + Ninja + MinGW-w64 GCC (x86_64)
```

The canonical focused preset is `mingw64-tests`.

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\setup-windows-dev.ps1
cmake --preset mingw64-tests
cmake --build --preset mingw64-tests
ctest --preset mingw64-tests --output-on-failure
```

The setup script installs/verifies the MSYS2 x86_64 toolchain, CMake, Ninja, WIDL, Python and Git. Visual Studio is not required for this path.

## Focused graph

`mingw64-tests` enables:

- `RTS_BUILD_TESTS_ONLY=ON`
- `RTS_BUILD_TESTS=ON`
- `RTS_BUILD_EVOLUTION_X64=ON`
- `RTS_BUILD_HEADLESS_CORE=ON`

It intentionally excludes Generals/Zero Hour monolithic runtime targets, core tools, D3D8, DirectInput and DirectSound dependencies that are irrelevant to the focused deterministic/protocol/W3X tests.

The full MinGW x64 game runtime remains subsystem-gated until the platform/renderer boundaries required by later milestones are ready. The focused graph is not a claim that the legacy D3D8 executable is already the Evolution runtime.

## MinGW runtime policy

Focused MinGW tests statically link `libgcc` and `libstdc++`. This prevents an ambient mixed MSYS2 `PATH` from loading an ABI-incompatible runtime family into binaries produced by the `mingw64` compiler.

The project configuration keeps MinGW-specific compatibility flags and Windows libraries target-scoped through `core_config` rather than injecting them globally into third-party targets.

## WIDL

Win64 IDL generation uses `--win64`. Full runtime configuration validates WIDL and the required Windows IDL imports before populating runtime dependencies. The focused test graph does not require unrelated runtime dependency population.

## Runtime installation

Generals and Zero Hour runtime installation is centralized through `rts_install_runtime_target()`:

- MSVC may install PDB files when present;
- MinGW Release builds use the existing `.debug` sidecar flow;
- unsupported linker-specific generator expressions are covered by `buildsystem_runtime_install_policy`.

## Legacy/reference presets

Historical VC6/MSVC/Win32 presets may remain in the wider repository for upstream compatibility and reference work. They are not supported Evolution runtime targets.

The active modernization tree must not restore `mingw32-*` modernization presets, an i686 MinGW toolchain wrapper, or the retired x86 determinism-oracle workflow.

## Test naming

Active test names describe subsystem responsibility rather than the migration step in which they were introduced. See [`../TESTING.md`](../TESTING.md) for the current list and validation commands.
