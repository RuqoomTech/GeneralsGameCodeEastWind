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

## Focused test paths

`mingw32-tests` sets `RTS_BUILD_TESTS_ONLY=ON` for the i686 compatibility/reference lane. The root graph configures only the focused modernization dependencies and tests, retaining ReactOS ATL only because the signed-off 32-bit determinism target consumes it.

`mingw64-tests` is the Step 04A x64 readiness lane. It also sets `RTS_BUILD_TESTS_ONLY=ON`, enables `RTS_BUILD_X64_READINESS`, uses the x86_64 MinGW wrapper, and intentionally does **not** populate ATL or link the legacy D3D8/DirectInput/DirectSound runtime libraries.

This separation lets architecture/serialization guards compile on x64 before the full legacy runtime is enabled. CTest owns W3X A0/A1/A2, Step 01 lightweight determinism, Step 02 install policy, Step 03 telemetry schema, and Step 04A architecture width coverage.

## Dependency and warning policy

Project compiler compatibility is propagated through `core_config` wherever practical. C++-only warnings use generator expressions so they do not reach C files. Fetched/vendor code should not inherit project-only warnings, and vendor include boundaries may be marked `SYSTEM`; warnings in our own source must remain visible.

Source-only FetchContent dependencies use the modern declare/make-available flow rather than direct `FetchContent_Populate()`.

## Tool discovery

MinGW tool discovery is shared by architecture wrappers in `cmake/toolchains/mingw-w64-common.cmake`:

- i686 defaults to MSYS2 `C:/msys64/mingw32` and triplet `i686-w64-mingw32`;
- x86_64 defaults to MSYS2 `C:/msys64/mingw64` and triplet `x86_64-w64-mingw32`.

`RTS_MINGW_ROOT` can override either root explicitly. The shared toolchain validates the selected compiler triplet before the project graph is configured.

WIDL is normally discovered from the selected MSYS2 MINGW32 root (`mingw-w64-i686-tools`, included in the i686 toolchain group). `RTS_WIDL_ROOT`/`WIDL_ROOT` can override the tool root, and `RTS_WIDL_INCLUDE_DIR` can override the directory containing `oaidl.idl`. Linux cross-host Wine include layouts remain supported. The focused test preset intentionally does not require WIDL.

## Runtime preflight and install artifacts

Step 02B moves mandatory full-runtime WIDL discovery ahead of ReactOS ATL and the other runtime dependency population. The focused `mingw32-tests` graph still skips WIDL entirely. For a real MinGW runtime, missing `widl.exe` now fails immediately with the MINGW32 package/override guidance; on native Windows the preflight also checks for `oaidl.idl` and `ocidl.idl`, which are imported by the in-tree EABrowser IDLs.

Installable runtime targets use `rts_install_runtime_target()` from `cmake/debug_strip.cmake`. This keeps compiler-specific debug artifacts out of game-specific CMakeLists:

- MSVC: install the target plus its PDB when present;
- MinGW Release: install the target plus the `.debug` sidecar created by `add_debug_strip_target()` when GNU binutils are available;
- other GNU configurations: install the runtime without ever evaluating the MSVC-only `TARGET_PDB_FILE` generator expression.

This specifically avoids the CMake generation failure that occurs when an installed game path causes an unconditional PDB rule to be evaluated with a GNU/MinGW linker. `buildsystem_runtime_install_policy` exercises this policy in a tiny nested CMake/Ninja project as part of the focused modernization test graph.

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

Step 02 is implementation-complete and user-accepted. On 2026-09-11 the user reported that the real Windows MinGW/Ninja path works, with expected legacy warnings. No console transcript was archived, so this document records acceptance without inventing a formal captured verification result.

## Profile telemetry build option

Completed Step 03 uses `RTS_BUILD_OPTION_PERF_TELEMETRY`. The canonical `mingw32-profile` preset enables it; `mingw32-release` and `mingw32-debug` leave it off. `RTS_PERF_TELEMETRY` brackets the engine update phases, client visibility snapshot, and primary WW3D render frame while reusing existing renderer statistics.

CSV output remains runtime opt-in through `RTS_PERF_CAPTURE=<path>`. Schema v2 is summarized with `scripts/perf-summary.py`; Tracy consumes the same sample rather than collecting a duplicate set.

## Step 04A x64 readiness command

```text
cmake --preset mingw64-tests
cmake --build --preset mingw64-tests
ctest --preset mingw64-tests --output-on-failure
```

This is intentionally a focused x64 gate. A full x64 runtime configure is rejected until later Step 04 slices migrate pointer/handle, allocator, serialization/network, and client/platform boundaries.
