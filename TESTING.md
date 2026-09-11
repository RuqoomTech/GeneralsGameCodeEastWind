# Test Replays

The GeneralsReplays folder contains replays and the required maps that are tested in CI to ensure that the game is retail compatible.

You can also test with these replays locally:
- Copy the replays into a subfolder in your `%USERPROFILE%/Documents/Command and Conquer Generals Zero Hour Data/Replays` folder.
- Copy the maps into `%USERPROFILE%/Documents/Command and Conquer Generals Zero Hour Data/Maps`
- Start the test with this: (copy into a .bat file next to your executable)
```
START /B /W generalszh.exe -jobs 4 -headless -replay subfolder/*.rep > replay_check.log
echo %errorlevel%
PAUSE
```
It will run the game in the background and check that each replay is compatible. You need to use a VC6 build with optimizations and RTS_BUILD_OPTION_DEBUG = OFF, otherwise the game won't be compatible.
## Modernization characterization tests

Step 02A/02B keep the W3X A0/A1/A2 tests and Step 01 determinism characterization integrated with CTest through `Core/Tests/CMakeLists.txt`. The focused graph avoids configuring the full runtime/dependency tree.

Host-native GCC/Clang smoke path:

```sh
cmake -S . -B build/local-modernization-tests -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DRTS_BUILD_TESTS_ONLY=ON
cmake --build build/local-modernization-tests
ctest --test-dir build/local-modernization-tests --output-on-failure
```

This runs:

- `w3x_asset_format_test` — W3X A0;
- `w3x_document_probe_test` — W3X A1;
- `w3x_child_discovery_test` — W3X A2;
- `determinism_step01` — lightweight CRC/RNG/float characterization on non-Windows hosts;
- `buildsystem_runtime_install_policy` — nested CMake/Ninja runtime-install policy regression, including the MinGW `.debug` sidecar expectation on MinGW.

The W3X targets remain C++98-compatible and still use the consolidated `rts/w3x_document.h` / `w3x_document.cpp` seam. No runtime W3X importer is exercised.

## Step 01 determinism guard

The full compatibility branch remains Win32-only. It compiles the production RandomValue/Snapshot/Xfer/XferCRC/Damage implementation units directly rather than linking the monolithic Zero Hour GameEngine archive. Step 02A relocates only the CMake wiring into `Core/Tests`; it preserves the Zero Hour header precedence, `Utility/CppMacros.h` prelude, and MinGW IPO/LTO isolation used by the successful Step 01G run.

Expected Windows final line:

```text
Step 01 determinism guard passed: float helpers, CRC/RNG, Xfer/XferCRC, snapshot, ABI, and replay checkpoints.
```

Canonical Step 02 MinGW test workflow:

```powershell
cmake --preset mingw32-tests
cmake --build --preset mingw32-tests
ctest --preset mingw32-tests --output-on-failure
cmake --build --preset mingw32-tests --target z_determinismcheck
```

The historical Step 01 command remains valid through the `mingw-w64-i686-determinism` compatibility alias.

**Step 01G Windows sign-off:** passed on 2026-09-10 with MinGW-w64 i686 / GCC 16.2 + Ninja. **Step 02B Windows status:** on 2026-09-11 the user reported that the real MinGW/Ninja path works, with expected warnings. No Step 02B console transcript was supplied, so do not upgrade that report into an archived verification record.

## Step 02B build-system regression probes

Step 02B additionally locks down two CMake/runtime-foundation behaviors:

- GNU/MinGW generation must not evaluate `$<TARGET_PDB_FILE:...>`; runtime installation is routed through `rts_install_runtime_target()`.
- a full MinGW runtime configure must require WIDL before runtime FetchContent population; native Windows also requires the `oaidl.idl`/`ocidl.idl` headers used by the EABrowser IDLs.

Local validation performed for this step:

- GCC 14.2 focused CMake/Ninja/CTest: 5/5 passed;
- Clang 17 focused CMake/Ninja/CTest: 5/5 passed;
- lightweight determinism matrix: GCC 14.2 and Clang 17 at `-O0`, `-O2`, and `-O3` passed;
- GCC AddressSanitizer and UBSan determinism runs passed;
- host-GNU runtime helper configure/build/install probe: passed;
- synthetic GNU execution of the MinGW Release debug-sidecar branch: executable and `.debug` file both built and installed;
- direct reproduction of the old unconditional GNU `TARGET_PDB_FILE` rule: fails at CMake generation as expected, proving the removed pattern was a real blocker.

The helper probes are build-system tests only. They are **not** substitutes for the Win32 ABI/determinism gate or a real MinGW `z_generals` build.

For explicit Step 02B Windows configure coverage, force a harmless install destination so the install rules are generated even if no retail registry key exists:

```powershell
cmake --preset mingw32-release -DRTS_INSTALL_PREFIX_ZEROHOUR:PATH="$PWD/build/step02b-install-probe"
cmake --build --preset mingw32-release --target z_generals
```

If the build succeeds, the install/debug-sidecar path can then be checked without touching the retail game directory:

```powershell
cmake --install build/mingw32-release
```

## Step 03A performance telemetry gate

The focused test graph includes `performance_telemetry_step03a`. It compiles the production capture implementation directly and checks stable CSV schema v1, frame identity fields, derived triangle/vertex totals, and render/resource counter serialization.

Final local Step 03A validation also compiles/runs that production capture path with GCC and Clang at `-O0`, `-O2`, and `-O3`, plus GCC AddressSanitizer and UBSan. The telemetry-enabled focused graph is separately configured with `RTS_BUILD_OPTION_PERF_TELEMETRY=ON`, while the ordinary focused Release graph is checked to ensure the define is absent.

Host-native regression:

```sh
cmake -S . -B build/step03a-tests -G Ninja -DRTS_BUILD_TESTS_ONLY=ON -DRTS_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build/step03a-tests
ctest --test-dir build/step03a-tests --output-on-failure
```

For the real Windows profile runtime:

```powershell
cmake --preset mingw32-profile
cmake --build --preset mingw32-profile --target z_generals
$env:RTS_PERF_CAPTURE = "Step03A-baseline.csv"
# Launch Zero Hour through the normal runtime/install workflow and run a repeatable scene/replay.
Remove-Item Env:RTS_PERF_CAPTURE
```

Expected capture header:

```text
schema_version,capture_index,render_frame,sync_time_ms,render_cpu_us,draw_calls,triangles,vertices,dx8_triangles,dx8_vertices,skin_draws,skin_triangles,skin_vertices,sorted_triangles,sorted_vertices,texture_bytes,texture_count,texture_changes,lightmap_texture_bytes,lightmap_texture_count,procedural_texture_bytes,procedural_texture_count,memory_allocations,memory_frees
```

The CSV capture itself intentionally adds profiling overhead (especially texture accounting). Compare only runs made with equivalent capture configuration. No Windows Step 03A pass is claimed until actual command/runtime output and a produced CSV are supplied.
