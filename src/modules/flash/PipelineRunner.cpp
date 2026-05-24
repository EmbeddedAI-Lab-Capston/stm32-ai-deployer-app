#include "PipelineRunner.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
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
                                        QString &summary,
                                        QString &errorMessage)
{
    if (programmerCliPath.isEmpty()) {
        errorMessage = QStringLiteral("STM32_Programmer_CLI yolu bos.");
        return false;
    }

    QProcess probe;
    probe.start(programmerCliPath, {QStringLiteral("-c"), QStringLiteral("port=SWD")});
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
    bool matched = false;
    if (family == "F4") {
        matched = upper.contains("STM32F4");
    } else if (family == "H7") {
        matched = upper.contains("STM32H7");
    } else if (family == "N6") {
        matched = upper.contains("STM32N6");
    }

    if (!matched) {
        errorMessage = QStringLiteral("Bagli kart hedef kart ailesiyle uyusmuyor.");
        return false;
    }

    return true;
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
    const QString boardTmpl   = tmplBase + "/base/" + m_config.targetBoard;
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

    m_flashRunner->run({
        QStringLiteral("-c"), QStringLiteral("port=SWD"),
        QStringLiteral("-d"), m_builtElfPath,
        QStringLiteral("-rst"),
    });
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
