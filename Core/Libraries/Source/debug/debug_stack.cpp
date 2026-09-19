/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
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

/////////////////////////////////////////////////////////////////////////EA-V1
// $File: //depot/GeneralsMD/Staging/code/Libraries/Source/debug/debug_stack.cpp $
// $Author: KMorness $
// $Revision: #2 $
// $DateTime: 2005/01/19 15:02:33 $
//
// (c) 2003 Electronic Arts
//
// Stack walker
//////////////////////////////////////////////////////////////////////////////

#include "debug.h"
#include "debug_stack.h"
#include <windows.h>
#include "WWLib/stringex.h"
#include <imagehlp.h>
#include <cinttypes>
#include <cstdint>

// Definitions to allow run-time linking to the dbghelp.dll functions.

#define DBGHELP(name,ret,par) typedef ret (WINAPI *name##Type) par;
#include "debug_stack.inl"
#undef DBGHELP

#define DBGHELP(name,ret,par) name##Type _##name;
static union
{
  struct
  {
#include "debug_stack.inl"
  };
  FARPROC funcPtr[1];
} gDbg;
#undef DBGHELP

#define DBGHELP(name,ret,par) #name,
static char const *const DebughelpFunctionNames[] =
{
#include "debug_stack.inl"
	nullptr
};
#undef DBGHELP

// local dbghelp.dll module handle
static HMODULE g_dbghelp;

// local flag that is true if we're using an old dbghelp.dll version
static bool g_oldDbghelp;

static void InitDbghelp()
{
  // already called?
  if (g_dbghelp)
    return;

	// firstly check for dbghelp.dll in the EXE directory
	char dbgHelpPath[256];
	if (GetModuleFileName(nullptr,dbgHelpPath,sizeof(dbgHelpPath)))
	{
		char *slash=strrchr(dbgHelpPath,'\\');
		if (slash)
		{
			strcpy(slash+1,"DBGHELP.DLL");
			g_dbghelp=::LoadLibrary(dbgHelpPath);
		}
	}
	if (!g_dbghelp)
		// load any version we can
		g_dbghelp=::LoadLibrary("DBGHELP.DLL");

  if (!g_dbghelp)
    return;

  // Get function addresses
  FARPROC *funcptr=gDbg.funcPtr;
  unsigned k=0;
  for (;DebughelpFunctionNames[k];++k,++funcptr)
  {
    *funcptr=GetProcAddress(g_dbghelp,DebughelpFunctionNames[k]);
    if (!*funcptr)
      break;
  }
  if (DebughelpFunctionNames[k])
  {
    // not all functions found -> clear them all
    while (funcptr!=gDbg.funcPtr)
      *--funcptr=0;
  }
  else
  {
    // Set options
    gDbg._SymSetOptions(gDbg._SymGetOptions()|SYMOPT_DEFERRED_LOADS|SYMOPT_LOAD_LINES);

    // Init module
    gDbg._SymInitialize(GetCurrentProcess(),nullptr,TRUE);

    // Check: are we using a newer version of dbghelp.dll?
    // (older versions have some serious issues.. err... bugs)
    if (!GetProcAddress(g_dbghelp,"SymEnumSymbolsForAddr"))
      g_oldDbghelp=true;
  }
}

//////////////////////////////////////////////////////////////////////////////

DebugStackwalk::Signature::Signature(const Signature &src)
{
  *this=src;
}

DebugStackwalk::Signature& DebugStackwalk::Signature::operator=(const Signature& src)
{
  if (&src!=this)
  {
    m_numAddr=src.m_numAddr;
    memcpy(m_addr,src.m_addr,m_numAddr*sizeof(*m_addr));
  }
  return *this;
}

std::uintptr_t DebugStackwalk::Signature::GetAddress(int n) const
{
  DFAIL_IF_MSG(n<0||n>=MAX_ADDR,n << "/" << MAX_ADDR) return 0;
  return m_addr[n];
}

