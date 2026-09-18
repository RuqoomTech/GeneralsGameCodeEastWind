include("${CMAKE_CURRENT_LIST_DIR}/PolicyTestHelpers.cmake")

rts_policy_require_contains(
    "Core/GameEngine/Include/GameClient/GameWindow.h"
    "typedef uintptr_t WindowMsgData;"
    "WindowMsgData must remain native-width")
rts_policy_require_contains(
    "Core/GameEngine/Source/Common/Audio/simpleplayer.cpp"
    "reinterpret_cast<DWORD_PTR>(this)"
    "waveOut callback userdata must remain native-width")
rts_policy_require_contains(
    "Core/GameEngine/Source/GameClient/GUI/IMEManager.cpp"
    "reinterpret_cast<UnsignedByte *>(clist) + clist->dwOffset[i]"
    "IME candidate-list arithmetic must remain pointer-safe")
rts_policy_require_absent(
    "Core/GameEngine/Include/GameNetwork/NetworkDefs.h"
    "sizeof(GameMessage)"
    "wire packet contracts must not depend on runtime GameMessage size")
rts_policy_require_absent(
    "Core/GameEngine/Source/Common/Audio/simpleplayer.cpp"
    "(DWORD)this"
    "audio callback userdata must not truncate pointers")
rts_policy_require_absent(
    "Core/GameEngine/Source/GameClient/GUI/IMEManager.cpp"
    "(UnsignedInt) clist"
    "IME candidate-list addresses must not truncate pointers")

message(STATUS "Wire/pointer policy passed")
