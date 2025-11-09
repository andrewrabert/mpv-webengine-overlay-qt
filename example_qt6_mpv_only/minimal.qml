import QtQuick
import QtQuick.Window
import mpvtest 1.0

Window {
    id: mainWindow
    width: 1280
    height: 720
    visible: true
    title: "Qt6 MPV Test - No WebEngine"
    color: "#000000"

    // MpvVideo item - hooks beforeRenderPassRecording to render MPV
    MpvVideo {
        id: video
        objectName: "video"
        width: 0
        height: 0
        visible: false
    }
}
