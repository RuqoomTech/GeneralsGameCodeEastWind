/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 TheSuperHackers
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include "imagehlp_adapter.h"

// This static class loads and unloads dbghelp.dll at runtime.
//
// The dbghelp functions declared in the DbgHelp namespace below are implemented in this library
// and forward to the matching export of the loaded module, so they can be called almost as if
// dbghelp.dll was linked statically. When the module is not loaded, every function fails with a
// neutral return value.
//
// Is thread safe, because dbghelp.dll is not thread safe for the most part. Every call to load
// needs a paired call to unload, no matter if the load was successful. Internally it must not use
// new and delete because it can be used during game memory initialization.

class DbgHelpLoader
{
public:

	// Returns whether dbghelp.dll is loaded
	static bool isLoaded();

	// Returns whether dbghelp.dll is loaded from the system directory
	static bool isLoadedFromSystem();

	// Returns whether dbghelp.dll was attempted to be loaded but failed
	static bool isFailed();

	static bool load();
	static void unload();
};

// The dbghelp functions are declared in a namespace, because they cannot take the global names of
// the declarations in imagehlp.h. Other libraries such as Tracy link dbghelp.lib, which already
// defines the global names, so defining them here too would fail to link with duplicate symbols.
// Calling the unqualified global names instead would bypass this loader.
namespace DbgHelp
{

BOOL WINAPI SymInitialize(
	HANDLE hProcess,
	PCSTR UserSearchPath,
	BOOL fInvadeProcess);

BOOL WINAPI SymCleanup(
	HANDLE hProcess);

// The 32-bit address API below is not available in the x64 process. On Win64 the
// Windows headers rename these functions and their types to the 64-bit variants,
// so the x64 runtime uses the explicit native-width functions further down.
#if !defined(_WIN64)

DWORD WINAPI SymLoadModule(
	HANDLE hProcess,
	HANDLE hFile,
	PCSTR ImageName,
	PCSTR ModuleName,
	DWORD BaseOfDll,
	DWORD SizeOfDll);

DWORD WINAPI SymGetModuleBase(
	HANDLE hProcess,
	DWORD dwAddr);

BOOL WINAPI SymUnloadModule(
	HANDLE hProcess,
	DWORD BaseOfDll);

BOOL WINAPI SymGetSymFromAddr(
	HANDLE hProcess,
	DWORD dwAddr,
	PDWORD pdwDisplacement,
	PIMAGEHLP_SYMBOL Symbol);

BOOL WINAPI SymGetLineFromAddr(
	HANDLE hProcess,
	DWORD dwAddr,
	PDWORD pdwDisplacement,
	PIMAGEHLP_LINE Line);

#endif // !defined(_WIN64)

DWORD WINAPI SymSetOptions(
	DWORD SymOptions);

#if !defined(_WIN64)

PVOID WINAPI SymFunctionTableAccess(
	HANDLE hProcess,
	DWORD AddrBase);

BOOL WINAPI StackWalk(
	DWORD MachineType,
	HANDLE hProcess,
	HANDLE hThread,
	LPSTACKFRAME StackFrame,
	PVOID ContextRecord,
	PREAD_PROCESS_MEMORY_ROUTINE ReadMemoryRoutine,
	PFUNCTION_TABLE_ACCESS_ROUTINE FunctionTableAccessRoutine,
	PGET_MODULE_BASE_ROUTINE GetModuleBaseRoutine,
	PTRANSLATE_ADDRESS_ROUTINE TranslateAddress);

#else // defined(_WIN64)

// Native-width dbghelp entry points used by the Evolution x64 crash and stack dump paths.
// Addresses stay DWORD64 end to end; they are never narrowed to DWORD.

BOOL WINAPI SymFromAddr(
	HANDLE hProcess,
	DWORD64 Address,
	PDWORD64 Displacement,
	PSYMBOL_INFO Symbol);

BOOL WINAPI SymGetLineFromAddr64(
	HANDLE hProcess,
	DWORD64 Address,
	PDWORD Displacement,
	PIMAGEHLP_LINE64 Line);

PVOID WINAPI SymFunctionTableAccess64(
	HANDLE hProcess,
	DWORD64 AddrBase);

DWORD64 WINAPI SymGetModuleBase64(
	HANDLE hProcess,
	DWORD64 Address);

BOOL WINAPI StackWalk64(
	DWORD MachineType,
	HANDLE hProcess,
	HANDLE hThread,
	LPSTACKFRAME64 StackFrame,
	PVOID ContextRecord,
	PREAD_PROCESS_MEMORY_ROUTINE64 ReadMemoryRoutine,
	PFUNCTION_TABLE_ACCESS_ROUTINE64 FunctionTableAccessRoutine,
	PGET_MODULE_BASE_ROUTINE64 GetModuleBaseRoutine,
	PTRANSLATE_ADDRESS_ROUTINE64 TranslateAddress);

#endif // !defined(_WIN64)

BOOL WINAPI MiniDumpWriteDump(
	HANDLE hProcess,
	DWORD ProcessId,
	HANDLE hFile,
	MINIDUMP_TYPE DumpType,
	PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,
	PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
	PMINIDUMP_CALLBACK_INFORMATION CallbackParam);

} // namespace DbgHelp
