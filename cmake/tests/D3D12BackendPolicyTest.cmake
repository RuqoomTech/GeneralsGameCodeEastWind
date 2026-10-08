include("${RTS_SOURCE_DIR}/cmake/tests/PolicyTestHelpers.cmake")

foreach(_filter_owner IN ITEMS
    Core/GameEngineDevice/Include/W3DDevice/GameClient/W3DShaderManager.h
    Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DShaderManager.cpp)
    foreach(_retired_filter IN ITEMS ScreenDefaultFilter ScreenBWFilterDOT3 m_dwBWPixelShader ScreenMotionBlurFilterList ScreenCrossFadeFilterList MaskTextureShader)
        rts_policy_require_absent("${_filter_owner}" "${_retired_filter}"
            "default rendering and monochrome must keep the retired native filter implementations removed")
    endforeach()
endforeach()
foreach(_game IN ITEMS Generals GeneralsMD)
    foreach(_retired_shader IN ITEMS monochrome.nvp invmonochrome.nvp motionblur.nvp MotionBlur.nvv)
        if(EXISTS "${RTS_SOURCE_DIR}/${_game}/Code/GameEngineDevice/Source/W3DDevice/GameClient/Shaders/${_retired_shader}")
            message(FATAL_ERROR "Policy regression: migrated monochrome shader ${_retired_shader} must not return")
        endif()
    endforeach()
endforeach()

rts_policy_require_absent(
    "Core/Libraries/Source/WWVegas/WW3D2/texproject.cpp"
    "DX8Wrapper"
    "projector target selection must remain on the renderer-neutral texture seam")
foreach(game IN ITEMS Generals GeneralsMD)
    foreach(retired_shadow_operation IN ITEMS "DX8Wrapper::Create_Render_Target" "Get_Surface_Level()")
        rts_policy_require_absent(
            "${game}/Code/GameEngineDevice/Source/W3DDevice/GameClient/Shadow/W3DProjectedShadow.cpp"
            "${retired_shadow_operation}"
            "projected-shadow texture allocation/copy must not restore legacy GPU surfaces")
    endforeach()
endforeach()

foreach(retired_file IN ITEMS
    "cmake/dx8.cmake"
    "Core/Libraries/Source/WWVegas/WW3D2/Backend/DX8Backend.cpp"
    "Core/Libraries/Source/WWVegas/WW3D2/Backend/DX8Backend.h")
    if(EXISTS "${RTS_SOURCE_DIR}/${retired_file}")
        message(FATAL_ERROR "Policy regression: retired renderer file ${retired_file} must not return")
    endif()
endforeach()

foreach(game IN ITEMS Generals GeneralsMD)
    rts_policy_require_absent(
        "${game}/Code/Libraries/Source/WWVegas/WW3D2/camera.cpp"
        "DX8Wrapper"
        "the migrated camera must apply viewport/transforms through the renderer-neutral seam")
    rts_policy_require_absent(
        "${game}/Code/Libraries/Source/WWVegas/WW3D2/camera.h"
        "Get_D3D_Projection_Matrix"
        "camera projection names must describe the depth range, not a retired API")
endforeach()
foreach(retired_debugger_file IN ITEMS dx8rendererdebugger.cpp dx8rendererdebugger.h)
    if(EXISTS "${RTS_SOURCE_DIR}/Core/Libraries/Source/WWVegas/WW3D2/${retired_debugger_file}")
        message(FATAL_ERROR "Policy regression: mesh debugging belongs to meshdebugger, not ${retired_debugger_file}")
    endif()
endforeach()

rts_policy_require_absent(
    "Core/Libraries/Source/WWVegas/WW3D2/shattersystem.cpp"
    "DX8Wrapper"
    "CPU shatter color interpolation must not depend on the retired renderer wrapper")

rts_policy_require_contains(
    "GeneralsMD/Code/GameEngineDevice/CMakeLists.txt"
    "core_browserhost"
    "the game-device target must own its direct BrowserHost include/link dependency")

