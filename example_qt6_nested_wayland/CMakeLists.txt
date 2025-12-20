cmake_minimum_required(VERSION 3.16)
project(qt6-mpv-webengine-overlay-nested-wayland CXX)

set(CMAKE_CXX_STANDARD 17)
set(QT_MIN_VERSION 6.6.0)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS
    Core
    Qml
    Quick
    WebEngineQuick
    WaylandCompositor
)

qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(qt6-mpv-webengine-overlay-nested-wayland
    main.cpp
)

qt_add_qml_module(qt6-mpv-webengine-overlay-nested-wayland
    URI Example
    VERSION 1.0
    QML_FILES
        Main.qml
)

target_link_libraries(qt6-mpv-webengine-overlay-nested-wayland PRIVATE
    Qt6::Core
    Qt6::Qml
    Qt6::Quick
    Qt6::WebEngineQuick
    Qt6::WaylandCompositor
)
