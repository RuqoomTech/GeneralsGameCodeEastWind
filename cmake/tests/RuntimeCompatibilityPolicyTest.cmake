# Runtime compatibility policy for selected upstream correctness fixes.
# Keep locally owned x64/determinism adaptations authoritative.

include("${CMAKE_CURRENT_LIST_DIR}/PolicyTestHelpers.cmake")

set(_dozer_sources
    Generals/Code/GameEngine/Source/GameLogic/Object/Update/AIUpdate/DozerAIUpdate.cpp
    Generals/Code/GameEngine/Source/GameLogic/Object/Update/AIUpdate/WorkerAIUpdate.cpp
    GeneralsMD/Code/GameEngine/Source/GameLogic/Object/Update/AIUpdate/DozerAIUpdate.cpp
    GeneralsMD/Code/GameEngine/Source/GameLogic/Object/Update/AIUpdate/WorkerAIUpdate.cpp
)
foreach(_file IN LISTS _dozer_sources)
    rts_policy_require_contains("${_file}" "if (version >= 2)")
    rts_policy_require_absent("${_file}" "if (currentVersion >= 2)")
    rts_policy_require_contains("${_file}" "onDisabledEdge(Bool nowDisabled)")
endforeach()

rts_policy_require_contains("Generals/Code/GameEngine/Source/GameLogic/Object/Object.cpp"
    "(*module)->onDisabledEdge( becomingDisabled )")
rts_policy_require_contains("GeneralsMD/Code/GameEngine/Source/GameLogic/Object/Object.cpp"
    "(*module)->onDisabledEdge( becomingDisabled )")
rts_policy_require_absent("GeneralsMD/Code/GameEngine/Source/GameLogic/Object/Object.cpp"
    "DozerAIInterface *dozerAI = getAI() ? getAI()->getDozerAIInterface() : nullptr;")

set(_production_sources
    Generals/Code/GameEngine/Source/GameLogic/Object/Update/ProductionUpdate.cpp
    GeneralsMD/Code/GameEngine/Source/GameLogic/Object/Update/ProductionUpdate.cpp
)
foreach(_file IN LISTS _production_sources)
    rts_policy_require_contains("${_file}" "Bool ProductionUpdate::cancelUpgrade( ProductionEntry *production )")
    rts_policy_require_contains("${_file}" "Bool ProductionUpdate::cancelUnitCreate( ProductionEntry *production )")
    rts_policy_require_contains("${_file}" "getProductionQuantityRemaining() < production->getProductionQuantity()")
endforeach()

rts_policy_require_contains("Core/GameEngine/Include/Common/GameDefines.h"
    "PRESERVE_RETAIL_NUKE_MISSILE_OUTER_RADIUS_SEARCH")
rts_policy_require_contains("Core/GameEngine/Include/Common/GameDefines.h"
    "PRESERVE_RETAIL_NUKE_MISSILE_OUTER_RADIUS_DAMAGE")
foreach(_file IN ITEMS
    Generals/Code/GameEngine/Source/GameLogic/Object/Update/NeutronMissileSlowDeathUpdate.cpp
    GeneralsMD/Code/GameEngine/Source/GameLogic/Object/Update/NeutronMissileSlowDeathUpdate.cpp)
    rts_policy_require_contains("${_file}" "FROM_BOUNDINGSPHERE_3D")
    rts_policy_require_contains("${_file}" "PRESERVE_RETAIL_NUKE_MISSILE_OUTER_RADIUS_SEARCH")
    rts_policy_require_contains("${_file}" "PRESERVE_RETAIL_NUKE_MISSILE_OUTER_RADIUS_DAMAGE")
endforeach()

# Keep EastWind's strict-aliasing-safe deterministic float path while importing
# only the coordinate operators needed by the upstream neutron fix.
rts_policy_require_contains("Core/Libraries/Include/Lib/BaseType.h" "memcpy(&bits, &value, sizeof(bits));")
rts_policy_require_contains("Core/Libraries/Include/Lib/BaseType.h" "Coord3D operator-() const")
rts_policy_require_absent("Core/Libraries/Include/Lib/BaseType.h" "unsigned x = *(unsigned *)&f;")

# Memory integration is adapted, never replaced by upstream's 32-bit allocator.
rts_policy_require_contains("Core/GameEngine/Source/Common/System/GameMemory.cpp"
    "#define MEM_BOUND_ALIGNMENT (sizeof(void *))")
rts_policy_require_contains("Core/GameEngine/Source/Common/System/GameMemory.cpp"
    "static NOINLINE void preMainInitMemoryManagerImpl()")
rts_policy_require_contains("Core/GameEngine/Source/Common/System/GameMemory.cpp"
    "void operator delete(void *p, size_t) noexcept")
rts_policy_require_contains("Core/GameEngine/Source/Common/System/GameMemory.cpp"
    "void operator delete[](void *p, size_t) noexcept")
rts_policy_require_contains("Core/GameEngine/Source/Common/System/GameMemory.cpp" "LINK_TESTER_INCREMENT()")
rts_policy_require_contains("Core/GameEngine/Source/Common/System/GameMemory.cpp" "static size_t roundUpMemBound(size_t i)")

# The local runtime loaders replace hard import-stub link dependencies. Their
# x64 behavior is intentionally neutral when the retail 32-bit DLL cannot load.
rts_policy_require_contains("CMakeLists.txt" "add_subdirectory(Dependencies/Bink)")
rts_policy_require_contains("CMakeLists.txt" "add_subdirectory(Dependencies/Miles)")
rts_policy_require_absent("CMakeLists.txt" "include(cmake/bink.cmake)")
rts_policy_require_absent("CMakeLists.txt" "include(cmake/miles.cmake)")
rts_policy_require_contains("Core/GameEngineDevice/Source/VideoDevice/Bink/BinkVideoPlayer.cpp" "BinkLoader::load()")
rts_policy_require_contains("Core/Libraries/Source/WWVegas/WWAudio/WWAudio.cpp" "MilesLoader::load ()")

file(GLOB_RECURSE _cmake_lists "${RTS_SOURCE_DIR}/*/CMakeLists.txt")
foreach(_cmake IN LISTS _cmake_lists)
    file(RELATIVE_PATH _cmake_rel "${RTS_SOURCE_DIR}" "${_cmake}")
    if(_cmake_rel MATCHES "^build/")
        continue()
    endif()
    file(READ "${_cmake}" _text)
    if(_text MATCHES "(^|[^A-Za-z0-9_])(binkstub|milesstub)([^A-Za-z0-9_]|$)")
        message(FATAL_ERROR "Runtime compatibility policy: legacy import-stub target remains in ${_cmake}")
    endif()
endforeach()

# Legacy renderer safety import: dynamic glyph buffers may exceed the historic
# fixed page length for a single oversized glyph.
rts_policy_require_contains("Core/Libraries/Source/WWVegas/WW3D2/render2dsentence.h" "int\t\t\t\tLength;")
rts_policy_require_contains("Core/Libraries/Source/WWVegas/WW3D2/render2dsentence.cpp"
    "const int length = max( (int)CHAR_BUFFER_LEN, char_len );")

message(STATUS "Runtime compatibility policy passed")
