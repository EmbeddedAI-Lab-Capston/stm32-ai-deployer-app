#pragma once

#include <QObject>
#include <QVariant>
#include <QVariantList>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>
#include <QHash>
#include <QDateTime>
#include "modules/flash/PipelineConfig.h"

class AppState;
class SerialManager;
class FlashManager;
class AnalysisManager;
class ToolDetector;
class PacketParser;
class SerialSimulator;
class PipelineRunner;
class QProcess;

// ── Backend ─────────────────────────────────────────────────────────────────
// Single QML-facing facade. Forwards to existing managers and adapts data
// into QML-friendly types. Exposed to QML as the context property "backend".
class Backend : public QObject
{
    Q_OBJECT

    // Tools
    Q_PROPERTY(QVariantList toolPaths   READ toolPaths   NOTIFY toolPathsChanged)
    Q_PROPERTY(bool scanning READ isScanning NOTIFY scanningChanged)

    // Monitor terminal
    Q_PROPERTY(QVariantList monitorLines READ monitorLines NOTIFY monitorLinesChanged)
    Q_PROPERTY(bool simRunning READ simRunning NOTIFY simRunningChanged)
    Q_PROPERTY(bool sensorAnalysisRunning READ sensorAnalysisRunning NOTIFY sensorAnalysisChanged)

    // Flash
    Q_PROPERTY(QVariantList flashLines   READ flashLines   NOTIFY flashLinesChanged)
    Q_PROPERTY(int  flashProgress        READ flashProgress NOTIFY flashProgressChanged)
    Q_PROPERTY(bool flashBusy            READ flashBusy     NOTIFY flashBusyChanged)

    // Pipeline
    Q_PROPERTY(QVariantList pipelineLines READ pipelineLines NOTIFY pipelineChanged)
    Q_PROPERTY(int pipelineProgress READ pipelineProgress NOTIFY pipelineChanged)
    Q_PROPERTY(bool pipelineBusy READ pipelineBusy NOTIFY pipelineChanged)
    Q_PROPERTY(QString pipelineStage READ pipelineStage NOTIFY pipelineChanged)

    // Benchmark
    Q_PROPERTY(QVariantList benchmarkLines READ benchmarkLines NOTIFY benchmarkChanged)
    Q_PROPERTY(QVariantMap benchmarkMetrics READ benchmarkMetrics NOTIFY benchmarkChanged)
    Q_PROPERTY(bool benchmarkBusy READ benchmarkBusy NOTIFY benchmarkChanged)

    // Board probe
    Q_PROPERTY(bool probeBusy READ probeBusy NOTIFY probeChanged)
    Q_PROPERTY(QString probeStatus READ probeStatus NOTIFY probeChanged)

    // Analysis
    Q_PROPERTY(QVariantList benchmarkRecords   READ benchmarkRecords   NOTIFY analysisChanged)
    Q_PROPERTY(QVariantList simulationRecords  READ simulationRecords  NOTIFY analysisChanged)
    Q_PROPERTY(QVariantList sensorRecords      READ sensorRecords      NOTIFY analysisChanged)
    Q_PROPERTY(QVariantList compiledRecords    READ compiledRecords    NOTIFY analysisChanged)

public:
    explicit Backend(AppState        *state,
                     SerialManager   *serial,
                     FlashManager    *flash,
                     AnalysisManager *analysis,
                     QObject         *parent = nullptr);

    // ── Tools ─────────────────────────────────────────────────────────────
    QVariantList toolPaths() const;
    bool         isScanning() const { return m_scanning; }
    Q_INVOKABLE void scanTools();
    Q_INVOKABLE void setToolPath(const QString &key, const QString &path);

    // ── Serial ────────────────────────────────────────────────────────────
    Q_INVOKABLE QStringList availablePorts() const;
    Q_INVOKABLE QString     detectedStLinkPort() const;
    Q_INVOKABLE void        connectSerial(const QString &portName, int baud);
    Q_INVOKABLE void        disconnectSerial();

    // ── Board selection ───────────────────────────────────────────────────
    Q_INVOKABLE void selectBoard(const QString &boardName);
    Q_INVOKABLE void addCustomBoard(const QString &name, int flashKb, int ramKb, int clockMhz);
    Q_INVOKABLE QVariantList customBoards() const;   // [{name, flashKb, ramKb, clockMhz}, ...]

    // ── Analysis seed data ────────────────────────────────────────────────
    Q_INVOKABLE void seedAnalysisIfEmpty();

