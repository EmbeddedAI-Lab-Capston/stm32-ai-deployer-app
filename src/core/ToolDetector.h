#pragma once

#include <QObject>
#include <QString>
#include <QList>

struct ToolInfo {
    QString name;     // "arm-none-eabi-gcc"
    QString key;      // AppSettings key ("tools/gcc_path")
    QString path;     // detected full path
    QString version;  // from --version output, first line
    bool    found = false;
};

// Searches for all required build tools on the system and emits results
// asynchronously. Detection runs in a worker thread so the UI stays responsive.
class ToolDetector : public QObject
{
    Q_OBJECT

public:
    explicit ToolDetector(QObject *parent = nullptr);
    ~ToolDetector() override;

    // Start async detection of all tools. Emits toolFound / toolNotFound per
    // tool, then detectionFinished when done.
    void detectAll();

    // Blocking single-tool detectors — safe to call from any thread.
    static QString detectGcc();
    static QString detectMake();
    static QString detectStm32Programmer();
    static QString detectXCubeAI();

    // Run the executable with args, capture first line of output. Returns
    // empty string on failure or timeout.
    static QString queryVersion(const QString &path,
                                const QStringList &args = {"--version"});

signals:
    void toolFound(const ToolInfo &info);
    void toolNotFound(const QString &toolName);
    void detectionFinished(const QList<ToolInfo> &results);

private:
    // Depth-limited search inside STM32CubeIDE plugin directories.
    static QString findInCubeIDE(const QString &fileName);

    // Check whether path exists and is a regular file.
    static bool fileExists(const QString &path);

    QThread *m_thread = nullptr;
};
