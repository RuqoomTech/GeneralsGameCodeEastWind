include("${RTS_SOURCE_DIR}/cmake/tests/PolicyTestHelpers.cmake")

rts_policy_require_contains(
    "CMakePresets.json"
    "mingw64-d3d12-shell"
    "the dedicated D3D12 shell preset must remain available")
rts_policy_require_contains(
    "cmake/config-build.cmake"
    "RTS_BUILD_D3D12_SHELL"
    "the D3D12 shell build option must remain explicit")
rts_policy_require_contains(
    "Evolution/CMakeLists.txt"
    "add_executable(generals_evolution"
    "the Evolution runtime shell must remain a standalone executable target")
rts_policy_require_contains(
    "CMakeLists.txt"
    "if(RTS_BUILD_D3D12_SHELL)"
    "the top-level build must expose the D3D12 shell as its own subsystem")
rts_policy_require_contains(
    "CMakeLists.txt"
    "if(NOT RTS_BUILD_TESTS_ONLY AND NOT RTS_BUILD_D3D12_SHELL)"
    "the D3D12 shell must stay outside the monolithic legacy runtime graph")
rts_policy_require_contains(
    "cmake/mingw.cmake"
    "NOT RTS_BUILD_D3D12_SHELL AND NOT TARGET d3dx8"
    "the D3D12 shell must not create the legacy D3DX8 compatibility target")
rts_policy_require_contains(
    "Evolution/CMakeLists.txt"
    "d3d12"
    "the runtime shell must link Direct3D 12")
rts_policy_require_contains(
    "Evolution/CMakeLists.txt"
    "dxgi"
    "the runtime shell must link DXGI")
rts_policy_require_contains(
    "Evolution/Source/D3D12Runtime.cpp"
    "#include <d3d12.h>"
    "the runtime shell must use Direct3D 12 directly")
rts_policy_require_contains(
    "Evolution/Source/D3D12Runtime.cpp"
    "CreateSwapChainForHwnd"
    "the runtime shell must own a real HWND swap chain")
rts_policy_require_contains(
    "Evolution/Source/D3D12Runtime.cpp"
    "ClearRenderTargetView"
    "the runtime shell must execute a real clear command")
rts_policy_require_contains(
    "Evolution/Source/D3D12Runtime.cpp"
    "DXGI_SWAP_EFFECT_FLIP_DISCARD"
    "the runtime shell must use a modern flip-model swap chain")
rts_policy_require_contains(
    "Evolution/Source/Main.cpp"
    "--frames"
    "the runtime shell must retain an automation-friendly frame-limited mode")

foreach(_legacy_header IN ITEMS "d3d8.h" "d3d9.h" "d3d11.h")
    rts_policy_require_absent(
        "Evolution/Source/D3D12Runtime.cpp"
        "${_legacy_header}"
        "the D3D12 shell must not include ${_legacy_header}")
endforeach()
foreach(_legacy_library IN ITEMS "d3d8" "d3d9" "d3d11" "dinput8" "dsound" "d3dx8")
    rts_policy_require_absent(
        "Evolution/CMakeLists.txt"
        "${_legacy_library}"
        "the standalone D3D12 shell must not link the legacy ${_legacy_library} library")
endforeach()

message(STATUS "D3D12 runtime shell policy passed: the x64 shell remains isolated from legacy renderer/input/audio dependencies")
