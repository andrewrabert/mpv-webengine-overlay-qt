# Minimal MPV + WebView Overlay Example (Qt6)

Demonstrates how Jellyfin Media Player overlays Qt WebEngine on top of MPV video. This is the Qt6 version.

## What It Does

Shows MPV video playback with a transparent WebEngineView overlay on top. The HTML overlay has a semi-transparent box in the center - video is visible around it. This is the exact technique JMP uses to overlay jellyfin-web UI on top of MPV video.

## Architecture

**Key technique:** MpvVideo QQuickItem (size 0x0, invisible) hooks `QQuickWindow::beforeRendering` to render MPV directly to OpenGL framebuffer. WebEngineView renders on top via normal scene graph with transparent background.

**Critical implementation details:**
- `window()->setClearBeforeRendering(false)` - Preserves video background
- `window()->setPersistentOpenGLContext(true)` - Required for MPV render context
- `window()->setPersistentSceneGraph(true)` - Keeps scene graph alive
- MPV `vo=libmpv` set AFTER window creation (not before mpv_initialize)
- WebEngineView with `backgroundColor: "transparent"`
- Video loaded via `QTimer::singleShot(100)` - small delay needed for WebEngine initialization (JMP doesn't have this since it initializes differently)

## Files

- `main.cpp` - PlayerQuickItem and PlayerRenderer classes, MPV initialization
- `minimal.qml` - Window with MpvVideo (background) + WebEngineView (overlay)
- `BigBuckBunny_512kb.mp4` - Test video file
- `CMakeLists.txt` - Build configuration

## Build & Run

```bash
cd example_qt6
mkdir build && cd build
cmake ..
make
./minimal-overlay ../BigBuckBunny_512kb.mp4
```

Usage: `./minimal-overlay <video-file>`

## Requirements

- Qt6 (Core, Qml, Quick, WebEngineQuick)
- libmpv
- OpenGL

## How It Works

1. **QtWebEngineQuick::initialize()** called before QGuiApplication
2. MPV created and initialized with basic options (before window)
3. QML Window loaded with:
   - MpvVideo item (0x0, invisible)
   - WebEngineView (fullscreen, transparent background)
4. On window creation:
   - `vo=libmpv` set (must be after window exists)
   - PlayerQuickItem given MPV handle
   - Video loading delayed 100ms for WebEngine initialization
5. **PlayerQuickItem** hooks into rendering:
   - Connects to `beforeSynchronizing` → creates PlayerRenderer
   - PlayerRenderer connects to `beforeRendering` → renders MPV
   - Sets critical window flags (no clear, persistent context/scenegraph)
6. **Rendering each frame:**
   - MPV renders to OpenGL framebuffer (beforeRendering signal)
   - Qt scene graph renders WebEngineView on top
   - Transparent HTML areas show video underneath

## Rendering Order

```
QQuickWindow::beforeRendering (render thread)
  → PlayerRenderer::render()
  → mpv_render_context_render() [MPV video to OpenGL FBO]

QQuickWindow::sceneGraph rendering
  → WebEngineView renders transparent HTML on top
  → Only non-transparent HTML pixels obscure video
```
