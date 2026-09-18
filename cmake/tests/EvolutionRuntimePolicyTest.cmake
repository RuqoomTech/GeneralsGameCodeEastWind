# Evolution runtime source policy. Gameplay commands may use the staged EVN1
# raw UDP path while legacy ACK/control traffic remains on the historical
# transport. Replay recording/playback must exercise EVR1 without removing the
# practical legacy .rep fallback.

include("${CMAKE_CURRENT_LIST_DIR}/PolicyTestHelpers.cmake")

rts_policy_read("Core/GameEngine/Include/GameNetwork/EvolutionProtocol.h" protocol_h)
rts_policy_read("Core/GameEngine/Include/GameNetwork/Transport.h" transport_h)
rts_policy_read("Core/GameEngine/Source/GameNetwork/Transport.cpp" transport_cpp)
rts_policy_read("Core/GameEngine/Source/GameNetwork/Connection.cpp" connection_cpp)
rts_policy_read("Core/GameEngine/Source/GameNetwork/ConnectionManager.cpp" manager_cpp)
rts_policy_read("Core/GameEngine/Source/Common/EvolutionReplayStream.cpp" replay_stream_cpp)
rts_policy_read("Generals/Code/GameEngine/Include/Common/Recorder.h" generals_recorder_h)
rts_policy_read("Generals/Code/GameEngine/Source/Common/Recorder.cpp" generals_recorder)
rts_policy_read("GeneralsMD/Code/GameEngine/Include/Common/Recorder.h" zh_recorder_h)
rts_policy_read("GeneralsMD/Code/GameEngine/Source/Common/Recorder.cpp" zh_recorder)
rts_policy_read("Generals/Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/PopupReplay.cpp" generals_popup)
rts_policy_read("GeneralsMD/Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/PopupReplay.cpp" zh_popup)


# The staged runtime bridge is an Evolution x64 feature. The retired i686 lane
# may compile/test the portable codec but must keep legacy runtime networking
# and replay class layouts/behavior.
foreach(_guarded IN ITEMS transport_h transport_cpp connection_cpp manager_cpp generals_recorder_h generals_recorder zh_recorder_h zh_recorder generals_popup zh_popup)
    string(FIND "${${_guarded}}" "#if defined(_WIN64)" _win64_guard)
    if(_win64_guard EQUAL -1)
        message(FATAL_ERROR "${_guarded} lost the Win64-only Evolution runtime guard")
    endif()
endforeach()

if(NOT protocol_h MATCHES "RoutedCommandBatch" OR
   NOT protocol_h MATCHES "relayMask" OR
   NOT protocol_h MATCHES "encodeRoutedCommandBatchV1")
    message(FATAL_ERROR "Routed EVN1 command framing is missing")
endif()

string(FIND "${transport_cpp}" "decodeNetworkPacketV1" _evn_decode)
string(FIND "${transport_cpp}" "decryptBuf(buf, len)" _legacy_decrypt)
if(_evn_decode EQUAL -1 OR _legacy_decrypt EQUAL -1 OR NOT _evn_decode LESS _legacy_decrypt)
    message(FATAL_ERROR "Transport must recognize EVN1 before legacy decrypt/CRC parsing")
endif()
if(NOT transport_h MATCHES "EvolutionTransportMessage" OR NOT transport_h MATCHES "queueEvolutionSend")
    message(FATAL_ERROR "Transport header lost the staged Win64 EVN1 queue contract")
endif()

if(NOT transport_cpp MATCHES "queueEvolutionSend" OR NOT transport_cpp MATCHES "m_evolutionInBuffer")
    message(FATAL_ERROR "Transport no longer exposes the staged raw EVN1 queues")
endif()

if(NOT connection_cpp MATCHES "encodeRoutedCommandPacketV1" OR
   NOT connection_cpp MATCHES "queueEvolutionSend" OR
   NOT connection_cpp MATCHES "NETCOMMANDTYPE_GAMECOMMAND")
    message(FATAL_ERROR "Connection no longer routes gameplay commands through EVN1")
endif()

if(NOT manager_cpp MATCHES "decodeRoutedCommandPacketV1" OR
   NOT manager_cpp MATCHES "ackCommand" OR
   NOT manager_cpp MATCHES "sendRemoteCommand")
    message(FATAL_ERROR "ConnectionManager no longer bridges EVN1 into existing ACK/relay semantics")
endif()

if(NOT replay_stream_cpp MATCHES "encodeReplayHeaderV1" OR
   NOT replay_stream_cpp MATCHES "encodeReplayCommandRecordV1" OR
   NOT replay_stream_cpp MATCHES "decodeReplayCommandRecordV1")
    message(FATAL_ERROR "EVR1 runtime stream no longer uses the shared replay format")
endif()

foreach(_rec IN ITEMS generals_recorder zh_recorder)
    string(FIND "${${_rec}}" "openForWrite" _open_write)
    string(FIND "${${_rec}}" "openForRead" _open_read)
    string(FIND "${${_rec}}" "writeGameMessage" _write_message)
    string(FIND "${${_rec}}" "m_useEvolutionReplay" _use_evolution)
    string(FIND "${${_rec}}" ".evr" _sidecar_suffix)
    string(FIND "${${_rec}}" "m_file->read" _legacy_read)
    if(_open_write EQUAL -1 OR _open_read EQUAL -1 OR _write_message EQUAL -1 OR
       _use_evolution EQUAL -1 OR _sidecar_suffix EQUAL -1)
        message(FATAL_ERROR "${_rec} no longer records/plays the transitional EVR1 sidecar")
    endif()
    if(_legacy_read EQUAL -1)
        message(FATAL_ERROR "${_rec} removed the practical legacy replay fallback too early")
    endif()
endforeach()

foreach(_popup IN ITEMS generals_popup zh_popup)
    string(FIND "${${_popup}}" "oldEvolutionFilename" _old_sidecar)
    string(FIND "${${_popup}}" "newEvolutionFilename" _new_sidecar)
    string(FIND "${${_popup}}" "DeleteFile(newEvolutionFilename.str())" _delete_stale)
    if(_old_sidecar EQUAL -1 OR _new_sidecar EQUAL -1 OR _delete_stale EQUAL -1)
        message(FATAL_ERROR "${_popup} can leave a stale/missing EVR1 sidecar when saving a named replay")
    endif()
endforeach()

message(STATUS "Evolution runtime policy passed")
