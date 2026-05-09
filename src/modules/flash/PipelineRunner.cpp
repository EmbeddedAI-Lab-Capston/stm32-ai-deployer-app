#include "PipelineRunner.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QProcessEnvironment>

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
    stepXCubeAI();
}

void PipelineRunner::cancel()
{
    m_cancelled = true;
    if (m_xcubeRunner) m_xcubeRunner->cancel();
    if (m_buildRunner) m_buildRunner->cancel();
    if (m_flashRunner) m_flashRunner->cancel();
}

// ── Step 1: X-CUBE-AI code generation ─────────────────────────────────────

void PipelineRunner::stepXCubeAI()
{
    emit stageChanged(tr("1/4 — X-CUBE-AI: C kodu üretiliyor..."));
    emit progressChanged(5);

    const QString xcubeOut = m_config.outputDir + "/xcubeai_output";

    m_xcubeRunner = new XCubeAIRunner(this);
    m_xcubeRunner->setCliPath(m_config.xcubeCliPath);

    connect(m_xcubeRunner, &XCubeAIRunner::outputLine,
            this, &PipelineRunner::outputLine);
    connect(m_xcubeRunner, &XCubeAIRunner::errorLine,
            this, &PipelineRunner::errorLine);
    connect(m_xcubeRunner, &XCubeAIRunner::progressChanged,
            this, [this](int p) { emit progressChanged(5 + p / 5); });
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
    emit progressChanged(25);
    stepPrepare();
}

// ── Step 2: Template project preparation ──────────────────────────────────

void PipelineRunner::stepPrepare()
{
    emit stageChanged(tr("2/4 — Şablon proje hazırlanıyor..."));
    emit progressChanged(30);

    if (m_cancelled) { emit finished(false); return; }

    // Resolve the templates/ root: first look next to the .exe (deployed),
    // then walk up to the project source tree (development build).
    auto findTemplatesRoot = []() -> QString {
        const QString appDir = QCoreApplication::applicationDirPath();
        const QStringList candidates = {
            appDir + "/templates",
            appDir + "/../templates",
            appDir + "/../../templates",
            appDir + "/../../../templates",
        };
        for (const QString &c : candidates)
            if (QDir(c).exists()) return QDir::cleanPath(c);
        return appDir + "/templates"; // fallback — will produce a clear error
    };

    const QString tmplBase    = findTemplatesRoot();
    const QString boardTmpl   = tmplBase + "/base/" + m_config.targetBoard;
    const QString sensorTmpl  = tmplBase + "/sensors/" + m_config.sensorType;
    const QString aiGlueDir   = tmplBase + "/ai_glue";

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
    emit progressChanged(45);
    stepBuild();
}

// ── Step 3: Compile ────────────────────────────────────────────────────────

void PipelineRunner::stepBuild()
{
    emit stageChanged(tr("3/4 — Derleniyor (arm-none-eabi-gcc)..."));
    emit progressChanged(50);

    if (m_cancelled) { emit finished(false); return; }

    // ── Validate make executable ───────────────────────────────────────────
    if (m_config.makePath.isEmpty() || !QFile::exists(m_config.makePath)) {
        fail(tr("✗ make.exe bulunamadı. Ayarlar → GNU Make yolunu kontrol edin.\n"
                "  Mevcut yol: ") + m_config.makePath);
        return;
    }

    // ── Validate GCC executable ────────────────────────────────────────────
    if (m_config.gccPath.isEmpty() || !QFile::exists(m_config.gccPath)) {
        fail(tr("✗ arm-none-eabi-gcc bulunamadı. Ayarlar → ARM GCC yolunu kontrol edin.\n"
                "  Mevcut yol: ") + m_config.gccPath);
        return;
    }

    // ── Auto-detect STM32CubeF4 SDK ────────────────────────────────────────
    QString cubeSdkPath = m_config.cubeSdkPath;
    if (cubeSdkPath.isEmpty() || !QDir(cubeSdkPath).exists()) {
        const QString repoBase =
            QDir::homePath() + "/STM32Cube/Repository";
        cubeSdkPath = findSdkDir(repoBase, "STM32Cube_FW_F4_*");
    }
    if (cubeSdkPath.isEmpty() || !QDir(cubeSdkPath).exists()) {
        fail(tr("✗ STM32CubeF4 SDK bulunamadı.\n"
                "  STM32CubeMX veya STM32CubeIDE ile STM32Cube_FW_F4 paketini indirin.\n"
                "  Beklenen konum: ~/STM32Cube/Repository/STM32Cube_FW_F4_Vx.xx.x\n"
                "  Ya da PipelineConfig::cubeSdkPath alanını ayarlayın."));
        return;
    }
    emit outputLine(tr("  SDK: ") + cubeSdkPath);

    // ── Set up process environment ─────────────────────────────────────────
    const QString gccDir = QFileInfo(m_config.gccPath).absolutePath();
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
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

    m_buildRunner->run({
        QStringLiteral("-C"),        m_config.outputDir,
        QStringLiteral("-j4"),
        QStringLiteral("GCC_PATH=") + gccDir,
        QStringLiteral("CUBE_SDK_PATH=") + QDir::toNativeSeparators(cubeSdkPath),
        QStringLiteral("all"),
    });
}

void PipelineRunner::onBuildFinished(bool success, int /*exitCode*/)
{
    if (m_cancelled) { emit finished(false); return; }

    if (!success) {
        fail(tr("✗ Derleme başarısız. Makefile çıktısını kontrol edin."));
        return;
    }

    // Find produced .elf
    QDirIterator it(m_config.outputDir, {"*.elf"},
                    QDir::Files, QDirIterator::Subdirectories);
    if (!it.hasNext()) {
        fail(tr("✗ .elf dosyası üretilemedi."));
        return;
    }
    m_builtElfPath = it.next();

    emit outputLine(tr("✓ Derleme tamamlandı: ")
                    + QFileInfo(m_builtElfPath).fileName());
    emit progressChanged(80);
    stepFlash();
}

// ── Step 4: Flash ──────────────────────────────────────────────────────────

void PipelineRunner::stepFlash()
{
    emit stageChanged(tr("4/4 — Karta yükleniyor (ST-Link)..."));

    if (m_cancelled) { emit finished(false); return; }

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
