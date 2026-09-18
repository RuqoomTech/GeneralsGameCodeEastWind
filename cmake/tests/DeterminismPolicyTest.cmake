include("${CMAKE_CURRENT_LIST_DIR}/PolicyTestHelpers.cmake")

rts_policy_read("Core/GameEngine/Source/GameLogic/System/FPUControl.cpp" _fp)
foreach(_needle IN ITEMS "FE_TONEAREST" "_PC_24" "defined(__i386__)" "fesetround")
    string(FIND "${_fp}" "${_needle}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR "Deterministic FPU policy is missing '${_needle}'")
    endif()
endforeach()

foreach(_game_logic IN ITEMS
    "${RTS_SOURCE_DIR}/Generals/Code/GameEngine/Source/GameLogic/System/GameLogic.cpp"
    "${RTS_SOURCE_DIR}/GeneralsMD/Code/GameEngine/Source/GameLogic/System/GameLogic.cpp")
    file(READ "${_game_logic}" _logic)
    string(FIND "${_logic}" "void setFPMode()" _duplicate)
    if(NOT _duplicate EQUAL -1)
        message(FATAL_ERROR "setFPMode() must remain centralized outside ${_game_logic}")
    endif()
endforeach()

rts_policy_read("Core/Tests/CMakeLists.txt" _tests)
foreach(_needle IN ITEMS "-fno-fast-math" "-ffp-contract=off" "headless_determinism")
    string(FIND "${_tests}" "${_needle}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR "Deterministic focused CMake policy is missing '${_needle}'")
    endif()
endforeach()

rts_policy_read("Core/Tests/DeterminismPrimitivesTest.cpp" _determinism_guard)
string(FIND "${_determinism_guard}" "#include \"Common/MessageStream.h\"" _message_stream_include)
if(_message_stream_include EQUAL -1)
    message(FATAL_ERROR
        "The determinism guard must include Common/MessageStream.h explicitly; "
        "do not rely on NetworkDefs.h or another transitive include for GameMessage ABI/replay definitions.")
endif()

rts_policy_read("Core/Tests/HeadlessDeterminismTest.cpp" _timeline)
foreach(_frame IN ITEMS "0U" "1U" "10U" "100U" "1000U" "5000U" "10000U" "kEndFrame")
    string(FIND "${_timeline}" "${_frame}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR "Headless determinism timeline is missing checkpoint ${_frame}")
    endif()
endforeach()

message(STATUS "Determinism policy passed")
