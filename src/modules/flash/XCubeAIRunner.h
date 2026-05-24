#pragma once

#include <QObject>
#include <QProcess>
#include <QStringList>

// Result from a stm32ai generate invocation
struct XCubeAIResult {
    bool        success      = false;
    QString     outputDir;
    quint32     flashKb      = 0;
    float       ramKb        = 0.0f;
    quint64     macc         = 0;
    QStringList generatedFiles;
    QString     errorMessage;
};

// Runs the stm32ai CLI (X-CUBE-AI) via QProcess to generate C code from
// a .tflite or .h5 model file. Mirrors the CliRunner pattern but with
// X-CUBE-AI-specific argument building and output parsing.
class XCubeAIRunner : public QObject
{
    Q_OBJECT

public:
    explicit XCubeAIRunner(QObject *parent = nullptr);
    ~XCubeAIRunner() override;

    void    setCliPath(const QString &path);
    QString cliPath() const { return m_cliPath; }

    // Search well-known locations for stm32ai.exe; returns empty string if not found.
    static QString detectCliPath();
    static bool    isAvailable();

    bool isRunning() const;
    void cancel();

public slots:
    void analyze(const QString &modelPath,
                 const QString &targetBoard,
                 const QString &quantization,
                 const QString &outputDir);

    void generate(const QString &modelPath,
                  const QString &targetBoard,
                  const QString &quantization,
                  const QString &outputDir);

signals:
    void outputLine(const QString &line);
    void errorLine(const QString &line);
    void progressChanged(int percent);
    void started();
    void finished(const XCubeAIResult &result);

private slots:
    void onReadyReadStdOut();
    void onReadyReadStdErr();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onProcessError(QProcess::ProcessError error);

private:
    QProcess *m_process      = nullptr;
    QString   m_cliPath;
    QString   m_outputDir;
    QString   m_stdoutBuffer;
    QString   m_stderrBuffer;

    void          runCommand(const QString &command,
                             const QString &modelPath,
                             const QString &targetBoard,
                             const QString &quantization,
                             const QString &outputDir);
    QStringList   buildArgs(const QString &command,
                            const QString &modelPath,
                            const QString &targetBoard,
                            const QString &quantization,
                            const QString &outputDir) const;
    XCubeAIResult parseResult(int exitCode) const;
    void          parseProgressFromLine(const QString &line);

    // Depth-limited file search to avoid scanning the entire drive
    static QString findInDir(const QString &baseDir,
                             const QString &fileName,
                             int maxDepth = 3);
};
