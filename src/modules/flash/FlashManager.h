#pragma once

#include <QObject>
#include <QFileInfo>
#include "CliRunner.h"

struct FlashConfig {
    QString hexPath;
    QString modelName;
    QString architecture;
    QString quantization;
    QString targetBoard;
    bool    simulationMode = false;
};

// Coordinates firmware flashing: validates input, builds CLI arguments,
// drives CliRunner for real flashing or a QTimer for simulation mode.
class FlashManager : public QObject
{
    Q_OBJECT

public:
    explicit FlashManager(QObject *parent = nullptr);

    void    setCliPath(const QString &path);
    QString cliPath() const;

    // Searches well-known install paths for STM32_Programmer_CLI.exe
    static QString detectCliPath();

    bool validateConfig(const FlashConfig &config, QString &errorMsg) const;

public slots:
    void flash(const FlashConfig &config);
    void cancel();

signals:
    void outputLine(const QString &line);
    void errorLine(const QString &line);
    void progressChanged(int percent);
    void flashStarted(const FlashConfig &config);
    void flashFinished(bool success, const FlashConfig &config);

private:
    CliRunner   *m_runner = nullptr;
    FlashConfig  m_currentConfig;

    QStringList buildArgs(const FlashConfig &config) const;
    void        runSimulation(const FlashConfig &config);
};