    // ── Monitor terminal ──────────────────────────────────────────────────
    QVariantList monitorLines() const { return m_monitorLines; }
    bool sensorAnalysisRunning() const { return m_sensorAnalysisRunning; }
    Q_INVOKABLE void clearMonitor();
    Q_INVOKABLE void startSensorAnalysis();
    Q_INVOKABLE void stopSensorAnalysis();
    Q_INVOKABLE void clearTodaySensorAnalysis();

    // ── Simulation (Monitor screen) ───────────────────────────────────────
    bool simRunning() const { return m_simRunning || m_hwSimRunning; }
    Q_INVOKABLE void startSimulation(int intervalMs, double minVal, double maxVal);
    Q_INVOKABLE void startSimulationWithInputs(int intervalMs, const QVariantList &inputs);
    Q_INVOKABLE void stopSimulation();
    Q_INVOKABLE void startHardwareSimulation(int intervalMs, double minVal, double maxVal);
    Q_INVOKABLE void startHardwareSimulationWithInputs(int intervalMs, const QVariantList &inputs);
    Q_INVOKABLE void stopHardwareSimulation();
    Q_INVOKABLE bool saveMonitorLog(const QString &path) const;

    // ── Flash ─────────────────────────────────────────────────────────────
    QVariantList flashLines()    const { return m_flashLines; }
    int          flashProgress() const { return m_flashProgress; }
    bool         flashBusy()     const { return m_flashBusy; }
    Q_INVOKABLE void flashFirmware(const QString &path, const QString &modelName,
                                   const QString &architecture, const QString &quantization,
                                   bool simulationMode);
    Q_INVOKABLE void cancelFlash();
    Q_INVOKABLE void clearFlashLog();
    Q_INVOKABLE QString fileInfo(const QString &path) const;

    // ── Pipeline ─────────────────────────────────────────────────────────
    QVariantList pipelineLines() const { return m_pipelineLines; }
    int          pipelineProgress() const { return m_pipelineProgress; }
    bool         pipelineBusy() const { return m_pipelineBusy; }
    QString      pipelineStage() const { return m_pipelineStage; }
    Q_INVOKABLE void runPipeline(const QVariantMap &config);
    Q_INVOKABLE void cancelPipeline();
    Q_INVOKABLE void clearPipelineLog();

    // ── Benchmark ────────────────────────────────────────────────────────
    QVariantList benchmarkLines() const { return m_benchmarkLines; }
    QVariantMap  benchmarkMetrics() const { return m_benchmarkMetrics; }
    bool         benchmarkBusy() const { return m_benchmarkBusy; }
    Q_INVOKABLE QVariantMap deployedModelInfo() const;
    Q_INVOKABLE QVariantList deployedModelInputSpecs();
    Q_INVOKABLE QVariantList modelInputSpecs(const QString &modelPath);
    Q_INVOKABLE void startBenchmark(int samples, double minValue, double maxValue, int seed);
    Q_INVOKABLE void startBenchmarkWithInputs(int samples, const QVariantList &inputs, int seed);
    Q_INVOKABLE void cancelBenchmark();
    Q_INVOKABLE void clearBenchmarkLog();

    // ── Board probe ──────────────────────────────────────────────────────
    bool probeBusy() const { return m_probeBusy; }
    QString probeStatus() const { return m_probeStatus; }
    Q_INVOKABLE QVariantList availablePortEntries() const;
    Q_INVOKABLE void probeStLinkBoard();
    Q_INVOKABLE void probeStLinkBoardForPort(const QString &portName);

