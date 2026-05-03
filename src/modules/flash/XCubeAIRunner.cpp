#include "XCubeAIRunner.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

XCubeAIRunner::XCubeAIRunner(QObject *parent)
    : QObject(parent)
    , m_process(new QProcess(this))
{
    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &XCubeAIRunner::onReadyReadStdOut);
    connect(m_process, &QProcess::readyReadStandardError,
            this, &XCubeAIRunner::onReadyReadStdErr);
    connect(m_process,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &XCubeAIRunner::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred,
            this, &XCubeAIRunner::onProcessError);
}

XCubeAIRunner::~XCubeAIRunner()
{
    if (m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(3000);
    }
}

void XCubeAIRunner::setCliPath(const QString &path)
{
    m_cliPath = path;
}

bool XCubeAIRunner::isRunning() const
{
    return m_process->state() != QProcess::NotRunning;
}

void XCubeAIRunner::cancel()
{
    if (isRunning()) {
        m_process->kill();
    }
}

// ── Static helpers ─────────────────────────────────────────────────────────

QString XCubeAIRunner::findInDir(const QString &baseDir,
                                 const QString &fileName,
                                 int maxDepth)
{
    QDir dir(baseDir);
    if (!dir.exists()) return {};

    if (dir.exists(fileName))
        return dir.absoluteFilePath(fileName);

    if (maxDepth <= 0) return {};

    const QStringList subdirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &sub : subdirs) {
        const QString found = findInDir(dir.absoluteFilePath(sub), fileName, maxDepth - 1);
        if (!found.isEmpty()) return found;
    }
    return {};
}

// Searches the STM32Cube Repository packs directory for stedgeai.exe,
// handling the version number wildcard in the path.
static QString findInRepository(const QString &repoBase)
{
    // Path pattern: <repoBase>/Packs/STMicroelectronics/X-CUBE-AI/<ver>/Utilities/windows/stedgeai.exe
    const QDir packsDir(repoBase + "/Packs/STMicroelectronics/X-CUBE-AI");
    if (!packsDir.exists()) return {};

    const QStringList versions = packsDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &ver : versions) {
        const QString candidate =
            packsDir.absoluteFilePath(ver + "/Utilities/windows/stedgeai.exe");
        if (QFile::exists(candidate)) return candidate;
    }
    return {};
}

QString XCubeAIRunner::detectCliPath()
{
    // 1. Check if stedgeai (v10+) or stm32ai (legacy) is on PATH
    for (const QString &cmd : {QStringLiteral("stedgeai"), QStringLiteral("stm32ai")}) {
        QProcess test;
        test.start(cmd, {"--version"});
        if (test.waitForFinished(2000) && test.exitCode() == 0)
            return cmd;
    }

    // 2. STM32Cube Repository packs (CubeMX installs X-CUBE-AI here)
    const QStringList repoBases = {
        QDir::homePath() + "/STM32Cube/Repository",
        "C:/Users/Public/STM32Cube/Repository",
    };
    for (const QString &base : repoBases) {
        const QString found = findInRepository(base);
        if (!found.isEmpty()) return found;
    }

    // 3. STM32CubeIDE plugin directory (depth-limited)
    const QStringList searchRoots = {
        "C:/ST",
        "C:/Program Files/ST",
        "C:/Program Files (x86)/ST",
    };
    for (const QString &root : searchRoots) {
        QString found = findInDir(root, "stedgeai.exe", 3);
        if (!found.isEmpty()) return found;
        found = findInDir(root, "stm32ai.exe", 3);
        if (!found.isEmpty()) return found;
    }

    // 4. Well-known standalone install paths
    const QStringList candidates = {
        "C:/ST/stm32ai-windows-latest/windows/stm32ai.exe",
        "C:/stm32ai/windows/stm32ai.exe",
    };
    for (const QString &p : candidates) {
        if (QFile::exists(p)) return p;
    }

    return {};
}

bool XCubeAIRunner::isAvailable()
{
    return !detectCliPath().isEmpty();
}

// ── Slot: generate ─────────────────────────────────────────────────────────

void XCubeAIRunner::generate(const QString &modelPath,
                             const QString &targetBoard,
                             const QString &quantization,
                             const QString &outputDir)
{
    if (isRunning()) return;

    const QString execPath = m_cliPath.isEmpty() ? detectCliPath() : m_cliPath;
    if (execPath.isEmpty()) {
        XCubeAIResult result;
        result.success      = false;
        result.errorMessage = "stedgeai / stm32ai bulunamadı. Ayarlar menüsünden yolu girin.";
        emit finished(result);
        return;
    }

    m_outputDir     = outputDir;
    m_stdoutBuffer.clear();

    QDir().mkpath(outputDir);

    const QStringList args = buildArgs(modelPath, targetBoard, quantization, outputDir);

    emit outputLine(QString("> %1 %2").arg(execPath, args.join(' ')));
    emit started();

    m_process->start(execPath, args);
}

