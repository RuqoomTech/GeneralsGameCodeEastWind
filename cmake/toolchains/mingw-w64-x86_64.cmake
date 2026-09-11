# Step 04 x64 bring-up toolchain. Initially used by the focused readiness/test lane;
# the full legacy D3D8 runtime remains intentionally blocked until its platform and
# renderer dependencies are separated from the engine core.
set(RTS_MINGW_ARCH "x86_64")
include("${CMAKE_CURRENT_LIST_DIR}/mingw-w64-common.cmake")
