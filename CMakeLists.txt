cmake_minimum_required(VERSION 3.10)
project(xmlpoints_pi VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 11)

# Find required packages
find_package(wxWidgets REQUIRED COMPONENTS core base xml aui gl)
include(${wxWidgets_USE_FILE})
find_package(OpenGL REQUIRED)

# OpenCPN SDK include path – prefer the local SDK if present
set(OPENCPN_INCLUDE_DIR "$ENV{HOME}/opencpn-sdk/include" CACHE PATH "OpenCPN include directory")

include_directories(
    ${OPENCPN_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${wxWidgets_INCLUDE_DIRS}
)

set(SOURCES
    src/xmlpoints_pi.cpp
    src/PointsLayer.cpp
    src/XmlLoader.cpp
    src/InfoDialog.cpp
)

add_library(xmlpoints_pi SHARED ${SOURCES})

target_link_libraries(xmlpoints_pi
    ${wxWidgets_LIBRARIES}
    ${OPENGL_LIBRARIES}
)

set_target_properties(xmlpoints_pi PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

# Install targets
install(TARGETS xmlpoints_pi
    LIBRARY DESTINATION lib/opencpn
)

install(FILES data/xmlpoints_pi.xml
    DESTINATION share/opencpn/plugins/xmlpoints_pi
    OPTIONAL
)
