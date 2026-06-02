#include "PipelineRunner.h"

#include <QCoreApplication>
#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QStringList>

#include "CliRunner.h"
#include "XCubeAIRunner.h"
#include "core/TemplateEngine.h"

// Returns the first existing path that matches a wildcard pattern in a parent dir.
static QString findSdkDir(const QString &parent, const QString &namePattern)
{
    const QDir parentDir(parent);
    if (!parentDir.exists()) return {};
    const QStringList entries = parentDir.entryList(
        QStringList{namePattern}, QDir::Dirs, QDir::Name | QDir::Reversed);
    if (entries.isEmpty()) return {};
    return QDir::cleanPath(parent + "/" + entries.first());
}

static QString sdkPatternForBoard(const QString &board)
{
    if (board.contains("H7", Qt::CaseInsensitive))
        return QStringLiteral("STM32Cube_FW_H7_*");
    if (board.contains("N6", Qt::CaseInsensitive))
        return QStringLiteral("STM32Cube_FW_N6_*");
    return QStringLiteral("STM32Cube_FW_F4_*");
}

static QString boardFamily(const QString &board)
{
    if (board.contains("H7", Qt::CaseInsensitive))
        return QStringLiteral("H7");
    if (board.contains("N6", Qt::CaseInsensitive))
        return QStringLiteral("N6");
    return QStringLiteral("F4");
}

static QString templateBoardDir(const QString &board)
{
    const QString family = boardFamily(board);
    if (family == "N6")
        return QStringLiteral("STM32N6");
    if (family == "H7")
        return QStringLiteral("STM32H7");
    return QStringLiteral("STM32F4");
}

static bool outputLooksLikeFamily(const QString &output, const QString &family)
{
    const QString upper = output.toUpper();
    if (family == "F4")
        return upper.contains("STM32F4");
    if (family == "H7")
        return upper.contains("STM32H7");
    if (family == "N6") {
        return upper.contains("STM32N6")
               || upper.contains("STM32N65")
               || upper.contains("N657")
               || upper.contains("N655")
               || upper.contains("NUCLEO-N657")
               || upper.contains("CORTEX-M55");
    }
    return false;
}

static QStringList programmerConnectArgsForBoard(const QString &programmerCliPath,
                                                 const QString &targetBoard)
{
    QStringList args{QStringLiteral("port=SWD")};

    QProcess list;
    list.start(programmerCliPath, {QStringLiteral("-l")});
    if (!list.waitForFinished(10000))
        return args;

    const QString output = QString::fromLocal8Bit(list.readAllStandardOutput()
                                                  + list.readAllStandardError());
    const QString family = boardFamily(targetBoard);
    QString currentSn;
    QString currentBlock;

    auto flushBlock = [&]() -> QString {
        if (!currentSn.isEmpty() && outputLooksLikeFamily(currentBlock, family))
            return currentSn;
        return {};
    };

    for (const QString &line : output.split(QRegularExpression("[\\r\\n]+"),
                                            Qt::SkipEmptyParts)) {
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith("ST-LINK SN", Qt::CaseInsensitive)
            || trimmed.startsWith("STLink SN", Qt::CaseInsensitive)) {
            const QString found = flushBlock();
            if (!found.isEmpty()) {
                args << QStringLiteral("sn=%1").arg(found);
                return args;
            }
            currentSn = trimmed.section(':', 1).trimmed();
            currentBlock = trimmed + QLatin1Char('\n');
        } else {
            currentBlock += trimmed + QLatin1Char('\n');
        }
    }

    const QString found = flushBlock();
    if (!found.isEmpty())
        args << QStringLiteral("sn=%1").arg(found);
    return args;
}

static QStringList n6ExternalFlashConnectArgs(const QStringList &baseArgs)
{
    QStringList args;
    bool hasFreq = false;
    for (const QString &arg : baseArgs) {
        if (arg.startsWith(QStringLiteral("mode="), Qt::CaseInsensitive))
            continue;
        if (arg.startsWith(QStringLiteral("freq="), Qt::CaseInsensitive))
            hasFreq = true;
        args << arg;
    }
    args << QStringLiteral("mode=HOTPLUG");
    if (!hasFreq)
        args << QStringLiteral("freq=8000");
    return args;
}

static QStringList requiredTemplateFiles(const QString &board)
{
    const QString family = boardFamily(board);
    if (family == "H7") {
        return {
            QStringLiteral("Makefile"),
            QStringLiteral("Src/main.c"),
            QStringLiteral("Src/stm32h7xx_hal_msp.c"),
            QStringLiteral("Src/stm32h7xx_it.c"),
            QStringLiteral("startup_stm32h723xx.s"),
            QStringLiteral("STM32H723ZGTx_FLASH.ld"),
        };
    }
    if (family == "N6") {
        return {
            QStringLiteral("Makefile"),
            QStringLiteral("Src/main.c"),
            QStringLiteral("Src/stm32n6xx_hal_msp.c"),
            QStringLiteral("Src/stm32n6xx_it.c"),
            QStringLiteral("startup_stm32n6xx.s"),
            QStringLiteral("STM32N6xx_FLASH.ld"),
        };
    }
    return {
        QStringLiteral("Makefile"),
        QStringLiteral("Src/main.c"),
        QStringLiteral("Src/stm32f4xx_hal_msp.c"),
        QStringLiteral("Src/stm32f4xx_it.c"),
        QStringLiteral("startup_stm32f407xx.s"),
        QStringLiteral("STM32F407VGTx_FLASH.ld"),
    };
}

