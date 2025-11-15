# MPV + WebEngine Overlay for Qt

Demonstrates transparent Qt WebEngine overlays on MPV video rendering.

Both Qt5 and Qt6 implementations work when using MpvQt library with proper initialization.

## Examples

| Example       | Description                                    |
|---------------|------------------------------------------------|
| `example_qt5` | Qt5 implementation using raw libmpv            |
| `example_qt6` | Qt6 implementation using MpvQt library         |

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

## Download Test Video
Or use any local MP4 file.

```bash
curl -L -o BigBuckBunny_512kb.mp4 https://archive.org/download/BigBuckBunny_328/BigBuckBunny_512kb.mp4
```

## Build & Run

```bash
# Qt5
cd example_qt5
cmake -B build && cmake --build build
./build/minimal-overlay ../BigBuckBunny_512kb.mp4

# Qt6
cd example_qt6
cmake -B build && cmake --build build
./build/qt6-mpv-webengine-overlay ../BigBuckBunny_512kb.mp4
```
