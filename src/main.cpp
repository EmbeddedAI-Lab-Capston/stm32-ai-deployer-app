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
#include "core/ToolDetector.h"
#include "modules/serial/SerialManager.h"
#include "modules/flash/FlashManager.h"
#include "modules/analysis/AnalysisManager.h"
#include "modules/simulation/FactorySimulator.h"
#include "modules/registers/RegisterInspector.h"
#include "modules/registers/RegisterAdvisor.h"
#include "modules/debug/DebugLink.h"
#include "modules/watcher/VariableWatcher.h"
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

    // Shared ST-Link debug connection (GDB Remote Serial Protocol over
    // ST-LINK_gdbserver.exe). One instance for the whole app — Register
    // Inspector's GDB backend (Faz 2) and the Variable Watcher (Faz 4) both
    // retain()/release() this same link instead of opening their own
    // (docs/variable_watcher_plan.md Bolum 2.1, 4.6). Constructed before
    // Backend so Backend can hold a reference too (needed to push a path
    // picked in Ayarlar mid-session — see Backend::setToolPath()).
    auto *debugLink = new DebugLink(&app);
    {
        AppSettings settings;

        QString gdbServerPath = settings.gdbServerPath();
        if (gdbServerPath.isEmpty() || !QFile::exists(gdbServerPath)) {
            gdbServerPath = ToolDetector::detectGdbServer();
            if (!gdbServerPath.isEmpty())
                settings.setGdbServerPath(gdbServerPath);
        }

        QString cubeProgrammerBinDir = settings.cubeProgrammerBinDir();
        if (cubeProgrammerBinDir.isEmpty() || !QDir(cubeProgrammerBinDir).exists()) {
            cubeProgrammerBinDir = ToolDetector::detectCubeProgrammerBinDir();
            if (!cubeProgrammerBinDir.isEmpty())
                settings.setCubeProgrammerBinDir(cubeProgrammerBinDir);
        }

        debugLink->setPaths(gdbServerPath, cubeProgrammerBinDir);

        // arm-none-eabi-nm.exe (Degisken Izleyici symbol layer, Faz 3/4).
        QString armNmPath = settings.armNmPath();
        if (armNmPath.isEmpty() || !QFile::exists(armNmPath)) {
            armNmPath = ToolDetector::detectArmNm();
            if (!armNmPath.isEmpty())
                settings.setArmNmPath(armNmPath);
        }
    }

    // Register Inspector's GDB backend (Faz 2) shares this same link — see
    // RegisterInspector::resolveActiveReader() for how it decides whether to
    // actually use it per snapshot.
    registers->setDebugLink(debugLink);

    // Variable Watcher (Faz 4) — same shared link, retained/released
    // explicitly via Backend::openWatchLink()/closeWatchLink(), never by
    // VariableWatcher itself (plan Bolum 7.4/7.5).
    auto *watcher = new VariableWatcher(debugLink, &app);

    auto *backend = new Backend(appState, serial, flash, analysis, registers, advisor, debugLink, watcher, &app);

    // Factory Simulation engine (synthetic large-factory data for the demo mode).
    auto *factorySim = new FactorySimulator(&app);

    // Process cleanup at exit is unconditional — shutdownNow() ignores the
    // reference count so a leaked retain() can never strand a gdbserver
    // process holding the ST-Link (plan Bolum 4.4 point 5, 4.6).
    QObject::connect(&app, &QApplication::aboutToQuit, debugLink, &DebugLink::shutdownNow);

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