static QString aiMiddlewarePathFromCli(const QString &cliPath)
{
    if (!cliPath.isEmpty()) {
        QDir dir = QFileInfo(cliPath).absoluteDir();
        for (int i = 0; i < 6; ++i) {
            const QString candidate = dir.absoluteFilePath("Middlewares/ST/AI");
            if (QDir(candidate).exists())
                return QDir::cleanPath(candidate);
            if (!dir.cdUp())
                break;
        }
    }

    const QString packsBase = QDir::homePath()
        + "/STM32Cube/Repository/Packs/STMicroelectronics/X-CUBE-AI";
    const QDir packsDir(packsBase);
    if (packsDir.exists()) {
        const QStringList versions = packsDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot,
                                                        QDir::Name | QDir::Reversed);
        for (const QString &version : versions) {
            const QString candidate = packsDir.absoluteFilePath(version + "/Middlewares/ST/AI");
            if (QDir(candidate).exists())
                return QDir::cleanPath(candidate);
        }
    }
    return {};
}

static bool connectedTargetMatchesBoard(const QString &programmerCliPath,
                                        const QString &targetBoard,
                                        QStringList &connectArgs,
                                        QString &summary,
                                        QString &errorMessage)
{
    if (programmerCliPath.isEmpty()) {
        errorMessage = QStringLiteral("STM32_Programmer_CLI yolu bos.");
        return false;
    }

    connectArgs = programmerConnectArgsForBoard(programmerCliPath, targetBoard);

    QStringList args{QStringLiteral("-c")};
    args += connectArgs;

    QProcess probe;
    probe.start(programmerCliPath, args);
    if (!probe.waitForFinished(10000)) {
        probe.kill();
        errorMessage = QStringLiteral("Bagli kart bilgisi okunamadi (timeout).");
        return false;
    }

    const QString output = QString::fromLocal8Bit(probe.readAllStandardOutput()
                                                  + probe.readAllStandardError());
    QStringList interesting;
    for (const QString &line : output.split(QRegularExpression("[\\r\\n]+"),
                                            Qt::SkipEmptyParts)) {
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith("Board", Qt::CaseInsensitive)
            || trimmed.startsWith("Device name", Qt::CaseInsensitive)
            || trimmed.startsWith("Device ID", Qt::CaseInsensitive)
            || trimmed.startsWith("Device CPU", Qt::CaseInsensitive)) {
            interesting.append(trimmed);
        }
    }
    summary = interesting.join(QStringLiteral("\n"));

    if (probe.exitCode() != 0) {
        errorMessage = QStringLiteral("STM32_Programmer_CLI karta baglanamadi.");
        return false;
    }

    const QString family = boardFamily(targetBoard);
    const QString upper = output.toUpper();
    const bool matched = outputLooksLikeFamily(output, family);

    if (!matched) {
        errorMessage = QStringLiteral("Bagli kart hedef kart ailesiyle uyusmuyor.");
        if (family == "N6" && (upper.contains("NUCLEO-H")
                               || upper.contains("STM32H7")
                               || upper.contains("STM32H72")
                               || upper.contains("STM32H73"))) {
            errorMessage += QStringLiteral(
                "\n  STM32_Programmer_CLI N6 yerine H7 kartini goruyor. "
                "NUCLEO-H723ZG bagliysa cikarin veya NUCLEO-N657X0-Q'nun ST-LINK USB'sini baglayin.");
        }
        return false;
    }

    return true;
}

static bool runBlockingTool(const QString &program,
                            const QStringList &args,
                            int timeoutMs,
                            QString &output)
{
    QProcess process;
    process.start(program, args);
    if (!process.waitForFinished(timeoutMs)) {
        process.kill();
        output = QStringLiteral("Timeout: ") + program + QLatin1Char(' ')
                 + args.join(QLatin1Char(' '));
        return false;
    }
    output = QString::fromLocal8Bit(process.readAllStandardOutput()
                                    + process.readAllStandardError());
    return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}

static quint32 readLe32(const QByteArray &bytes, int offset)
{
    if (bytes.size() < offset + 4)
        return 0;
    return static_cast<quint8>(bytes.at(offset))
           | (static_cast<quint8>(bytes.at(offset + 1)) << 8)
           | (static_cast<quint8>(bytes.at(offset + 2)) << 16)
           | (static_cast<quint8>(bytes.at(offset + 3)) << 24);
}

