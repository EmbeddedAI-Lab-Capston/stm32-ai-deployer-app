#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QIcon>
#include <QFile>
#include <QTimer>
#include <QDateTime>
#include <QTextStream>
#include <QDir>
#include <QMutex>

#include "core/AppState.h"
#include "core/AppSettings.h"
#include "modules/serial/SerialManager.h"
#include "modules/flash/FlashManager.h"
#include "modules/analysis/AnalysisManager.h"
#include "modules/simulation/FactorySimulator.h"
#include "modules/registers/RegisterInspector.h"
#include "modules/registers/RegisterAdvisor.h"
#include "bridge/Backend.h"
#include "ui/SplashScreen.h"

// Persistent trace log next to the executable: every qDebug()/qWarning()/QML
// console.log() call lands here, timestamped, appended across runs. Lets you
// inspect what actually happened (including QML-side console.log tracing)
// without needing QT_FORCE_STDERR_LOGGING + a fresh capture each time — just
// read app_trace.log. Not gated behind a build flag; the write cost is
// negligible and having it always on is the point.
static void fileTraceHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    static QMutex mutex;
    QMutexLocker locker(&mutex);
    static QFile logFile(QDir(QCoreApplication::applicationDirPath()).filePath("app_trace.log"));
    if (!logFile.isOpen())
        logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    const char *level = "DEBUG";
    switch (type) {
    case QtWarningMsg:  level = "WARN";  break;
    case QtCriticalMsg: level = "CRIT";  break;
    case QtFatalMsg:    level = "FATAL"; break;
    default: break;
    }
    QTextStream ts(&logFile);
    ts << QDateTime::currentDateTime().toString("HH:mm:ss.zzz") << " [" << level << "] " << msg << "\n";
    ts.flush();
    Q_UNUSED(context);
}

int main(int argc, char *argv[])
{
    qInstallMessageHandler(fileTraceHandler);
    QApplication app(argc, argv);
    app.setApplicationName("STM32 AI Deployer");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("Marmara University");
    app.setOrganizationDomain("marmara.edu.tr");
    app.setWindowIcon(QIcon(":/app_icon.png"));

    // QtQuick.Controls customisation requires a non-native style.
    QQuickStyle::setStyle("Basic");

    // ── Core objects (owned by the app, outlive the engine) ────────────────
    auto *appState  = new AppState(&app);
    auto *serial    = new SerialManager(&app);
    auto *flash     = new FlashManager(&app);
    auto *analysis  = new AnalysisManager(&app);

    // Restore persisted baud rate so AppState starts with the last-used value.
    {
        AppSettings settings;
        const int lastBaud = settings.lastBaud(); // defaults to 115200
        appState->setActiveBaud(static_cast<qint32>(lastBaud));
    }

    // Resolve programmer CLI path (same bootstrap as the old MainWindow).
    {
        AppSettings settings;
        QString cliPath = settings.programmerCliPath();
        if (cliPath.isEmpty() || !QFile::exists(cliPath)) {
            cliPath = FlashManager::detectCliPath();
            if (!cliPath.isEmpty())
                settings.setProgrammerCliPath(cliPath);
        }
        flash->setCliPath(cliPath);
    }

    // Register Inspector orchestrator (owns SVD catalog + reader). Backend is the
    // only QML-facing facade, so the inspector is handed to it, not to QML.
    auto *registers = new RegisterInspector(&app);
    // Optional LLM diagnosis layer (Bolum 1c) — inert until Ayarlar provides a
    // base URL + API key; every other register feature works without it.
    auto *advisor = new RegisterAdvisor(&app);

    auto *backend = new Backend(appState, serial, flash, analysis, registers, advisor, &app);

    // Factory Simulation engine (synthetic large-factory data for the demo mode).
    auto *factorySim = new FactorySimulator(&app);

    // ── QML engine ─────────────────────────────────────────────────────────
    // The QML window is the primary window; closing the transient splash must
    // not quit the app.
    app.setQuitOnLastWindowClosed(true);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("appState", appState);
    engine.rootContext()->setContextProperty("backend",  backend);
    engine.rootContext()->setContextProperty("factorySim", factorySim);
    engine.loadFromModule("STM32AiDeployer", "Main");

    if (engine.rootObjects().isEmpty())
        return -1;

    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());

    // ── Splash screen (kept from the old app) ──────────────────────────────
    auto *splash = new SplashScreen();
    splash->show();
    app.processEvents();

    QObject::connect(splash, &SplashScreen::done, &app, [splash, window]() {
        if (window) {
            window->show();
            window->raise();
            window->requestActivate();
        }
        splash->close();
        splash->deleteLater();
    });
    splash->startClosingSequence(3500);

    return app.exec();
}
