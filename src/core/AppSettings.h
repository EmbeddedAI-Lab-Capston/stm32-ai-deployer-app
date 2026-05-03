#pragma once

#include <QString>

// Thin wrapper around QSettings for type-safe access to application preferences.
class AppSettings
{
public:
    AppSettings();
    ~AppSettings() = default;

    // STM32_Programmer_CLI.exe full path
    QString programmerCliPath() const;
    void    setProgrammerCliPath(const QString &path);

    // Last used COM port (e.g. "COM3")
    QString lastComPort() const;
    void    setLastComPort(const QString &port);

    // Last selected board preset name
    QString lastBoard() const;
    void    setLastBoard(const QString &board);

    // UI theme: "light" or "dark"
    QString theme() const;
    void    setTheme(const QString &theme);

    // stm32ai.exe full path (X-CUBE-AI CLI)
    QString xcubeAICliPath() const;
    void    setXCubeAICliPath(const QString &path);

private:
    static constexpr auto kKeyCliPath       = "programmer/cli_path";
    static constexpr auto kKeyComPort       = "serial/last_com_port";
    static constexpr auto kKeyBoard         = "board/last_board";
    static constexpr auto kKeyTheme         = "ui/theme";
    static constexpr auto kKeyXCubeAIPath   = "tools/xcubeai_cli_path";
};
