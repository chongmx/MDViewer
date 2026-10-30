cmake_minimum_required(VERSION 3.22)

project(MDViewer VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ---- Target platform ----------------------------------------------------------------------
if(EMSCRIPTEN)
    set(MDV_PLATFORM web)
elseif(ANDROID)
    set(MDV_PLATFORM android)
elseif(WIN32)
    set(MDV_PLATFORM windows)
elseif(APPLE)
    set(MDV_PLATFORM macos)
else()
    set(MDV_PLATFORM linux)
endif()
message(STATUS "MDViewer: platform=${MDV_PLATFORM} processor=${CMAKE_SYSTEM_PROCESSOR}")

set(MDV_IS_DESKTOP OFF)
if(MDV_PLATFORM MATCHES "windows|macos|linux")
    set(MDV_IS_DESKTOP ON)
endif()

option(MDV_BUILD_TESTS "Build the unit tests (desktop only)" ${MDV_IS_DESKTOP})
option(MDV_USE_CURL "Use libcurl for HTTP(S) on Linux/macOS when available" ON)
option(MDV_FETCH_CURL "If libcurl is not installed, download and build it (with mbedTLS) in the build tree" ON)
option(MDV_FETCH_GLFW "Download GLFW if it is not installed (desktop)" ON)
set(MDV_WEB_GLFW "contrib" CACHE STRING "Emscripten GLFW implementation: 'contrib' (--use-port=contrib.glfw3) or 'builtin' (-sUSE_GLFW=3)")

set(MDV_THIRD_PARTY ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty)
if(NOT EXISTS ${MDV_THIRD_PARTY}/imgui/imgui.h OR NOT EXISTS ${MDV_THIRD_PARTY}/IconFontCppHeaders/IconsFontAwesome6.h)
    message(FATAL_ERROR "Submodules missing. Run: git submodule update --init --recursive")
endif()

if(MSVC)
    add_compile_options(/utf-8)
endif()

# Warnings for MDViewer's own targets (third-party code is built with its defaults).
function(mdv_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /wd4100 /wd4996)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wno-unused-parameter)
    endif()
endfunction()

if(MDV_PLATFORM STREQUAL "web")
    if(MDV_WEB_GLFW STREQUAL "contrib")
        set(MDV_WEB_GLFW_FLAG "--use-port=contrib.glfw3")
    else()
        set(MDV_WEB_GLFW_FLAG "-sUSE_GLFW=3")
    endif()
endif()

# ---- Dear ImGui ---------------------------------------------------------------------------
set(IMGUI_DIR ${MDV_THIRD_PARTY}/imgui)
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/misc/cpp/imgui_stdlib.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_compile_definitions(imgui PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS)
if(NOT MSVC)
    target_compile_options(imgui PRIVATE -w)
endif()

# Font Awesome codepoint headers (header only).
add_library(icon_font_headers INTERFACE)
target_include_directories(icon_font_headers INTERFACE ${MDV_THIRD_PARTY}/IconFontCppHeaders)

# ---- Windowing / GL per platform -----------------------------------------------------------
if(MDV_IS_DESKTOP)
    find_package(glfw3 3.3 CONFIG QUIET)
    if(NOT glfw3_FOUND)
        if(NOT MDV_FETCH_GLFW)
            message(FATAL_ERROR "GLFW not found; install it or enable MDV_FETCH_GLFW")
        endif()
        include(FetchContent)
        set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(glfw
            GIT_REPOSITORY https://github.com/glfw/glfw.git
            GIT_TAG 3.4
            GIT_SHALLOW TRUE)
        FetchContent_MakeAvailable(glfw)
    endif()

    add_library(glad STATIC ${MDV_THIRD_PARTY}/glad/src/glad.c)
    target_include_directories(glad PUBLIC ${MDV_THIRD_PARTY}/glad/include)
    if(UNIX)
        target_link_libraries(glad PUBLIC ${CMAKE_DL_LIBS})
    endif()

    find_package(OpenGL REQUIRED)
    target_sources(imgui PRIVATE ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp)
    target_link_libraries(imgui PUBLIC glfw OpenGL::GL)
elseif(MDV_PLATFORM STREQUAL "web")
    target_sources(imgui PRIVATE ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp)
    target_compile_options(imgui PUBLIC ${MDV_WEB_GLFW_FLAG})