rts_policy_require_absent(
    "Core/Libraries/Source/WWVegas/WW3D2/ww3d.cpp"
    "#if !defined(RTS_EVOLUTION_X64)"
    "WW3D must not retain an alternative DX8 device lifecycle")
rts_policy_require_absent(
    "Core/Libraries/Source/WWVegas/WW3D2/ww3d.cpp"
    "_Get_DX8_Back_Buffer"
    "screen/movie capture must use the renderer-neutral D3D12 readback seam")
rts_policy_require_absent(
    "Core/GameEngine/Source/Common/CommandLine.cpp"
    "DX8Wrapper_PreserveFPU"
    "the retired DX8 device FPU flag must not be reintroduced")
rts_policy_require_absent(
    "Core/GameEngine/Source/Common/System/Debug.cpp"
    "DX8Wrapper_IsWindowed"
    "crash reporting must retain confirmed presentation mode in its own owner")

file(GLOB_RECURSE renderer_build_files "${RTS_SOURCE_DIR}/Core/CMakeLists.txt"
    "${RTS_SOURCE_DIR}/Core/*/CMakeLists.txt" "${RTS_SOURCE_DIR}/Generals/CMakeLists.txt"
    "${RTS_SOURCE_DIR}/Generals/*/CMakeLists.txt" "${RTS_SOURCE_DIR}/GeneralsMD/CMakeLists.txt"
    "${RTS_SOURCE_DIR}/GeneralsMD/*/CMakeLists.txt")
foreach(renderer_build_file IN LISTS renderer_build_files)
    file(READ "${renderer_build_file}" renderer_build_source)
    if(renderer_build_source MATCHES "(^|[ \t\r\n])(d3d8|d3dx8|d3d8lib)([ \t\r\n)]|$)")
        message(FATAL_ERROR "Policy regression: DX8 build dependency in ${renderer_build_file}")
    endif()
endforeach()

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
    "Core/Libraries/Source/WWVegas/WW3D2/CMakeLists.txt"
    "Shaders/PrimitiveColor.hlsl"
    "the x64 game executable must stage the same canonical HLSL shader asset used by the production backend")
rts_policy_require_contains(
    "Core/Tests/CMakeLists.txt"
    "LINK_DEPENDS"
    "shader edits must invalidate the Windows smoke executable so its staged HLSL stays current")
rts_policy_require_contains(
    "Core/Libraries/Source/WWVegas/WW3D2/CMakeLists.txt"
    "LINK_DEPENDS"
    "shader edits must invalidate the x64 game executable so its staged HLSL stays current")
rts_policy_require_contains(
    "GeneralsMD/Code/Main/CMakeLists.txt"
    "rts_stage_ww3d_shaders(z_generals)"
    "the game must call the shared WW3D shader staging owner")
rts_policy_require_contains(
    "GeneralsMD/CMakeLists.txt"
    "PrimitiveColor.hlsl"
    "the installed x64 game must carry the canonical D3D12 shader asset")


rts_policy_require_contains(
    "CMakePresets.json"
    "mingw64-game"
    "Step 05H must expose a normal x64 z_generals migration preset instead of remaining tests-only")
rts_policy_require_absent(
    "cmake/mingw.cmake"
    "The x64 full-game runtime remains gated"
    "the Step 05H normal game build must no longer be blocked before compilation")
rts_policy_require_contains(
    "cmake/config-build.cmake"
    "RTS_EVOLUTION_X64=1"
    "real WW3D callers need an explicit source-level guard for the x64 Evolution migration path")
rts_policy_require_contains(
    "GeneralsMD/Code/Libraries/Source/WWVegas/WW3D2/render2d.cpp"
    "Draw_2D_Indexed_Triangles"
    "the first real W3DDisplay/Render2D screen-space primitive caller must cross IRenderBackend")
