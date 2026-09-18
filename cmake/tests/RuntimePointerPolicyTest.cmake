include("${CMAKE_CURRENT_LIST_DIR}/PolicyTestHelpers.cmake")

rts_policy_read("Core/Libraries/Source/WWVegas/WWLib/mempool.h" _mempool)
rts_policy_require_text("${_mempool}" "struct BlockHeader" "WWLib object-pool blocks must use a native-pointer header")
rts_policy_require_text("${_mempool}" "BlockHeader *BlockListHead" "WWLib object-pool block chain must be pointer-width native")
rts_policy_forbid_text("${_mempool}" "uint32 *\tBlockListHead" "WWLib object-pool block chain regressed to a 32-bit surrogate")
rts_policy_forbid_text("${_mempool}" "sizeof(uint32 *)" "WWLib object-pool allocation must not model a pointer header with uint32")

rts_policy_read("Core/Libraries/Source/WWVegas/WWLib/FastAllocator.h" _fast_allocator)
rts_policy_require_text("${_fast_allocator}" "struct alignas(void*) AllocationHeader" "FastAllocator metadata must preserve native pointer alignment")
rts_policy_require_text("${_fast_allocator}" "alignas(void*) char mem[size]" "FastFixedAllocator chunk storage must preserve native pointer alignment")
rts_policy_require_text("${_fast_allocator}" "return header + 1" "FastAllocator user storage must begin after the native-aligned header")
rts_policy_forbid_text("${_fast_allocator}" "return ((unsigned int*)pMemory)+1" "FastAllocator regressed to a four-byte user-data prefix")

rts_policy_read("Core/GameEngine/Source/Common/System/GameMemory.cpp" _game_memory)
rts_policy_require_text("${_game_memory}" "#define MEM_BOUND_ALIGNMENT (sizeof(void *))" "GameMemory pool alignment must follow native pointer width")
rts_policy_require_text("${_game_memory}" "static size_t roundUpMemBound(size_t i)" "GameMemory native byte arithmetic must use size_t")
rts_policy_require_text("${_game_memory}" "reinterpret_cast<uintptr_t>(result)" "GameMemory alignment checks must not truncate pointers")
rts_policy_forbid_text("${_game_memory}" "#define MEM_BOUND_ALIGNMENT 4" "GameMemory pool alignment regressed to a Win32-only constant")
rts_policy_forbid_text("${_game_memory}" "if (unsigned(result)&3)" "GameMemory pointer alignment check regressed to 32-bit truncation")

rts_policy_read("Core/GameEngine/Include/GameClient/WindowVideoManager.h" _window_video)
rts_policy_require_text("${_window_video}" "std::hash<uintptr_t>" "pointer-key hashing must use native uintptr_t")
rts_policy_forbid_text("${_window_video}" "std::hash<UnsignedInt>" "pointer-key hashing regressed to a 32-bit logical integer")

rts_policy_read("Core/GameEngine/Source/GameClient/Input/Keyboard.cpp" _keyboard)
rts_policy_require_text("${_keyboard}" "reinterpret_cast<uintptr_t>(kLayout)" "HKL extraction must pass through native-width uintptr_t")
rts_policy_forbid_text("${_keyboard}" "(UnsignedInt)kLayout" "HKL extraction regressed to direct 32-bit handle truncation")

rts_policy_read("Core/GameEngine/Source/GameClient/GUI/Gadget/GadgetListBox.cpp" _listbox)
rts_policy_require_text("${_listbox}" "*(Int**)mData2 = list->selections" "multi-select pointer return must remain native pointer width")
rts_policy_forbid_text("${_listbox}" "*(Int*)mData2 = (Int)list->selections" "multi-select pointer return regressed to Int truncation")

rts_policy_read("Core/GameEngine/Source/GameNetwork/GameSpy/Chat.cpp" _chat)
rts_policy_require_text("${_chat}" "GadgetListBoxGetSelected(playerListbox, &selections);" "multi-select callers should use the native Int** contract")

# Modern C++ runtime ABI guard. The standard unsized global delete
# replacements must match <new>'s noexcept contract on current GCC/Clang/MSVC.
rts_policy_read("Core/Libraries/Source/WWVegas/WWLib/always.h" _ww_always)
rts_policy_require_text("${_ww_always}" "operator delete\t\t(void *p) noexcept;" "WWLib global operator delete must match the standard noexcept contract")
rts_policy_require_text("${_ww_always}" "operator delete[]\t(void *p) noexcept;" "WWLib global operator delete[] must match the standard noexcept contract")

rts_policy_read("Core/GameEngine/Include/Common/GameMemory.h" _game_memory_header)
rts_policy_require_text("${_game_memory_header}" "operator delete\t\t(void *p) noexcept;" "GameMemory global operator delete must match the standard noexcept contract")
rts_policy_require_text("${_game_memory_header}" "operator delete[]\t(void *p) noexcept;" "GameMemory global operator delete[] must match the standard noexcept contract")

rts_policy_read("Core/GameEngine/Include/Common/GameMemoryNull.h" _game_memory_null_header)
rts_policy_require_text("${_game_memory_null_header}" "operator delete(void *p) noexcept;" "GameMemoryNull global operator delete must match the standard noexcept contract")
rts_policy_require_text("${_game_memory_null_header}" "operator delete[](void *p) noexcept;" "GameMemoryNull global operator delete[] must match the standard noexcept contract")

rts_policy_read("Core/GameEngine/Source/Common/System/GameMemory.cpp" _game_memory_source)
rts_policy_require_text("${_game_memory_source}" "void operator delete(void *p) noexcept" "GameMemory delete definition must preserve noexcept")
rts_policy_require_text("${_game_memory_source}" "void operator delete[](void *p) noexcept" "GameMemory delete[] definition must preserve noexcept")

rts_policy_read("Core/GameEngine/Source/Common/System/GameMemoryNull.cpp" _game_memory_null_source)
rts_policy_require_text("${_game_memory_null_source}" "operator delete(void *p) noexcept" "GameMemoryNull delete definition must preserve noexcept")
rts_policy_require_text("${_game_memory_null_source}" "operator delete[](void *p) noexcept" "GameMemoryNull delete[] definition must preserve noexcept")

rts_policy_read("Core/Libraries/Source/WWVegas/WWStub/wwallocstub.cpp" _ww_alloc_stub)
rts_policy_require_text("${_ww_alloc_stub}" "void operator delete(void *p) noexcept" "WWStub delete definition must preserve noexcept")
rts_policy_require_text("${_ww_alloc_stub}" "void operator delete[](void *p) noexcept" "WWStub delete[] definition must preserve noexcept")

rts_policy_require_text("${_mempool}" "#ifdef DEBUG_CRASHING\n\tint block_count = 0;" "ObjectPool debug-only block counter must not trigger release -Werror builds")

rts_policy_read("Core/Tests/RuntimeNativeWidthTest.cpp" _runtime_native_width_test)
rts_policy_require_text("${_runtime_native_width_test}" "std::uintptr_t marker" "native-width allocator probe marker must follow the 64-bit native pointer width")
rts_policy_require_text("${_runtime_native_width_test}" "alignof(PoolProbe) <= alignof(void *)" "native-width allocator probe must not impose stronger-than-pointer alignment")
rts_policy_forbid_text("${_runtime_native_width_test}" "std::uint64_t marker" "allocator probe must use uintptr_t rather than a synthetic fixed-width marker")

message(STATUS "Runtime pointer policy passed")
