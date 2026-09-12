# Step 04E source-policy regression. Keep the Evolution wire/replay contract
# explicit and fixed-width while allowing the legacy replay reader to remain as
# an old-format compatibility path until the full x64 runtime is switched over.

if(NOT DEFINED RTS_SOURCE_DIR)
    message(FATAL_ERROR "RTS_SOURCE_DIR is required")
endif()

function(rts_read relative out_var)
    file(READ "${RTS_SOURCE_DIR}/${relative}" _content)
    set(${out_var} "${_content}" PARENT_SCOPE)
endfunction()

rts_read("Core/GameEngine/Source/Common/EvolutionCommandCodec.cpp" codec_cpp)
rts_read("Core/GameEngine/Source/Common/EvolutionReplayFormat.cpp" replay_cpp)
rts_read("Core/GameEngine/Source/GameNetwork/EvolutionProtocol.cpp" protocol_cpp)
rts_read("Core/GameEngine/Source/GameNetwork/NetPacketStructs.cpp" netpacket_cpp)
rts_read("Core/GameEngine/Include/GameNetwork/NetworkDefs.h" network_defs)
rts_read("Core/Tests/Fixtures/Step04EEvolutionProtocolV1.txt" fixture)

foreach(_content IN ITEMS codec_cpp replay_cpp protocol_cpp)
    if("${${_content}}" MATCHES "reinterpret_cast<[^>]*\\*>\\([^)]*\\)" OR
       "${${_content}}" MATCHES "writeObject" OR
       "${${_content}}" MATCHES "sizeof\\(GameMessage\\)" OR
       "${${_content}}" MATCHES "size_t[ \t]+(frame|messageType|playerIndex|sequence)")
        message(FATAL_ERROR "Step 04E Evolution wire code regressed to native-layout serialization in ${_content}")
    endif()
endforeach()

if(NOT codec_cpp MATCHES "appendU16" OR NOT codec_cpp MATCHES "appendU32" OR
   NOT codec_cpp MATCHES "COMMAND_CODEC_VERSION_V1")
    message(FATAL_ERROR "Evolution command codec no longer owns explicit little-endian fixed-width serialization")
endif()

if(NOT protocol_cpp MATCHES "NETWORK_PROTOCOL_VERSION_V1" OR
   NOT protocol_cpp MATCHES "encodeCommandBatchV1" OR
   NOT protocol_cpp MATCHES "PayloadSizeMismatch")
    message(FATAL_ERROR "Evolution network v1 framing/version validation is missing")
endif()

if(NOT replay_cpp MATCHES "REPLAY_FORMAT_VERSION_V1" OR
   NOT replay_cpp MATCHES "COMMAND_CODEC_VERSION_V1")
    message(FATAL_ERROR "Evolution replay v1 no longer pins both replay and command-codec versions")
endif()

if(NOT netpacket_cpp MATCHES "NetGameCommandToEvolutionBytes" OR
   NOT netpacket_cpp MATCHES "decodeCommandV1")
    message(FATAL_ERROR "Live game-command packet data is no longer routed through the Evolution command codec")
endif()

# Step 04B invariant must stay in place: the frozen legacy packet container is
# explicit bytes, never runtime GameMessage layout.
if(network_defs MATCHES "sizeof\\(GameMessage\\)")
    message(FATAL_ERROR "NetworkDefs regressed to sizeof(GameMessage) wire sizing")
endif()

if(NOT fixture MATCHES "command=" OR NOT fixture MATCHES "network_batch=" OR NOT fixture MATCHES "network=" OR
   NOT fixture MATCHES "replay_header=" OR NOT fixture MATCHES "replay_record=")
    message(FATAL_ERROR "Step 04E golden fixture is incomplete")
endif()

message(STATUS "Step 04E Evolution protocol source policy passed")
