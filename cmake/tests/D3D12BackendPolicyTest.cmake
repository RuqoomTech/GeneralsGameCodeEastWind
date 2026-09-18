include("${RTS_SOURCE_DIR}/cmake/tests/PolicyTestHelpers.cmake")

if(EXISTS "${RTS_SOURCE_DIR}/Evolution")
    message(FATAL_ERROR "Policy regression: the standalone Evolution application tree must not return")
endif()

rts_policy_require_absent(
    "cmake/config-build.cmake"
    "RTS_BUILD_D3D12_SHELL"
    "the temporary standalone D3D12 shell option must stay removed")
rts_policy_require_absent(
    "CMakePresets.json"
    "mingw64-d3d12-shell"
    "the temporary standalone D3D12 shell preset must stay removed")
rts_policy_require_contains(
    "Core/Libraries/Source/WWVegas/WW3D2/CMakeLists.txt"
    "Backend/D3D12Backend.cpp"
    "the x64 renderer must live behind the existing WW3D backend seam")
rts_policy_require_contains(
    "Core/Libraries/Source/WWVegas/WW3D2/CMakeLists.txt"
    "if((WIN32 OR \"\${CMAKE_SYSTEM}\" MATCHES \"Windows\") AND CMAKE_SIZEOF_VOID_P EQUAL 8)"
    "backend selection must remain Windows-x64 explicit")
rts_policy_require_contains(
    "Core/Libraries/Source/WWVegas/WW3D2/Backend/D3D12Backend.cpp"
    "CreateSwapChainForHwnd"
    "the in-place backend must own a real HWND swap chain")
rts_policy_require_contains(
    "Core/Libraries/Source/WWVegas/WW3D2/Backend/D3D12Backend.cpp"
    "ClearRenderTargetView"
    "the in-place backend must issue real D3D12 color clears")
rts_policy_require_contains(
    "Core/Libraries/Source/WWVegas/WW3D2/Backend/D3D12Backend.cpp"
    "CreateDepthStencilView"
    "the in-place backend must own depth/stencil state rather than delegating to DX8")
rts_policy_require_contains(
    "Core/Tests/D3D12BackendSmokeTest.cpp"
    "End_Scene(false)"
    "the Windows smoke test must cover WW3D deferred-present semantics")
rts_policy_require_contains(
    "GeneralsMD/Code/Main/CMakeLists.txt"
    "target_link_libraries(z_generals PRIVATE d3d12 dxgi)"
    "the x64 game executable must select Direct3D 12 and DXGI")
rts_policy_require_contains(
    "Core/Tests/CMakeLists.txt"
    "d3d12_backend_smoke_test"
    "the production backend must retain a Windows smoke build")
rts_policy_require_contains(
    "Core/Tests/CMakeLists.txt"
    "Core/Libraries/Include"
    "the Windows smoke target must inherit the public RTS include root used by WW3D headers")
rts_policy_require_contains(
    "Core/Tests/CMakeLists.txt"
    "target_include_directories(d3d12_backend_smoke_test SYSTEM PRIVATE"
    "legacy WWVegas headers must not turn their pre-existing warnings into smoke-target errors")
rts_policy_require_contains(
    "Core/Libraries/Source/WWVegas/WW3D2/Backend/D3D12Backend.cpp"
    "#include \"Utility/CppMacros.h\""
    "the production backend must establish the WWVegas compatibility macros before legacy headers")
rts_policy_require_contains(
    "Core/Tests/D3D12BackendSmokeTest.cpp"
    "#include \"Utility/CppMacros.h\""
    "the smoke translation unit must establish the WWVegas compatibility macros before legacy headers")

foreach(_legacy_header IN ITEMS "d3d8.h" "d3d9.h" "d3d11.h")
    rts_policy_require_absent(
        "Core/Libraries/Source/WWVegas/WW3D2/Backend/D3D12Backend.cpp"
        "${_legacy_header}"
        "the D3D12 backend must not include ${_legacy_header}")
endforeach()

message(STATUS "D3D12 backend policy passed: the renderer is integrated in-place and the standalone shell remains removed")