static bool readVectorInfo(const QString &binPath,
                           quint32 &loadAddress,
                           quint32 &entryPoint,
                           QString &errorMessage)
{
    QFile file(binPath);
    if (!file.open(QIODevice::ReadOnly)) {
        errorMessage = QStringLiteral("Binary okunamadi: ") + binPath;
        return false;
    }
    const QByteArray header = file.read(8);
    if (header.size() < 8) {
        errorMessage = QStringLiteral("Binary vektor tablosu okunamadi: ") + binPath;
        return false;
    }
    Q_UNUSED(readLe32(header, 0));
    entryPoint = readLe32(header, 4);

    if (binPath.contains(QStringLiteral("FSBL"), Qt::CaseInsensitive))
        loadAddress = 0x34180400U;
    else
        loadAddress = 0x34000400U;
    return true;
}

static bool ensureN6FsblLrunLayout(const QString &fsblProject, QString &errorMessage)
{
    const QString confPath = QDir(fsblProject).filePath(
        QStringLiteral("../../FSBL/Inc/stm32_extmem_conf.h"));
    QFile file(confPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        errorMessage = QStringLiteral("FSBL extmem konfig okunamadi: ") + confPath;
        return false;
    }

    QString text = QString::fromUtf8(file.readAll());
    file.close();

    static const QRegularExpression destinationRe(
        QStringLiteral("#define\\s+EXTMEM_LRUN_DESTINATION_ADDRESS\\s+0x[0-9A-Fa-f]+"));
    static const QRegularExpression sourceSizeRe(
        QStringLiteral("#define\\s+EXTMEM_LRUN_SOURCE_SIZE\\s+0x[0-9A-Fa-f]+"));
    static const QRegularExpression headerOffsetRe(
        QStringLiteral("#define\\s+EXTMEM_HEADER_OFFSET\\s+0x[0-9A-Fa-f]+"));
    if (!text.contains(destinationRe) || !text.contains(sourceSizeRe) || !text.contains(headerOffsetRe)) {
        errorMessage = QStringLiteral("FSBL LRUN adres konfig bulunamadi: ") + confPath;
        return false;
    }

    QString patched = text;
    patched.replace(destinationRe,
                    QStringLiteral("#define EXTMEM_LRUN_DESTINATION_ADDRESS  0x34000000"));
    patched.replace(sourceSizeRe,
                    QStringLiteral("#define EXTMEM_LRUN_SOURCE_SIZE 0x00100000"));
    patched.replace(headerOffsetRe,
                    QStringLiteral("#define EXTMEM_HEADER_OFFSET 0x400"));
    if (patched == text)
        return true;

    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        errorMessage = QStringLiteral("FSBL extmem konfig yazilamadi: ") + confPath;
        return false;
    }
    file.write(patched.toUtf8());
    return true;
}

static QString cubeProgrammerBinDir(const QString &programmerCliPath)
{
    return QFileInfo(programmerCliPath).absoluteDir().absolutePath();
}

static QString signingToolPath(const QString &programmerCliPath)
{
    return QDir(cubeProgrammerBinDir(programmerCliPath))
        .filePath(QStringLiteral("STM32_SigningTool_CLI.exe"));
}

static QString externalLoaderPath(const QString &programmerCliPath)
{
    return QDir(cubeProgrammerBinDir(programmerCliPath))
        .filePath(QStringLiteral("ExternalLoader/MX25UM51245G_STM32N6570-NUCLEO.stldr"));
}

static QString cubeIdeCliPath(const QString &gccPath)
{
    QString normalized = QDir::fromNativeSeparators(gccPath);
    const int marker = normalized.indexOf(QStringLiteral("/STM32CubeIDE/"),
                                          0,
                                          Qt::CaseInsensitive);
    if (marker >= 0) {
        const QString candidate = normalized.left(marker + QStringLiteral("/STM32CubeIDE").size())
            + QStringLiteral("/stm32cubeidec.exe");
        if (QFileInfo::exists(candidate))
            return QDir::toNativeSeparators(candidate);
    }

    const QString fallback =
        QStringLiteral("C:/ST/STM32CubeIDE_1.19.0/STM32CubeIDE/stm32cubeidec.exe");
    if (QFileInfo::exists(fallback))
        return QDir::toNativeSeparators(fallback);
    return {};
}

PipelineRunner::PipelineRunner(QObject *parent)
    : QObject(parent)
{}

PipelineRunner::~PipelineRunner()
{
    cancel();
}

// ── Public API ─────────────────────────────────────────────────────────────

void PipelineRunner::run(const PipelineConfig &config)
{
    if (m_running) return;

    m_config    = config;
    m_running   = true;
    m_cancelled = false;

    emit progressChanged(0);
    stepAnalyze();
}

void PipelineRunner::cancel()
{
    m_cancelled = true;
    if (m_xcubeRunner) m_xcubeRunner->cancel();
    if (m_buildRunner) m_buildRunner->cancel();
    if (m_flashRunner) m_flashRunner->cancel();
}

// ── Step 1: X-CUBE-AI code generation ─────────────────────────────────────

