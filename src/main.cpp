#include <QApplication>
#include <QFile>
#include <QStyleFactory>
#include "mainwindow.h"
#include "core/AppSettings.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("STM32 AI Deployer");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("Marmara University");
    app.setOrganizationDomain("marmara.edu.tr");

    // Load stylesheet
    QFile styleFile(":/style.qss");
    if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
        QString style = QLatin1String(styleFile.readAll());
        app.setStyleSheet(style);
        styleFile.close();
    }

    MainWindow window;
    window.show();

    return app.exec();
}