void DebugStackwalk::Signature::GetSymbol(std::uintptr_t addr, char *buf, unsigned bufSize)
{
  DFAIL_IF(!buf) return;
  DFAIL_IF(bufSize<64||bufSize>=0x80000000) return;

  InitDbghelp();

  char *bufEnd=buf+bufSize;
  *buf=0;
#if defined(_WIN64)
  buf+=sprintf(buf,"%016llx",static_cast<unsigned long long>(addr));
  DWORD64 modBase=gDbg._SymGetModuleBase64(GetCurrentProcess(),static_cast<DWORD64>(addr));
#else
  buf+=sprintf(buf,"%08x",static_cast<unsigned>(addr));
  DWORD modBase=gDbg._SymGetModuleBase(GetCurrentProcess(),static_cast<DWORD>(addr));
#endif
  if (!modBase)
  {
    strcpy(buf," (unknown module)");
    return;
  }

  char symbolBuffer[sizeof(SYMBOL_INFO)+MAX_SYM_NAME+1];
  char moduleBuffer[MAX_PATH];
  GetModuleFileName(reinterpret_cast<HMODULE>(static_cast<std::uintptr_t>(modBase)),moduleBuffer,sizeof(moduleBuffer));

  char *p=strrchr(moduleBuffer,'\\');
  p=p?p+1:moduleBuffer;
  *buf++=' ';
  strcpy(buf,p);
  buf+=strlen(buf);
  if (bufEnd-buf<32)
    return;
  buf+=sprintf(buf,"+0x%llx",static_cast<unsigned long long>(addr-static_cast<std::uintptr_t>(modBase)));

#if defined(_WIN64)
  PSYMBOL_INFO symPtr=reinterpret_cast<PSYMBOL_INFO>(symbolBuffer);
  memset(symPtr,0,sizeof(symbolBuffer));
  symPtr->SizeOfStruct=sizeof(SYMBOL_INFO);
  symPtr->MaxNameLen=MAX_SYM_NAME;
  DWORD64 displacement=0;
  if (!gDbg._SymFromAddr(GetCurrentProcess(),static_cast<DWORD64>(addr),&displacement,symPtr))
    return;
#else
  PIMAGEHLP_SYMBOL symPtr=reinterpret_cast<PIMAGEHLP_SYMBOL>(symbolBuffer);
  memset(symPtr,0,sizeof(symbolBuffer));
  symPtr->SizeOfStruct=sizeof(IMAGEHLP_SYMBOL);
  symPtr->MaxNameLength=sizeof(symbolBuffer)-sizeof(IMAGEHLP_SYMBOL);
  DWORD displacement=0;
  if (!gDbg._SymGetSymFromAddr(GetCurrentProcess(),static_cast<DWORD>(addr),&displacement,symPtr))
    return;
#endif
  if (static_cast<unsigned>(bufEnd-buf)<strlen(symPtr->Name)+24)
    return;
  buf+=sprintf(buf,", %s+0x%llx",symPtr->Name,static_cast<unsigned long long>(displacement));

#if defined(_WIN64)
  IMAGEHLP_LINE64 line;
#else
  IMAGEHLP_LINE line;
#endif
  memset(&line,0,sizeof(line));
  line.SizeOfStruct=sizeof(line);
  DWORD lineDisplacement=0;
#if defined(_WIN64)
  if (!gDbg._SymGetLineFromAddr64(GetCurrentProcess(),static_cast<DWORD64>(addr),&lineDisplacement,&line))
#else
  if (!gDbg._SymGetLineFromAddr(GetCurrentProcess(),static_cast<DWORD>(addr),&lineDisplacement,&line))
#endif
    return;

  p=strrchr(line.FileName,'\\');
  p=p?p+1:line.FileName;
  if (static_cast<unsigned>(bufEnd-buf)<strlen(p)+24)
    return;
  sprintf(buf,", %s:%lu+0x%lx",p,static_cast<unsigned long>(line.LineNumber),static_cast<unsigned long>(lineDisplacement));
}

