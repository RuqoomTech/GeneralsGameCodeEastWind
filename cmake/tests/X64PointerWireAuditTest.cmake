if(NOT DEFINED RTS_SOURCE_DIR)
    message(FATAL_ERROR "RTS_SOURCE_DIR is required")
endif()

function(rts_require_contains relative_path needle description)
    file(READ "${RTS_SOURCE_DIR}/${relative_path}" _content)
    string(FIND "${_content}" "${needle}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR "Step 04B regression: ${description} was not found in ${relative_path}")
    endif()
endfunction()

function(rts_require_absent relative_path needle description)
    file(READ "${RTS_SOURCE_DIR}/${relative_path}" _content)
    string(FIND "${_content}" "${needle}" _pos)
    if(NOT _pos EQUAL -1)
        message(FATAL_ERROR "Step 04B regression: ${description} remains in ${relative_path}")
    endif()
endfunction()

rts_require_contains(
    "Core/GameEngine/Include/GameClient/GameWindow.h"
    "typedef uintptr_t WindowMsgData;"
    "native-width WindowMsgData")
rts_require_contains(
    "Core/GameEngine/Source/Common/Audio/simpleplayer.cpp"
    "reinterpret_cast<DWORD_PTR>(this)"
    "native-width waveOut callback userdata")
rts_require_contains(
    "Core/GameEngine/Source/GameClient/GUI/IMEManager.cpp"
    "reinterpret_cast<UnsignedByte *>(clist) + clist->dwOffset[i]"
    "typed IME candidate-list address arithmetic")
rts_require_absent(
    "Core/GameEngine/Include/GameNetwork/NetworkDefs.h"
    "sizeof(GameMessage)"
    "runtime GameMessage size dependency in the wire packet contract")
rts_require_absent(
    "Core/GameEngine/Source/Common/Audio/simpleplayer.cpp"
    "(DWORD)this"
    "32-bit truncation of audio callback userdata")
rts_require_absent(
    "Core/GameEngine/Source/GameClient/GUI/IMEManager.cpp"
    "(UnsignedInt) clist"
    "32-bit truncation in IME candidate-list address arithmetic")

message(STATUS "Step 04B pointer/wire source audit passed")