rts_policy_require_contains(
    "GeneralsMD/Code/Libraries/Source/WWVegas/WW3D2/render2d.cpp"
    "#if !defined(RTS_EVOLUTION_X64)"
    "the x64 Render2D slice must not compile its migrated untextured path through DX8 buffer/wrapper headers")
rts_policy_require_contains(
    "Core/Libraries/Source/WWVegas/WW3D2/IRenderBackend.h"
    "RenderBackend2DBlendMode"
    "screen-space blend responsibility must be explicit and renderer-neutral rather than a DX8 state facade")
rts_policy_require_contains(
    "Core/Libraries/Source/WWVegas/WW3D2/IRenderBackend.h"
    "operator == (const RenderBackendColorVertex &other) const"
    "renderer-neutral color vertices must satisfy the legacy VectorClass value contract used by the real Render2D caller")
rts_policy_require_contains(
    "Core/Libraries/Source/WWVegas/WW3D2/IRenderBackend.h"
    "operator != (const RenderBackendTexturedVertex &other) const"
    "renderer-neutral textured vertices must remain compatible with legacy value containers as textured callers migrate")
rts_policy_require_contains(
    "Core/Tests/D3D12BackendSmokeTest.cpp"
    "Draw_2D_Indexed_Triangles"
    "the Windows production-backend smoke must compile and execute the first real 2D PSO path")

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
    "Release_Texture"
    "the Windows GPU smoke test must cover explicit texture destruction")

foreach(_legacy_header IN ITEMS "d3d8.h" "d3d9.h" "d3d11.h")
    rts_policy_require_absent(
        "Core/Libraries/Source/WWVegas/WW3D2/Backend/D3D12Backend.cpp"
        "${_legacy_header}"
        "the D3D12 backend must not include ${_legacy_header}")
endforeach()

# Step 05H1K: active Evolution WW3D sources must not require the retired
# D3DX8 utility library. Pure math moves to WWMath; CPU image conversion and
# mip generation reuse BitmapHandler. Archival DX8-only source may retain D3DX.
foreach(_d3dx_free_source IN ITEMS
    "GeneralsMD/Code/Libraries/Source/WWVegas/WW3D2/assetmgr.cpp"
    "Core/Libraries/Source/WWVegas/WW3D2/texture.cpp"
    "Core/Libraries/Source/WWVegas/WW3D2/textureloader.cpp"
    "Core/Libraries/Source/WWVegas/WW3D2/surfaceclass.cpp"
    "Core/Libraries/Source/WWVegas/WW3D2/pointgr.cpp"
    "Core/Libraries/Source/WWVegas/WW3D2/sortingrenderer.cpp"
    "Core/Libraries/Source/WWVegas/WW3D2/missingtexture.cpp")
    rts_policy_require_absent(
        "${_d3dx_free_source}"
        "d3dx8"
        "active Evolution WW3D source ${_d3dx_free_source} must not include D3DX8")
    rts_policy_require_absent(
        "${_d3dx_free_source}"
        "D3DX"
        "active Evolution WW3D source ${_d3dx_free_source} must not call D3DX8")
endforeach()
rts_policy_require_absent(
    "Core/Libraries/Source/WWVegas/WW3D2/surfaceclass.cpp"
    "DX8Wrapper::"
    "CPU SurfaceClass must not allocate or copy through the retired renderer")
rts_policy_require_contains(
    "Core/Libraries/Source/WWVegas/WW3D2/surfaceclass.cpp"
    "BitmapHandlerClass::Copy_Pixel"
    "SurfaceClass conversion must use the shared CPU bitmap owner")
rts_policy_require_contains(
    "Core/Libraries/Source/WWVegas/WW3D2/missingtexture.cpp"
    "BitmapHandlerClass::Create_Mipmap_B8G8R8A8"
    "Evolution missing-texture mip generation must use the existing CPU bitmap helper")

