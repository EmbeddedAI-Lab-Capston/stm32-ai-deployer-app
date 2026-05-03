#include <QApplication>
#include <QFile>
#include <QStyleFactory>
#include <QIcon>
#include <QTimer>
#include "mainwindow.h"
#include "core/AppSettings.h"
#include "ui/SplashScreen.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("STM32 AI Deployer");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("Marmara University");
    app.setOrganizationDomain("marmara.edu.tr");
    app.setWindowIcon(QIcon(":/app_icon.png"));

    // Load stylesheet
    QFile styleFile(":/style.qss");
    if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
        QString style = QLatin1String(styleFile.readAll());
        app.setStyleSheet(style);
        styleFile.close();
    }

    // Show splash screen
    SplashScreen *splash = new SplashScreen();
    splash->show();
    app.processEvents();

    // Create main window (hidden for now)
    MainWindow *window = new MainWindow();

    // When splash is done, show main window
    QObject::connect(splash, &SplashScreen::done, [splash, window]() {
        window->show();
        splash->hide();
        splash->deleteLater();
    });

    // Start the closing sequence after 3.5 seconds
    splash->startClosingSequence(3500);

    return app.exec();
}
