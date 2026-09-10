# Step 02 — Command-Line Build System Foundation

## Status

**ACTIVE. Step 02A implemented locally; Windows MinGW sign-off is still required.**

Step 01G remains the deterministic compatibility baseline. Step 02A changes build-system structure only; it does not change simulation, replay, CRC, Xfer, network, W3D/W3X runtime behavior, or renderer code.

## Purpose

Make CMake + Ninja + MinGW-w64 GCC the canonical Windows command-line path for the real Zero Hour codebase without requiring the Visual Studio IDE, while keeping the Step 01 determinism gate cheap enough to run on every build-system change.

## Step 02A implementation

### Canonical preset family

The primary names are now:

- `mingw32-release`
- `mingw32-debug`
- `mingw32-profile`
- `mingw32-tests`

All use Ninja and `cmake/toolchains/mingw-w64-i686.cmake`. The older `mingw-w64-i686*` names remain compatibility aliases, including `mingw-w64-i686-determinism`, so the signed-off Step 01 command remains valid.

The runtime presets build Zero Hour by default and disable Generals plus MFC-dependent tool targets. They do not change the eventual x64/D3D12 direction; these i686 presets are the compatibility/reference build used while modernization proceeds.

### Focused modernization test graph

`RTS_BUILD_TESTS_ONLY=ON` configures only the small dependency set needed by modernization characterization tests. It does not configure the game runtime, renderer, legacy tools, WIDL-dependent browser libraries, DirectX SDK FetchContent dependencies, GameSpy, LZHL, stb, or resources.

`Core/Tests/CMakeLists.txt` now owns the test wiring for:

- W3X A0 asset-format recognition;
- W3X A1 document-envelope probing;
- W3X A2 direct-child discovery;
- Step 01 determinism characterization.

On Win32 the `z_determinismtest` target preserves the Step 01G production-unit set, Zero Hour header precedence, `Utility/CppMacros.h` prelude, ReactOS ATL compatibility, and required MinGW IPO/LTO link isolation. On non-Windows hosts the same test source runs its lightweight CRC/RNG/float characterization path. `z_determinismcheck` remains available as the mandatory direct gate.

### Compiler/warning scoping

`-Wsuggest-override` is no longer injected into every language/target. It is attached to `core_config` only for C++ compilation, so C sources no longer receive a C++-only warning flag and fetched third-party targets do not inherit it.

MinGW compatibility flags/defines and legacy Windows libraries are also attached through `core_config` instead of global `add_compile_options()`, `add_compile_definitions()`, `add_link_options()`, and `link_libraries()` calls. This reduces build leakage into vendored projects while preserving the compatibility tree's `-fno-strict-aliasing` and static GCC runtime behavior.

The global `-mwindows` injection was removed. GUI subsystem selection remains target-driven (`WIN32` executables already request it), while console tests can remain console binaries.

### FetchContent cleanup

The source-only ReactOS ATL, zlib 1.1.4, and LZHL downloads no longer use deprecated direct `FetchContent_Populate()` calls. They use `FetchContent_Declare()` + `FetchContent_MakeAvailable()` with a deliberately non-existent `SOURCE_SUBDIR` so CMake fetches their source without trying to configure the upstream projects.

ReactOS ATL, legacy zlib/LZHL headers, and stb includes are marked as system includes at their project-facing boundary to reduce third-party header noise without suppressing warnings in project code.

### MinGW toolchain discovery

The i686 toolchain now:

- defaults native Windows builds to the MSYS2 `MINGW32` root (`C:/msys64/mingw32`);
- honors `MINGW_PREFIX` / `MSYSTEM_PREFIX` when present;
- accepts explicit `-DRTS_MINGW_ROOT=...` without requiring PATH edits;
- resolves compiler/binutils from that root first;
- validates `gcc -dumpmachine` and fails early if a 64-bit/wrong MinGW environment was selected;
- exposes the resolved MinGW bin directory to WIDL/debug-strip discovery;
- uses static-library try-compiles so cross-toolchain feature checks do not require executable runtime setup.

Debug symbol stripping now searches only the selected compiler/bin directory instead of falling through to unrelated host `objcopy`/`strip` tools.

### WIDL discovery

`cmake/widl.cmake` is now MinGW-specific and can discover WIDL from:

- `RTS_WIDL_ROOT`;
- `WIDL_ROOT`;
- the selected MinGW bin directory (the native MSYS2 MINGW32 location for `widl.exe`).

IDL include discovery prefers `${RTS_MINGW_ROOT}/include` on native MSYS2 MINGW32 (where `oaidl.idl` is installed), while retaining Wine include fallbacks for Linux cross-build hosts. `RTS_WIDL_INCLUDE_DIR` remains an explicit override. The focused `mingw32-tests` graph does not require WIDL at all.

## Local regression gate

A host-native lightweight graph can be exercised without downloading the runtime dependency set:

```sh
cmake -S . -B build/local-step02a-tests -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DRTS_BUILD_TESTS_ONLY=ON
cmake --build build/local-step02a-tests
ctest --test-dir build/local-step02a-tests --output-on-failure
cmake --build build/local-step02a-tests --target z_determinismcheck
```

This validates build-system wiring plus W3X A0/A1/A2 and the lightweight Step 01 determinism path. It does **not** replace the Win32 ABI/Xfer/replay gate.

### Local validation performed for Step 02A

- GCC 14.2: focused CMake/Ninja build + CTest, 4/4 tests passed.
- Clang 17: focused CMake/Ninja build + CTest, 4/4 tests passed.
- Lightweight determinism: GCC and Clang at `-O0`, `-O2`, and `-O3` passed.
- GCC AddressSanitizer and UBSan determinism runs passed.
- Standalone W3X A0/A1/A2 passed with both GCC and Clang.
- Preset/test/workflow JSON was accepted by CMake/CTest listing commands.

## Windows validation required for Step 02A sign-off

From PowerShell in an MSYS2 MINGW32-capable environment:

```powershell
cmake --preset mingw32-tests
cmake --build --preset mingw32-tests
ctest --preset mingw32-tests --output-on-failure
cmake --build --preset mingw32-tests --target z_determinismcheck
```

Then validate the real runtime configure/build path:

```powershell
cmake --preset mingw32-release
cmake --build --preset mingw32-release --target z_generals
```

`mingw32-debug` and `mingw32-profile` are equivalent configure/build entry points for those configurations.

Do not mark Step 02A Windows-verified until the actual Windows output is supplied. In particular, the expected Step 01 final line must still be:

```text
Step 01 determinism guard passed: float helpers, CRC/RNG, Xfer/XferCRC, snapshot, ABI, and replay checkpoints.
```

## Remaining Step 02 work

Step 02A intentionally does not attempt a broad legacy CMake rewrite. Remaining work after Windows feedback includes fixing concrete compile/link blockers in the full `z_generals` MinGW build, reviewing any remaining target-global dependency leakage, adding/adjusting CI only after the canonical Windows commands are proven, and then deciding whether a secondary Clang preset is mature enough to promote.

## Non-goals

- no Direct3D 12 renderer implementation;
- no Direct3D 11 intermediate renderer;
- no x64 runtime port yet;
- no simulation/replay/network/Xfer format changes;
- no W3X runtime importer expansion;
- no removal of VC6/reference build support.
