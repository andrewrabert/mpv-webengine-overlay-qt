#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLFramebufferObject>
#include <QRunnable>
#include <QDebug>
#include <QTimer>
#include <QtWebEngine/QtWebEngine>
#include <mpv/client.h>
#include <mpv/render_gl.h>
#include <clocale>
#include <cstdio>

// Get OpenGL proc address for MPV
static void* get_proc_address(void* ctx, const char* name)
{
    Q_UNUSED(ctx);
    QOpenGLContext* glctx = QOpenGLContext::currentContext();
    if (!glctx) return nullptr;
    return (void*)glctx->getProcAddress(QByteArray(name));
}

// Render job to trigger window updates from render thread
class RequestRepaintJob : public QRunnable
{
public:
    explicit RequestRepaintJob(QQuickWindow* window) : m_window(window) {}
    void run() override { m_window->update(); }
private:
    QQuickWindow* m_window;
};

// Forward declaration
class PlayerQuickItem;

// Renderer that draws MPV video to OpenGL framebuffer
class PlayerRenderer : public QObject
{
    Q_OBJECT
    friend class PlayerQuickItem;
public:
    PlayerRenderer(mpv_handle* mpv, QQuickWindow* window)
        : m_mpv(mpv), m_mpvGL(nullptr), m_window(window), m_size()
    {}

    bool init()
    {
        mpv_opengl_init_params opengl_params = {
            get_proc_address,
            nullptr
        };

        mpv_render_param params[] = {
            {MPV_RENDER_PARAM_API_TYPE, (void*)MPV_RENDER_API_TYPE_OPENGL},
            {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &opengl_params},
            {MPV_RENDER_PARAM_INVALID}
        };

        if (mpv_render_context_create(&m_mpvGL, m_mpv, params) >= 0) {
            mpv_render_context_set_update_callback(m_mpvGL, on_update, this);
            return true;
        }
        return false;
    }

    ~PlayerRenderer() override
    {
        if (m_mpvGL)
            mpv_render_context_free(m_mpvGL);
    }

    void render()
    {
        QOpenGLContext* context = QOpenGLContext::currentContext();
        if (!context) return;

        GLint fbo = 0;
        context->functions()->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fbo);

        m_window->resetOpenGLState();

        mpv_opengl_fbo mpv_fbo = {
            fbo,
            m_size.width(),
            m_size.height()
        };
        int flip = -1;
        mpv_render_param params[] = {
            {MPV_RENDER_PARAM_OPENGL_FBO, &mpv_fbo},
            {MPV_RENDER_PARAM_FLIP_Y, &flip},
            {MPV_RENDER_PARAM_INVALID}
        };
        mpv_render_context_render(m_mpvGL, params);

        m_window->resetOpenGLState();
    }

    void swap()
    {
        if (m_mpvGL)
            mpv_render_context_report_swap(m_mpvGL);
    }

    QSize m_size;

private:
    static void on_update(void* ctx)
    {
        PlayerRenderer* self = (PlayerRenderer*)ctx;
        if (self->thread() == self->m_window->thread())
            QMetaObject::invokeMethod(self->m_window, "update", Qt::QueuedConnection);
        else
            self->m_window->scheduleRenderJob(new RequestRepaintJob(self->m_window), QQuickWindow::NoStage);
    }

    mpv_handle* m_mpv;
    mpv_render_context* m_mpvGL;
    QQuickWindow* m_window;
};

// QML item that hooks into rendering pipeline
class PlayerQuickItem : public QQuickItem
{
    Q_OBJECT
public:
    explicit PlayerQuickItem(QQuickItem* parent = nullptr)
        : QQuickItem(parent), m_mpv(nullptr), m_renderer(nullptr)
    {
        connect(this, &QQuickItem::windowChanged, this, &PlayerQuickItem::onWindowChanged, Qt::DirectConnection);
    }

    ~PlayerQuickItem() override
    {
        if (m_renderer && m_renderer->m_mpvGL)
            mpv_render_context_set_update_callback(m_renderer->m_mpvGL, nullptr, nullptr);
    }

