if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "The native browser host requires Windows x64")
endif()

# Official, immutable SDK package. The x64 loader is loaded through its C export;
# this avoids coupling MinGW to an MSVC import library.
include(FetchContent)
FetchContent_Declare(webview2_sdk
    URL https://api.nuget.org/v3-flatcontainer/microsoft.web.webview2/1.0.4258.31/microsoft.web.webview2.1.0.4258.31.nupkg
    URL_HASH SHA256=56f7f4b8bf9aee4b8efefbbdd4f67d5f74ebd1b100ed0806da71bf76af481aa9
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(webview2_sdk)
add_library(core_browserhost STATIC browserhost.cpp browserhost.h)
target_compile_features(core_browserhost PRIVATE cxx_std_17)
target_include_directories(core_browserhost PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(core_browserhost SYSTEM PRIVATE "${webview2_sdk_SOURCE_DIR}/build/native/include")
target_link_libraries(core_browserhost PUBLIC ole32 oleaut32 user32 uuid urlmon)
set_property(TARGET core_browserhost PROPERTY RTS_BROWSER_LOADER "${webview2_sdk_SOURCE_DIR}/build/native/x64/WebView2Loader.dll")

function(rts_stage_browser_loader target_name)
    get_target_property(_loader core_browserhost RTS_BROWSER_LOADER)
    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${_loader}"
            "$<TARGET_FILE_DIR:${target_name}>/WebView2Loader.dll"
        VERBATIM)
endfunction()
