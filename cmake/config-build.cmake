# Do we want to build extra SDK stuff or just the game binary?
option(RTS_BUILD_CORE_TOOLS "Build core tools" ON)
option(RTS_BUILD_CORE_EXTRAS "Build core extra tools/tests" OFF)
option(RTS_BUILD_ZEROHOUR "Build Zero Hour code." ON)
option(RTS_BUILD_GENERALS "Build Generals code." ON)
option(RTS_BUILD_OPTION_PROFILE "Build code with the \"Profile\" configuration." OFF)
option(RTS_BUILD_OPTION_PROFILE_TRACY "Build code with Tracy profiling enabled." OFF)
option(RTS_BUILD_OPTION_PERF_TELEMETRY "Build renderer performance telemetry instrumentation." OFF)
option(RTS_BUILD_OPTION_DEBUG "Build code with the \"Debug\" configuration." OFF)
option(RTS_BUILD_OPTION_ASAN "Build code with Address Sanitizer." OFF)
option(RTS_BUILD_OPTION_VC6_FULL_DEBUG "Build VC6 with full debug info." OFF)
option(RTS_BUILD_TESTS "Build modernization characterization/regression tests." OFF)
option(RTS_BUILD_TESTS_ONLY "Configure only the lightweight modernization test graph." OFF)
option(RTS_BUILD_X64_READINESS "Configure the staged 64-bit migration/readiness graph." OFF)
option(RTS_BUILD_X64_HEADLESS_CORE "Build the Step 04D deterministic/headless x64 core lane." OFF)

if(RTS_BUILD_TESTS_ONLY)
    set(RTS_BUILD_TESTS ON CACHE BOOL "Build modernization characterization/regression tests." FORCE)
    set(RTS_BUILD_CORE_TOOLS OFF CACHE BOOL "Build core tools" FORCE)
    set(RTS_BUILD_ZEROHOUR OFF CACHE BOOL "Build Zero Hour code." FORCE)
    set(RTS_BUILD_GENERALS OFF CACHE BOOL "Build Generals code." FORCE)
endif()

if(NOT RTS_BUILD_ZEROHOUR AND NOT RTS_BUILD_GENERALS AND NOT RTS_BUILD_TESTS_ONLY)
    set(RTS_BUILD_ZEROHOUR TRUE)
    message("You must select one project to build, building Zero Hour by default.")
endif()

add_feature_info(CoreTools RTS_BUILD_CORE_TOOLS "Build Core Mod Tools")
add_feature_info(CoreExtras RTS_BUILD_CORE_EXTRAS "Build Core Extra Tools/Tests")
add_feature_info(ZeroHourStuff RTS_BUILD_ZEROHOUR "Build Zero Hour code")
add_feature_info(GeneralsStuff RTS_BUILD_GENERALS "Build Generals code")
add_feature_info(ProfileBuild RTS_BUILD_OPTION_PROFILE "Building as a \"Profile\" build")
add_feature_info(PerformanceTelemetry RTS_BUILD_OPTION_PERF_TELEMETRY "Building renderer performance telemetry instrumentation")
add_feature_info(DebugBuild RTS_BUILD_OPTION_DEBUG "Building as a \"Debug\" build")
add_feature_info(AddressSanitizer RTS_BUILD_OPTION_ASAN "Building with address sanitizer")
add_feature_info(Vc6FullDebug RTS_BUILD_OPTION_VC6_FULL_DEBUG "Building VC6 with full debug info")
add_feature_info(ModernizationTests RTS_BUILD_TESTS "Build modernization characterization/regression tests")
add_feature_info(TestsOnly RTS_BUILD_TESTS_ONLY "Configure only the lightweight modernization test graph")
add_feature_info(X64Readiness RTS_BUILD_X64_READINESS "Configure the staged 64-bit migration/readiness graph")
add_feature_info(X64HeadlessCore RTS_BUILD_X64_HEADLESS_CORE "Build the Step 04D deterministic/headless x64 core lane")
add_feature_info(FFmpegSupport RTS_BUILD_OPTION_FFMPEG "Building with FFmpeg support")

