cmake_minimum_required(VERSION 3.10)
project(xmlpoints_pi VERSION 4.0.0)

set(CMAKE_CXX_STANDARD 11)

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

include_directories(
    ${OPENCPN_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${wxWidgets_INCLUDE_DIRS}
)

set(SOURCES
    src/xmlpoints_pi.cpp
    src/PointsLayer.cpp
    src/S124Warning.cpp
    src/S124Parser.cpp
    src/SecomClient.cpp
    src/SecomDialog.cpp
    src/InfoDialog.cpp
)

add_library(xmlpoints_pi SHARED ${SOURCES})

target_link_libraries(xmlpoints_pi
    ${wxWidgets_LIBRARIES}
    ${OPENGL_LIBRARIES}
)

if(CURL_FOUND)
    target_include_directories(xmlpoints_pi PRIVATE ${CURL_INCLUDE_DIRS})
    target_link_libraries(xmlpoints_pi ${CURL_LIBRARIES})
endif()

set_target_properties(xmlpoints_pi PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

install(TARGETS xmlpoints_pi LIBRARY DESTINATION lib/opencpn)