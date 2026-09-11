/*
** Command & Conquer Generals / Zero Hour
** Deterministic floating-point control shared by the Evolution x64 migration.
*/

#include "GameLogic/FPUControl.h"

#if defined(_WIN32)
#include <float.h>
#endif

#if !defined(_MSC_VER) || _MSC_VER >= 1300
#include <fenv.h>
#endif

void setFPMode()
{
#if defined(_WIN32) && (defined(_M_IX86) || defined(__i386__))
    // Preserve the signed-off 32-bit oracle behavior. The original game forces
    // x87 to round-to-nearest with 24-bit precision before deterministic logic.
    _fpreset();
    unsigned int current = _statusfp();
    unsigned int desired = current;
    desired = (desired & ~_MCW_RC) | (_RC_NEAR & _MCW_RC);
    desired = (desired & ~_MCW_PC) | (_PC_24 & _MCW_PC);
    _controlfp(desired, _MCW_PC | _MCW_RC);
#else
    // x64 uses SSE/SSE2 floating point and has no x87 precision-width contract.
    // Keep the deterministic contract to the rounding mode and compiler policy;
    // do not attempt to emulate the legacy process ABI or widen Real.
#if defined(_WIN32)
    _fpreset();
    _controlfp(_RC_NEAR, _MCW_RC);
#endif
#if !defined(_MSC_VER) || _MSC_VER >= 1300
    (void)fesetround(FE_TONEAREST);
#endif
#endif
}
