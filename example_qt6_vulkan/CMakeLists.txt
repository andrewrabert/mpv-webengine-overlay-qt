cmake_minimum_required(VERSION 3.16)
project(qt6-mpv-webengine-overlay CXX)

set(CMAKE_CXX_STANDARD 17)

set(QT_MIN_VERSION 6.7.0)

# Build mpv from submodule using meson
set(MPV_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/mpv")
set(MPV_BUILD_DIR "${MPV_SOURCE_DIR}/build")

# Check if mpv needs to be built
if(NOT EXISTS "${MPV_BUILD_DIR}/libmpv.so")
    message(STATUS "Building mpv from submodule...")
    execute_process(
        COMMAND meson setup build --default-library=shared -Dlibmpv=true
        WORKING_DIRECTORY ${MPV_SOURCE_DIR}
        RESULT_VARIABLE MPV_SETUP_RESULT
    )
    if(NOT MPV_SETUP_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to configure mpv with meson")
    endif()
    execute_process(
        COMMAND meson compile -C build
        WORKING_DIRECTORY ${MPV_SOURCE_DIR}
        RESULT_VARIABLE MPV_BUILD_RESULT
    )
    if(NOT MPV_BUILD_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to build mpv")
    endif()
endif()

# Set hints for FindLibmpv to find local mpv (FORCE to override any stale cache)
set(Libmpv_INCLUDE_DIRS "${MPV_SOURCE_DIR}/include" CACHE PATH "libmpv include dir" FORCE)
set(Libmpv_LIBRARIES "${MPV_BUILD_DIR}/libmpv.so" CACHE FILEPATH "libmpv library" FORCE)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Core Qml Quick WebEngineQuick)
find_package(Vulkan REQUIRED)
add_subdirectory(mpvqt)

qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(qt6-mpv-webengine-overlay-vulkan
    main.cpp
    mpvitem.h
    mpvitem.cpp
)

qt_add_qml_module(qt6-mpv-webengine-overlay-vulkan
    URI Example
    VERSION 1.0
    QML_FILES
        Main.qml
)

target_link_libraries(qt6-mpv-webengine-overlay-vulkan PRIVATE
    Qt6::Core
    Qt6::Qml
    Qt6::Quick
    Qt6::WebEngineQuick
    MpvQt::MpvQt
    Vulkan::Vulkan
)

# Ensure mpv library can be found at runtime
set_target_properties(qt6-mpv-webengine-overlay-vulkan PROPERTIES
    BUILD_RPATH "${MPV_BUILD_DIR}"
)
