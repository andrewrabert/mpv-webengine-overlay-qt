#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QProcess>
#include <QTimer>
#include <QLocalSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QtWebEngineQuick/QtWebEngineQuick>
#include <QWaylandSeat>
#include <QWaylandQuickItem>
#include <QWaylandCompositor>
#include <QWaylandViewporter>
#include <QQuickWindow>

#include <cstdio>

class InputForwarder : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QWaylandSeat* seat READ seat WRITE setSeat NOTIFY seatChanged)
    Q_PROPERTY(QWaylandQuickItem* target READ target WRITE setTarget NOTIFY targetChanged)

public:
    explicit InputForwarder(QObject* parent = nullptr) : QObject(parent), m_seat(nullptr), m_target(nullptr) {}

    QWaylandSeat* seat() const { return m_seat; }
    void setSeat(QWaylandSeat* s) { if (m_seat != s) { m_seat = s; emit seatChanged(); } }

    QWaylandQuickItem* target() const { return m_target; }
    void setTarget(QWaylandQuickItem* t) { if (m_target != t) { m_target = t; emit targetChanged(); } }

    Q_INVOKABLE void mouseMove(qreal x, qreal y) {
        if (m_seat && m_target && m_target->view()) {
            m_seat->sendMouseMoveEvent(m_target->view(), QPointF(x, y));
        }
    }

    Q_INVOKABLE void mousePress(int button) {
        if (m_seat) {
            m_seat->sendMousePressEvent(static_cast<Qt::MouseButton>(button));
        }
    }

    Q_INVOKABLE void mouseRelease(int button) {
        if (m_seat) {
            m_seat->sendMouseReleaseEvent(static_cast<Qt::MouseButton>(button));
        }
    }

    Q_INVOKABLE void mouseWheel(int orientation, int delta) {
        if (m_seat) {
            m_seat->sendMouseWheelEvent(static_cast<Qt::Orientation>(orientation), delta);
        }
    }

Q_SIGNALS:
    void seatChanged();
    void targetChanged();

private:
    QWaylandSeat* m_seat;
    QWaylandQuickItem* m_target;
};

class ViewporterHelper : public QObject
{
    Q_OBJECT
public:
    explicit ViewporterHelper(QObject* parent = nullptr) : QObject(parent), m_viewporter(nullptr) {}

    Q_INVOKABLE void attach(QWaylandCompositor* compositor) {
        if (compositor && !m_viewporter) {
            m_viewporter = new QWaylandViewporter(compositor);
        }
    }

private:
    QWaylandViewporter* m_viewporter;
};

class MpvLauncher : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString socketName READ socketName CONSTANT)
    Q_PROPERTY(QString videoFile READ videoFile CONSTANT)

public:
    MpvLauncher(const QString& socket, const QString& video, QObject* parent = nullptr)
        : QObject(parent), m_socketName(socket), m_videoFile(video), m_process(nullptr), m_ipcSocket(nullptr), m_stopped(false)
    {
        m_ipcPath = QString("/tmp/mpv-ipc-%1.sock").arg(QCoreApplication::applicationPid());
    }

    ~MpvLauncher() override {
        stop();
    }

    QString socketName() const { return m_socketName; }
    QString videoFile() const { return m_videoFile; }

    Q_INVOKABLE void start() {
        if (m_process) return;

        m_process = new QProcess(this);
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("WAYLAND_DISPLAY", m_socketName);
        m_process->setProcessEnvironment(env);

        QStringList args = {
            "--vo=wlshm",
            "--vf=format=fmt=bgr0",
            "--force-window=yes",
            "--no-border",
            "--geometry=1280x720",
            QString("--input-ipc-server=%1").arg(m_ipcPath),
            m_videoFile
        };

        connect(m_process, &QProcess::finished, this, [](int exitCode) {
            QCoreApplication::exit(exitCode);
        });
        m_process->start("mpv", args);
        QTimer::singleShot(500, this, &MpvLauncher::connectIpc);
    }

    Q_INVOKABLE void stop() {
        if (m_stopped) return;
        m_stopped = true;
        // Send quit command via IPC
        sendCommand({"quit"});
        if (m_ipcSocket) {
            m_ipcSocket->flush();
            m_ipcSocket->disconnectFromServer();
            delete m_ipcSocket;
            m_ipcSocket = nullptr;
        }
        if (m_process) {
            disconnect(m_process, nullptr, this, nullptr);
            while (m_process->state() != QProcess::NotRunning) {
                QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
            }
            delete m_process;
            m_process = nullptr;
        }
        QFile::remove(m_ipcPath);
    }

    Q_INVOKABLE void resize(int width, int height) {
        sendCommand({"set_property", "geometry", QString("%1x%2").arg(width).arg(height)});
    }

private:
    void connectIpc() {
        if (m_stopped) return;
        if (m_ipcSocket) delete m_ipcSocket;
        m_ipcSocket = new QLocalSocket(this);
        m_ipcSocket->connectToServer(m_ipcPath);
        if (!m_ipcSocket->waitForConnected(2000)) {
            if (!m_stopped) {
                QTimer::singleShot(500, this, &MpvLauncher::connectIpc);
            }
        }
    }

    void sendCommand(const QVariantList& command) {
        if (!m_ipcSocket || m_ipcSocket->state() != QLocalSocket::ConnectedState) return;
        QJsonObject msg;
        msg["command"] = QJsonArray::fromVariantList(command);
        m_ipcSocket->write(QJsonDocument(msg).toJson(QJsonDocument::Compact) + "\n");
        m_ipcSocket->flush();
    }

    QString m_socketName;
    QString m_videoFile;
    QString m_ipcPath;
    QProcess* m_process;
    QLocalSocket* m_ipcSocket;
    bool m_stopped;
};

int main(int argc, char* argv[])
{
    QtWebEngineQuick::initialize();

    QGuiApplication app(argc, argv);

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <video-file>\n", argv[0]);
        return 1;
    }

    QString videoFile = QString::fromUtf8(argv[1]);
    QString socketName = QString("mpv-embed-%1").arg(app.applicationPid());

    MpvLauncher launcher(socketName, videoFile);
    InputForwarder inputForwarder;
    ViewporterHelper viewporterHelper;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("mpvLauncher", &launcher);
    engine.rootContext()->setContextProperty("inputForwarder", &inputForwarder);
    engine.rootContext()->setContextProperty("viewporterHelper", &viewporterHelper);
    engine.loadFromModule("Example", "Main");

    return app.exec();
}

#include "main.moc"
