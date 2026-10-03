cmake_minimum_required(VERSION 3.21)
cmake_policy(SET CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
project(UtilHTTPClient_libcurl LANGUAGES C CXX RC)

if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "UtilHTTPClient_libcurl requires Windows MSVC x86. Configure with -A Win32.")
endif()
get_property(multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(multi_config)
    set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "Supported configurations" FORCE)
elseif(NOT CMAKE_BUILD_TYPE MATCHES "^(Debug|Release)$")
    message(FATAL_ERROR "Select Debug or Release.")
endif()

include(CTest)
include(cmake/Dependencies.cmake)
utilhttpclient_prepare_dependencies()
include("${VC_LTL_Root}/VC-LTL helper for cmake.cmake")
include(cmake/Curl.cmake)
utilhttpclient_add_curl()

add_library(utilhttpclient_settings INTERFACE)
target_compile_features(utilhttpclient_settings INTERFACE cxx_std_20)
target_include_directories(utilhttpclient_settings INTERFACE
    "${PROJECT_SOURCE_DIR}/include/Interface"
    "${METAHOOK_SOURCE_PATH}/include/HLSDK/common")
target_compile_definitions(utilhttpclient_settings INTERFACE
    WIN32 _WINDOWS UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS
    $<$<CONFIG:Debug>:_DEBUG> $<$<CONFIG:Release>:NDEBUG>)
target_compile_options(utilhttpclient_settings INTERFACE /W3 /permissive- /wd4819 /Zi)

add_library(UtilHTTPClient_libcurl SHARED
    src/UtilHTTPClient_libcurl.cpp src/dllmain.cpp
    "${METAHOOK_SOURCE_PATH}/include/HLSDK/common/interface.cpp")
target_link_libraries(UtilHTTPClient_libcurl PRIVATE utilhttpclient_settings CURL::libcurl)
target_include_directories(UtilHTTPClient_libcurl PRIVATE "${SCOPEEXIT_SOURCE_PATH}/include")
target_compile_definitions(UtilHTTPClient_libcurl PRIVATE UTILHTTPCLIENT_LIBCURL_EXPORTS _USRDLL)
target_compile_options(UtilHTTPClient_libcurl PRIVATE /MP /sdl $<$<CONFIG:Release>:/Gy;/Oi>)
target_link_options(UtilHTTPClient_libcurl PRIVATE /DEBUG /SUBSYSTEM:WINDOWS /MANIFESTUAC:NO
    $<$<CONFIG:Debug>:/INCREMENTAL>
    $<$<CONFIG:Release>:/INCREMENTAL:NO;/OPT:REF;/OPT:ICF>)
set_target_properties(UtilHTTPClient_libcurl PROPERTIES
    DEBUG_POSTFIX "" INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)

install(TARGETS UtilHTTPClient_libcurl RUNTIME DESTINATION svencoop/metahook/dlls)
install(FILES "$<TARGET_PDB_FILE:UtilHTTPClient_libcurl>" DESTINATION svencoop/metahook/dlls)
install(FILES "$<TARGET_FILE:libcurl_shared>" DESTINATION .)
install(FILES include/Interface/IUtilHTTPClient.h DESTINATION include/Interface)
install(FILES "${METAHOOK_SOURCE_PATH}/include/HLSDK/common/interface.h" DESTINATION include/HLSDK/common)
install(FILES README.md README.zh-CN.md LICENSE THIRD-PARTY-NOTICES.md DESTINATION .)
install(FILES "${CURL_SOURCE_PATH}/COPYING" DESTINATION licenses/curl)
install(FILES "${SCOPEEXIT_SOURCE_PATH}/LICENSE" DESTINATION licenses/ScopeExit)
install(FILES "${METAHOOK_SOURCE_PATH}/LICENSE" DESTINATION licenses/MetaHook)
install(FILES "${METAHOOK_SOURCE_PATH}/include/HLSDK/common/interface.h" DESTINATION licenses/HLSDK)
install(FILES licenses/VC-LTL/LICENSE "${VC_LTL_Root}/Readme.md" DESTINATION licenses/VC-LTL)

if(BUILD_TESTING)
    add_executable(UtilHTTPClientTests tests/RegressionTests.cpp)
    target_link_libraries(UtilHTTPClientTests PRIVATE utilhttpclient_settings ws2_32)
    target_include_directories(UtilHTTPClientTests PRIVATE "${CURL_SOURCE_PATH}/include")
    target_compile_definitions(UtilHTTPClientTests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    set_target_properties(UtilHTTPClientTests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tests")
    add_dependencies(UtilHTTPClientTests UtilHTTPClient_libcurl)
    foreach(scenario Factory Url SyncGet Post AsyncPool Stream HttpError TransportError TLS)
        add_test(NAME ${scenario} COMMAND UtilHTTPClientTests
            "$<TARGET_FILE:UtilHTTPClient_libcurl>" "$<TARGET_FILE:libcurl_shared>" ${scenario})
        set_tests_properties(${scenario} PROPERTIES TIMEOUT 30)
    endforeach()
endif()