elseif(MDV_PLATFORM STREQUAL "android")
    target_sources(imgui PRIVATE ${IMGUI_DIR}/backends/imgui_impl_android.cpp)
    target_compile_definitions(imgui PUBLIC IMGUI_IMPL_OPENGL_ES3)
    target_link_libraries(imgui PUBLIC android EGL GLESv3 log)
endif()

# ---- OS abstraction layer --------------------------------------------------------------------
set(MDV_OS_SOURCES
    src/os/common/os_common.cpp
    src/os/common/dispatch.cpp
    src/os/common/path.cpp
    src/os/common/url.cpp
)
if(MDV_PLATFORM STREQUAL "windows")
    list(APPEND MDV_OS_SOURCES src/os/common/http_threaded.cpp src/os/win32/os_win32.cpp src/os/win32/http_win32.cpp)
elseif(MDV_PLATFORM MATCHES "linux|macos")
    list(APPEND MDV_OS_SOURCES src/os/common/http_threaded.cpp src/os/posix/fs_posix.cpp src/os/unix/os_unix.cpp)
elseif(MDV_PLATFORM STREQUAL "android")
    list(APPEND MDV_OS_SOURCES src/os/common/http_threaded.cpp src/os/posix/fs_posix.cpp
         src/os/android/os_android.cpp src/os/android/http_android.cpp)
elseif(MDV_PLATFORM STREQUAL "web")
    list(APPEND MDV_OS_SOURCES src/os/posix/fs_posix.cpp src/os/web/os_web.cpp src/os/web/http_web.cpp)
endif()

if(MDV_PLATFORM MATCHES "linux|macos")
    set(MDV_CURL_TARGET "")
    if(MDV_USE_CURL)
        find_package(CURL QUIET)
        if(CURL_FOUND)
            set(MDV_CURL_TARGET CURL::libcurl)
        elseif(MDV_FETCH_CURL)
            include(cmake/FetchCurl.cmake) # sets MDV_CURL_TARGET
        endif()
    endif()
    if(MDV_CURL_TARGET)
        list(APPEND MDV_OS_SOURCES src/os/posix/http_curl.cpp)
        message(STATUS "MDViewer: HTTP(S) via libcurl")
    else()
        list(APPEND MDV_OS_SOURCES src/os/posix/http_socket.cpp)
        message(STATUS "MDViewer: libcurl not found, plain-HTTP socket backend (no HTTPS)")
    endif()
endif()

add_library(mdv_os STATIC ${MDV_OS_SOURCES})
target_include_directories(mdv_os PUBLIC include)
if(MDV_PLATFORM STREQUAL "windows")
    target_link_libraries(mdv_os PRIVATE winhttp comdlg32 shell32)
    target_compile_definitions(mdv_os PRIVATE UNICODE _UNICODE)
elseif(MDV_PLATFORM MATCHES "linux|macos")
    find_package(Threads REQUIRED)
    target_link_libraries(mdv_os PUBLIC Threads::Threads)
    if(MDV_CURL_TARGET)
        target_link_libraries(mdv_os PRIVATE ${MDV_CURL_TARGET})
        target_compile_definitions(mdv_os PRIVATE MDV_HAS_CURL=1)
    endif()
elseif(MDV_PLATFORM STREQUAL "android")
    add_library(native_app_glue STATIC ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)
    target_include_directories(native_app_glue PUBLIC ${ANDROID_NDK}/sources/android/native_app_glue)
    target_link_libraries(native_app_glue PUBLIC android log)
    target_link_libraries(mdv_os PUBLIC native_app_glue android log)
endif()

# ---- MVVM layers -----------------------------------------------------------------------------
# model: pure domain data and logic (no OS, no UI)
add_library(mdv_model STATIC
    src/model/markdown_parser.cpp
    src/model/search.cpp
    src/model/settings.cpp
    src/model/file_entry.cpp
)
target_include_directories(mdv_model PUBLIC include)

# interfaces: service contracts (header only)
add_library(mdv_interfaces INTERFACE)
target_include_directories(mdv_interfaces INTERFACE include)
target_link_libraries(mdv_interfaces INTERFACE mdv_model)

