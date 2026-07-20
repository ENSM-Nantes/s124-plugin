cmake_minimum_required(VERSION 3.10)
project(s124navwarnings_pi VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 11)

# The source files are UTF-8 (no BOM) and contain non-ASCII punctuation
# (en dashes, ellipses, arrows, degree signs) inside translatable strings.
# MSVC otherwise guesses the source/execution charset from the system
# codepage and mangles those bytes; GCC/Clang already default to UTF-8,
# so this is a no-op there.
if(MSVC)
    add_compile_options(/utf-8)
endif()

find_package(wxWidgets REQUIRED COMPONENTS core base xml aui gl net)
include(${wxWidgets_USE_FILE})

if(POLICY CMP0072)
    cmake_policy(SET CMP0072 NEW)
endif()
set(OpenGL_GL_PREFERENCE "GLVND")
find_package(OpenGL REQUIRED)

# libcurl – optional but required for SECOM support
find_package(CURL)
if(CURL_FOUND)
    add_definitions(-DHAVE_CURL)
    message(STATUS "libcurl found – SECOM support enabled")
else()
    message(WARNING "libcurl NOT found – SECOM support disabled (local file only)")
endif()

set(OPENCPN_INCLUDE_DIR "$ENV{HOME}/opencpn-sdk/include"
    CACHE PATH "OpenCPN include directory")

# Windows/MSVC only: opencpn_plugin/PlugInChartBase etc. are implemented inside
# opencpn.exe itself. On Linux those symbols are left unresolved at link time
# and satisfied at dlopen() time by the running opencpn process, but the MSVC
# linker requires them resolved up front, so the plugin must link against a
# prebuilt import library that exports them (see OpenCPN/opencpn-libs, e.g.
# api-21/msvc-wx32/opencpn.lib, built against wxWidgets 3.2).
set(OPENCPN_IMPORT_LIB "" CACHE FILEPATH
    "Path to opencpn.lib (required on Windows/MSVC only)")

include_directories(
    ${OPENCPN_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${wxWidgets_INCLUDE_DIRS}
)

set(SOURCES
    src/s124navwarnings_pi.cpp
    src/PointsLayer.cpp
    src/S124Warning.cpp
    src/S124Parser.cpp
    src/SecomClient.cpp
    src/SecomDialog.cpp
    src/SecomListDialog.cpp
    src/InfoDialog.cpp
    src/ToolbarMenuDialog.cpp
    src/WarningColorDialog.cpp
)

add_library(s124navwarnings_pi SHARED ${SOURCES})

target_link_libraries(s124navwarnings_pi
    ${wxWidgets_LIBRARIES}
    ${OPENGL_LIBRARIES}
)

if(CURL_FOUND)
    target_include_directories(s124navwarnings_pi PRIVATE ${CURL_INCLUDE_DIRS})
    target_link_libraries(s124navwarnings_pi ${CURL_LIBRARIES})
endif()

if(WIN32 AND MSVC)
    if(NOT OPENCPN_IMPORT_LIB)
        message(FATAL_ERROR
            "OPENCPN_IMPORT_LIB is not set. On Windows/MSVC this plugin must "
            "link against opencpn.lib (see OpenCPN/opencpn-libs, e.g. "
            "api-21/msvc-wx32/opencpn.lib) to resolve opencpn_plugin/"
            "PlugInChartBase symbols that live inside opencpn.exe.")
    endif()
    target_compile_definitions(s124navwarnings_pi PRIVATE UNICODE _UNICODE)
    target_link_libraries(s124navwarnings_pi ${OPENCPN_IMPORT_LIB})
endif()

set_target_properties(s124navwarnings_pi PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

install(TARGETS s124navwarnings_pi LIBRARY DESTINATION lib/opencpn)