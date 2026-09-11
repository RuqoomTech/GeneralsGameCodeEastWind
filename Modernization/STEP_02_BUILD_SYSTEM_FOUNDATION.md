# Step 02 — Command-Line Build System Foundation

## Status

**IMPLEMENTATION COMPLETE / USER ACCEPTED.** The user reports the Step 02B Windows build path working; no console transcript was supplied for archival, so this document does not claim a newly captured Windows test pass.

Step 01G remains the deterministic compatibility baseline. Steps 02A/02B change build-system structure only; they do not change simulation, replay, CRC, Xfer, network, W3D/W3X runtime behavior, or renderer code.

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

## Step 02B implementation

### Real runtime generation blocker: MSVC-only PDB install rules

The Generals and Zero Hour top-level install blocks previously emitted `$<TARGET_PDB_FILE:...>` for every installable executable whenever an install prefix was detected or supplied. CMake evaluates that generator expression during generation; with a GNU/MinGW linker it is unsupported, so `OPTIONAL` does not protect the configure/generate step. A minimal GNU reproduction fails with `TARGET_PDB_FILE is not supported by the target linker`.

Step 02B centralizes runtime installation in `cmake/debug_strip.cmake` through `rts_install_runtime_target(target, destination)`. The helper:

- always installs the runtime target;
- emits the PDB rule only when `MSVC` is true;
- emits a MinGW Release `.debug` install rule only when the existing GNU debug-strip tools were found;
- rejects accidental calls for non-target names.

The duplicated per-target PDB blocks in both `Generals/CMakeLists.txt` and `GeneralsMD/CMakeLists.txt` are replaced by this one shared policy. `Core/Tests` now registers `buildsystem_runtime_install_policy`, backed by `cmake/tests/RuntimeInstallPolicyTest.cmake`, which creates a tiny nested runtime target and exercises configure/build/install through the same helper. This is deliberately a build/install correction, not a runtime-code change.

### WIDL is now a true preflight

The full MinGW graph used to discover WIDL after ReactOS ATL had already been populated, then fail much later when EABrowser IDL generation was configured or built. Step 02B moves WIDL discovery and `rts_require_widl_for_runtime()` ahead of ReactOS ATL and all remaining runtime dependency population.

For native Windows the preflight also validates the actual system IDL imports used in-tree: `oaidl.idl` and `ocidl.idl`. The canonical MSYS2 MINGW32 toolchain group supplies `widl.exe` through `mingw-w64-i686-tools` and the IDL files through `mingw-w64-i686-headers`. Explicit `RTS_WIDL_ROOT`, `WIDL_ROOT`, and `RTS_WIDL_INCLUDE_DIR` overrides remain supported. Linux-hosted Wine WIDL discovery remains less strict because Wine may provide built-in/default include search paths.

The focused `mingw32-tests` graph intentionally continues to bypass WIDL and the runtime dependency graph.

### Step 02B local validation

- GCC 14.2 focused CMake/Ninja/CTest: 5/5 tests passed.
- Clang 17 focused CMake/Ninja/CTest: 5/5 tests passed.
- Lightweight determinism reran successfully under GCC 14.2 and Clang 17 at `-O0`, `-O2`, and `-O3`; GCC ASan and UBSan also passed.
- Generic host-GNU `rts_install_runtime_target()` configure/build/install probe passed.
- Synthetic GNU execution of the MinGW Release debug-strip/install branch produced and installed both the executable and `.debug` sidecar.
- The old unconditional GNU `$<TARGET_PDB_FILE:...>` pattern was reproduced separately and failed at CMake generation exactly as expected.

No i686 MinGW cross compiler is installed in the execution environment used for Step 02B, and external package installation is unavailable there. Therefore the real Win32 `z_generals` compiler/linker frontier still requires Windows validation; Step 02B is not marked Windows-verified.

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

## Windows validation record

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

The user reports these paths working after Step 02B. Because the actual Step 02 Windows console output is not stored with this baseline, retain the commands below as the reproducible re-validation gate and do not invent a formal transcript. The Step 01 direct gate must still end with:

```text
Step 01 determinism guard passed: float helpers, CRC/RNG, Xfer/XferCRC, snapshot, ABI, and replay checkpoints.
```

## Follow-up build-system work

Step 02 is no longer the active milestone. Build warnings, secondary Clang promotion, CI refinements, and remaining target-global dependency cleanup are maintenance items unless they block a later milestone. Warning suppression must remain scoped to vendored/third-party code; warnings from project-owned code must remain visible.

## Non-goals

- no Direct3D 12 renderer implementation;
- no Direct3D 11 intermediate renderer;
- no x64 runtime port yet;
- no simulation/replay/network/Xfer format changes;
- no W3X runtime importer expansion;
- no removal of VC6/reference build support.
