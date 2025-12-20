# MPV + WebEngine Overlay for Qt

Demonstrates transparent Qt WebEngine overlays on MPV video rendering.

Both Qt5 and Qt6 implementations work when using MpvQt library with proper initialization.

## Examples

| Example                      | Qt     | API           | Notes                                                                                                   |
|------------------------------|--------|---------------|---------------------------------------------------------------------------------------------------------|
| `example_qt5_opengl`         | Qt5    | OpenGL        |                                                                                                         |
| `example_qt6_opengl`         | Qt6.5+ | OpenGL        | libmpv via [mpvqt](https://invent.kde.org/libraries/mpvqt)                                              |
| `example_qt6_vulkan`         | Qt6.7+ | Vulkan        | non-upstream [libmpv](https://github.com/andrewrabert/mpv/tree/libmpv-vulkan) with added vulkan support |
| `example_qt6_nested_wayland` | Qt6.6+ | Wayland (shm) | Nested Wayland compositor embeds external mpv process window                                            |

## Key Implementation Details

### Qt6 Requirements
- Use `MpvQt` library (provides `MpvAbstractItem`)
- Set `vo=libmpv` property in MpvItem constructor
- Wait for `ready` signal before loading media
- Set `QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL)`
- Initialize QtWebEngine before QGuiApplication

### WebEngine Overlay Technique
- WebEngineView with `backgroundColor: "transparent"`
- Position as sibling to MpvItem with higher z-index
- Both anchor to `window.contentItem` for proper layering

## Requirements

- **Qt5:** Qt5 (Core, Qml, Quick, WebEngine), libmpv, OpenGL
- **Qt6:** Qt6 (Core, Qml, Quick, WebEngineQuick), MpvQt, OpenGL
- **Qt6 Vulkan:** Qt 6.7+, Vulkan SDK, meson, extra-cmake-modules
- **Qt6 Nested Wayland:** Qt6 (Core, Qml, Quick, WebEngineQuick, WaylandCompositor), mpv (system), Wayland session

## Download Test Video
Or use any local MP4 file.

```bash
curl -L -o BigBuckBunny_512kb.mp4 https://archive.org/download/BigBuckBunny_328/BigBuckBunny_512kb.mp4
```

## Build & Run

```bash
# Qt5 OpenGL
cd example_qt5_opengl
cmake -B build && cmake --build build
./build/qt5-mpv-webengine-overlay-opengl ../BigBuckBunny_512kb.mp4

# Qt6 OpenGL
cd example_qt6_opengl
cmake -B build && cmake --build build
./build/qt6-mpv-webengine-overlay-opengl ../BigBuckBunny_512kb.mp4

# Qt6 Vulkan (builds mpv from submodule)
cd example_qt6_vulkan
git submodule update --init
cmake -B build && cmake --build build
./build/qt6-mpv-webengine-overlay-vulkan ../BigBuckBunny_512kb.mp4

# Qt6 Nested Wayland (requires mpv installed, Wayland session)
cd example_qt6_nested_wayland
cmake -B build && cmake --build build
./build/qt6-mpv-webengine-overlay-nested-wayland ../BigBuckBunny_512kb.mp4
```
