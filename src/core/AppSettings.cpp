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

QList<BoardInfo> AppSettings::customBoards() const
{
    QSettings s;
    QList<BoardInfo> boards;

    const int count = s.beginReadArray(kArrayCustomBoards);
    for (int i = 0; i < count; ++i) {
        s.setArrayIndex(i);
        BoardInfo board;
        board.name = s.value("name").toString();
        board.flashKb = s.value("flash_kb").toInt();
        board.ramKb = s.value("ram_kb").toInt();
        board.clockMhz = s.value("clock_mhz").toInt();
        board.isPreset = false;

        if (!board.name.trimmed().isEmpty())
            boards.append(board);
    }
    s.endArray();

    return boards;
}

QString AppSettings::gccPath() const
{
    QSettings s;
    return s.value(kKeyGccPath, QString{}).toString();
}

void AppSettings::setGccPath(const QString &path)
{
    QSettings s;
    s.setValue(kKeyGccPath, path);
}

QString AppSettings::makePath() const
{
    QSettings s;
    return s.value(kKeyMakePath, QString{}).toString();
}

void AppSettings::setMakePath(const QString &path)
{
    QSettings s;
    s.setValue(kKeyMakePath, path);
}

bool AppSettings::toolsAutoDetected() const
{
    QSettings s;
    return s.value(kKeyToolsAutoDetected, false).toBool();
}

void AppSettings::setToolsAutoDetected(bool value)
{
    QSettings s;
    s.setValue(kKeyToolsAutoDetected, value);
}

QString AppSettings::lastModelDir() const
{
    QSettings s;
    return s.value(kKeyLastModelDir, QDir::homePath()).toString();
}

void AppSettings::setLastModelDir(const QString &dir)
{
    QSettings s;
    s.setValue(kKeyLastModelDir, dir);
}

QString AppSettings::lastOutputDir() const
{
    QSettings s;
    return s.value(kKeyLastOutputDir, QDir::homePath()).toString();
}

void AppSettings::setLastOutputDir(const QString &dir)
{
    QSettings s;
    s.setValue(kKeyLastOutputDir, dir);
}

QString AppSettings::cubeSdkPath() const
{
    QSettings s;
    return s.value(kKeyCubeSdkPath, QString{}).toString();
}

void AppSettings::setCubeSdkPath(const QString &path)
{
    QSettings s;
    s.setValue(kKeyCubeSdkPath, path);
}

void AppSettings::addCustomBoard(const BoardInfo &board)
{
    if (board.name.trimmed().isEmpty())
        return;

    QList<BoardInfo> boards = customBoards();
    bool replaced = false;
    for (BoardInfo &existing : boards) {
        if (existing.name.compare(board.name, Qt::CaseInsensitive) == 0) {
            existing = board;
            existing.isPreset = false;
            replaced = true;
            break;
        }
    }

    if (!replaced) {
        BoardInfo custom = board;
        custom.isPreset = false;
        boards.append(custom);
    }

    QSettings s;
    s.beginWriteArray(kArrayCustomBoards);
    for (int i = 0; i < boards.size(); ++i) {
        s.setArrayIndex(i);
        s.setValue("name", boards.at(i).name);
        s.setValue("flash_kb", boards.at(i).flashKb);
        s.setValue("ram_kb", boards.at(i).ramKb);
        s.setValue("clock_mhz", boards.at(i).clockMhz);
    }
    s.endArray();
}
