set(METAHOOK_SOURCE_PATH "$ENV{METAHOOK_SOURCE_PATH}" CACHE PATH "MetaHook source tree; empty fetches the pinned SDK")
set(CURL_SOURCE_PATH "$ENV{CURL_SOURCE_PATH}" CACHE PATH "Clean curl Git checkout at the required commit; empty fetches it")
set(SCOPEEXIT_SOURCE_PATH "$ENV{SCOPEEXIT_SOURCE_PATH}" CACHE PATH "ScopeExit source tree; empty fetches the pinned commit")
set(UTILHTTPCLIENT_CURL_COMMIT "4f95f327093bef29a4f8fe188edc6c95f49980a5")

function(utilhttpclient_require_files name source)
    foreach(required IN LISTS ARGN)
        if(NOT EXISTS "${source}/${required}" OR IS_DIRECTORY "${source}/${required}")
            message(FATAL_ERROR "${name} is missing ${required}: ${source}")
        endif()
    endforeach()
endfunction()

function(utilhttpclient_validate_curl source)
    find_package(Git REQUIRED)
    file(REAL_PATH "${source}" source_root)
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${source}" rev-parse --show-toplevel
        RESULT_VARIABLE result OUTPUT_VARIABLE git_root ERROR_VARIABLE error OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "CURL_SOURCE_PATH must be a Git checkout: ${error}")
    endif()
    file(REAL_PATH "${git_root}" git_root)
    string(TOLOWER "${source_root}" source_root)
    string(TOLOWER "${git_root}" git_root)
    if(NOT source_root STREQUAL git_root)
        message(FATAL_ERROR "CURL_SOURCE_PATH must point to the curl Git root: ${source}")
    endif()
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${source}" rev-parse HEAD
        RESULT_VARIABLE result OUTPUT_VARIABLE commit OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT result EQUAL 0 OR NOT commit STREQUAL UTILHTTPCLIENT_CURL_COMMIT)
        message(FATAL_ERROR "curl must be pinned to ${UTILHTTPCLIENT_CURL_COMMIT}; found '${commit}' at ${source}")
    endif()
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${source}" diff --quiet HEAD --
        RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "curl checkout has tracked changes: ${source}. Use a clean checkout of ${UTILHTTPCLIENT_CURL_COMMIT}.")
    endif()
    message(STATUS "Verified curl commit: ${commit}")
endfunction()

function(utilhttpclient_fetch_source name url commit out_var)
    include(FetchContent)
    FetchContent_Declare(${name}
        GIT_REPOSITORY "${url}" GIT_TAG "${commit}"
        GIT_SUBMODULES "" GIT_SUBMODULES_RECURSE FALSE
        # Populate only; apply curl options before configuring curl.
        SOURCE_SUBDIR _utilhttpclient_source_only)
    FetchContent_MakeAvailable(${name})
    set(${out_var} "${${name}_SOURCE_DIR}" PARENT_SCOPE)
endfunction()

function(utilhttpclient_prepare_dependencies)
    set(metahook_files include/HLSDK/common/interface.h include/HLSDK/common/interface.cpp LICENSE)
    set(curl_files CMakeLists.txt include/curl/curl.h COPYING)
    set(scopeexit_files include/ScopeExit/ScopeExit.h LICENSE)

    # Reject invalid explicit paths before downloading anything. Never modify them.
    foreach(dependency METAHOOK CURL SCOPEEXIT)
        if(${dependency}_SOURCE_PATH)
            get_filename_component(source "${${dependency}_SOURCE_PATH}" ABSOLUTE BASE_DIR "${PROJECT_SOURCE_DIR}")
            string(TOLOWER "${dependency}" lower_name)
            utilhttpclient_require_files("${dependency}_SOURCE_PATH" "${source}" ${${lower_name}_files})
            set(${dependency}_SOURCE_PATH "${source}")
        endif()
    endforeach()
    if(CURL_SOURCE_PATH)
        utilhttpclient_validate_curl("${CURL_SOURCE_PATH}")
    endif()

    if(NOT METAHOOK_SOURCE_PATH)
        utilhttpclient_fetch_source(utilhttpclient_metahook
            https://github.com/MetaHookSv/MetaHook
            4d23b6fecd79dc949aabc2e145480cd1328d4a35 METAHOOK_SOURCE_PATH)
    endif()
    if(NOT CURL_SOURCE_PATH)
        utilhttpclient_fetch_source(utilhttpclient_curl https://github.com/curl/curl
            "${UTILHTTPCLIENT_CURL_COMMIT}" CURL_SOURCE_PATH)
        utilhttpclient_validate_curl("${CURL_SOURCE_PATH}")
    endif()
    if(NOT SCOPEEXIT_SOURCE_PATH)
        utilhttpclient_fetch_source(utilhttpclient_scopeexit
            https://github.com/SergiusTheBest/ScopeExit
            bd345da594a4675d04de663d93d00cb81b6678b2 SCOPEEXIT_SOURCE_PATH)
    endif()
    foreach(dependency METAHOOK CURL SCOPEEXIT)
        string(TOLOWER "${dependency}" lower_name)
        utilhttpclient_require_files("${dependency}_SOURCE_PATH" "${${dependency}_SOURCE_PATH}" ${${lower_name}_files})
        set(${dependency}_SOURCE_PATH "${${dependency}_SOURCE_PATH}" PARENT_SCOPE)
        message(STATUS "${dependency}_SOURCE_PATH: ${${dependency}_SOURCE_PATH}")
    endforeach()
endfunction()
