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

    // Last used baud rate (default 115200)
    int  lastBaud() const;
    void setLastBaud(int baud);

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

    // Last model deployed to the board through the pipeline
    QString deployedModelName() const;
    void    setDeployedModelName(const QString &name);
    QString deployedModelPath() const;
    void    setDeployedModelPath(const QString &path);
    QString deployedModelOutputDir() const;
    void    setDeployedModelOutputDir(const QString &dir);
    QString deployedSensorType() const;
    void    setDeployedSensorType(const QString &sensorType);

    // STM32CubeF4/H7/N6 SDK root (STM32Cube_FW_Fx_Vx.xx.x)
    QString cubeSdkPath() const;
    void    setCubeSdkPath(const QString &path);

    // Register Inspector: last selected peripherals per board (persisted so a
    // board reopens with the user's last selection). Stored as a JSON object
    // mapping board name -> array of peripheral names.
    QStringList registerPeripherals(const QString &boardName) const;
    void        setRegisterPeripherals(const QString &boardName,
                                       const QStringList &peripherals);

    // Optional override for the SVD directory; empty = exe-adjacent svd/.
    QString registerSvdDir() const;
    void    setRegisterSvdDir(const QString &dir);

    // Optional LLM diagnosis layer (Register Inspector Bolum 1c). Provider-
    // agnostic: any OpenAI-compatible chat/completions endpoint. Empty
    // baseUrl/apiKey means the feature is off — RegisterAdvisor treats that
    // as "not configured", not an error. Stored as plain QSettings text,
    // same as every other credential-free setting in this app; no encryption.
    QString llmBaseUrl() const;
    void    setLlmBaseUrl(const QString &url);
    QString llmApiKey() const;
    void    setLlmApiKey(const QString &key);
    QString llmModel() const;
    void    setLlmModel(const QString &model);

    // ST-LINK_gdbserver.exe full path (Degisken Izleyici / Register Inspector
    // GDB backend, docs/variable_watcher_plan.md Bolum 4.7)
    QString gdbServerPath() const;
    void    setGdbServerPath(const QString &path);

    // arm-none-eabi-nm.exe full path (Degisken Izleyici symbol layer)
    QString armNmPath() const;
    void    setArmNmPath(const QString &path);

    // Directory containing STM32_Programmer_CLI.exe — passed as gdbserver's -cp
    QString cubeProgrammerBinDir() const;
    void    setCubeProgrammerBinDir(const QString &dir);

    // gdbserver TCP port; 0 = pick an ephemeral free port automatically
    int  watchGdbPort() const;
    void setWatchGdbPort(int port);

    // Register Inspector read backend preference: "cli" | "gdb". Default is
    // PERMANENTLY "cli" — "gdb" is opt-in only, never the shipped default
    // (docs/variable_watcher_plan.md Bolum 5.2).
    QString registerReadBackend() const;
    void    setRegisterReadBackend(const QString &backend);

    // Variable Watcher (Faz 4). Item schema is owned by VariableWatcher —
    // AppSettings only stores/retrieves the raw JSON, same pattern as
    // registerPeripherals().
    QByteArray watchItemsJson(const QString &boardName) const;
    void       setWatchItemsJson(const QString &boardName, const QByteArray &json);

    QString lastWatchElfPath() const;
    void    setLastWatchElfPath(const QString &path);

    int  watchTargetRateHz() const;   // default 200
    void setWatchTargetRateHz(int hz);

private:
    static constexpr auto kKeyCliPath          = "programmer/cli_path";
    static constexpr auto kKeyComPort          = "serial/last_com_port";
    static constexpr auto kKeyBaud            = "serial/last_baud";
    static constexpr auto kKeyBoard            = "board/last_board";
    static constexpr auto kKeyTheme            = "ui/theme";
    static constexpr auto kKeyXCubeAIPath      = "tools/xcubeai_cli_path";
    static constexpr auto kKeyFirmwareDir      = "flash/last_firmware_dir";
    static constexpr auto kKeyGccPath          = "tools/gcc_path";
    static constexpr auto kKeyMakePath         = "tools/make_path";
    static constexpr auto kKeyToolsAutoDetected = "tools/auto_detected";
    static constexpr auto kKeyLastModelDir     = "flash/last_model_dir";
    static constexpr auto kKeyLastOutputDir    = "flash/last_output_dir";
    static constexpr auto kKeyDeployedModelName = "benchmark/deployed_model_name";
    static constexpr auto kKeyDeployedModelPath = "benchmark/deployed_model_path";
    static constexpr auto kKeyDeployedOutputDir = "benchmark/deployed_output_dir";
    static constexpr auto kKeyDeployedSensorType = "benchmark/deployed_sensor_type";
    static constexpr auto kKeyCubeSdkPath     = "tools/cube_sdk_path";
    static constexpr auto kArrayCustomBoards   = "boards/custom";
    static constexpr auto kKeyRegisterPeripherals = "registers/last_peripherals";
    static constexpr auto kKeyRegisterSvdDir   = "registers/svd_dir";
    static constexpr auto kKeyLlmBaseUrl       = "llm/base_url";
    static constexpr auto kKeyLlmApiKey        = "llm/api_key";
    static constexpr auto kKeyLlmModel         = "llm/model";
    static constexpr auto kKeyGdbServerPath    = "tools/gdbserver_path";
    static constexpr auto kKeyArmNmPath        = "tools/arm_nm_path";
    static constexpr auto kKeyCubeProgrammerBinDir = "tools/cubeprogrammer_bin_dir";
    static constexpr auto kKeyWatchGdbPort     = "watch/gdb_port";
    static constexpr auto kKeyRegisterReadBackend = "registers/read_backend";
    static constexpr auto kKeyWatchItems       = "watch/items";
    static constexpr auto kKeyLastWatchElfPath = "watch/last_elf_path";
    static constexpr auto kKeyWatchTargetRateHz = "watch/target_rate_hz";
};
