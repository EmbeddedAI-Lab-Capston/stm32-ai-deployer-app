#include "ToolDetector.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QThread>

ToolDetector::ToolDetector(QObject *parent)
    : QObject(parent)
{}

ToolDetector::~ToolDetector()
{
    if (m_thread && m_thread->isRunning()) {
        m_thread->quit();
        m_thread->wait(3000);
    }
}

// ── Async detectAll ────────────────────────────────────────────────────────

void ToolDetector::detectAll()
{
    if (m_thread && m_thread->isRunning())
        return;

    // QThread::create() runs the lambda in a new thread (Qt 5.10+).
    // Signals emitted from within use Qt::QueuedConnection to recipients
    // in the main thread, which is correct behaviour.
    m_thread = QThread::create([this]() {
        QList<ToolInfo> results;

        struct Spec {
            QString name;
            QString key;
            QString (*detect)();
        };

        const QList<Spec> specs = {
            { "arm-none-eabi-gcc",    "tools/gcc_path",           &detectGcc           },
            { "make",                 "tools/make_path",           &detectMake          },
            { "STM32_Programmer_CLI", "programmer/cli_path",       &detectStm32Programmer },
            { "stedgeai (X-CUBE-AI)", "tools/xcubeai_cli_path",   &detectXCubeAI       },
        };

        for (const Spec &spec : specs) {
            ToolInfo info;
            info.name  = spec.name;
            info.key   = spec.key;
            info.path  = spec.detect();
            info.found = !info.path.isEmpty();
            if (info.found)
                info.version = queryVersion(info.path);
            results.append(info);

            if (info.found)
                emit toolFound(info);
            else
                emit toolNotFound(info.name);
        }

        emit detectionFinished(results);
    });

    m_thread->setParent(this);
    m_thread->start();
}

// ── Static detectors ───────────────────────────────────────────────────────

QString ToolDetector::detectGcc()
{
    // 1. Available on PATH?
    {
        QProcess test;
        test.start("arm-none-eabi-gcc", {"--version"});
        if (test.waitForFinished(2000) && test.exitCode() == 0)
            return QStringLiteral("arm-none-eabi-gcc");
    }

    // 2. STM32CubeIDE plugin directory
    {
        const QString found = findInCubeIDE("arm-none-eabi-gcc.exe");
        if (!found.isEmpty()) return found;
    }

    // 3. Known fixed paths (STM32CubeIDE 1.x + standalone toolchain)
    const QStringList candidates = {
        "C:/ST/STM32CubeIDE_1.19.0/STM32CubeIDE/plugins/"
        "com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32"
        ".13.3.rel1.win32_1.0.0.202411081344/tools/bin/arm-none-eabi-gcc.exe",

        "C:/ST/STM32CubeIDE_1.18.0/STM32CubeIDE/plugins/"
        "com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32"
        ".12.3.rel1.win32_1.0.200.202312181210/tools/bin/arm-none-eabi-gcc.exe",

        "C:/Program Files/GNU Arm Embedded Toolchain/13 2023-q4-major/bin/arm-none-eabi-gcc.exe",
        "C:/Program Files/GNU Arm Embedded Toolchain/10 2021-10/bin/arm-none-eabi-gcc.exe",
    };

    for (const QString &p : candidates)
        if (fileExists(p)) return p;

    return QString();
}

QString ToolDetector::detectMake()
{
    // 1. Available on PATH?
    {
        QProcess test;
        test.start("make", {"--version"});
        if (test.waitForFinished(2000) && test.exitCode() == 0)
            return QStringLiteral("make");
    }

    // 2. STM32CubeIDE plugin directory
    {
        const QString found = findInCubeIDE("make.exe");
        if (!found.isEmpty()) return found;
    }

    // 3. Known fixed paths
    const QStringList candidates = {
        "C:/ST/STM32CubeIDE_1.19.0/STM32CubeIDE/plugins/"
        "com.st.stm32cube.ide.mcu.externaltools.make.win32_"
        "2.2.0.202409170845/tools/bin/make.exe",

        "C:/ST/STM32CubeIDE_1.18.0/STM32CubeIDE/plugins/"
        "com.st.stm32cube.ide.mcu.externaltools.make.win32_"
        "2.1.0.202305021511/tools/bin/make.exe",
    };

    for (const QString &p : candidates)
        if (fileExists(p)) return p;

    return QString();
}

QString ToolDetector::detectStm32Programmer()
{
    // 1. Available on PATH?
    {
        QProcess test;
        test.start("STM32_Programmer_CLI", {"--version"});
        if (test.waitForFinished(2000) && test.exitCode() == 0)
            return QStringLiteral("STM32_Programmer_CLI");
    }

    // 2. Known install paths (STM32CubeProgrammer 2.x)
    const QStringList candidates = {
        "C:/Program Files/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin/STM32_Programmer_CLI.exe",
        "C:/Program Files (x86)/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin/STM32_Programmer_CLI.exe",
    };

    for (const QString &p : candidates)
        if (fileExists(p)) return p;

    return QString();
}

QString ToolDetector::detectXCubeAI()
{
    // 1. Available on PATH?
    {
        QProcess test;
        test.start("stedgeai", {"--version"});
        if (test.waitForFinished(2000) && test.exitCode() == 0)
            return QStringLiteral("stedgeai");
    }

    // 2. Search in STM32Cube Repository packs
    const QStringList baseDirs = {
        QDir::homePath() + "/STM32Cube/Repository/Packs/STMicroelectronics/X-CUBE-AI",
        "C:/Users/Default/STM32Cube/Repository/Packs/STMicroelectronics/X-CUBE-AI",
    };

    for (const QString &base : baseDirs) {
        if (!QDir(base).exists()) continue;
        QDirIterator it(base, {"stedgeai.exe"}, QDir::Files,
                        QDirIterator::Subdirectories);
        int depth = 0;
        while (it.hasNext() && depth < 50) {
            const QString p = it.next();
            ++depth;
            if (fileExists(p)) return p;
        }
    }

    return QString();
}

// ── queryVersion ──────────────────────────────────────────────────────────

QString ToolDetector::queryVersion(const QString &path,
                                   const QStringList &args)
{
    if (path.isEmpty()) return QString();

    QProcess proc;
    proc.setProgram(path);
    proc.setArguments(args);
    proc.start();

    if (!proc.waitForFinished(3000))
        return QString();

    const QByteArray out = proc.readAllStandardOutput()
                         + proc.readAllStandardError();
    const QString line = QString::fromLocal8Bit(out).split('\n').first().trimmed();
    return line.left(60);
}

// ── Helpers ───────────────────────────────────────────────────────────────

QString ToolDetector::findInCubeIDE(const QString &fileName)
{
    const QStringList baseDirs = {
        "C:/ST",
        "C:/Program Files/ST",
        "C:/Program Files (x86)/ST",
    };

    for (const QString &base : baseDirs) {
        if (!QDir(base).exists()) continue;

        QDirIterator it(base, {fileName}, QDir::Files,
                        QDirIterator::Subdirectories);
        int visited = 0;
        while (it.hasNext() && visited < 300) {
            const QString path = it.next();
            ++visited;
            // Prefer paths inside a "plugins" directory
            if (path.contains("plugins"))
                return path;
        }
    }

    return QString();
}

bool ToolDetector::fileExists(const QString &path)
{
    return QFile::exists(path);
}
