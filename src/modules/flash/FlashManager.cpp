#include "FlashManager.h"

#include <QFile>
#include <QFileInfo>
#include <QTimer>
#include <QStringList>

FlashManager::FlashManager(QObject *parent)
    : QObject(parent)
    , m_runner(new CliRunner(this))
{
    connect(m_runner, &CliRunner::outputLine,
            this,     &FlashManager::outputLine);
    connect(m_runner, &CliRunner::errorLine,
            this,     &FlashManager::errorLine);
    connect(m_runner, &CliRunner::progressChanged,
            this,     &FlashManager::progressChanged);
    connect(m_runner, &CliRunner::finished,
            this, [this](bool success, int) {
                emit flashFinished(success, m_currentConfig);
            });
}

void FlashManager::setCliPath(const QString &path)
{
    m_runner->setCliPath(path);
}

QString FlashManager::cliPath() const
{
    return m_runner->cliPath();
}

QString FlashManager::detectCliPath()
{
    const QStringList candidates = {
        "C:/Program Files/STMicroelectronics/STM32Cube/"
        "STM32CubeProgrammer/bin/STM32_Programmer_CLI.exe",
        "C:/Program Files (x86)/STMicroelectronics/STM32Cube/"
        "STM32CubeProgrammer/bin/STM32_Programmer_CLI.exe",
        "C:/STM32CubeProgrammer/bin/STM32_Programmer_CLI.exe",
    };
    for (const QString &path : candidates) {
        if (QFile::exists(path))
            return path;
    }
    return QString();
}

bool FlashManager::validateConfig(const FlashConfig &config,
                                   QString &errorMsg) const
{
    if (config.simulationMode) return true;

    if (config.modelName.trimmed().isEmpty()) {
        errorMsg = tr("Model adı boş olamaz.");
        return false;
    }
    if (config.hexPath.isEmpty()) {
        errorMsg = tr("Firmware dosyası seçilmedi.");
        return false;
    }
    const QFileInfo fi(config.hexPath);
    if (!fi.exists()) {
        errorMsg = tr("Dosya bulunamadı: ") + config.hexPath;
        return false;
    }
    const QString ext = fi.suffix().toLower();
    if (ext != "hex" && ext != "bin") {
        errorMsg = tr("Geçersiz dosya türü — .hex veya .bin seçin.");
        return false;
    }
    if (m_runner->cliPath().isEmpty()) {
        errorMsg = tr("STM32_Programmer_CLI yolu ayarlanmamış.\n"
                      "Araçlar → Ayarlar menüsünden yolu girin.");
        return false;
    }
    return true;
}

void FlashManager::flash(const FlashConfig &config)
{
    m_currentConfig = config;

    QString errMsg;
    if (!validateConfig(config, errMsg)) {
        emit errorLine(tr("Doğrulama hatası: ") + errMsg);
        emit flashFinished(false, config);
        return;
    }

    emit flashStarted(config);

    if (config.simulationMode) {
        runSimulation(config);
        return;
    }

    const QStringList args = buildArgs(config);
    emit outputLine(QString("> Komut: STM32_Programmer_CLI %1")
                        .arg(args.join(" ")));
    m_runner->run(args);
}

void FlashManager::cancel()
{
    m_runner->cancel();
}

QStringList FlashManager::buildArgs(const FlashConfig &config) const
{
    return {
        "-c", "port=SWD",
        "-d", config.hexPath,
        "-rst"
    };
}

void FlashManager::runSimulation(const FlashConfig &config)
{
    const QString fileName = QFileInfo(
        config.hexPath.isEmpty() ? "test_model.hex" : config.hexPath).fileName();

    const QStringList fakeOutput = {
        "> [Simülasyon Modu] Gerçek flash atılmıyor.",
        "> STM32CubeProgrammer v2.15.0 (simulated)",
        "> -------------------------------------------",
        "> Connecting to ST-LINK...",
        "> ST-LINK SN: 066DFF303030303030303031",
        "> ST-LINK FW: V3J112M27",
        "> Board: " + config.targetBoard,
        "> Voltage: 3.28V",
        "> SWD freq: 4000 KHz",
        "> Connect mode: Under Reset",
        "> Reset: Enabled",
        "> -------------------------------------------",
        "> Memory Programming...",
        "> File: " + fileName,
        "> Size: 96.0 KB",
        "> Address: 0x08000000",
        "> Erasing memory corresponding to sector ...",
        "> Download in Progress:",
        "> [====================] 100%",
        "> File download complete",
        "> Time elapsed: 00:00:03",
        "> Verifying ...",
        "> Read progress:",
        "> [====================] 100%",
        "> Download verified successfully",
        "> -------------------------------------------",
        "> Programming Complete. Model: " + config.modelName,
    };

    auto *timer = new QTimer(this);
    int step = 0;
    const int total = fakeOutput.size();

    connect(timer, &QTimer::timeout,
            this, [=]() mutable {
                if (step < total) {
                    emit outputLine(fakeOutput[step]);
                    emit progressChanged((step * 100) / total);
                    ++step;
                } else {
                    timer->stop();
                    timer->deleteLater();
                    emit progressChanged(100);
                    emit flashFinished(true, config);
                }
            });

    timer->start(120);
}