rts_policy_require_contains(
    "Core/Libraries/Source/WWVegas/WW3D2/pointgr.cpp"
    "rot_mat.Rotate_Vector(GroundMultiplierX)"
    "point-group orientation must use the always-available WWMath rotation API")
rts_policy_require_contains(
    "Core/Libraries/Source/WWVegas/WW3D2/pointgr.cpp"
    "rot_mat.Rotate_Vector(GroundMultiplierY)"
    "point-group orientation must use the always-available WWMath rotation API")
rts_policy_require_absent(
    "Core/Libraries/Source/WWVegas/WW3D2/pointgr.cpp"
    "rot_mat * GroundMultiplier"
    "point-group orientation must not depend on the ALLOW_TEMPORARIES Matrix3D operator")

# Pure game/client math and files with unused D3DX includes must also stay
# independent of the retired utility library so the normal x64 graph does not
# stop before reaching genuine renderer migration work.
foreach(_d3dx_free_game_source IN ITEMS
    "Core/GameEngine/Include/Common/BezierSegment.h"
    "Core/GameEngine/Source/Common/Bezier/BezierSegment.cpp"
    "Core/GameEngine/Source/Common/Bezier/BezFwdIterator.cpp"
    "Core/GameEngineDevice/Source/W3DDevice/GameClient/BaseHeightMap.cpp"
    "Core/GameEngineDevice/Source/W3DDevice/GameClient/CameraShakeSystem.cpp"
    "Core/GameEngineDevice/Source/W3DDevice/GameClient/FlatHeightMap.cpp"
    "Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DView.cpp"
    "Core/GameEngineDevice/Source/W3DDevice/GameClient/HeightMap.cpp"
    "GeneralsMD/Code/GameEngineDevice/Source/W3DDevice/GameClient/Shadow/W3DProjectedShadow.cpp"
    "GeneralsMD/Code/GameEngineDevice/Source/W3DDevice/GameClient/Shadow/W3DVolumetricShadow.cpp"
    "GeneralsMD/Code/GameEngineDevice/Source/W3DDevice/GameClient/Shadow/W3DShadow.cpp"
    "GeneralsMD/Code/GameEngineDevice/Source/W3DDevice/GameClient/W3DWebBrowser.cpp"
    "Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DShaderManager.cpp"
    "Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DTreeBuffer.cpp")
    rts_policy_require_absent(
        "${_d3dx_free_game_source}"
        "d3dx8"
        "active x64 source ${_d3dx_free_game_source} must not include D3DX8")
    rts_policy_require_absent(
        "${_d3dx_free_game_source}"
        "D3DX"
        "active x64 source ${_d3dx_free_game_source} must not call D3DX8")
endforeach()
foreach(_cpu_mip_source IN ITEMS
    "Core/GameEngineDevice/Source/W3DDevice/GameClient/TerrainTex.cpp"
    "Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DTreeBuffer.cpp")
    rts_policy_require_absent(
        "${_cpu_mip_source}"
        "d3dx8tex.h"
        "procedural terrain/tree mip generation must not depend on the D3DX8 texture utility header")
    rts_policy_require_absent(
        "${_cpu_mip_source}"
        "D3DXFilterTexture"
        "procedural terrain/tree mip generation must use the shared CPU bitmap path instead of D3DXFilterTexture")
endforeach()

