# Build System Modernization

## Goal

The project must configure, build, test, and eventually package from the command line without requiring the Visual Studio IDE.

## Primary Windows path

```text
CMake -> CMake Presets -> Ninja -> MinGW-w64 GCC
```

Secondary compiler work may use Clang. Standalone MSVC Build Tools remain an optional comparison path; the Visual Studio IDE is not a project dependency.

## Canonical i686 compatibility presets

Step 02A establishes these primary names:

- `mingw32-release`
- `mingw32-debug`
- `mingw32-profile`
- `mingw32-tests`

The older `mingw-w64-i686*` names remain compatibility aliases so Step 01 scripts/commands do not break.

The i686 runtime presets are deliberately Zero Hour-first: Generals and MFC-dependent legacy tools are disabled unless explicitly requested/supported. The long-term Evolution runtime remains x64 + D3D12; these are compatibility/reference presets, not the final renderer architecture.

## Focused test path

`mingw32-tests` sets `RTS_BUILD_TESTS_ONLY=ON`. The root CMake graph then configures only `core_config`, Utility compatibility headers, ReactOS ATL on MinGW, and `Core/Tests`.

This prevents a determinism/W3X regression run from fetching or configuring unrelated renderer/runtime dependencies. CTest owns W3X A0/A1/A2 and the Step 01 gate.

## Dependency and warning policy

Project compiler compatibility is propagated through `core_config` wherever practical. C++-only warnings use generator expressions so they do not reach C files. Fetched/vendor code should not inherit project-only warnings, and vendor include boundaries may be marked `SYSTEM`; warnings in our own source must remain visible.

Source-only FetchContent dependencies use the modern declare/make-available flow rather than direct `FetchContent_Populate()`.

## Tool discovery

Native Windows MinGW defaults to the MSYS2 MINGW32 root. `RTS_MINGW_ROOT` can override it explicitly. The toolchain validates the compiler target triplet before the full graph is configured.

WIDL is normally discovered from the selected MSYS2 MINGW32 root (`mingw-w64-i686-tools`, included in the i686 toolchain group). `RTS_WIDL_ROOT`/`WIDL_ROOT` can override the tool root, and `RTS_WIDL_INCLUDE_DIR` can override the directory containing `oaidl.idl`. Linux cross-host Wine include layouts remain supported. The focused test preset intentionally does not require WIDL.

## Required Windows commands

Focused regression:

```text
cmake --preset mingw32-tests
cmake --build --preset mingw32-tests
ctest --preset mingw32-tests --output-on-failure
```

Real Zero Hour release build:

```text
cmake --preset mingw32-release
cmake --build --preset mingw32-release --target z_generals
```

Step 02 remains active until those real Windows paths are verified and remaining concrete runtime blockers are resolved.
