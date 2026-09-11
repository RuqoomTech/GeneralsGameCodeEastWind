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
- `performance_telemetry_step03a` — completed Step 03 CSV schema-v2/phase/resource characterization (historical test name retained);
- `buildsystem_runtime_install_policy` — nested CMake/Ninja runtime-install policy regression, including the MinGW `.debug` sidecar expectation on MinGW;
- `architecture_width_step04a` — native pointer-width/fixed wire-width migration guard.

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

## Step 03 performance telemetry gate

The focused graph retains the historical CTest name `performance_telemetry_step03a`, but the test now protects completed Step 03 CSV schema v2: update/render/logic state markers, engine phase fields, drawable visibility fields, derived geometry totals, and resource counters.

Host-native regression:

```sh
cmake -S . -B build/step03-tests -G Ninja -DRTS_BUILD_TESTS_ONLY=ON -DRTS_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build/step03-tests
ctest --test-dir build/step03-tests --output-on-failure
```

For the real Windows profile runtime:

```powershell
cmake --preset mingw32-profile
cmake --build --preset mingw32-profile --target z_generals
$env:RTS_PERF_CAPTURE = "Step03-baseline.csv"
# Launch Zero Hour and run a repeatable scene/replay.
Remove-Item Env:RTS_PERF_CAPTURE
python scripts/perf-summary.py Step03-baseline.csv
```

Schema v2 begins with:

```text
schema_version,capture_index,render_frame,logic_frame,sync_time_ms,rendered,logic_updated,update_cpu_us,client_cpu_us,logic_cpu_us,network_cpu_us,message_cpu_us,render_cpu_us,...
```

The capture intentionally adds profile overhead. Compare only equivalent capture configurations. GPU timestamp validation is deferred to the D3D12 renderer.


### Step 03 completion / Step 04A local validation (2026-09-11)

Performed against the exact Step 03A baseline used for this implementation:

- GCC 14.2 focused CMake/Ninja/CTest at `-O0`, `-O2`, and `-O3`: 7/7 passed in each configuration;
- Clang 17 focused CMake/Ninja/CTest at `-O0`, `-O2`, and `-O3`: 7/7 passed in each configuration;
- GCC ASan focused suite: 7/7 passed;
- GCC UBSan focused suite: 7/7 passed;
- `z_determinismcheck` passed in the host GCC and Clang focused graphs;
- `scripts/perf-summary.py` parsed a synthetic schema-v2 capture successfully in text and JSON modes;
- compile-command inspection confirmed `RTS_PERF_TELEMETRY` is present only when the telemetry option is enabled;
- host-native 64-bit `architecture_width_step04a` reported a 64-bit native pointer with 32-bit fixed wire IDs.

No MinGW-w64 x86_64 compiler is installed in the validation container, so no Windows/Win64 Step 04A pass is claimed here. The `mingw64-tests` workflow below remains the authoritative Windows x64 readiness gate.

## Step 04A x64 readiness gate

The first x64 migration gate is intentionally focused and does not build the full legacy runtime. It checks that project/toolchain configuration can target Windows x86_64 while fixed wire/game widths remain explicit.

Canonical Windows commands:

```powershell
cmake --preset mingw64-tests
cmake --build --preset mingw64-tests
ctest --preset mingw64-tests --output-on-failure
```

Expected architecture-test line:

```text
Step 04A architecture guard passed: native pointer width=64, fixed wire IDs remain 32-bit.
```

`mingw64-tests` uses the MSYS2 MINGW64 toolchain root (`C:/msys64/mingw64`) unless `RTS_MINGW_ROOT` selects another matching x86_64 MinGW-w64 installation. A full x64 runtime configure is expected to fail deliberately at this stage; only the readiness/test graph is enabled until Step 04B+ migrate runtime dependencies.

The i686 compatibility gate remains mandatory in parallel:

```powershell
cmake --preset mingw32-tests
cmake --build --preset mingw32-tests --target z_determinismcheck
```

Do not treat an x64 readiness pass as permission to relax or delete the signed-off Win32 replay/network/Xfer fixtures.
