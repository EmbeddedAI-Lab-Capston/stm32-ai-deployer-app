#pragma once

#include <QList>
#include <QString>

#include "modules/board/BoardPresets.h"

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

    // Last directory used in the firmware file picker
    QString lastFirmwareDir() const;
    void    setLastFirmwareDir(const QString &dir);

    // User-defined boards shown together with built-in board presets
    QList<BoardInfo> customBoards() const;
    void             addCustomBoard(const BoardInfo &board);

    // arm-none-eabi-gcc full path
    QString gccPath() const;
    void    setGccPath(const QString &path);

    // make executable full path
    QString makePath() const;
    void    setMakePath(const QString &path);

    // Whether first-launch tool auto-detection has run
    bool toolsAutoDetected() const;
    void setToolsAutoDetected(bool value);

    // Last directory used in the AI model file picker
    QString lastModelDir() const;
    void    setLastModelDir(const QString &dir);

    // Last pipeline output directory
    QString lastOutputDir() const;
    void    setLastOutputDir(const QString &dir);

    // STM32CubeF4/H7/N6 SDK root (STM32Cube_FW_Fx_Vx.xx.x)
    QString cubeSdkPath() const;
    void    setCubeSdkPath(const QString &path);

private:
    static constexpr auto kKeyCliPath          = "programmer/cli_path";
    static constexpr auto kKeyComPort          = "serial/last_com_port";
    static constexpr auto kKeyBoard            = "board/last_board";
    static constexpr auto kKeyTheme            = "ui/theme";
    static constexpr auto kKeyXCubeAIPath      = "tools/xcubeai_cli_path";
    static constexpr auto kKeyFirmwareDir      = "flash/last_firmware_dir";
    static constexpr auto kKeyGccPath          = "tools/gcc_path";
    static constexpr auto kKeyMakePath         = "tools/make_path";
    static constexpr auto kKeyToolsAutoDetected = "tools/auto_detected";
    static constexpr auto kKeyLastModelDir     = "flash/last_model_dir";
    static constexpr auto kKeyLastOutputDir    = "flash/last_output_dir";
    static constexpr auto kKeyCubeSdkPath     = "tools/cube_sdk_path";
    static constexpr auto kArrayCustomBoards   = "boards/custom";
};
