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
## W3X asset-format recognition pre-step

The W3X-A0 characterization test is intentionally standalone until the command-line test harness is modernized in Step 02.

From the repository root with GCC available:

```sh
g++ -std=c++98 -Wall -Wextra -pedantic -I Core/Libraries/Include Core/Tests/W3XAssetFormatTest.cpp -o w3x_asset_format_test
./w3x_asset_format_test
```

Expected result:

```text
W3X asset-format recognition tests passed.
```

This test verifies only W3D/W3X recognition and conservative XML sniffing. It does not exercise runtime asset loading or XML parsing.

## W3X document-envelope probe pre-step

W3X-A1 remains standalone until the command-line test harness is modernized in Step 02.

From the repository root with GCC available:

```sh
g++ -std=c++98 -Wall -Wextra -pedantic -I Core/Libraries/Include \
  Core/Tests/W3XDocumentProbeTest.cpp Core/Libraries/Source/rts/w3x_document.cpp \
  -o w3x_document_probe_test
./w3x_document_probe_test
```

Expected result:

```text
W3X document-probe tests passed.
```

The test uses synthetic XML and verifies only document-envelope handling: BOM/declaration/comments, root-name extraction, namespace resolution, SAGE `AssetDeclaration` recognition, and predictable failure for malformed/unsupported envelope constructs. It does not parse mesh/material/animation content or load runtime assets.

## W3X top-level child discovery pre-step

W3X-A2 uses the same consolidated W3X document implementation as A1.

From the repository root with GCC available:

```sh
g++ -std=c++98 -Wall -Wextra -pedantic -I Core/Libraries/Include \
  Core/Tests/W3XChildDiscoveryTest.cpp Core/Libraries/Source/rts/w3x_document.cpp \
  -o w3x_child_discovery_test
./w3x_child_discovery_test
```

Expected result:

```text
W3X top-level child-discovery tests passed.
```

A1 and A2 deliberately share one implementation source. Adding another standalone W3X scanner/header for subsequent XML features is not the intended architecture; extend or replace the consolidated document component during Step 05.

## Step 01 determinism guard

The lightweight gate runs production CRC/RNG code plus the compiler-sensitive float-helper characterization. Modern GCC no longer needs a strict-aliasing suppression.

```sh
g++ -std=c++20 -O2 -Wall -Wextra -Werror \
  -Wno-unknown-pragmas -Wno-unused-parameter -pedantic \
  -DRTS_STANDALONE_DETERMINISM_TEST \
  -IDependencies/Utility \
  -ICore/Libraries/Include \
  -ICore/Libraries/Source/WWVegas \
  -ICore/GameEngine/Include \
  Core/Tests/DeterminismPrimitivesTest.cpp \
  Core/GameEngine/Source/Common/RandomValue.cpp \
  -o DeterminismPrimitivesTest
./DeterminismPrimitivesTest
```

The full compatibility gate is Windows-only because it links the real Zero Hour GameEngine/Xfer path. Configure with `RTS_BUILD_ZEROHOUR_EXTRAS=ON`, build `z_determinismtest`, then run the produced executable. It covers Xfer, XferCRC, snapshot ordering, Win32 packet/replay ABI assumptions, and the replay command-record checkpoint in addition to the lightweight tests.

Expected final line:

```text
Step 01 determinism guard passed: float helpers, CRC/RNG, Xfer/XferCRC, snapshot, ABI, and replay checkpoints.
```

See `Modernization/STEP_01_DETERMINISM_GUARD.md` for exact MinGW-w64 i686 and MSVC Win32 commands.
