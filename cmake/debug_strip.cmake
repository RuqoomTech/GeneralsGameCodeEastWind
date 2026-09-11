# TheSuperHackers @build JohnsterID 05/01/2026 Add debug symbol stripping for MinGW Release builds
# Runtime debug-artifact handling for installable Windows binaries.
#
# MinGW Release builds keep GNU debug information in a sibling .debug file.
# MSVC continues to install its PDB when one exists. Keeping this policy in one
# helper avoids leaking MSVC-only TARGET_PDB_FILE generator expressions into
# GNU/MinGW generate steps.

set(DEBUG_STRIP_AVAILABLE FALSE)

# Find the required MinGW tools for symbol stripping.
if(MINGW)
    get_filename_component(COMPILER_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)

    find_program(MINGW_OBJCOPY
        NAMES ${CMAKE_SYSTEM_PROCESSOR}-w64-mingw32-objcopy objcopy
        HINTS ${COMPILER_DIR} ${RTS_MINGW_BIN_DIR}
        NO_DEFAULT_PATH
        DOC "MinGW objcopy tool for extracting debug symbols"
    )

    find_program(MINGW_STRIP
        NAMES ${CMAKE_SYSTEM_PROCESSOR}-w64-mingw32-strip strip
        HINTS ${COMPILER_DIR} ${RTS_MINGW_BIN_DIR}
        NO_DEFAULT_PATH
        DOC "MinGW strip tool for removing debug symbols"
    )

    if(MINGW_OBJCOPY AND MINGW_STRIP)
        message(STATUS "Debug symbol stripping enabled:")
        message(STATUS "  objcopy: ${MINGW_OBJCOPY}")
        message(STATUS "  strip:   ${MINGW_STRIP}")
        set(DEBUG_STRIP_AVAILABLE TRUE)
    else()
        message(WARNING "Debug symbol stripping not available - tools not found")
        if(NOT MINGW_OBJCOPY)
            message(WARNING "  objcopy not found")
        endif()
        if(NOT MINGW_STRIP)
            message(WARNING "  strip not found")
        endif()
    endif()
endif()

# Extract and link a separate GNU debug file for a MinGW Release target.
function(add_debug_strip_target target_name)
    if(NOT MINGW OR NOT DEBUG_STRIP_AVAILABLE)
        return()
    endif()
    if(NOT TARGET ${target_name})
        message(FATAL_ERROR "add_debug_strip_target: '${target_name}' is not a CMake target")
    endif()

    # Debug builds keep symbols embedded for development convenience.
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${MINGW_OBJCOPY}
                --only-keep-debug
                $<TARGET_FILE:${target_name}>
                $<TARGET_FILE:${target_name}>.debug
            COMMAND ${MINGW_STRIP}
                --strip-debug
                --strip-unneeded
                $<TARGET_FILE:${target_name}>
            COMMAND ${MINGW_OBJCOPY}
                --add-gnu-debuglink=$<TARGET_FILE:${target_name}>.debug
                $<TARGET_FILE:${target_name}>
            COMMENT "Stripping debug symbols from ${target_name} (Release)"
            VERBATIM
        )

        message(STATUS "Debug symbol stripping configured for target: ${target_name}")
    endif()
endfunction()

# Install one runtime target and the platform-appropriate debug artifact.
#
# Do not spell TARGET_PDB_FILE at call sites: CMake evaluates that generator
# expression during generation and rejects it for GNU/MinGW linkers even when
# the install rule is OPTIONAL.
function(rts_install_runtime_target target_name destination)
    if(NOT TARGET ${target_name})
        message(FATAL_ERROR "rts_install_runtime_target: '${target_name}' is not a CMake target")
    endif()

    install(TARGETS ${target_name} RUNTIME DESTINATION "${destination}")

    if(MSVC)
        install(
            FILES "$<TARGET_PDB_FILE:${target_name}>"
            DESTINATION "${destination}"
            OPTIONAL
        )
    elseif(MINGW AND DEBUG_STRIP_AVAILABLE AND CMAKE_BUILD_TYPE STREQUAL "Release")
        install(
            FILES "$<TARGET_FILE:${target_name}>.debug"
            DESTINATION "${destination}"
            OPTIONAL
        )
    endif()
endfunction()