void PipelineRunner::stepAnalyze()
{
    emit stageChanged(tr("1/5 - X-CUBE-AI: model uyumlulugu kontrol ediliyor..."));
    emit progressChanged(5);

    const QString xcubeOut = m_config.outputDir + "/xcubeai_output";

    m_xcubeRunner = new XCubeAIRunner(this);
    m_xcubeRunner->setCliPath(m_config.xcubeCliPath);

    connect(m_xcubeRunner, &XCubeAIRunner::outputLine,
            this, &PipelineRunner::outputLine);
    connect(m_xcubeRunner, &XCubeAIRunner::errorLine,
            this, &PipelineRunner::errorLine);
    connect(m_xcubeRunner, &XCubeAIRunner::progressChanged,
            this, [this](int p) { emit progressChanged(5 + p / 10); });
    connect(m_xcubeRunner, &XCubeAIRunner::finished,
            this, &PipelineRunner::onXCubeAnalyzeFinished);

    m_xcubeRunner->analyze(m_config.modelPath,
                           m_config.targetBoard,
                           m_config.quantization,
                           xcubeOut);
}

void PipelineRunner::onXCubeAnalyzeFinished(const XCubeAIResult &result)
{
    if (m_cancelled) { emit finished(false); return; }

    if (!result.success) {
        fail(tr("X-CUBE-AI analiz adimi basarisiz. Model desteklenmiyor veya donusturme gerekiyor."));
        if (!result.errorMessage.isEmpty())
            emit errorLine(result.errorMessage);
        return;
    }

    emit outputLine(tr("Model X-CUBE-AI tarafinda destekleniyor, C kodu uretimine geciliyor."));
    emit progressChanged(15);

    m_xcubeRunner->deleteLater();
    m_xcubeRunner = nullptr;
    stepXCubeAI();
}

void PipelineRunner::stepXCubeAI()
{
    emit stageChanged(tr("2/5 - X-CUBE-AI: C kodu uretiliyor..."));
    emit progressChanged(20);

    const QString xcubeOut = m_config.outputDir + "/xcubeai_output";

    m_xcubeRunner = new XCubeAIRunner(this);
    m_xcubeRunner->setCliPath(m_config.xcubeCliPath);

    connect(m_xcubeRunner, &XCubeAIRunner::outputLine,
            this, &PipelineRunner::outputLine);
    connect(m_xcubeRunner, &XCubeAIRunner::errorLine,
            this, &PipelineRunner::errorLine);
    connect(m_xcubeRunner, &XCubeAIRunner::progressChanged,
            this, [this](int p) { emit progressChanged(20 + p / 5); });
    connect(m_xcubeRunner, &XCubeAIRunner::finished,
            this, &PipelineRunner::onXCubeFinished);

    m_xcubeRunner->generate(m_config.modelPath,
                            m_config.targetBoard,
                            m_config.quantization,
                            xcubeOut);
}

void PipelineRunner::onXCubeFinished(const XCubeAIResult &result)
{
    if (m_cancelled) { emit finished(false); return; }

    if (!result.success) {
        fail(tr("✗ X-CUBE-AI C kodu üretilemedi."));
        if (!result.errorMessage.isEmpty())
            emit errorLine(result.errorMessage);
        return;
    }

    emit outputLine(tr("✓ C kodu üretildi: ") + result.outputDir);
    emit progressChanged(35);
    stepPrepare();
}

// ── Step 2: Template project preparation ──────────────────────────────────

