pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtWebEngine
import Example

ApplicationWindow {
    id: mainWindow
    width: 1280
    height: 720
    visible: true
    title: "mpv with Qt WebEngine Overlay (Qt6 Vulkan)"
    color: "#000000"

    // MpvItem - handles video rendering
    MpvItem {
        id: mpv
        objectName: "mpv"

        width: mainWindow.contentItem.width
        height: mainWindow.contentItem.height
        anchors.left: mainWindow.contentItem.left
        anchors.right: mainWindow.contentItem.right
        anchors.top: mainWindow.contentItem.top

        onReady: {
            console.log("MPV ready, loading:", videoFile)
            commandAsync(["loadfile", videoFile])
        }
    }

    // WebEngineView overlays on top of MPV video
    // Key technique: transparent background + z-index positioning
    WebEngineView {
        id: webOverlay
        width: mpv.width
        height: mpv.height
        anchors.left: mpv.left
        anchors.top: mpv.top
        z: 100  // Stack above mpv
        backgroundColor: "transparent"
        settings.showScrollBars: false

        url: "data:text/html," + encodeURIComponent(`
<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <style>
        * {
            box-sizing: border-box;
        }
        html, body {
            margin: 0;
            padding: 0;
            width: 100%;
            height: 100%;
            overflow: hidden;
            font-family: sans-serif;
            color: #fff;
            user-select: none;
        }

        /* Demo overlay box */
        .overlay-box {
            position: absolute;
            top: 50%;
            left: 50%;
            transform: translate(-50%, -50%);
            background: rgba(0, 0, 0, 0.8);
            color: white;
            padding: 40px 60px;
            border-radius: 10px;
            border: 2px solid rgba(255, 255, 255, 0.3);
            text-align: center;
            box-shadow: 0 4px 20px rgba(0, 0, 0, 0.5);
        }

        h1 {
            margin: 0 0 15px 0;
            font-size: 28px;
            font-weight: 400;
        }

        p {
            margin: 8px 0;
            font-size: 14px;
            color: #ccc;
        }

        .tech-note {
            margin-top: 20px;
            padding-top: 15px;
            border-top: 1px solid rgba(255, 255, 255, 0.2);
            font-size: 12px;
            color: #888;
        }
    </style>
</head>
<body>
    <div class="overlay-box">
        <h1>mpv with Qt WebEngine Overlay<br>Qt6<br>Vulkan</h1>
        <p>This box is rendered by Qt WebEngine</p>
        <p>The video underneath is rendered by mpv (via MpvQt)</p>
        <p class="tech-note">
            Technique: WebEngineView with transparent background<br>
            positioned above MpvObject using z-index
        </p>
    </div>
</body>
</html>
        `)
    }
}
