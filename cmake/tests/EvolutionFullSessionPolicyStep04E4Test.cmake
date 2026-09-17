# Step 04E4 source-policy regression. The golden full-session gate must remain
# additive: EVN1/EVR1 v1 bytes are frozen, legacy .rep metadata/fallback remains
# available, and a sidecar selected successfully at playback start must fail
# closed on later corruption rather than silently changing command sources.

if(NOT DEFINED RTS_SOURCE_DIR)
    message(FATAL_ERROR "RTS_SOURCE_DIR is required")
endif()

function(rts_read relative out_var)
    file(READ "${RTS_SOURCE_DIR}/${relative}" _content)
    set(${out_var} "${_content}" PARENT_SCOPE)
endfunction()

rts_read("Core/GameEngine/Include/Common/EvolutionReplayFormat.h" replay_h)
rts_read("Core/GameEngine/Source/Common/EvolutionReplayStream.cpp" replay_stream_cpp)
rts_read("Core/GameEngine/Include/GameNetwork/EvolutionProtocol.h" protocol_h)
rts_read("Generals/Code/GameEngine/Source/Common/Recorder.cpp" generals_recorder)
rts_read("GeneralsMD/Code/GameEngine/Source/Common/Recorder.cpp" zh_recorder)
rts_read("Core/Tests/EvolutionFullSessionStep04E4Test.cpp" full_session_test)
rts_read("Core/Tests/CMakeLists.txt" tests_cmake)

if(NOT replay_h MATCHES "REPLAY_MAGIC_V1" OR
   NOT replay_h MATCHES "REPLAY_FORMAT_VERSION_V1 = 1" OR
   NOT replay_h MATCHES "REPLAY_HEADER_BYTES_V1 = 16" OR
   NOT protocol_h MATCHES "NETWORK_PROTOCOL_VERSION_V1 = 1" OR
   NOT protocol_h MATCHES "NETWORK_HEADER_BYTES_V1 = 20")
    message(FATAL_ERROR "Step 04E4 must not mutate the frozen EVN1/EVR1 v1 framing contract")
endif()

if(NOT replay_stream_cpp MATCHES "decodeReplayHeaderV1" OR
   NOT replay_stream_cpp MATCHES "decodeReplayCommandRecordV1" OR
   NOT replay_stream_cpp MATCHES "advanceReplaySequenceV1" OR
   NOT replay_stream_cpp MATCHES "MAX_ENCODED_COMMAND_BYTES_V1")
    message(FATAL_ERROR "EVR1 runtime stream lost shared version/corruption validation")
endif()

foreach(_rec IN ITEMS generals_recorder zh_recorder)
    string(FIND "${${_rec}}" "m_useEvolutionReplay = m_evolutionReplay.openForRead" _select_sidecar)
    string(FIND "${${_rec}}" "m_evolutionReplayPath.clear();" _fallback_open)
    string(FIND "${${_rec}}" "m_file->read(&m_nextFrame" _legacy_read)
    string(FIND "${${_rec}}" "ReplayStreamReadStatus::Error" _sidecar_error)
    string(FIND "${${_rec}}" "stopPlayback();" _stop_playback)
    if(_select_sidecar EQUAL -1 OR _fallback_open EQUAL -1 OR _legacy_read EQUAL -1)
        message(FATAL_ERROR "${_rec} lost open-time EVR1 preference with practical legacy .rep fallback")
    endif()
    if(_sidecar_error EQUAL -1 OR _stop_playback EQUAL -1 OR NOT _sidecar_error LESS _stop_playback)
        message(FATAL_ERROR "${_rec} no longer fails closed after a selected EVR1 stream becomes corrupt")
    endif()
endforeach()

string(FIND "${tests_cmake}" "if(CMAKE_SIZEOF_VOID_P EQUAL 8)" _x64_guard)
string(FIND "${tests_cmake}" "evolution_full_session_x64_step04e4" _e4_target)
if(_x64_guard EQUAL -1 OR _e4_target EQUAL -1 OR NOT _x64_guard LESS _e4_target)
    message(FATAL_ERROR "Step 04E4 full-session gate must remain x64-only; i686 is frozen")
endif()

foreach(_required IN ITEMS
    "buildNetworkTranscript"
    "buildReplayTranscript"
    "deterministicNetworkDelivery"
    "validateLogicCrcAndCheckpoints"
    "advanceReplaySequenceV1"
    "LOGIC_CRC_MESSAGE_TYPE = 1095"
    "UnsupportedVersion"
    "UnsupportedFormatVersion"
    "UnsupportedCommandCodecVersion"
    "backward EVR1 frame ordering")
    string(FIND "${full_session_test}" "${_required}" _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR "Step 04E4 golden session harness lost required coverage: ${_required}")
    endif()
endforeach()

message(STATUS "Step 04E4 Evolution full-session/compatibility source policy passed")