void DebugStackwalk::Signature::GetSymbol(std::uintptr_t addr,
                                          char *bufMod, unsigned sizeMod, std::uintptr_t *relMod,
                                          char *bufSym, unsigned sizeSym, std::uintptr_t *relSym,
                                          char *bufFile, unsigned sizeFile, unsigned *linePtr, std::uintptr_t *relLine)
{
  InitDbghelp();

  if (bufMod) *bufMod=0;
  if (relMod) *relMod=0;
  if (bufSym) *bufSym=0;
  if (relSym) *relSym=0;
  if (bufFile) *bufFile=0;
  if (linePtr) *linePtr=0;
  if (relLine) *relLine=0;

  DFAIL_IF(bufMod&&sizeMod<16) return;
  DFAIL_IF(bufSym&&sizeSym<16) return;
  DFAIL_IF(bufFile&&sizeFile<16) return;

#if defined(_WIN64)
  DWORD64 modBase=gDbg._SymGetModuleBase64(GetCurrentProcess(),static_cast<DWORD64>(addr));
#else
  DWORD modBase=gDbg._SymGetModuleBase(GetCurrentProcess(),static_cast<DWORD>(addr));
#endif
  if (!modBase)
  {
    if (bufMod) strcpy(bufMod,"(unknown mod)");
    if (bufSym) strcpy(bufSym,"(unknown)");
    return;
  }

  char symbolBuffer[sizeof(SYMBOL_INFO)+MAX_SYM_NAME+1];
  char moduleBuffer[MAX_PATH];
  if (bufMod)
  {
    GetModuleFileName(reinterpret_cast<HMODULE>(static_cast<std::uintptr_t>(modBase)),moduleBuffer,sizeof(moduleBuffer));
    char *p=strrchr(moduleBuffer,'\\');
    p=p?p+1:moduleBuffer;
    strlcpy(bufMod,p,sizeMod);
  }
  if (relMod) *relMod=addr-static_cast<std::uintptr_t>(modBase);

  if (bufSym)
  {
#if defined(_WIN64)
    PSYMBOL_INFO symPtr=reinterpret_cast<PSYMBOL_INFO>(symbolBuffer);
    memset(symPtr,0,sizeof(symbolBuffer));
    symPtr->SizeOfStruct=sizeof(SYMBOL_INFO);
    symPtr->MaxNameLen=MAX_SYM_NAME;
    DWORD64 displacement=0;
    if (gDbg._SymFromAddr(GetCurrentProcess(),static_cast<DWORD64>(addr),&displacement,symPtr))
#else
    PIMAGEHLP_SYMBOL symPtr=reinterpret_cast<PIMAGEHLP_SYMBOL>(symbolBuffer);
    memset(symPtr,0,sizeof(symbolBuffer));
    symPtr->SizeOfStruct=sizeof(IMAGEHLP_SYMBOL);
    symPtr->MaxNameLength=sizeof(symbolBuffer)-sizeof(IMAGEHLP_SYMBOL);
    DWORD displacement=0;
    if (gDbg._SymGetSymFromAddr(GetCurrentProcess(),static_cast<DWORD>(addr),&displacement,symPtr))
#endif
    {
      strlcpy(bufSym,symPtr->Name,sizeSym);
      if (relSym) *relSym=static_cast<std::uintptr_t>(displacement);
    }
    else
      strcpy(bufSym,"(unknown)");
  }

  if (bufFile)
  {
#if defined(_WIN64)
    IMAGEHLP_LINE64 line;
#else
    IMAGEHLP_LINE line;
#endif
    memset(&line,0,sizeof(line));
    line.SizeOfStruct=sizeof(line);
    DWORD displacement=0;
#if defined(_WIN64)
    if (!gDbg._SymGetLineFromAddr64(GetCurrentProcess(),static_cast<DWORD64>(addr),&displacement,&line))
#else
    if (!gDbg._SymGetLineFromAddr(GetCurrentProcess(),static_cast<DWORD>(addr),&displacement,&line))
#endif
      strcpy(bufFile,"(unknown)");
    else
    {
      char *p=strrchr(line.FileName,'\\');
      p=p?p+1:line.FileName;
      strlcpy(bufFile,p,sizeFile);
      if (linePtr) *linePtr=line.LineNumber;
      if (relLine) *relLine=static_cast<std::uintptr_t>(displacement);
    }
  }
}