    void setMpvHandle(mpv_handle* mpv)
    {
        m_mpv = mpv;
        if (window())
            window()->update();
    }

private slots:
    void onWindowChanged(QQuickWindow* win)
    {
        if (win) {
            connect(win, &QQuickWindow::beforeSynchronizing, this, &PlayerQuickItem::onSynchronize, Qt::DirectConnection);
            connect(win, &QQuickWindow::sceneGraphInvalidated, this, &PlayerQuickItem::onInvalidate, Qt::DirectConnection);
        }
    }

    void onSynchronize()
    {
        if (!m_renderer && m_mpv) {
            m_renderer = new PlayerRenderer(m_mpv, window());
            if (!m_renderer->init()) {
                delete m_renderer;
                m_renderer = nullptr;
                qFatal("Could not initialize OpenGL");
                return;
            }

            // Hook into rendering pipeline
            connect(window(), &QQuickWindow::beforeRendering, m_renderer, &PlayerRenderer::render, Qt::DirectConnection);
            connect(window(), &QQuickWindow::frameSwapped, m_renderer, &PlayerRenderer::swap, Qt::DirectConnection);

            // Critical settings for overlay technique
            window()->setPersistentOpenGLContext(true);
            window()->setPersistentSceneGraph(true);
            window()->setClearBeforeRendering(false);  // Don't clear - video renders to background
        }

        if (m_renderer) {
            m_renderer->m_size = window()->size() * window()->devicePixelRatio();
        }
    }

    void onInvalidate()
    {
        if (m_renderer)
            delete m_renderer;
        m_renderer = nullptr;
    }

private:
    mpv_handle* m_mpv;
    PlayerRenderer* m_renderer;
};

int main(int argc, char* argv[])
{
    // Must initialize QtWebEngine before QGuiApplication
    QtWebEngine::initialize();

    QGuiApplication app(argc, argv);

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <video-file>\n", argv[0]);
        return 1;
    }

    const char* videoFile = argv[1];

    // Required for mpv to work correctly with number formatting
    // Must be set after QGuiApplication as Qt may override it
    setlocale(LC_NUMERIC, "C");

    // Create and initialize MPV (following JMP's initialization)
    mpv_handle* mpv = mpv_create();
    if (!mpv) {
        qFatal("Failed to create MPV instance");
        return 1;
    }

    // Set properties BEFORE initialization (like JMP does)
    mpv_set_option_string(mpv, "osd-level", "0");  // Disable OSD
    mpv_set_option_string(mpv, "ytdl", "no");      // Disable ytdl
    mpv_set_option_string(mpv, "audio-fallback-to-null", "yes");
    mpv_set_option_string(mpv, "terminal", "yes");
    mpv_set_option_string(mpv, "msg-level", "all=v");

    if (mpv_initialize(mpv) < 0) {
        qFatal("Failed to initialize MPV");
        return 1;
    }

    // Register QML type
    qmlRegisterType<PlayerQuickItem>("mpvtest", 1, 0, "MpvVideo");

    int result;
    {
        // Create QML engine and expose MPV handle
        QQmlApplicationEngine engine;
        PlayerQuickItem* videoItem = nullptr;

        // Load video file after QML is loaded
        QObject::connect(&engine, &QQmlApplicationEngine::objectCreated, [&](QObject* obj, const QUrl&) {
            if (obj) {
                QQuickWindow* window = qobject_cast<QQuickWindow*>(obj);
                if (window) {
                    // Set vo=libmpv AFTER window is ready (critical - must happen after window creation)
                    mpv_set_property_string(mpv, "vo", "libmpv");

                    videoItem = window->findChild<PlayerQuickItem*>("video");
                    if (videoItem) {
                        videoItem->setMpvHandle(mpv);

                        // Load video with small delay to ensure rendering pipeline is fully initialized
                        // JMP uses Qt::QueuedConnection pattern, but here we need more time for WebEngine
                        QTimer::singleShot(100, [videoFile, mpv]() {
                            const char* cmd[] = {"loadfile", videoFile, nullptr};
                            mpv_command_async(mpv, 0, cmd);
                        });
                    }
                }
            }
        });

        engine.load(QUrl(QStringLiteral("minimal.qml")));

        result = app.exec();
    } // engine destroyed here, before mpv cleanup

    mpv_terminate_destroy(mpv);
    return result;
}

#include "main.moc"