void PipelineRunner::stepPrepare()
{
    emit stageChanged(tr("3/5 - Sablon proje hazirlaniyor..."));
    emit progressChanged(40);

    if (m_cancelled) { emit finished(false); return; }

    // Resolve the templates/ root: first look next to the .exe (deployed),
    // then walk up to the project source tree (development build).
    auto findTemplatesRoot = []() -> QString {
        const QString appDir = QCoreApplication::applicationDirPath();
        const QStringList candidates = {
            appDir + "/../templates",
            appDir + "/../../templates",
            appDir + "/../../../templates",
            appDir + "/templates",
        };
        for (const QString &c : candidates)
            if (QDir(c).exists()) return QDir::cleanPath(c);
        return appDir + "/templates"; // fallback — will produce a clear error
    };

    const QString tmplBase    = findTemplatesRoot();
    const QString boardTmpl   = tmplBase + "/base/" + templateBoardDir(m_config.targetBoard);
    const QString sensorTmpl  = tmplBase + "/sensors/" + m_config.sensorType;
    const QString aiGlueDir   = tmplBase + "/ai_glue";

    if (!QDir(boardTmpl).exists()) {
        fail(tr("Sablon bulunamadi: ") + boardTmpl);
        return;
    }

    QStringList missingTemplateFiles;
    for (const QString &relPath : requiredTemplateFiles(m_config.targetBoard)) {
        if (!QFile::exists(boardTmpl + "/" + relPath))
            missingTemplateFiles.append(relPath);
    }
    if (!missingTemplateFiles.isEmpty()) {
        fail(tr("Secilen kart sablonu derleme icin eksik: ")
             + m_config.targetBoard + "\n"
             + tr("Eksik dosyalar: ")
             + missingTemplateFiles.join(QStringLiteral(", ")));
        return;
    }

    // Placeholder variables for the template engine
    const QString sensorLower = m_config.sensorType.toLower();
    QMap<QString, QString> vars;
    vars["MODEL_NAME"]    = m_config.modelName;
    vars["BOARD_NAME"]    = m_config.targetBoard;
    vars["SENSOR_TYPE"]   = m_config.sensorType;
    vars["SENSOR_TYPE_LOWER"] = sensorLower;
    vars["I2C_INSTANCE"]  = m_config.i2cInstance;
    vars["SDA_PORT"]      = m_config.sdaPort;
    vars["SDA_PIN"]       = m_config.sdaPin;
    vars["SCL_PORT"]      = m_config.sclPort;
    vars["SCL_PIN"]       = m_config.sclPin;
    vars["I2C_ADDRESS"]   = m_config.i2cAddress;
    vars["SAI_INSTANCE"]  = m_config.saiInstance;
    vars["CLK_PORT"]      = m_config.clkPort;
    vars["CLK_PIN"]       = m_config.clkPin;
    vars["DATA_PORT"]     = m_config.dataPort;
    vars["DATA_PIN"]      = m_config.dataPin;

    QString errMsg;

    // 1. Board base template → outputDir
    if (!TemplateEngine::instantiate(boardTmpl, m_config.outputDir, vars, errMsg)) {
        fail(tr("Şablon hatası: ") + errMsg);
        return;
    }

    // 2. Sensor template → outputDir/Src  (headers go to Inc)
    if (QDir(sensorTmpl).exists()) {
        if (!TemplateEngine::instantiate(sensorTmpl,
                m_config.outputDir + "/Src", vars, errMsg)) {
            fail(tr("Sensör şablon hatası: ") + errMsg);
            return;
        }
        // Move .h files to Inc/
        const QDir srcDir(m_config.outputDir + "/Src");
        const QDir incDir(m_config.outputDir + "/Inc");
        incDir.mkpath(".");
        for (const QString &name : srcDir.entryList({"*.h"}, QDir::Files)) {
            QFile::rename(srcDir.filePath(name), incDir.filePath(name));
        }
    }

    // 3. AI glue files → outputDir/Src  (headers → Inc)
    if (QDir(aiGlueDir).exists()) {
        for (const QString &fname :
             QDir(aiGlueDir).entryList({"*.c", "*.h"}, QDir::Files)) {
            const QString src = aiGlueDir + "/" + fname;
            const QString dst = fname.endsWith(".h")
                ? m_config.outputDir + "/Inc/" + fname
                : m_config.outputDir + "/Src/" + fname;
            QFile::remove(dst);
            QFile::copy(src, dst);
        }
    }

    // 4. X-CUBE-AI output (network.c, network.h, ...) → outputDir
    const QString xcubeOut = m_config.outputDir + "/xcubeai_output";
    const QDir xcubeOutDir(xcubeOut);
    if (xcubeOutDir.exists()) {
        const QDir incDst(m_config.outputDir + "/Inc");
        const QDir srcDst(m_config.outputDir + "/Src");
        incDst.mkpath(".");
        srcDst.mkpath(".");
        for (const QString &fname :
             xcubeOutDir.entryList({"*.c", "*.h"}, QDir::Files)) {
            const QString from = xcubeOut + "/" + fname;
            const QString to   = fname.endsWith(".h")
                ? incDst.filePath(fname)
                : srcDst.filePath(fname);
            QFile::remove(to);
            QFile::copy(from, to);
        }
    }

    emit outputLine(tr("✓ Proje hazırlandı: ") + m_config.outputDir);
    emit progressChanged(50);
    stepBuild();
}

// ── Step 3: Compile ────────────────────────────────────────────────────────

