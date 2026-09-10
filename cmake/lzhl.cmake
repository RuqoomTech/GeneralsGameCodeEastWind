set(LZHL_DIR ${CMAKE_CURRENT_BINARY_DIR}/_deps/lzhl-src/CompLibHeader)

FetchContent_Declare(
    lzhl_legacy_source
    GIT_REPOSITORY https://github.com/TheSuperHackers/lzhl-1.0
    GIT_TAG        dfd96e2ca64adaddb35dd4ebadd6add7d5586783
    SOURCE_DIR     ${LZHL_DIR}
    SOURCE_SUBDIR  __rts_source_only__
)
FetchContent_MakeAvailable(lzhl_legacy_source)

add_library(liblzhl STATIC)
target_sources(liblzhl PRIVATE
    "${LZHL_DIR}/_huff.h"
    "${LZHL_DIR}/_lz.h"
    "${LZHL_DIR}/_lzhl.h"
    "${LZHL_DIR}/Huff.cpp"
    "${LZHL_DIR}/Lz.cpp"
    "${LZHL_DIR}/Lzhl.cpp"
    "${LZHL_DIR}/lzhl.h"
)
target_include_directories(liblzhl SYSTEM PUBLIC ${LZHL_DIR} ${LZHL_DIR}/..)