    // ── Analysis (read) ───────────────────────────────────────────────────
    QVariantList benchmarkRecords()  const;
    QVariantList simulationRecords() const;
    QVariantList sensorRecords()     const;
    QVariantList compiledRecords()   const;
    Q_INVOKABLE void deleteAnalysisRecord(int id);
    Q_INVOKABLE QVariantList recordsForKindQml(const QString &kind) const;
    Q_INVOKABLE QVariantList recentAnalysisRecords(int limit) const;
    Q_INVOKABLE bool flashCompiledModel(int recordId);
    Q_INVOKABLE bool exportAnalysisCsv(const QString &path,
                                       const QVariantList &columns,
                                       const QVariantList &rows);
    Q_INVOKABLE bool exportAnalysisPdf(const QString &path,
                                       const QString &title,
                                       const QVariantList &columns,
                                       const QVariantList &rows);

signals:
    void toolPathsChanged();
    void scanningChanged();
    void monitorLinesChanged();
    void simRunningChanged();
    void flashLinesChanged();
    void flashProgressChanged();
    void flashBusyChanged();
    void analysisChanged();
    void pipelineChanged();
    void pipelineFinished(bool success, const QVariantMap &artifact);
    void benchmarkChanged();
    void probeChanged();
    void probeFinished(bool success, const QString &message);
    void statusMessage(const QString &text);
    void sensorAnalysisChanged();

private:
    void appendMonitorLine(const QString &text, const QString &type);
    void appendFlashLine(const QString &text, const QString &type);
    void appendPipelineLine(const QString &text, const QString &type);
    void appendBenchmarkLine(const QString &text, const QString &type);
    QVariantList recordsForKind(const QString &kind) const;
    PipelineConfig pipelineConfigFromMap(const QVariantMap &config) const;
    void addCompiledRecord(const PipelineConfig &config);
    void applyDetectedStLinkBoard(const QString &probeOutput);
    void requestBoardInfoBurst();
    void wireSerial();
    void wireFlash();
    void wireAnalysis();

    // simulation helpers
    void tickSimulation();
    void tickHardwareSimulation();

    AppState        *m_state    = nullptr;
    SerialManager   *m_serial   = nullptr;
    FlashManager    *m_flash    = nullptr;
    AnalysisManager *m_analysis = nullptr;
    ToolDetector    *m_detector = nullptr;

    // monitor
    QVariantList m_monitorLines;
    bool         m_scanning    = false;
    qint64       m_lastInferenceLogMs = 0;
    qint64       m_lastSensorLogMs = 0;
    qint64       m_lastSysLogMs = 0;

    bool         m_sensorAnalysisRunning = false;
    QDateTime    m_sensorAnalysisStartedAt;
    quint32      m_sensorAnalysisCount = 0;
    quint64      m_sensorAnalysisTotalInfUs = 0;
    quint64      m_sensorAnalysisTotalRamB = 0;
    quint64      m_sensorAnalysisTotalAcc = 0;
    QString      m_sensorAnalysisLastModel;
    QString      m_sensorAnalysisLastSensor;
    QString      m_sensorAnalysisLastCard;
    QString      m_sensorAnalysisLastLabel;
    QHash<QString, int> m_sensorAnalysisLabelCounts;

    // simulation
    QTimer       *m_simTimer   = nullptr;
    PacketParser *m_simParser  = nullptr;
    bool          m_simRunning = false;
    double        m_simMin     = 0.0;
    double        m_simMax     = 1.0;
    QVariantList  m_simInputRanges;
    quint32       m_simUptime  = 0;
    // Session accumulators: collect every tick, persist the average on stop.
    quint32       m_simSampleCount = 0;
    quint64       m_simTotalInfUs  = 0;
    quint64       m_simTotalRamB   = 0;
    quint64       m_simTotalAcc    = 0;
    QString       m_simLastLabel;
    QString       m_simLastModel;
    QString       m_simLastCard;
    // Returns label list appropriate for the currently active sensor type.
    QStringList simLabelsForSensor() const;

    // flash
    QVariantList m_flashLines;
    int          m_flashProgress = 0;
    bool         m_flashBusy     = false;

    // pipeline
    PipelineRunner *m_pipelineRunner = nullptr;
    QTimer         *m_pipelinePulseTimer = nullptr;
    QVariantList    m_pipelineLines;
    int             m_pipelineProgress = 0;
    bool            m_pipelineBusy = false;
    QString         m_pipelineStage = "Hazır";
    PipelineConfig  m_lastPipelineConfig;

    // benchmark
    QVariantList m_benchmarkLines;
    QVariantMap  m_benchmarkMetrics;
    bool         m_benchmarkBusy = false;
    QTimer      *m_benchmarkTimeout = nullptr;

    // hardware simulation
    QTimer  *m_hwSimTimer = nullptr;
    bool     m_hwSimRunning = false;
    double   m_hwSimMin = 0.0;
    double   m_hwSimMax = 1.0;
    QVariantList m_hwSimInputRanges;
    quint32  m_hwSimSeed = 1234;
    quint32  m_hwSimSentCount = 0;
    quint32  m_hwSimResponseCount = 0;

    // ST-Link probe
    QProcess *m_stlinkProbe = nullptr;
    bool      m_probeBusy = false;
    QString   m_probeStatus;

    static constexpr int kMaxMonitorLines = 500;
};