void PipelineRunner::stepBuild()
{
    emit stageChanged(tr("4/5 - Derleniyor (arm-none-eabi-gcc)..."));
    emit progressChanged(55);

    if (m_cancelled) { emit finished(false); return; }

    const auto missingExecutable = [](const QString &path) {
        if (path.isEmpty()) return true;
        const QFileInfo info(path);
        return info.isAbsolute() && !info.exists();
    };

    // ── Validate make executable ───────────────────────────────────────────
    if (missingExecutable(m_config.makePath)) {
        fail(tr("✗ make.exe bulunamadı. Ayarlar → GNU Make yolunu kontrol edin.\n"
                "  Mevcut yol: ") + m_config.makePath);
        return;
    }

    // ── Validate GCC executable ────────────────────────────────────────────
    if (missingExecutable(m_config.gccPath)) {
        fail(tr("✗ arm-none-eabi-gcc bulunamadı. Ayarlar → ARM GCC yolunu kontrol edin.\n"
                "  Mevcut yol: ") + m_config.gccPath);
        return;
    }

    QString cubeSdkPath = m_config.cubeSdkPath;
    const QString expectedSdkPattern = sdkPatternForBoard(m_config.targetBoard);
    const QString expectedSdkToken = QStringLiteral("STM32Cube_FW_")
        + boardFamily(m_config.targetBoard) + QStringLiteral("_");

    if (!cubeSdkPath.isEmpty()
        && !QDir(cubeSdkPath).dirName().contains(expectedSdkToken, Qt::CaseInsensitive)) {
        emit outputLine(tr("  SDK secimi karta uymuyor, yeniden aranacak: ") + cubeSdkPath);
        cubeSdkPath.clear();
    }

    if (cubeSdkPath.isEmpty() || !QDir(cubeSdkPath).exists()) {
        const QString repoBase =
            QDir::homePath() + "/STM32Cube/Repository";
        cubeSdkPath = findSdkDir(repoBase, expectedSdkPattern);
    }
    if (cubeSdkPath.isEmpty() || !QDir(cubeSdkPath).exists()) {
        fail(tr("✗ STM32Cube SDK bulunamadı.\n"
                "  STM32CubeMX veya STM32CubeIDE ile secilen karta ait firmware paketini indirin.\n"
                "  Beklenen konum: ~/STM32Cube/Repository/")
             + expectedSdkPattern + tr("\n"
                "  Ya da PipelineConfig::cubeSdkPath alanını ayarlayın."));
        return;
    }
    m_cubeSdkPath = cubeSdkPath;
    emit outputLine(tr("  SDK: ") + cubeSdkPath);

    // ── Set up process environment ─────────────────────────────────────────
    const QFileInfo gccInfo(m_config.gccPath);
    const QString gccDir = gccInfo.isAbsolute()
        ? gccInfo.absolutePath()
        : QString();
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    if (!gccDir.isEmpty())
        env.insert("PATH", gccDir + ";" + env.value("PATH"));

    m_buildRunner = new CliRunner(this);
    m_buildRunner->setCliPath(m_config.makePath);
    m_buildRunner->setEnvironment(env);

    connect(m_buildRunner, &CliRunner::outputLine,
            this, &PipelineRunner::outputLine);
    connect(m_buildRunner, &CliRunner::errorLine,
            this, &PipelineRunner::errorLine);
    connect(m_buildRunner, &CliRunner::progressChanged,
            this, [this](int p) { emit progressChanged(50 + p / 5); });
    connect(m_buildRunner, &CliRunner::finished,
            this, &PipelineRunner::onBuildFinished);

    QStringList makeArgs = {
        QStringLiteral("-C"),        m_config.outputDir,
        QStringLiteral("-j4"),
        QStringLiteral("CUBE_SDK_PATH=") + QDir::toNativeSeparators(cubeSdkPath),
        QStringLiteral("all"),
    };
    if (!gccDir.isEmpty())
        makeArgs.insert(3, QStringLiteral("GCC_PATH=") + QDir::toNativeSeparators(gccDir));
    const QString aiMiddlewarePath = aiMiddlewarePathFromCli(m_config.xcubeCliPath);
    if (!aiMiddlewarePath.isEmpty())
        makeArgs.insert(makeArgs.size() - 1,
                        QStringLiteral("X_CUBE_AI_PATH=")
                        + QDir::toNativeSeparators(aiMiddlewarePath));

    m_buildRunner->run(makeArgs);
}

void PipelineRunner::onBuildFinished(bool success, int /*exitCode*/)
{
    if (m_cancelled) { emit finished(false); return; }

    if (!success) {
        fail(tr("✗ Derleme başarısız. Makefile çıktısını kontrol edin."));
        return;
    }

    const QString expectedElfPath = QDir(m_config.outputDir).filePath(
        QStringLiteral("build/%1_%2.elf")
            .arg(m_config.modelName, m_config.targetBoard));

    if (!QFileInfo::exists(expectedElfPath)) {
        fail(tr("✗ .elf dosyası üretilemedi."));
        return;
    }
    m_builtElfPath = expectedElfPath;

    emit outputLine(tr("✓ Derleme tamamlandı: ")
                    + QFileInfo(m_builtElfPath).fileName());
    emit progressChanged(82);
    stepFlash();
}

// ── Step 4: Flash ──────────────────────────────────────────────────────────