set(RTS_BUILD_OUTPUT_SUFFIX "" CACHE STRING "Suffix appended to output names of installable targets")

if(RTS_BUILD_ZEROHOUR)
    option(RTS_BUILD_ZEROHOUR_TOOLS "Build tools for Zero Hour" ON)
    option(RTS_BUILD_ZEROHOUR_EXTRAS "Build extra tools/tests for Zero Hour" OFF)
    option(RTS_BUILD_ZEROHOUR_DOCS "Build documentation for Zero Hour" OFF)

    add_feature_info(ZeroHourTools RTS_BUILD_ZEROHOUR_TOOLS "Build Zero Hour Mod Tools")
    add_feature_info(ZeroHourExtras RTS_BUILD_ZEROHOUR_EXTRAS "Build Zero Hour Extra Tools/Tests")
    add_feature_info(ZeroHourDocs RTS_BUILD_ZEROHOUR_DOCS "Build Zero Hour Documentation")

    # The historical extras switch includes characterization tests. Keep that
    # behavior while centralizing the tests under Core/Tests.
    if(RTS_BUILD_ZEROHOUR_EXTRAS)
        set(RTS_BUILD_TESTS ON CACHE BOOL "Build modernization characterization/regression tests." FORCE)
    endif()
endif()

if(RTS_BUILD_GENERALS)
    option(RTS_BUILD_GENERALS_TOOLS "Build tools for Generals" ON)
    option(RTS_BUILD_GENERALS_EXTRAS "Build extra tools/tests for Generals" OFF)
    option(RTS_BUILD_GENERALS_DOCS "Build documentation for Generals" OFF)

    add_feature_info(GeneralsTools RTS_BUILD_GENERALS_TOOLS "Build Generals Mod Tools")
    add_feature_info(GeneralsExtras RTS_BUILD_GENERALS_EXTRAS "Build Generals Extra Tools/Tests")
    add_feature_info(GeneralsDocs RTS_BUILD_GENERALS_DOCS "Build Generals Documentation")
endif()

if(NOT IS_VS6_BUILD)
    # Because we set CMAKE_CXX_STANDARD_REQUIRED and CMAKE_CXX_EXTENSIONS in the compilers.cmake this should be enforced.
    target_compile_features(core_config INTERFACE cxx_std_20)

    # Keep C++-only diagnostics off C translation units and off vendored targets.
    if(NOT MSVC)
        target_compile_options(core_config INTERFACE
            $<$<COMPILE_LANGUAGE:CXX>:-Wsuggest-override>
        )
    endif()
endif()

if(IS_VS6_BUILD AND RTS_BUILD_OPTION_VC6_FULL_DEBUG)
    target_compile_options(core_config INTERFACE ${RTS_FLAGS} /Zi)
else()
    target_compile_options(core_config INTERFACE ${RTS_FLAGS})
endif()

# This disables a lot of warnings steering developers to use windows only functions/function names.
if(MSVC)
    target_compile_definitions(core_config INTERFACE _CRT_NONSTDC_NO_WARNINGS _CRT_SECURE_NO_WARNINGS $<$<CONFIG:DEBUG>:_DEBUG_CRT>)
endif()

if(UNIX)
    target_compile_definitions(core_config INTERFACE _UNIX)
endif()

if(RTS_BUILD_OPTION_DEBUG)
    target_compile_definitions(core_config INTERFACE RTS_DEBUG WWDEBUG DEBUG)
else()
    target_compile_definitions(core_config INTERFACE RTS_RELEASE NDEBUG)
endif()

if(RTS_BUILD_OPTION_PROFILE)
    target_compile_definitions(core_config INTERFACE RTS_PROFILE_LEGACY)
endif()

if(RTS_BUILD_OPTION_PERF_TELEMETRY)
    target_compile_definitions(core_config INTERFACE RTS_PERF_TELEMETRY)
endif()

# Define a dummy Tracy target when the build option is disabled.
if(RTS_BUILD_OPTION_PROFILE_TRACY)
    include(cmake/tracy.cmake)
else()
    add_library(core_profile_tracy INTERFACE)
endif()
