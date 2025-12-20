cmake_minimum_required(VERSION 3.16)
project(qt6-mpv-webengine-overlay CXX)

set(CMAKE_CXX_STANDARD 17)

set(QT_MIN_VERSION 6.5.0)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Core Qml Quick WebEngineQuick)
add_subdirectory(mpvqt)

qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(mpv-webengine-overlay
    main.cpp
    mpvitem.h
    mpvitem.cpp
)

qt_add_qml_module(mpv-webengine-overlay
    URI Example
    VERSION 1.0
    QML_FILES
        Main.qml
)

target_link_libraries(mpv-webengine-overlay PRIVATE
    Qt6::Core
    Qt6::Qml
    Qt6::Quick
    Qt6::WebEngineQuick
    MpvQt::MpvQt
)
