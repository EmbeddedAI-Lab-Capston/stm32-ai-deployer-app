#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QIcon>
#include <QFile>
#include <QTimer>

#include "core/AppState.h"
#include "core/AppSettings.h"
#include "modules/serial/SerialManager.h"
#include "modules/flash/FlashManager.h"
#include "modules/analysis/AnalysisManager.h"
#include "bridge/Backend.h"
#include "ui/SplashScreen.h"

int main(int argc, char *argv[])
{
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

    auto *backend = new Backend(appState, serial, flash, analysis, &app);

    // ── QML engine ─────────────────────────────────────────────────────────
    // The QML window is the primary window; closing the transient splash must
    // not quit the app.
    app.setQuitOnLastWindowClosed(true);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("appState", appState);
    engine.rootContext()->setContextProperty("backend",  backend);
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
