#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QtWebEngineQuick/QtWebEngineQuick>

#include <cstdio>

int main(int argc, char* argv[])
{
    // Must initialize QtWebEngine before QGuiApplication
    QtWebEngineQuick::initialize();

    QGuiApplication app(argc, argv);

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <video-file>\n", argv[0]);
        return 1;
    }

    QString videoFile = QString::fromUtf8(argv[1]);

    // Required by mpv
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

    // Create QML engine
    QQmlApplicationEngine engine;

    // Pass video file to QML
    engine.rootContext()->setContextProperty("videoFile", videoFile);

    // Load QML from module
    engine.loadFromModule("Example", "Main");

    return app.exec();
}