Debug& operator<<(Debug &dbg, const DebugStackwalk::Signature &sig)
{
  dbg << sig.Size() << " addresses:\n";

  for (unsigned k=0;k<sig.Size();k++)
  {
    char buf[512];
    sig.GetSymbol(sig.GetAddress(k),buf,sizeof(buf));
    dbg << buf << "\n";
  }

  return dbg;
}

//////////////////////////////////////////////////////////////////////////////

DebugStackwalk::DebugStackwalk()
{
  // it doesn't harm to do this here
  InitDbghelp();
}

DebugStackwalk::~DebugStackwalk()
{
}

void *DebugStackwalk::GetDbghelpHandle()
{
  return g_dbghelp;
}

bool DebugStackwalk::IsOldDbghelp()
{
  return g_oldDbghelp;
}

int DebugStackwalk::StackWalk(Signature &sig, struct _CONTEXT *ctx)
{
  InitDbghelp();
  sig.m_numAddr=0;

#if defined(_WIN64)
  if (!gDbg._StackWalk64)
    return 0;

  CONTEXT localContext;
  if (ctx)
    localContext=*ctx;
  else
    RtlCaptureContext(&localContext);

  STACKFRAME64 stackFrame;
  memset(&stackFrame,0,sizeof(stackFrame));
  stackFrame.AddrPC.Mode=AddrModeFlat;
  stackFrame.AddrStack.Mode=AddrModeFlat;
  stackFrame.AddrFrame.Mode=AddrModeFlat;
  stackFrame.AddrPC.Offset=localContext.Rip;
  stackFrame.AddrStack.Offset=localContext.Rsp;
  stackFrame.AddrFrame.Offset=localContext.Rbp;

  bool skipFirst=!ctx;
  while (sig.m_numAddr<Signature::MAX_ADDR &&
         gDbg._StackWalk64(IMAGE_FILE_MACHINE_AMD64,GetCurrentProcess(),GetCurrentThread(),
                           &stackFrame,&localContext,nullptr,gDbg._SymFunctionTableAccess64,
                           gDbg._SymGetModuleBase64,nullptr))
  {
    if (!stackFrame.AddrPC.Offset)
      break;
    if (skipFirst)
      skipFirst=false;
    else
      sig.m_addr[sig.m_numAddr++]=static_cast<std::uintptr_t>(stackFrame.AddrPC.Offset);
  }
#else
  if (!gDbg._StackWalk)
    return 0;

  STACKFRAME stackFrame;
  memset(&stackFrame,0,sizeof(stackFrame));
  stackFrame.AddrPC.Mode=AddrModeFlat;
  stackFrame.AddrStack.Mode=AddrModeFlat;
  stackFrame.AddrFrame.Mode=AddrModeFlat;

  CONTEXT localContext;
  if (ctx)
    localContext=*ctx;
  else
  {
    RtlCaptureContext(&localContext);
    ctx=&localContext;
  }
  stackFrame.AddrPC.Offset=ctx->Eip;
  stackFrame.AddrStack.Offset=ctx->Esp;
  stackFrame.AddrFrame.Offset=ctx->Ebp;

  bool skipFirst=(ctx==&localContext);
  while (sig.m_numAddr<Signature::MAX_ADDR &&
         gDbg._StackWalk(IMAGE_FILE_MACHINE_I386,GetCurrentProcess(),GetCurrentThread(),
                         &stackFrame,ctx,nullptr,gDbg._SymFunctionTableAccess,gDbg._SymGetModuleBase,nullptr))
  {
    if (!stackFrame.AddrPC.Offset)
      break;
    if (skipFirst)
      skipFirst=false;
    else
      sig.m_addr[sig.m_numAddr++]=static_cast<std::uintptr_t>(stackFrame.AddrPC.Offset);
  }
#endif
  return sig.m_numAddr;
}