# services: interface implementations on top of the OS layer
add_library(mdv_services STATIC
    src/services/document_service.cpp
    src/services/settings_service.cpp
    src/services/file_system_service.cpp
    src/services/platform_service.cpp
    src/services/service_container.cpp
)
target_link_libraries(mdv_services PUBLIC mdv_interfaces PRIVATE mdv_os)

# viewmodel: presentation logic, depends on interfaces + model only
add_library(mdv_viewmodel STATIC
    src/viewmodel/viewer_view_model.cpp
    src/viewmodel/file_browser_view_model.cpp
)
target_link_libraries(mdv_viewmodel PUBLIC mdv_interfaces)

# view: Dear ImGui rendering bound to the view-models
add_library(mdv_view STATIC
    src/view/theme.cpp
    src/view/fonts.cpp
    src/view/markdown_renderer.cpp
    src/view/main_view.cpp
)
target_link_libraries(mdv_view PUBLIC mdv_viewmodel imgui PRIVATE icon_font_headers mdv_os)
target_compile_definitions(mdv_view PRIVATE MDV_VERSION="${PROJECT_VERSION}")

# ---- Application -------------------------------------------------------------------------------
set(MDV_APP_SOURCES src/app/application.cpp)

if(MDV_PLATFORM STREQUAL "android")
    add_library(mdviewer SHARED ${MDV_APP_SOURCES} src/app/main_android.cpp)
    target_link_libraries(mdviewer PRIVATE mdv_view mdv_services mdv_os native_app_glue)
    target_link_options(mdviewer PRIVATE "-u" "ANativeActivity_onCreate")
else()
    add_executable(MDViewer ${MDV_APP_SOURCES} src/app/main_glfw.cpp)
    target_link_libraries(MDViewer PRIVATE mdv_view mdv_services mdv_os)

    if(MDV_IS_DESKTOP)
        target_link_libraries(MDViewer PRIVATE glad)
        # Ship bundled resources next to the executable.
        add_custom_command(TARGET MDViewer POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    ${CMAKE_CURRENT_SOURCE_DIR}/resources $<TARGET_FILE_DIR:MDViewer>/resources
            COMMENT "Copying resources")
        install(TARGETS MDViewer RUNTIME DESTINATION bin)
        install(DIRECTORY resources/ DESTINATION share/mdviewer/resources)
    endif()

    if(MDV_PLATFORM STREQUAL "windows")
        if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug") # keep the console for Debug builds
            set_target_properties(MDViewer PROPERTIES WIN32_EXECUTABLE TRUE)
        endif()
        if(MSVC)
            target_link_options(MDViewer PRIVATE /ENTRY:mainCRTStartup)
        endif()
    elseif(MDV_PLATFORM STREQUAL "web")
        set_target_properties(MDViewer PROPERTIES SUFFIX ".html" OUTPUT_NAME "index")
        target_link_options(MDViewer PRIVATE
            ${MDV_WEB_GLFW_FLAG}
            -sFETCH=1
            -sALLOW_MEMORY_GROWTH=1
            -sMIN_WEBGL_VERSION=2
            -sMAX_WEBGL_VERSION=2
            -sEXPORTED_RUNTIME_METHODS=ccall
            "-sEXPORTED_FUNCTIONS=_main,_mdv_web_file_dialog_result"
            "-sDEFAULT_LIBRARY_FUNCS_TO_INCLUDE=$UTF8ToString"
            --preload-file ${CMAKE_CURRENT_SOURCE_DIR}/resources@/assets
            --shell-file ${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html)
        set_property(TARGET MDViewer APPEND PROPERTY LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html)
    endif()
endif()

foreach(target mdv_os mdv_model mdv_services mdv_viewmodel mdv_view)
    mdv_set_warnings(${target})
endforeach()
if(TARGET MDViewer)
    mdv_set_warnings(MDViewer)
elseif(TARGET mdviewer)
    mdv_set_warnings(mdviewer)
endif()

# ---- Tests ------------------------------------------------------------------------------------
if(MDV_BUILD_TESTS AND MDV_IS_DESKTOP)
    enable_testing()
    add_executable(mdv_tests tests/test_main.cpp)
    target_link_libraries(mdv_tests PRIVATE mdv_viewmodel mdv_model mdv_os)
    mdv_set_warnings(mdv_tests)
    add_test(NAME mdv_tests COMMAND mdv_tests)
endif()
