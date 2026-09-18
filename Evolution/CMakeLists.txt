add_executable(generals_evolution
    Source/D3D12Runtime.cpp
    Source/D3D12Runtime.h
    Source/Main.cpp
)

set_target_properties(generals_evolution PROPERTIES
    OUTPUT_NAME GeneralsEvolution
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

target_compile_definitions(generals_evolution PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(generals_evolution PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

if(MINGW)
    target_link_options(generals_evolution PRIVATE
        -static-libgcc
        -static-libstdc++
    )
endif()

target_link_libraries(generals_evolution PRIVATE
    d3d12
    dxgi
    dxguid
    user32
)

install(TARGETS generals_evolution
    RUNTIME DESTINATION .
)
