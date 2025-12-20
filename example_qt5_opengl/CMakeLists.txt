cmake_minimum_required(VERSION 3.5.0)
project(mpv-webengine-overlay CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt5 REQUIRED COMPONENTS Core Qml Quick WebEngine)
find_package(PkgConfig REQUIRED)
pkg_check_modules(MPV REQUIRED mpv)

add_executable(mpv-webengine-overlay
    main.cpp
)

target_link_libraries(mpv-webengine-overlay
    Qt5::Core
    Qt5::Qml
    Qt5::Quick
    Qt5::WebEngine
    ${MPV_LIBRARIES}
)

target_include_directories(mpv-webengine-overlay PRIVATE ${MPV_INCLUDE_DIRS})

# Copy QML file to build directory
configure_file(Main.qml Main.qml COPYONLY)
