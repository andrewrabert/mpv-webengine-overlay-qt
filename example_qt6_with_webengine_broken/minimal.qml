import QtQuick
import QtQuick.Window
import QtWebEngine
import mpvtest 1.0

Window {
    id: mainWindow
    width: 1280
    height: 720
    visible: true
    title: "Qt6 MPV + WebEngine Overlay"
    color: "#000000"

    // MpvVideo item - hooks beforeRenderPassRecording to render MPV
    MpvVideo {
        id: video
        objectName: "video"
        width: 0
        height: 0
        visible: false
    }

    // WebEngineView overlays on top
    WebEngineView {
        id: web
        objectName: "web"
        anchors.fill: parent
        backgroundColor: "transparent"
        settings.showScrollBars: false

        url: "data:text/html," + encodeURIComponent(`
            <!DOCTYPE html>
            <html>
            <head>
                <style>
                    html, body {
                        margin: 0;
                        padding: 0;
                        background: transparent !important;
                        overflow: hidden;
                    }
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
                    }
                    p {
                        margin: 8px 0;
                        font-size: 14px;
                        color: #ccc;
                    }
                </style>
            </head>
            <body>
                <div class="overlay-box">
                    <h1>WebView Overlay Demo</h1>
                    <p>This box is rendered by Qt WebEngine</p>
                    <p>The video around it is rendered by MPV</p>
                    <p style="margin-top: 15px; font-size: 12px; color: #888;">
                        Transparent areas show MPV video underneath
                    </p>
                </div>
            </body>
            </html>
        `)
    }
}