// ── Private: buildArgs ─────────────────────────────────────────────────────

QStringList XCubeAIRunner::buildArgs(const QString &modelPath,
                                     const QString &targetBoard,
                                     const QString &quantization,
                                     const QString &outputDir) const
{
    // stedgeai (X-CUBE-AI v10+) syntax:
    //   stedgeai generate --model FILE --target stm32 --output DIR [--compression ...]
    // Legacy stm32ai syntax used -m and numeric compression, but stedgeai takes --model.
    // We always build stedgeai-style args; legacy stm32ai is not supported by this runner.
    Q_UNUSED(targetBoard); // stedgeai only accepts "stm32" as target (MCU series-agnostic)

    QStringList args = {
        "generate",
        "--model", modelPath,
        "--target", "stm32",
        "--output", outputDir,
    };

    // Compression/weight quantization
    if (quantization == "INT8") {
        args << "--compression" << "lossless";
    } else if (quantization == "Dynamic Q") {
        args << "--compression" << "low";
    }
    // Float32 → no --compression flag (default is none)

    return args;
}

// ── Private slots ──────────────────────────────────────────────────────────

void XCubeAIRunner::onReadyReadStdOut()
{
    while (m_process->canReadLine()) {
        const QString line =
            QString::fromLocal8Bit(m_process->readLine()).trimmed();
        if (line.isEmpty()) continue;

        m_stdoutBuffer += line + '\n';
        emit outputLine(line);
        parseProgressFromLine(line);
    }
}

void XCubeAIRunner::onReadyReadStdErr()
{
    while (m_process->canReadLine()) {
        const QString line =
            QString::fromLocal8Bit(m_process->readLine()).trimmed();
        if (!line.isEmpty())
            emit errorLine(line);
    }
}

void XCubeAIRunner::onProcessFinished(int exitCode,
                                      QProcess::ExitStatus /*status*/)
{
    // Drain any remaining output
    onReadyReadStdOut();
    onReadyReadStdErr();

    emit finished(parseResult(exitCode));
}

void XCubeAIRunner::onProcessError(QProcess::ProcessError error)
{
    if (error == QProcess::FailedToStart) {
        XCubeAIResult result;
        result.success      = false;
        result.outputDir    = m_outputDir;
        result.errorMessage = "stedgeai başlatılamadı. CLI yolunu kontrol edin.";
        emit finished(result);
    }
}

// ── Private: progress & result parsing ────────────────────────────────────

void XCubeAIRunner::parseProgressFromLine(const QString &line)
{
    if (line.contains("Analyzing",  Qt::CaseInsensitive)) emit progressChanged(20);
    else if (line.contains("Flash", Qt::CaseInsensitive)) emit progressChanged(50);
    else if (line.contains("Generating", Qt::CaseInsensitive)) emit progressChanged(70);
    else if (line.contains("Done",  Qt::CaseInsensitive) ||
             line.contains("done",  Qt::CaseSensitive))    emit progressChanged(100);
}

XCubeAIResult XCubeAIRunner::parseResult(int exitCode) const
{
    XCubeAIResult result;
    result.success   = (exitCode == 0);
    result.outputDir = m_outputDir;

    if (!result.success) return result;

    static const QRegularExpression flashRe(R"(Flash.*?(\d+\.?\d*)\s*KByte)",
                                            QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression ramRe(  R"(RAM.*?(\d+\.?\d*)\s*KByte)",
                                            QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression maccRe( R"(MACC.*?(\d+))",
                                            QRegularExpression::CaseInsensitiveOption);

    const auto fm = flashRe.match(m_stdoutBuffer);
    const auto rm = ramRe.match(m_stdoutBuffer);
    const auto mm = maccRe.match(m_stdoutBuffer);

    if (fm.hasMatch()) result.flashKb = fm.captured(1).toUInt();
    if (rm.hasMatch()) result.ramKb   = rm.captured(1).toFloat();
    if (mm.hasMatch()) result.macc    = mm.captured(1).toULongLong();

    // List generated source files
    const QDir outDir(m_outputDir);
    result.generatedFiles = outDir.entryList({"*.c", "*.h"}, QDir::Files);

    return result;
}
