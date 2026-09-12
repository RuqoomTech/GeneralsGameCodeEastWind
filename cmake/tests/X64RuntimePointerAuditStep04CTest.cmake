if(NOT DEFINED RTS_SOURCE_DIR)
    message(FATAL_ERROR "RTS_SOURCE_DIR is required")
endif()

function(_read_repo_file relative_path out_var)
    set(_path "${RTS_SOURCE_DIR}/${relative_path}")
    if(NOT EXISTS "${_path}")
        message(FATAL_ERROR "Step 04C audit input is missing: ${relative_path}")
    endif()
    file(READ "${_path}" _contents)
    set(${out_var} "${_contents}" PARENT_SCOPE)
endfunction()

function(_require_contains contents token description)
    string(FIND "${contents}" "${token}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR "Step 04C regression: ${description}")
    endif()
endfunction()

function(_require_absent contents token description)
    string(FIND "${contents}" "${token}" _pos)
    if(NOT _pos EQUAL -1)
        message(FATAL_ERROR "Step 04C regression: ${description}")
    endif()
endfunction()

_read_repo_file("Core/Libraries/Source/WWVegas/WWLib/mempool.h" _mempool)
_require_contains("${_mempool}" "struct BlockHeader" "WWLib object-pool blocks must use a native-pointer header")
_require_contains("${_mempool}" "BlockHeader *BlockListHead" "WWLib object-pool block chain must be pointer-width native")
_require_absent("${_mempool}" "uint32 *\tBlockListHead" "WWLib object-pool block chain regressed to a 32-bit surrogate")
_require_absent("${_mempool}" "sizeof(uint32 *)" "WWLib object-pool allocation must not model a pointer header with uint32")

_read_repo_file("Core/Libraries/Source/WWVegas/WWLib/FastAllocator.h" _fast_allocator)
_require_contains("${_fast_allocator}" "struct alignas(void*) AllocationHeader" "FastAllocator metadata must preserve native pointer alignment")
_require_contains("${_fast_allocator}" "alignas(void*) char mem[size]" "FastFixedAllocator chunk storage must preserve native pointer alignment")
_require_contains("${_fast_allocator}" "return header + 1" "FastAllocator user storage must begin after the native-aligned header")
_require_absent("${_fast_allocator}" "return ((unsigned int*)pMemory)+1" "FastAllocator regressed to a four-byte user-data prefix")

_read_repo_file("Core/GameEngine/Source/Common/System/GameMemory.cpp" _game_memory)
_require_contains("${_game_memory}" "#define MEM_BOUND_ALIGNMENT (sizeof(void *))" "GameMemory pool alignment must follow native pointer width")
_require_contains("${_game_memory}" "static size_t roundUpMemBound(size_t i)" "GameMemory native byte arithmetic must use size_t")
_require_contains("${_game_memory}" "reinterpret_cast<uintptr_t>(result)" "GameMemory alignment checks must not truncate pointers")
_require_absent("${_game_memory}" "#define MEM_BOUND_ALIGNMENT 4" "GameMemory pool alignment regressed to a Win32-only constant")
_require_absent("${_game_memory}" "if (unsigned(result)&3)" "GameMemory pointer alignment check regressed to 32-bit truncation")

_read_repo_file("Core/GameEngine/Include/GameClient/WindowVideoManager.h" _window_video)
_require_contains("${_window_video}" "std::hash<uintptr_t>" "pointer-key hashing must use native uintptr_t")
_require_absent("${_window_video}" "std::hash<UnsignedInt>" "pointer-key hashing regressed to a 32-bit logical integer")

_read_repo_file("Core/GameEngine/Source/GameClient/Input/Keyboard.cpp" _keyboard)
_require_contains("${_keyboard}" "reinterpret_cast<uintptr_t>(kLayout)" "HKL extraction must pass through native-width uintptr_t")
_require_absent("${_keyboard}" "(UnsignedInt)kLayout" "HKL extraction regressed to direct 32-bit handle truncation")

_read_repo_file("Core/GameEngine/Source/GameClient/GUI/Gadget/GadgetListBox.cpp" _listbox)
_require_contains("${_listbox}" "*(Int**)mData2 = list->selections" "multi-select pointer return must remain native pointer width")
_require_absent("${_listbox}" "*(Int*)mData2 = (Int)list->selections" "multi-select pointer return regressed to Int truncation")

_read_repo_file("Core/GameEngine/Source/GameNetwork/GameSpy/Chat.cpp" _chat)
_require_contains("${_chat}" "GadgetListBoxGetSelected(playerListbox, &selections);" "multi-select callers should use the native Int** contract")

# Step 04D2 modern C++ runtime ABI guard. The standard unsized global delete
# replacements must match <new>'s noexcept contract on current GCC/Clang/MSVC.
_read_repo_file("Core/Libraries/Source/WWVegas/WWLib/always.h" _ww_always)
_require_contains("${_ww_always}" "operator delete\t\t(void *p) noexcept;" "WWLib global operator delete must match the standard noexcept contract")
_require_contains("${_ww_always}" "operator delete[]\t(void *p) noexcept;" "WWLib global operator delete[] must match the standard noexcept contract")

_read_repo_file("Core/GameEngine/Include/Common/GameMemory.h" _game_memory_header)
_require_contains("${_game_memory_header}" "operator delete\t\t(void *p) noexcept;" "GameMemory global operator delete must match the standard noexcept contract")
_require_contains("${_game_memory_header}" "operator delete[]\t(void *p) noexcept;" "GameMemory global operator delete[] must match the standard noexcept contract")

_read_repo_file("Core/GameEngine/Include/Common/GameMemoryNull.h" _game_memory_null_header)
_require_contains("${_game_memory_null_header}" "operator delete(void *p) noexcept;" "GameMemoryNull global operator delete must match the standard noexcept contract")
_require_contains("${_game_memory_null_header}" "operator delete[](void *p) noexcept;" "GameMemoryNull global operator delete[] must match the standard noexcept contract")

_read_repo_file("Core/GameEngine/Source/Common/System/GameMemory.cpp" _game_memory_source)
_require_contains("${_game_memory_source}" "void operator delete(void *p) noexcept" "GameMemory delete definition must preserve noexcept")
_require_contains("${_game_memory_source}" "void operator delete[](void *p) noexcept" "GameMemory delete[] definition must preserve noexcept")

_read_repo_file("Core/GameEngine/Source/Common/System/GameMemoryNull.cpp" _game_memory_null_source)
_require_contains("${_game_memory_null_source}" "operator delete(void *p) noexcept" "GameMemoryNull delete definition must preserve noexcept")
_require_contains("${_game_memory_null_source}" "operator delete[](void *p) noexcept" "GameMemoryNull delete[] definition must preserve noexcept")

_read_repo_file("Core/Libraries/Source/WWVegas/WWStub/wwallocstub.cpp" _ww_alloc_stub)
_require_contains("${_ww_alloc_stub}" "void operator delete(void *p) noexcept" "WWStub delete definition must preserve noexcept")
_require_contains("${_ww_alloc_stub}" "void operator delete[](void *p) noexcept" "WWStub delete[] definition must preserve noexcept")

_require_contains("${_mempool}" "#ifdef DEBUG_CRASHING\n\tint block_count = 0;" "ObjectPool debug-only block counter must not trigger release -Werror builds")

message(STATUS "Step 04C runtime pointer-width source audit passed")