foreach(_cpu_tree_shroud_source IN ITEMS
    "Core/GameEngineDevice/Source/W3DDevice/GameClient/BaseHeightMap.cpp"
    "Core/GameEngineDevice/Include/W3DDevice/GameClient/W3DTreeBuffer.h"
    "Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DTreeBuffer.cpp"
    "Generals/Code/GameEngineDevice/Include/W3DDevice/GameClient/W3DShroud.h"
    "Generals/Code/GameEngineDevice/Source/W3DDevice/GameClient/W3DShroud.cpp"
    "GeneralsMD/Code/GameEngineDevice/Include/W3DDevice/GameClient/W3DShroud.h"
    "GeneralsMD/Code/GameEngineDevice/Source/W3DDevice/GameClient/W3DShroud.cpp")
    rts_policy_require_absent(
        "${_cpu_tree_shroud_source}" "DX8" "migrated tree/shroud owners must not restore native DX8 resources or draws")
    rts_policy_require_absent(
        "${_cpu_tree_shroud_source}" "dx8" "migrated tree/shroud owners must not restore the native DX8 headers")
    rts_policy_require_absent(
        "${_cpu_tree_shroud_source}" "IDirect3D" "migrated tree/shroud owners must keep geometry and pixels renderer neutral")
endforeach()
rts_policy_require_contains(
    "Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DTreeBuffer.cpp"
    "Draw_Indexed_Material_Triangles" "real tree batches must submit through the active render backend")

foreach(_terrain_geometry_owner IN ITEMS
    "Core/GameEngineDevice/Include/W3DDevice/GameClient/W3DTerrainBackground.h"
    "Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DTerrainBackground.cpp")
    rts_policy_require_absent("${_terrain_geometry_owner}" "DX8"
        "migrated terrain tessellation must not restore native buffers or draws")
    rts_policy_require_absent("${_terrain_geometry_owner}" "dx8"
        "migrated terrain tessellation must keep native buffer headers retired")
    rts_policy_require_absent("${_terrain_geometry_owner}" "IDirect3D"
        "terrain CPU geometry must stay renderer neutral")
endforeach()
rts_policy_require_contains(
    "Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DTerrainBackground.cpp"
    "Draw_Static_Indexed_Terrain_Geometry" "the actual flat terrain tile must reuse backend geometry")
rts_policy_require_absent(
    "Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DShaderManager.cpp"
    "FlatTerrainShader" "flat terrain shader implementations have moved to the canonical HLSL")
rts_policy_require_contains(
    "Core/Libraries/Source/WWVegas/WW3D2/Shaders/PrimitiveColor.hlsl"
    "PSTerrainTexture" "flat terrain must retain its single-pass tile/shroud/diffuse/cloud/noise shader")

rts_policy_require_contains(
    "Core/Libraries/Source/WWVegas/WW3D2/bitmaphandler.cpp"
    "void BitmapHandlerClass::Create_Mipmap("
    "procedural texture mip generation must remain owned by the renderer-neutral CPU bitmap helper")
rts_policy_require_contains(
    "Core/Libraries/Source/WWVegas/WW3D2/texture.cpp"
    "BitmapHandlerClass::Create_Mipmap("
    "TextureClass procedural mip chains must reuse the shared CPU bitmap helper instead of D3DX8")

rts_policy_require_contains(
    "Core/GameEngine/Include/Common/BezierSegment.h"
    "static const Matrix4x4 s_bezBasisMatrix"
    "Bezier basis math must use renderer-neutral WWMath")

# WWMath is renderer-neutral. Legacy D3D8 matrix conversion remains only with
# the archival DX8 backend until those callers cross the renderer seam.
foreach(_wwmath_matrix IN ITEMS
    "Core/Libraries/Source/WWVegas/WWMath/matrix3d.cpp"
    "Core/Libraries/Source/WWVegas/WWMath/matrix3d.h"
    "Core/Libraries/Source/WWVegas/WWMath/matrix4.cpp"
    "Core/Libraries/Source/WWVegas/WWMath/matrix4.h")
    rts_policy_require_absent(
        "${_wwmath_matrix}"
        "d3dx8math.h"
        "renderer-neutral WWMath must not depend on the retired D3DX8 math header")
    rts_policy_require_absent(
        "${_wwmath_matrix}"
        "To_D3DMATRIX"
        "legacy D3D matrix conversion must not live in renderer-neutral WWMath")
    rts_policy_require_absent(
        "${_wwmath_matrix}"
        "To_D3DXMATRIX"
        "dead D3DX8 matrix conversion must not return to renderer-neutral WWMath")
