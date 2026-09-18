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
    "Core/Libraries/Source/WWVegas/WW3D2/Backend/D3D12Backend.cpp"
    "CreateGraphicsPipelineState"
    "the in-place backend must own a real Direct3D 12 graphics pipeline")
rts_policy_require_contains(
    "Core/Libraries/Source/WWVegas/WW3D2/Backend/D3D12Backend.cpp"
    "DrawIndexedInstanced"
    "the in-place backend must issue indexed Direct3D 12 geometry draws")
rts_policy_require_contains(
    "Core/Libraries/Source/WWVegas/WW3D2/Backend/D3D12Backend.cpp"
    "D3D12_HEAP_TYPE_DEFAULT"
    "persistent indexed geometry must live in Direct3D 12 default-heap resources")
rts_policy_require_contains(
    "Core/Libraries/Source/WWVegas/WW3D2/Backend/D3D12Backend.cpp"
    "CopyBufferRegion"
    "persistent geometry must use an explicit upload-to-default copy path")
rts_policy_require_contains(
    "Core/Libraries/Source/WWVegas/WW3D2/IRenderBackend.h"
    "Create_Static_Indexed_Color_Geometry"
    "persistent geometry lifetime must cross the existing renderer-neutral backend seam")
rts_policy_require_contains(
    "Core/Libraries/Source/WWVegas/WW3D2/IRenderBackend.h"
    "Release_Static_Geometry"
    "persistent geometry lifetime must have an explicit renderer-neutral release operation")
rts_policy_require_contains(
    "Core/Libraries/Source/WWVegas/WW3D2/IRenderBackend.h"
    "generation"
    "persistent geometry handles must reject stale slot reuse rather than aliasing a new resource")
rts_policy_require_contains(
    "Core/Tests/D3D12BackendSmokeTest.cpp"
    "Draw_Static_Indexed_Color_Geometry"
    "the Windows smoke test must reuse persistent geometry across frames")
rts_policy_require_contains(
    "Core/Tests/D3D12BackendSmokeTest.cpp"
    "Release_Static_Geometry"
    "the Windows smoke test must exercise persistent geometry destruction")
rts_policy_require_contains(
    "Core/Libraries/Source/WWVegas/WW3D2/Backend/D3D12Backend.cpp"
    "D3DCompileFromFile"
    "the D3D12 backend must compile canonical HLSL shader assets rather than embedding shader source in C++")
rts_policy_require_absent(
    "Core/Libraries/Source/WWVegas/WW3D2/Backend/D3D12Backend.cpp"
    "BasicPrimitiveShader"
    "the temporary embedded bootstrap shader must not return")
rts_policy_require_contains(
    "Core/Libraries/Source/WWVegas/WW3D2/Shaders/PrimitiveColor.hlsl"
    "PSInput VSMain"
    "the first canonical D3D12 shader asset must contain the indexed primitive vertex shader")
rts_policy_require_contains(
    "Core/Libraries/Source/WWVegas/WW3D2/Shaders/PrimitiveColor.hlsl"
    "float4 PSMain"
    "the first canonical D3D12 shader asset must contain the indexed primitive pixel shader")
rts_policy_require_contains(
    "Core/Libraries/Source/WWVegas/WW3D2/IRenderBackend.h"
    "Draw_Indexed_Triangles"
    "indexed geometry must enter D3D12 through the existing renderer-neutral backend seam")
rts_policy_require_contains(
    "Core/Tests/D3D12BackendSmokeTest.cpp"
    "Draw_Indexed_Triangles"
    "the Windows smoke test must submit real indexed geometry")
rts_policy_require_contains(
    "Core/Tests/CMakeLists.txt"
    "d3dcompiler"
    "the Windows backend smoke target must link the shader compiler used by the primitive pipeline")
rts_policy_require_contains(
    "Core/Libraries/Source/WWVegas/WW3D2/CMakeLists.txt"
    "d3dcompiler"
    "the production x64 WW3D backend must carry its shader compiler dependency")
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

rts_policy_require_contains(
    "Core/Tests/CMakeLists.txt"
    "Shaders/PrimitiveColor.hlsl"
    "the Windows backend smoke test must stage the canonical HLSL shader asset beside the executable")
