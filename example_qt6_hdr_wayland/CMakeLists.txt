cmake_minimum_required(VERSION 3.16)
project(qt6-mpv-webengine-overlay VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(QT_MIN_VERSION 6.7.0)

# Build mpv from submodule using meson
set(MPV_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/mpv")
set(MPV_BUILD_DIR "${MPV_SOURCE_DIR}/build")

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

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Core Gui GuiPrivate Widgets WebEngineWidgets)
find_package(Vulkan REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(WAYLAND_CLIENT REQUIRED wayland-client)

# Main target: mpv + WebEngine overlay using Wayland subsurfaces
# mpv renders HDR to its own subsurface, WebEngine renders transparently on top
add_executable(mpv-webengine-overlay main.cpp)
target_include_directories(mpv-webengine-overlay PRIVATE
    ${MPV_SOURCE_DIR}/include
    ${WAYLAND_CLIENT_INCLUDE_DIRS}
)
target_link_libraries(mpv-webengine-overlay PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::GuiPrivate
    Qt6::Widgets
    Qt6::WebEngineWidgets
    ${MPV_BUILD_DIR}/libmpv.so
    Vulkan::Vulkan
    ${WAYLAND_CLIENT_LIBRARIES}
)
set_target_properties(mpv-webengine-overlay PROPERTIES BUILD_RPATH "${MPV_BUILD_DIR}")
