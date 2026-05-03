#include "AppSettings.h"

#include <QDir>
#include <QSettings>

AppSettings::AppSettings() = default;

QString AppSettings::programmerCliPath() const
{
    QSettings s;
    return s.value(kKeyCliPath, QString{}).toString();
}

void AppSettings::setProgrammerCliPath(const QString &path)
{
    QSettings s;
    s.setValue(kKeyCliPath, path);
}

QString AppSettings::lastComPort() const
{
    QSettings s;
    return s.value(kKeyComPort, QString{}).toString();
}

void AppSettings::setLastComPort(const QString &port)
{
    QSettings s;
    s.setValue(kKeyComPort, port);
}

QString AppSettings::lastBoard() const
{
    QSettings s;
    return s.value(kKeyBoard, QString{}).toString();
}

void AppSettings::setLastBoard(const QString &board)
{
    QSettings s;
    s.setValue(kKeyBoard, board);
}

QString AppSettings::theme() const
{
    QSettings s;
    return s.value(kKeyTheme, QStringLiteral("dark")).toString();
}

void AppSettings::setTheme(const QString &theme)
{
    QSettings s;
    s.setValue(kKeyTheme, theme);
}

QString AppSettings::xcubeAICliPath() const
{
    QSettings s;
    return s.value(kKeyXCubeAIPath, QString{}).toString();
}

void AppSettings::setXCubeAICliPath(const QString &path)
{
    QSettings s;
    s.setValue(kKeyXCubeAIPath, path);
}

QString AppSettings::lastFirmwareDir() const
{
    QSettings s;
    return s.value(kKeyFirmwareDir, QDir::homePath()).toString();
}

void AppSettings::setLastFirmwareDir(const QString &dir)
{
    QSettings s;
    s.setValue(kKeyFirmwareDir, dir);
}