endforeach()
rts_policy_require_contains(
    "Core/Libraries/Source/WWVegas/WW3D2/dx8wrapper.h"
    "WWINLINE D3DMATRIX To_D3DMATRIX(const Matrix3D &m)"
    "remaining D3D8 Matrix3D conversion must stay with the archival DX8 backend")
rts_policy_require_contains(
    "Core/Libraries/Source/WWVegas/WW3D2/dx8wrapper.h"
    "WWINLINE void To_Matrix4x4(Matrix4x4 &m, const D3DMATRIX &dxm)"
    "remaining D3D8 Matrix4x4 conversion must stay with the archival DX8 backend")
rts_policy_require_absent(
    "Core/Libraries/Source/WWVegas/WW3D2/dx8wrapper.h"
    "To_D3DXMATRIX"
    "unused D3DX8 matrix conversion should remain deleted rather than perpetuating D3DX8")

# Step 05H1S: the active Evolution water source must not depend on D3DX8 math.
# The archival inline ps.1.1 assembler remains isolated to the non-Evolution
# reference branch until the real water caller is translated through D3D12.
rts_policy_require_absent(
    "Core/GameEngineDevice/Source/W3DDevice/GameClient/Water/W3DWater.cpp"
    "d3dx8math.h"
    "Evolution water math must not depend on the retired D3DX8 math header")
rts_policy_require_absent(
    "Core/GameEngineDevice/Source/W3DDevice/GameClient/Water/W3DWater.cpp"
    "D3DXMATRIX"
    "Evolution water matrix math must use D3DMATRIX plus WWMath")
rts_policy_require_absent(
    "Core/GameEngineDevice/Source/W3DDevice/GameClient/Water/W3DWater.cpp"
    "D3DXVECTOR4"
    "Evolution water shader constants must use renderer-neutral vectors")
rts_policy_require_absent(
    "Core/GameEngineDevice/Source/W3DDevice/GameClient/Water/W3DWater.cpp"
    "D3DXMatrix"
    "Evolution water matrix operations must use existing WWMath conversions")
rts_policy_require_contains(
    "Core/GameEngineDevice/Source/W3DDevice/GameClient/Water/W3DWater.cpp"
    "Build_Water_Noise_Texture_Transform"
    "water texture-coordinate generation must preserve the legacy inverse-view transform without D3DX8")
rts_policy_require_contains(
    "Core/GameEngineDevice/Source/W3DDevice/GameClient/Water/W3DWater.cpp"
    "Build_Water_World_View_Projection_Constants"
    "water vertex-shader constants must preserve the legacy transposed WVP calculation through WWMath")
rts_policy_require_contains(
    "Core/GameEngineDevice/Source/W3DDevice/GameClient/Water/W3DWater.cpp"
    "#if !defined(RTS_EVOLUTION_X64)"
    "legacy D3DX8 shader assembly must stay outside the Evolution x64 path")

message(STATUS "D3D12 backend policy passed: the in-place renderer owns canonical HLSL, persistent geometry, sampled textures, the Step05H real Render2D path, and active Evolution shader/tree/water math no longer depends on D3DX8")

foreach(_bib_owner IN ITEMS
    Generals/Code/GameEngineDevice/Include/W3DDevice/GameClient/W3DBibBuffer.h
    Generals/Code/GameEngineDevice/Source/W3DDevice/GameClient/W3DBibBuffer.cpp
    GeneralsMD/Code/GameEngineDevice/Include/W3DDevice/GameClient/W3DBibBuffer.h
    GeneralsMD/Code/GameEngineDevice/Source/W3DDevice/GameClient/W3DBibBuffer.cpp)
    rts_policy_require_absent("${_bib_owner}" "DX8" "migrated bibs must use persistent backend geometry")
    rts_policy_require_absent("${_bib_owner}" "dx8" "migrated bibs must keep native buffer headers retired")
