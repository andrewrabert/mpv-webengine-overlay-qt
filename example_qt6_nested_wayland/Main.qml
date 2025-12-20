import QtQuick
import QtQuick.Controls
import QtWebEngine
import QtWayland.Compositor
import QtWayland.Compositor.XdgShell

WaylandCompositor {
    id: compositor

    socketName: mpvLauncher.socketName
    additionalShmFormats: [
        WaylandCompositor.ShmFormat_XRGB8888,
        WaylandCompositor.ShmFormat_XBGR8888,
        WaylandCompositor.ShmFormat_ABGR8888,
        WaylandCompositor.ShmFormat_RGB888,
        WaylandCompositor.ShmFormat_BGR888
    ]

    property var mpvToplevel: null

    WaylandOutput {
        id: output
        sizeFollowsWindow: true
        window: mainWindow
        automaticFrameCallback: true
    }

    XdgShell {
        onToplevelCreated: (toplevel, xdgSurface) => {
            mpvSurfaceItem.surface = xdgSurface.surface
            compositor.mpvToplevel = toplevel
        }
    }

    Component.onCompleted: {
        viewporterHelper.attach(compositor)
    }

    ApplicationWindow {
        id: mainWindow
        width: 1280
        height: 720
        visible: true
        title: "mpv with Qt WebEngine Overlay (Qt6 Nested Wayland)"
        color: "#000000"

        onWidthChanged: handleResize()
        onHeightChanged: handleResize()

        property bool resizedSinceTick: false

        function handleResize() {
            if (!mpvSurfaceItem.surface) return
            mpvLauncher.resize(mainWindow.width, mainWindow.height)
        }

        WaylandQuickItem {
            id: mpvSurfaceItem
            anchors.centerIn: parent

            // Scale to fit window maintaining buffer aspect ratio
            property real bufferAspect: surface && surface.bufferSize.height > 0
                ? surface.bufferSize.width / surface.bufferSize.height : 16/9
            property real windowAspect: mainWindow.width / mainWindow.height
            width: windowAspect > bufferAspect ? mainWindow.height * bufferAspect : mainWindow.width
            height: windowAspect > bufferAspect ? mainWindow.height : mainWindow.width / bufferAspect

            output: output
            paintEnabled: true
            inputEventsEnabled: true
            focusOnClick: true
            focus: true
            layer.enabled: true
        }

        WebEngineView {
            id: webOverlay
            anchors.fill: parent
            z: 100
            backgroundColor: "transparent"
            settings.showScrollBars: false
            layer.enabled: true

            url: "data:text/html," + encodeURIComponent(`
<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <style>
        * { box-sizing: border-box; }
        html, body {
            margin: 0; padding: 0;
            width: 100%; height: 100%;
            overflow: hidden;
            font-family: sans-serif;
            color: #fff;
            user-select: none;
            pointer-events: none;
        }
        .overlay-box {
            position: absolute;
            top: 50%; left: 50%;
            transform: translate(-50%, -50%);
            background: rgba(0, 0, 0, 0.8);
            padding: 40px 60px;
            border-radius: 10px;
            border: 2px solid rgba(255, 255, 255, 0.3);
            text-align: center;
            box-shadow: 0 4px 20px rgba(0, 0, 0, 0.5);
        }
        h1 { margin: 0 0 15px 0; font-size: 28px; font-weight: 400; }
        p { margin: 8px 0; font-size: 14px; color: #ccc; }
        .tech-note {
            margin-top: 20px; padding-top: 15px;
            border-top: 1px solid rgba(255, 255, 255, 0.2);
            font-size: 12px; color: #888;
        }
    </style>
</head>
<body>
    <div class="overlay-box">
        <h1>mpv with Qt WebEngine Overlay<br>Qt6<br>Nested Wayland</h1>
        <p>This box is rendered by Qt WebEngine</p>
        <p>The video underneath is mpv running in a nested Wayland compositor</p>
        <p class="tech-note">
            Technique: WaylandCompositor embeds external mpv process<br>
            WebEngineView overlays with transparent background
        </p>
    </div>
</body>
</html>
            `)
        }

        MouseArea {
            id: inputArea
            anchors.fill: parent
            z: 200
            hoverEnabled: true
            acceptedButtons: Qt.AllButtons

            Component.onCompleted: {
                inputForwarder.seat = compositor.defaultSeat
                inputForwarder.target = mpvSurfaceItem
            }

            onPositionChanged: (mouse) => {
                inputForwarder.mouseMove(mouse.x, mouse.y)
            }

            onPressed: (mouse) => {
                inputForwarder.mousePress(mouse.button)
            }

            onReleased: (mouse) => {
                inputForwarder.mouseRelease(mouse.button)
            }

            onWheel: (wheel) => {
                inputForwarder.mouseWheel(Qt.Vertical, wheel.angleDelta.y)
            }
        }

        Component.onCompleted: {
            mpvLauncher.start()
        }

        Component.onDestruction: {
            mpvLauncher.stop()
        }
    }
}
