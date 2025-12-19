import QtQuick 2.4
import QtQuick.Window 2.2
import QtWebEngine 1.7
import mpvtest 1.0

Window {
    id: mainWindow
    width: 1280
    height: 720
    visible: true
    title: "mpv with Qt WebEngine Overlay (Qt5 OpenGL)"
    color: "#000000"

    // MpvVideo item - doesn't render as normal QML item
    // Instead hooks into beforeRendering signal and draws to OpenGL framebuffer
    // This is the KEY technique: size 0x0, invisible, but renders to background
    MpvVideo {
        id: video
        objectName: "video"
        // It's not a real item. Its renderer draws onto the window's background.
        width: 0
        height: 0
        visible: false
    }

    // WebEngineView overlays on top of the MPV video
    WebEngineView {
        id: web
        objectName: "web"
        width: parent.width
        height: parent.height
        backgroundColor: "transparent"
        settings.showScrollBars: false

        Component.onCompleted: {
            console.log("WebEngineView loaded")
        }

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
        <h1>mpv with Qt WebEngine Overlay (Qt5 OpenGL)</h1>
        <p>This box is rendered by Qt WebEngine</p>
        <p>The video underneath is rendered by mpv</p>
        <p class="tech-note">
            Technique: WebEngineView with transparent background<br>
            positioned above mpv video layer
        </p>
    </div>
</body>
</html>
        `)
    }
}
