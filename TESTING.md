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
g++ -std=c++98 -Wall -Wextra -pedantic -I Core/Libraries/Include Core/Tests/W3XDocumentProbeTest.cpp -o w3x_document_probe_test
./w3x_document_probe_test
```

Expected result:

```text
W3X document-probe tests passed.
```

The test uses synthetic XML and verifies only document-envelope handling: BOM/declaration/comments, root-name extraction, namespace resolution, SAGE `AssetDeclaration` recognition, and predictable failure for malformed/unsupported envelope constructs. It does not parse mesh/material/animation content or load runtime assets.
