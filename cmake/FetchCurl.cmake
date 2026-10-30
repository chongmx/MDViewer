# Downloads and builds a minimal static libcurl (HTTP/HTTPS only) with mbedTLS inside the
# build directory, for Linux/macOS hosts without the libcurl development package.
# Sets MDV_CURL_TARGET to the target to link.
include(FetchContent)

set(MDV_MBEDTLS_VERSION 3.6.2)
set(MDV_CURL_VERSION 8.11.1)

# ---- mbedTLS ----------------------------------------------------------------------------------
set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(MBEDTLS_FATAL_WARNINGS OFF CACHE BOOL "" FORCE)
set(USE_SHARED_MBEDTLS_LIBRARY OFF CACHE BOOL "" FORCE)
set(USE_STATIC_MBEDTLS_LIBRARY ON CACHE BOOL "" FORCE)
FetchContent_Declare(mbedtls
    URL https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-${MDV_MBEDTLS_VERSION}/mbedtls-${MDV_MBEDTLS_VERSION}.tar.bz2
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(mbedtls)

# curl's FindMbedTLS accepts these hints. They must be file paths, not target names, because
# curl exports its targets and an export cannot reference mbedTLS targets.
set(_mdv_mbedtls_lib_dir ${mbedtls_BINARY_DIR}/library)
macro(_mdv_static_lib var name)
    set(${var} ${_mdv_mbedtls_lib_dir}/${CMAKE_STATIC_LIBRARY_PREFIX}${name}${CMAKE_STATIC_LIBRARY_SUFFIX} CACHE FILEPATH "" FORCE)
endmacro()
set(MBEDTLS_INCLUDE_DIR ${mbedtls_SOURCE_DIR}/include CACHE PATH "" FORCE)
_mdv_static_lib(MBEDTLS_LIBRARY mbedtls)
_mdv_static_lib(MBEDX509_LIBRARY mbedx509)
_mdv_static_lib(MBEDCRYPTO_LIBRARY mbedcrypto)

# ---- curl -------------------------------------------------------------------------------------
foreach(option BUILD_SHARED_LIBS BUILD_CURL_EXE BUILD_TESTING BUILD_LIBCURL_DOCS BUILD_MISC_DOCS ENABLE_CURL_MANUAL
               CURL_USE_OPENSSL CURL_USE_LIBPSL CURL_USE_LIBSSH2 USE_LIBIDN2 CURL_ZLIB CURL_BROTLI CURL_ZSTD
               USE_NGHTTP2 CURL_USE_GSSAPI CURL_DISABLE_INSTALL_DOCS PICKY_COMPILER)
    set(${option} OFF CACHE BOOL "" FORCE)
endforeach()
set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
set(HTTP_ONLY ON CACHE BOOL "" FORCE)
set(CURL_USE_MBEDTLS ON CACHE BOOL "" FORCE)
set(CURL_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
FetchContent_Declare(curl
    URL https://curl.se/download/curl-${MDV_CURL_VERSION}.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(curl)

# The paths above carry no build-order information, so add it explicitly.
foreach(curl_target libcurl_static libcurl_shared libcurl)
    if(TARGET ${curl_target})
        get_target_property(_mdv_aliased ${curl_target} ALIASED_TARGET)
        if(NOT _mdv_aliased)
            add_dependencies(${curl_target} mbedtls mbedx509 mbedcrypto)
        endif()
    endif()
endforeach()

if(TARGET CURL::libcurl_static)
    set(MDV_CURL_TARGET CURL::libcurl_static)
elseif(TARGET libcurl_static)
    set(MDV_CURL_TARGET libcurl_static)
else()
    set(MDV_CURL_TARGET CURL::libcurl)
endif()
message(STATUS "MDViewer: using libcurl ${MDV_CURL_VERSION} + mbedTLS ${MDV_MBEDTLS_VERSION} built in the build tree")