void PipelineRunner::stepFlash()
{
    emit stageChanged(tr("5/5 - Karta yukleniyor (ST-Link)..."));

    if (m_cancelled) { emit finished(false); return; }

    QString probeSummary;
    QString probeError;
    if (!connectedTargetMatchesBoard(m_config.programmerCliPath,
                                     m_config.targetBoard,
                                     m_programmerConnectArgs,
                                     probeSummary,
                                     probeError)) {
        if (!probeSummary.isEmpty())
            emit outputLine(probeSummary);
        fail(tr("✗ Flash iptal edildi: ") + probeError + "\n"
             + tr("  Hedef: ") + m_config.targetBoard + "\n"
             + tr("  Derlenen dosya: ") + QFileInfo(m_builtElfPath).fileName());
        return;
    }
    if (!probeSummary.isEmpty())
        emit outputLine(tr("  Bagli kart dogrulandi:\n") + probeSummary);

    if (boardFamily(m_config.targetBoard) == QStringLiteral("N6")) {
        const QString appBinPath = QFileInfo(m_builtElfPath).absoluteDir().filePath(
            QFileInfo(m_builtElfPath).completeBaseName() + QStringLiteral(".bin"));
        if (!QFileInfo::exists(appBinPath)) {
            fail(tr("✗ N6 icin .bin dosyasi uretilemedi: ") + appBinPath);
            return;
        }

        const QString signer = signingToolPath(m_config.programmerCliPath);
        const QString loader = externalLoaderPath(m_config.programmerCliPath);
        if (!QFileInfo::exists(signer)) {
            fail(tr("✗ STM32_SigningTool_CLI bulunamadi: ") + signer);
            return;
        }
        if (!QFileInfo::exists(loader)) {
            fail(tr("✗ NUCLEO-N657 external loader bulunamadi: ") + loader);
            return;
        }

        QString output;
        quint32 loadAddress = 0;
        quint32 entryPoint = 0;
        QString err;
        if (!readVectorInfo(appBinPath, loadAddress, entryPoint, err)) {
            fail(tr("✗ ") + err);
            return;
        }

        const QDir buildDir(QFileInfo(m_builtElfPath).absolutePath());
        const QString trustedApp = buildDir.filePath(
            QFileInfo(appBinPath).completeBaseName() + QStringLiteral("-trusted.bin"));
        QFile::remove(trustedApp);

        emit outputLine(tr("  N6 uygulama imzalaniyor: load=0x%1 entry=0x%2")
                            .arg(loadAddress, 8, 16, QLatin1Char('0'))
                            .arg(entryPoint, 8, 16, QLatin1Char('0')));
        QStringList signAppArgs = {
            QStringLiteral("-bin"), appBinPath,
            QStringLiteral("-nk"),
            QStringLiteral("-la"), QStringLiteral("0x%1").arg(loadAddress, 8, 16, QLatin1Char('0')),
            QStringLiteral("-ep"), QStringLiteral("0x%1").arg(entryPoint, 8, 16, QLatin1Char('0')),
            QStringLiteral("-of"), QStringLiteral("0x80000000"),
            QStringLiteral("-t"), QStringLiteral("fsbl"),
            QStringLiteral("-o"), trustedApp,
            QStringLiteral("-hv"), QStringLiteral("2.3"),
            QStringLiteral("-align"),
            QStringLiteral("-s"),
        };
        if (!runBlockingTool(signer, signAppArgs, 30000, output)) {
            emit errorLine(output);
            fail(tr("✗ N6 uygulama imzalama basarisiz."));
            return;
        }
        emit outputLine(output.trimmed());
        emit progressChanged(86);

        const QString fsblProject = QDir(m_cubeSdkPath).filePath(
            QStringLiteral("Projects/NUCLEO-N657X0-Q/Templates/Template_FSBL_LRUN/STM32CubeIDE/Boot"));
        const QString fsblBin = QDir(fsblProject).filePath(
            QStringLiteral("Debug/Template_LRUN_FSBL.bin"));
        if (!ensureN6FsblLrunLayout(fsblProject, err)) {
            fail(tr("✗ ") + err);
            return;
        }
        emit outputLine(tr("  N6 FSBL LRUN yerlesimi ayarlandi: dest=0x34000000 header=0x400 size=1MB."));
        const QString cubeIdeCli = cubeIdeCliPath(m_config.gccPath);
        if (!QFileInfo::exists(cubeIdeCli)) {
            fail(tr("✗ stm32cubeidec.exe bulunamadi. FSBL derlenemiyor."));
            return;
        }

        emit outputLine(tr("  N6 FSBL derleniyor..."));
        const QString fsblWorkspace = QDir(m_config.outputDir).filePath(QStringLiteral("fsbl_workspace"));
        QDir(fsblWorkspace).removeRecursively();
        QDir().mkpath(fsblWorkspace);
        QStringList buildFsblArgs = {
            QStringLiteral("-nosplash"),
            QStringLiteral("-application"), QStringLiteral("org.eclipse.cdt.managedbuilder.core.headlessbuild"),
            QStringLiteral("-data"), QDir::toNativeSeparators(fsblWorkspace),
            QStringLiteral("-import"), QDir::toNativeSeparators(fsblProject),
            QStringLiteral("-cleanBuild"), QStringLiteral("Template_LRUN_FSBL/Debug"),
        };
        if (!runBlockingTool(cubeIdeCli, buildFsblArgs, 180000, output)) {
            emit errorLine(output);
            fail(tr("✗ N6 FSBL derleme basarisiz."));
            return;
        }
        emit outputLine(output.trimmed());
        if (!QFileInfo::exists(fsblBin)) {
            fail(tr("✗ FSBL binary bulunamadi: ") + fsblBin);
            return;
        }
        emit progressChanged(90);

        quint32 fsblLoad = 0;
        quint32 fsblEntry = 0;
        if (!readVectorInfo(fsblBin, fsblLoad, fsblEntry, err)) {
            fail(tr("✗ ") + err);
            return;
        }
        const QString trustedFsbl = buildDir.filePath(QStringLiteral("Template_LRUN_FSBL-trusted.bin"));
        QFile::remove(trustedFsbl);

        emit outputLine(tr("  N6 FSBL imzalaniyor: load=0x%1 entry=0x%2")
                            .arg(fsblLoad, 8, 16, QLatin1Char('0'))
                            .arg(fsblEntry, 8, 16, QLatin1Char('0')));
        QStringList signFsblArgs = {
            QStringLiteral("-bin"), fsblBin,
            QStringLiteral("-nk"),
            QStringLiteral("-la"), QStringLiteral("0x%1").arg(fsblLoad, 8, 16, QLatin1Char('0')),
            QStringLiteral("-ep"), QStringLiteral("0x%1").arg(fsblEntry, 8, 16, QLatin1Char('0')),
            QStringLiteral("-of"), QStringLiteral("0x80000000"),
            QStringLiteral("-t"), QStringLiteral("fsbl"),
            QStringLiteral("-o"), trustedFsbl,
            QStringLiteral("-hv"), QStringLiteral("2.3"),
            QStringLiteral("-align"),
            QStringLiteral("-s"),
        };
        if (!runBlockingTool(signer, signFsblArgs, 30000, output)) {
            emit errorLine(output);
            fail(tr("✗ N6 FSBL imzalama basarisiz."));
            return;
        }
        emit outputLine(output.trimmed());

        auto flashTrusted = [&](const QString &file, const QString &address) -> bool {
            QStringList args{QStringLiteral("-c")};
            args += n6ExternalFlashConnectArgs(m_programmerConnectArgs);
            args += {
                QStringLiteral("-el"), loader,
                QStringLiteral("-hardRst"),
                QStringLiteral("-w"), file, address,
                QStringLiteral("-v"),
            };
            emit outputLine(QStringLiteral("> STM32_Programmer_CLI ")
                            + args.join(QLatin1Char(' ')));
            if (!runBlockingTool(m_config.programmerCliPath, args, 120000, output)) {
                emit errorLine(output);
                emit errorLine(tr("  N6 external flash yazimi icin kart development mode'da olmali: BOOT1=2-3, BOOT0 fark etmez. Yazimdan sonra external flash boot icin BOOT0=1-2 ve BOOT1=1-2 yapin."));
                return false;
            }
            emit outputLine(output.trimmed());
            return true;
        };

        if (!flashTrusted(trustedApp, QStringLiteral("0x70100000"))) {
            fail(tr("✗ N6 uygulama external flash yazimi basarisiz."));
            return;
        }
        emit progressChanged(95);

        if (!flashTrusted(trustedFsbl, QStringLiteral("0x70000000"))) {
            fail(tr("✗ N6 FSBL external flash yazimi basarisiz."));
            return;
        }

        m_running = false;
        emit progressChanged(100);
        emit outputLine(tr("✓ N6 LRUN flash tamamlandi. BOOT0/BOOT1 external flash konumundaysa reset sonrasi calisir."));
        emit finished(true);
        return;
    }

    m_flashRunner = new CliRunner(this);
    m_flashRunner->setCliPath(m_config.programmerCliPath);

    connect(m_flashRunner, &CliRunner::outputLine,
            this, &PipelineRunner::outputLine);
    connect(m_flashRunner, &CliRunner::errorLine,
            this, &PipelineRunner::errorLine);
    connect(m_flashRunner, &CliRunner::progressChanged,
            this, [this](int p) { emit progressChanged(80 + p / 5); });
    connect(m_flashRunner, &CliRunner::finished,
            this, &PipelineRunner::onFlashFinished);

    QStringList args{QStringLiteral("-c")};
    args += m_programmerConnectArgs;
    args += {
        QStringLiteral("-d"), m_builtElfPath,
        QStringLiteral("-rst"),
    };
    m_flashRunner->run(args);
}

void PipelineRunner::onFlashFinished(bool success, int /*exitCode*/)
{
    m_running = false;

    if (m_cancelled) { emit finished(false); return; }

    emit progressChanged(100);

    if (success) {
        emit outputLine(
            "┌─────────────────────────────────┐\n"
            "│  ✓  PIPELINE TAMAMLANDI         │\n"
            "│  Model karta başarıyla yüklendi │\n"
            "└─────────────────────────────────┘");
        emit finished(true);
    } else {
        fail(tr("✗ Flash başarısız. ST-Link bağlantısını kontrol edin."));
    }
}

// ── Helpers ───────────────────────────────────────────────────────────────

void PipelineRunner::fail(const QString &message)
{
    m_running = false;
    emit errorLine(message);
    emit finished(false);
}
