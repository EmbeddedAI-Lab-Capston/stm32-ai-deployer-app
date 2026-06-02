#pragma once

#include <QObject>
#include <QString>
#include "PipelineConfig.h"

class XCubeAIRunner;
class XCubeAIResult;
class CliRunner;

// Orchestrates the full .tflite → C code → compile → flash pipeline.
// All steps are asynchronous; progress is reported via signals.
// Call run() once; call cancel() to abort at any stage.
class PipelineRunner : public QObject
{
    Q_OBJECT

public:
    explicit PipelineRunner(QObject *parent = nullptr);
    ~PipelineRunner() override;

    void run(const PipelineConfig &config);
    void cancel();

    bool isRunning() const { return m_running; }

signals:
    // Human-readable stage description, e.g. "1/4 — X-CUBE-AI: C kodu üretiliyor..."
    void stageChanged(const QString &stage);
    void outputLine(const QString &line);
    void errorLine(const QString &line);
    void progressChanged(int percent);    // 0-100
    void finished(bool success);

private slots:
    void onXCubeAnalyzeFinished(const XCubeAIResult &result);
    void onXCubeFinished(const XCubeAIResult &result);
    void onBuildFinished(bool success, int exitCode);
    void onFlashFinished(bool success, int exitCode);

private:
    void stepAnalyze();
    void stepXCubeAI();
    void stepPrepare();
    void stepBuild();
    void stepFlash();

    void fail(const QString &message);

    PipelineConfig  m_config;
    bool            m_running       = false;
    bool            m_cancelled     = false;
    QString         m_builtElfPath;
    QString         m_cubeSdkPath;
    QStringList     m_programmerConnectArgs;

    XCubeAIRunner  *m_xcubeRunner   = nullptr;
    CliRunner      *m_buildRunner   = nullptr;
    CliRunner      *m_flashRunner   = nullptr;
};
