function(utilhttpclient_add_curl)
    # Function scope keeps vendor options (especially BUILD_TESTING) out of our build.
    set(BUILD_SHARED_LIBS ON)
    set(BUILD_STATIC_LIBS OFF)
    set(CURL_STATIC_CRT ON)
    set(CURL_ENABLE_SSL ON)
    set(CURL_USE_SCHANNEL ON)
    set(CURL_USE_OPENSSL OFF)
    set(CURL_DISABLE_INSTALL ON)
    set(CURL_ENABLE_EXPORT_TARGET OFF)
    foreach(option BUILD_CURL_EXE BUILD_EXAMPLES BUILD_TESTING BUILD_LIBCURL_DOCS
        BUILD_MISC_DOCS ENABLE_CURL_MANUAL ENABLE_THREADED_RESOLVER ENABLE_ARES
        PICKY_COMPILER CURL_ZLIB CURL_ZSTD CURL_BROTLI USE_LIBIDN2 USE_NGHTTP2
        CURL_USE_LIBPSL USE_LIBPSL CURL_USE_LIBSSH2 USE_LIBSSH2)
        set(${option} OFF)
    endforeach()
    # These three vendor macros use CACHE STRING rather than option(), so set
    # their cache values explicitly as well (CMP0126 is unset in this curl).
    foreach(dependency CURL_ZLIB CURL_ZSTD CURL_BROTLI)
        set(${dependency} OFF CACHE STRING "Disabled optional curl dependency" FORCE)
    endforeach()
    set(USE_WIN32_IDN ON)
    add_subdirectory("${CURL_SOURCE_PATH}" "${CMAKE_CURRENT_BINARY_DIR}/thirdparty/curl" EXCLUDE_FROM_ALL)
    # Group the vendored curl libraries under one IDE filter.
    foreach(curl_target libcurl_shared libcurl_object)
        if(TARGET ${curl_target})
            set_target_properties(${curl_target} PROPERTIES FOLDER "libcurl")
        endif()
    endforeach()
    if(NOT TARGET CURL::libcurl OR NOT TARGET libcurl_shared)
        message(FATAL_ERROR "The pinned curl must provide CURL::libcurl and libcurl_shared.")
    endif()
    target_compile_options(libcurl_shared PRIVATE /Zi)
    target_link_options(libcurl_shared PRIVATE /DEBUG)
endfunction()