endforeach()

foreach(_road_owner IN ITEMS
    Generals/Code/GameEngineDevice/Include/W3DDevice/GameClient/W3DRoadBuffer.h
    Generals/Code/GameEngineDevice/Source/W3DDevice/GameClient/W3DRoadBuffer.cpp
    GeneralsMD/Code/GameEngineDevice/Include/W3DDevice/GameClient/W3DRoadBuffer.h
    GeneralsMD/Code/GameEngineDevice/Source/W3DDevice/GameClient/W3DRoadBuffer.cpp)
    rts_policy_require_absent("${_road_owner}" "DX8" "migrated roads must use retained backend geometry")
    rts_policy_require_absent("${_road_owner}" "dx8" "migrated roads must keep native buffer headers retired")
endforeach()
rts_policy_require_absent(
    "Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DShaderManager.cpp"
    "RoadShader" "all road and extra-blend callers must use the canonical HLSL")
rts_policy_require_contains(
    "Core/GameEngineDevice/Source/W3DDevice/GameClient/HeightMap.cpp"
    "Draw_Indexed_Terrain_Triangles" "the real third-texture terrain blend must draw through the backend")

foreach(_migrated_owner IN ITEMS
    Core/GameEngineDevice/Include/W3DDevice/GameClient/HeightMap.h
    Core/GameEngineDevice/Source/W3DDevice/GameClient/HeightMap.cpp
    Core/GameEngineDevice/Include/W3DDevice/GameClient/BaseHeightMap.h
    Core/GameEngineDevice/Include/W3DDevice/GameClient/W3DWaterTracks.h
    Core/GameEngineDevice/Source/W3DDevice/GameClient/Water/W3DWaterTracks.cpp
    Core/GameEngineDevice/Source/W3DDevice/GameClient/FlatHeightMap.cpp
    Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DScorch.cpp
    Core/GameEngineDevice/Include/W3DDevice/GameClient/W3DScorch.h
    Generals/Code/GameEngineDevice/Include/W3DDevice/GameClient/W3DBridgeBuffer.h
    Generals/Code/GameEngineDevice/Source/W3DDevice/GameClient/W3DBridgeBuffer.cpp
    GeneralsMD/Code/GameEngineDevice/Include/W3DDevice/GameClient/W3DBridgeBuffer.h
    GeneralsMD/Code/GameEngineDevice/Source/W3DDevice/GameClient/W3DBridgeBuffer.cpp)
    rts_policy_require_absent("${_migrated_owner}" "DX8" "migrated terrain callers must keep native rendering retired")
    rts_policy_require_absent("${_migrated_owner}" "dx8" "migrated terrain callers must keep native buffer headers retired")
endforeach()

rts_policy_require_absent(
    "Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DShaderManager.cpp"
    "TerrainShader" "all foreground and flat terrain shaders must use canonical HLSL")
rts_policy_require_absent(
    "Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DShaderManager.cpp"
    "CloudTextureShader" "cloud projection must use the backend terrain constants")
rts_policy_require_absent(
    "Core/Libraries/Source/WWVegas/WW3D2/dx8wrapper.cpp"
    "D3DXGetErrorString" "shared result diagnostics must remain in existing WWDebug")
foreach(_diagnostic_owner IN ITEMS
    Core/Libraries/Source/WWVegas/WW3D2/dx8wrapper.h
    Core/Libraries/Source/WWVegas/WW3D2/dx8wrapper.cpp)
    rts_policy_require_absent("${_diagnostic_owner}" "Get_DX8_Render_State_Value_Name"
        "snapshot diagnostics must not restore retired renderer label dependencies")
    rts_policy_require_absent("${_diagnostic_owner}" "Get_DX8_Texture_Stage_State_Value_Name"
        "snapshot diagnostics must retain values through the existing logger")
endforeach()
