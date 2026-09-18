# Step 04E3 source-policy regression. The deterministic two-endpoint harness
# must stay attached to the production EVN1 codec/framing seam while runtime
# reliability, relay, duplicate/order, disconnect and ACK behavior remains in
# the existing legacy networking machinery during this staged migration.

if(NOT DEFINED RTS_SOURCE_DIR)
    message(FATAL_ERROR "RTS_SOURCE_DIR is required")
endif()

function(rts_read relative out_var)
    file(READ "${RTS_SOURCE_DIR}/${relative}" _content)
    set(${out_var} "${_content}" PARENT_SCOPE)
endfunction()

rts_read("Core/GameEngine/Include/GameNetwork/EvolutionProtocol.h" protocol_h)
rts_read("Core/GameEngine/Source/GameNetwork/EvolutionProtocol.cpp" protocol_cpp)
rts_read("Core/GameEngine/Source/GameNetwork/Transport.cpp" transport_cpp)
rts_read("Core/GameEngine/Source/GameNetwork/Connection.cpp" connection_cpp)
rts_read("Core/GameEngine/Source/GameNetwork/ConnectionManager.cpp" manager_cpp)
rts_read("Core/GameEngine/Source/GameNetwork/NetCommandList.cpp" command_list_cpp)
rts_read("Core/Tests/EvolutionSessionStep04E3Test.cpp" session_test)
rts_read("Core/Tests/CMakeLists.txt" tests_cmake)

if(NOT protocol_h MATCHES "encodeRoutedCommandPacketV1" OR
   NOT protocol_h MATCHES "decodeRoutedCommandPacketV1" OR
   NOT protocol_cpp MATCHES "encodeRoutedCommandBatchV1" OR
   NOT protocol_cpp MATCHES "decodeRoutedCommandBatchV1")
    message(FATAL_ERROR "Step 04E3 lost the consolidated production routed-packet composition seam")
endif()

if(NOT connection_cpp MATCHES "#if defined\\(_WIN64\\)" OR
   NOT connection_cpp MATCHES "encodeRoutedCommandPacketV1" OR
   NOT connection_cpp MATCHES "NETCOMMANDTYPE_GAMECOMMAND" OR
   NOT connection_cpp MATCHES "queueEvolutionSend")
    message(FATAL_ERROR "Evolution gameplay emission is no longer Win64-only or no longer uses production EVN1")
endif()

# Successful EVN1 gameplay emission must leave this retry iteration immediately;
# the legacy packet.addCommand path below it is intentional fallback, not a second send.
string(FIND "${connection_cpp}" "QueueEvolutionGameCommand(m_transport, m_user, *msg)" _evn_send)
string(FIND "${connection_cpp}" "msg = next;\n\t\t\t\t\tcontinue;" _evn_continue)
string(FIND "${connection_cpp}" "notDone = packet.addCommand(msg);" _legacy_fallback)
if(_evn_send EQUAL -1 OR _evn_continue EQUAL -1 OR _legacy_fallback EQUAL -1 OR
   NOT _evn_send LESS _evn_continue OR NOT _evn_continue LESS _legacy_fallback)
    message(FATAL_ERROR "EVN1 success/fallback ordering can duplicate gameplay commands or bypass intentional fallback")
endif()

if(NOT manager_cpp MATCHES "decodeRoutedCommandPacketV1" OR
   NOT manager_cpp MATCHES "routedResult.ok\\(\\)" OR
   NOT manager_cpp MATCHES "ackCommand\\(ref, m_localSlot\\)" OR
   NOT manager_cpp MATCHES "processNetCommand\\(ref\\)" OR
   NOT manager_cpp MATCHES "sendRemoteCommand\\(ref\\)")
    message(FATAL_ERROR "ConnectionManager no longer feeds validated EVN1 into existing ACK/process/relay machinery")
endif()

# EVN1 must still be recognized before legacy decrypt; the legacy transport is
# still authoritative for ACK/control/session/disconnect traffic in Step 04E3.
string(FIND "${transport_cpp}" "decodeNetworkPacketV1" _evn_decode)
string(FIND "${transport_cpp}" "decryptBuf(buf, len)" _legacy_decrypt)
if(_evn_decode EQUAL -1 OR _legacy_decrypt EQUAL -1 OR NOT _evn_decode LESS _legacy_decrypt)
    message(FATAL_ERROR "Transport receive ordering regressed: EVN1 must be detected before legacy decrypt")
endif()

# Duplicate suppression and deterministic command ordering remain production
# NetCommandList responsibilities. Do not let the session harness become a
# replacement networking implementation.
if(NOT command_list_cpp MATCHES "isEqualCommandMsg" OR
   NOT command_list_cpp MATCHES "getPlayerID\\(\\)" OR
   NOT command_list_cpp MATCHES "getID\\(\\)" OR
   NOT command_list_cpp MATCHES "isCommandIdNewer" OR
   NOT command_list_cpp MATCHES "diff < 0x8000")
    message(FATAL_ERROR "Production command duplicate/order semantics needed by Step 04E3 are missing")
endif()

if(NOT tests_cmake MATCHES "if\\(CMAKE_SIZEOF_VOID_P EQUAL 8\\)" OR
   NOT tests_cmake MATCHES "evolution_session_x64_step04e3")
    message(FATAL_ERROR "Step 04E3 session gate must remain x64-only")
endif()

foreach(_required IN ITEMS
    "encodeRoutedCommandPacketV1"
    "decodeRoutedCommandPacketV1"
    "LOGIC_CRC_MESSAGE_TYPE = 1095"
    "testRetryAckAndDuplicate"
    "testDelayedOrderingFrameSyncAndCrc"
    "testMalformedRuntimeBoundary"
    "testIntentionalEvN1EmissionFailure")
    string(FIND "${session_test}" "${_required}" _found)
    if(_found EQUAL -1)
        message(FATAL_ERROR "Step 04E3 deterministic session harness lost required coverage: ${_required}")
    endif()
endforeach()

message(STATUS "Step 04E3 Evolution session integration source policy passed")
