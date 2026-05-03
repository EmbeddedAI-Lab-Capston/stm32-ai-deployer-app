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
    const QStringList validExts = {"hex", "bin", "elf"};
    if (!validExts.contains(ext)) {
        errorMsg = tr("Geçersiz dosya türü — .hex, .bin veya .elf seçin.");
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
    QStringList args = {"-c", "port=SWD"};

    // .bin files don't carry address info — provide the default STM32 flash base.
    // .hex and .elf embed address information, so the CLI parses it automatically.
    if (config.hexPath.endsWith(".bin", Qt::CaseInsensitive)) {
        args << "-d" << config.hexPath << "0x08000000";
    } else {
        args << "-d" << config.hexPath;
    }

    args << "-rst";
    return args;
}

void FlashManager::runSimulation(const FlashConfig &config)
{
    const QString filePath = config.hexPath.isEmpty()
                                 ? "firmware.elf"
                                 : config.hexPath;
    const QString fileName = QFileInfo(filePath).fileName();
    const QString fileExt  = QFileInfo(fileName).suffix().toUpper();

    QString parseMsg;
    QString addrMsg;
    if (fileExt == "ELF") {
        parseMsg = "> Opening and parsing file: " + fileName + " (ELF)";
        addrMsg  = "> Address: parsed from ELF header";
    } else if (fileExt == "HEX") {
        parseMsg = "> Opening and parsing file: " + fileName + " (Intel HEX)";
        addrMsg  = "> Address: parsed from Intel HEX";
    } else {
        parseMsg = "> Opening and parsing file: " + fileName + " (Binary)";
        addrMsg  = "> Address: 0x08000000 (default)";
    }

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
        parseMsg,
        "> Size: 96.0 KB",
        addrMsg,
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
