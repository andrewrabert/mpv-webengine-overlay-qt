cmake_minimum_required(VERSION 3.16)
project(minimal-overlay-example CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt6 REQUIRED COMPONENTS Core Qml Quick WebEngineQuick)
find_package(PkgConfig REQUIRED)
pkg_check_modules(MPV REQUIRED mpv)

add_executable(minimal-overlay
    main.cpp
)

target_link_libraries(minimal-overlay
    Qt6::Core
    Qt6::Qml
    Qt6::Quick
    Qt6::WebEngineQuick
    ${MPV_LIBRARIES}
)

target_include_directories(minimal-overlay PRIVATE ${MPV_INCLUDE_DIRS})

# Copy QML file to build directory
configure_file(minimal.qml minimal.qml COPYONLY)
