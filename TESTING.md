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

Step 02A integrates the W3X A0/A1/A2 tests and Step 01 determinism characterization into CTest through `Core/Tests/CMakeLists.txt`. The focused graph avoids configuring the full runtime/dependency tree.

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
- `determinism_step01` — lightweight CRC/RNG/float characterization on non-Windows hosts.

The W3X targets remain C++98-compatible and still use the consolidated `rts/w3x_document.h` / `w3x_document.cpp` seam. No runtime W3X importer is exercised.

## Step 01 determinism guard

The full compatibility branch remains Win32-only. It compiles the production RandomValue/Snapshot/Xfer/XferCRC/Damage implementation units directly rather than linking the monolithic Zero Hour GameEngine archive. Step 02A relocates only the CMake wiring into `Core/Tests`; it preserves the Zero Hour header precedence, `Utility/CppMacros.h` prelude, and MinGW IPO/LTO isolation used by the successful Step 01G run.

Expected Windows final line:

```text
Step 01 determinism guard passed: float helpers, CRC/RNG, Xfer/XferCRC, snapshot, ABI, and replay checkpoints.
```

Canonical Step 02A MinGW test workflow:

```powershell
cmake --preset mingw32-tests
cmake --build --preset mingw32-tests
ctest --preset mingw32-tests --output-on-failure
cmake --build --preset mingw32-tests --target z_determinismcheck
```

The historical Step 01 command remains valid through the `mingw-w64-i686-determinism` compatibility alias.

**Step 01G Windows sign-off:** passed on 2026-09-10 with MinGW-w64 i686 / GCC 16.2 + Ninja. **Step 02A Windows verification:** pending fresh user output; do not infer it from the Step 01G result.