rts_policy_require_contains(
    "GeneralsMD/Code/Main/CMakeLists.txt"
    "Shaders/PrimitiveColor.hlsl"
    "the x64 game executable must stage the same canonical HLSL shader asset used by the production backend")
rts_policy_require_contains(
    "Core/Tests/CMakeLists.txt"
    "LINK_DEPENDS"
    "shader edits must invalidate the Windows smoke executable so its staged HLSL stays current")
rts_policy_require_contains(
    "GeneralsMD/Code/Main/CMakeLists.txt"
    "LINK_DEPENDS"
    "shader edits must invalidate the x64 game executable so its staged HLSL stays current")
rts_policy_require_contains(
    "GeneralsMD/CMakeLists.txt"
    "PrimitiveColor.hlsl"
    "the installed x64 game must carry the canonical D3D12 shader asset")


rts_policy_require_contains(
    "Core/Libraries/Source/WWVegas/WW3D2/IRenderBackend.h"
    "Create_Static_RGBA8_Texture"
    "D3D12 texture lifetime must cross the existing renderer-neutral backend seam")
rts_policy_require_contains(
    "Core/Libraries/Source/WWVegas/WW3D2/IRenderBackend.h"
    "Create_Static_Indexed_Textured_Geometry"
    "textured indexed geometry must use a renderer-neutral WW3D vertex contract")
rts_policy_require_contains(
    "Core/Libraries/Source/WWVegas/WW3D2/Backend/D3D12Backend.cpp"
    "CopyTextureRegion"
    "RGBA8 texture creation must upload through an explicit D3D12 texture copy")
rts_policy_require_contains(
    "Core/Libraries/Source/WWVegas/WW3D2/Backend/D3D12Backend.cpp"
    "D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE"
    "sampled textures must live in a shader-visible SRV descriptor heap")
rts_policy_require_contains(
    "Core/Libraries/Source/WWVegas/WW3D2/Backend/D3D12Backend.cpp"
    "CreateShaderResourceView"
    "sampled textures must expose real D3D12 shader-resource views")
rts_policy_require_contains(
    "Core/Libraries/Source/WWVegas/WW3D2/Backend/D3D12Backend.cpp"
    "D3D12_STATIC_SAMPLER_DESC"
    "the first texture path must bind an explicit D3D12 sampler rather than inherit DX8 state")
rts_policy_require_contains(
    "Core/Libraries/Source/WWVegas/WW3D2/Backend/D3D12Backend.cpp"
    "SetGraphicsRootDescriptorTable"
    "textured draws must bind SRV descriptors through the D3D12 root signature")
rts_policy_require_contains(
    "Core/Libraries/Source/WWVegas/WW3D2/Shaders/PrimitiveColor.hlsl"
    "Texture2D PrimitiveTexture"
    "the canonical D3D12 shader asset must include the first sampled-texture path")
rts_policy_require_contains(
    "Core/Libraries/Source/WWVegas/WW3D2/Shaders/PrimitiveColor.hlsl"
    "float4 PSTextured"
    "the canonical D3D12 shader asset must shade textured indexed geometry")
rts_policy_require_contains(
    "Core/Tests/D3D12BackendSmokeTest.cpp"
    "Create_Static_RGBA8_Texture"
    "the Windows GPU smoke test must upload real RGBA8 texture data")
rts_policy_require_contains(
    "Core/Tests/D3D12BackendSmokeTest.cpp"
    "Draw_Static_Indexed_Textured_Geometry"
    "the Windows GPU smoke test must sample the uploaded texture on indexed geometry")
rts_policy_require_contains(
    "Core/Tests/D3D12BackendSmokeTest.cpp"
    "Release_Static_Texture"
    "the Windows GPU smoke test must cover explicit texture destruction")

foreach(_legacy_header IN ITEMS "d3d8.h" "d3d9.h" "d3d11.h")
    rts_policy_require_absent(
        "Core/Libraries/Source/WWVegas/WW3D2/Backend/D3D12Backend.cpp"
        "${_legacy_header}"
        "the D3D12 backend must not include ${_legacy_header}")
endforeach()

message(STATUS "D3D12 backend policy passed: the in-place renderer owns canonical HLSL, persistent geometry, and sampled RGBA8 SRV/static-sampler texture binding")
