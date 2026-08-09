#pragma once

#include <QObject>
#include <QVariant>
#include <QVariantList>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>
#include <QHash>
#include <QDateTime>
#include <QElapsedTimer>
#include <QMap>
#include "modules/flash/PipelineConfig.h"
#include "modules/watcher/TraceEventLog.h"

class AppState;
class SerialManager;
class FlashManager;
class AnalysisManager;
class ToolDetector;
class PacketParser;
class SerialSimulator;
class PipelineRunner;
class RegisterInspector;
class RegisterAdvisor;
struct RegisterSnapshot;
struct SnapshotDiff;
class QProcess;
class DebugLink;
class VariableWatcher;

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

    // Register Inspector
    Q_PROPERTY(bool    registerBusy         READ registerBusy         NOTIFY registerChanged)
    Q_PROPERTY(QString registerStage        READ registerStage        NOTIFY registerChanged)
    Q_PROPERTY(QString registerSupportLevel READ registerSupportLevel NOTIFY registerChanged)
    Q_PROPERTY(QVariantList registerModel   READ registerModel   NOTIFY registerModelChanged)

    // Variable Watcher (Faz 4)
    Q_PROPERTY(bool         watchLinkOpen    READ watchLinkOpen    NOTIFY watchLinkChanged)
    Q_PROPERTY(QString      watchLinkState   READ watchLinkState   NOTIFY watchLinkChanged)
    Q_PROPERTY(QString      watchLinkError   READ watchLinkError   NOTIFY watchLinkChanged)
    Q_PROPERTY(bool         watchRunning     READ watchRunning     NOTIFY watchRunChanged)
    Q_PROPERTY(bool         watchPlayback    READ watchPlayback    NOTIFY watchLinkChanged)
    Q_PROPERTY(QVariantMap  watchRateInfo    READ watchRateInfo    NOTIFY watchStatsChanged)
    Q_PROPERTY(QVariantList watchItems       READ watchItems       NOTIFY watchItemsChanged)
    Q_PROPERTY(QVariantList watchViolations  READ watchViolations  NOTIFY watchViolationsChanged)
    Q_PROPERTY(QString      stlinkOwner      READ stlinkOwner      NOTIFY stlinkOwnerChanged)
    Q_PROPERTY(QString      watchElfMatch       READ watchElfMatch       NOTIFY watchElfMatchChanged)
    Q_PROPERTY(QVariantMap  watchElfMatchDetail READ watchElfMatchDetail NOTIFY watchElfMatchChanged)

public:
    explicit Backend(AppState          *state,
                     SerialManager     *serial,
                     FlashManager      *flash,
                     AnalysisManager   *analysis,
                     RegisterInspector *registers,
                     RegisterAdvisor   *advisor,
                     DebugLink         *debugLink,
                     VariableWatcher   *watcher,
                     QObject           *parent = nullptr);

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

    // ── Register Inspector ─────────────────────────────────────────────────
    bool    registerBusy() const;
    QString registerStage() const;
    QString registerSupportLevel() const;   // stable/experimental/unsupported
    QVariantList registerModel() const;      // decoded tree of the current view slot

    // Load the active board's SVD (async); registerCatalogReady fires when ready.
    Q_INVOKABLE void prepareRegisters();
    // All peripheral names for the active board (empty until SVD is parsed).
    Q_INVOKABLE QStringList registerPeripheralList() const;
    // Persisted selection (or the board's default preset if none saved).
    Q_INVOKABLE QStringList registerSelectedPeripherals() const;
    // The board's default preset (from boards.json), regardless of saved state.
    Q_INVOKABLE QStringList registerDefaultPeripherals() const;
    // Take a snapshot into slot 0(A)/1(B); persists the selection.
    Q_INVOKABLE void takeRegisterSnapshot(int slot, const QStringList &peripherals);
    // Metadata for a slot: {valid, board, device, svd, mode, support, takenAt,...}.
    Q_INVOKABLE QVariantMap registerSnapshotInfo(int slot) const;
    Q_INVOKABLE void setRegisterViewSlot(int slot);
    Q_INVOKABLE void clearRegisterSnapshots();

    // ── Register read backend (Faz 2) ────────────────────────────────────
    // "cli" | "gdb" — a PREFERENCE; RegisterInspector re-verifies it every
    // snapshot and silently falls back to "cli" if unavailable. Default is
    // permanently "cli" (docs/variable_watcher_plan.md Bolum 5.2).
    Q_INVOKABLE QString registerReadBackend() const;
    Q_INVOKABLE void    setRegisterReadBackend(const QString &backend);

    // ── Register diff (Bolum 1a) ─────────────────────────────────────────
    // True once both Snapshot A and B are filled (regardless of whether they
    // actually differ) — used to enable the "Diff" button.
    Q_INVOKABLE bool registerDiffAvailable() const;
    // {comparable, incomparableReason, changedRegisterCount, changedFieldCount,
    //  changedRegisters:[{peripheral,register,addr,statusA,statusB,rawA,rawB,
    //    changedFields:[{name,description,bitOffset,bitWidth,valueA,valueB,enumNameA,enumNameB}]}]}
    Q_INVOKABLE QVariantMap registerDiff() const;

    // ── Register rule engine (Bolum 1b) ──────────────────────────────────
    // Deterministic (non-LLM) consistency checks against a decoded slot.
    // [{ruleId,severity,peripheral,register,field,message}]
    Q_INVOKABLE QVariantList registerRuleViolations(int slot) const;

    // ── Optional LLM diagnosis (Bolum 1c) ────────────────────────────────
    // False whenever base URL or API key is unset — UI should hide/disable
    // the diagnosis panel in that case rather than let the user hit "failed".
    Q_INVOKABLE bool llmConfigured() const;
    Q_INVOKABLE bool llmBusy() const;
    Q_INVOKABLE QVariantMap llmSettings() const;   // {baseUrl, apiKey, model}
    Q_INVOKABLE void setLlmSettings(const QString &baseUrl, const QString &apiKey,
                                    const QString &model);
    // Sends the current diff (if both slots filled) + rule violations for the
    // active view slot. Result arrives via registerDiagnosisReady/Failed.
    Q_INVOKABLE void requestRegisterDiagnosis();

    // ── JSON export (Bolum 1d) ───────────────────────────────────────────
    // Exports the active view slot, plus diff + rule violations when both
    // slots are available. See docs/register_export_schema.md.
    Q_INVOKABLE bool exportRegisterSnapshotJson(const QString &path);

    // ── Variable Watcher (Faz 4) ──────────────────────────────────────────
    bool         watchLinkOpen() const;
    QString      watchLinkState() const;
    QString      watchLinkError() const;
    bool         watchRunning() const;
    bool         watchPlayback() const { return false; }   // Faz 7 not implemented yet
    QVariantMap  watchRateInfo() const;
    QVariantList watchItems() const;
    QVariantList watchViolations() const { return {}; }     // Faz 8 (TimeSeriesRuleEngine) not implemented yet
    QString      stlinkOwner() const { return m_stlinkOwner; }
    QString      watchElfMatch() const;
    QVariantMap  watchElfMatchDetail() const;

    // Link — retain()/release() on the shared DebugLink, arbitrated against
    // flash/pipeline/probe/register (Bolum 7.5).
    Q_INVOKABLE void openWatchLink();
    Q_INVOKABLE void closeWatchLink();

    // Symbols
    Q_INVOKABLE QString      watchElfPath() const;
    Q_INVOKABLE QString      suggestedElfPath() const;   // deployedModelOutputDir()/build/*.elf
    Q_INVOKABLE void         loadWatchElf(const QString &path);
    Q_INVOKABLE QVariantList watchSymbols(const QString &filter, int limit) const;

    // Watch items
    Q_INVOKABLE void addWatchSymbol(const QString &symbolName);
    Q_INVOKABLE void addWatchAddress(const QString &addrHex, const QString &type, const QString &label);
    Q_INVOKABLE void updateWatchItem(const QString &id, const QVariantMap &props);
    Q_INVOKABLE void removeWatchItem(const QString &id);
    Q_INVOKABLE void clearWatchItems();

    // Run
    Q_INVOKABLE void startWatch(int targetRateHz);   // 0 = max
    Q_INVOKABLE void stopWatch();
    Q_INVOKABLE void clearWatchData();
    // watchElfMatch == "mismatch" iken startWatch() reddedilir; acikca
    // cagrilmadan ornekleme baslamaz.
    Q_INVOKABLE void acknowledgeElfMismatch();

    // ── Variable Watcher - Faz 6 (grafik + zaman ekseni) ─────────────────
    // Frame for TracePlot: [{ id, label, color, unit, points, yMin, yMax,
    // laneIndex }], one entry per ENABLED item, decimated over
    // [now-windowSec, now]. Y auto-scale grows instantly (never clips live
    // data) but shrinks with ~1s smoothing (plan Bolum 9.3 — "grafik
    // ziplamaz"). columns is clamped to [1,800] (plan Bolum 9.4 payload cap).
    Q_INVOKABLE QVariantList watchPlotFrame(int columns, double windowSec);
    // Events in [fromT, toT] of the shared session clock (TraceEventLog).
    Q_INVOKABLE QVariantList watchEvents(double fromT, double toT) const;
    // Full WatchStats for one item, formatted with ValueCodec (same as the
    // table columns) plus raw numeric fields for programmatic use.
    Q_INVOKABLE QVariantMap  watchItemStats(const QString &id) const;
    // Cursor read: nearest-sample value per item at time t (session clock).
    Q_INVOKABLE QVariantMap  watchValuesAt(double t) const;
    // Seconds elapsed on the shared session clock "now" (TraceEventLog) -
    // the upper bound TracePlotView should use for a live-scrolling window.
    Q_INVOKABLE double       watchSessionNow() const;

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
    void registerChanged();
    void registerModelChanged();
    void registerCatalogReady(const QString &boardName);
    void registerSnapshotReady(int slot);
    void registerDiagnosisReady(const QVariantList &hypotheses);
    void registerDiagnosisFailed(const QString &message);

    void watchLinkChanged();
    void watchRunChanged();
    void watchItemsChanged();
    void watchStatsChanged();
    void watchViolationsChanged();
    void watchSymbolsLoaded(int count);
    void watchError(const QString &message);
    void stlinkOwnerChanged();
    void watchElfMatchChanged();

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
    void handleN6TextLine(const QString &line);
    void resetN6TargetForCapture(const QString &reason, bool benchmarkLog);
    void wireSerial();
    void wireFlash();
    void wireAnalysis();
    void wireRegisters();
    QVariantList snapshotToVariant(const RegisterSnapshot &snap) const;
    QVariantMap  diffToVariant(const SnapshotDiff &diff) const;

    void wireWatcher();
    // Single named arbiter for the shared ST-Link (Bolum 7.5). Tracked in
    // PARALLEL to but SEPARATE from DebugLink's own retain/release refcount —
    // this answers "which FEATURE is using the ST-Link", the refcount
    // answers "how many owners does the link have".
    bool acquireStLink(const QString &who);
    void releaseStLink(const QString &who);

    // simulation helpers
    void tickSimulation();
    void tickHardwareSimulation();

    AppState          *m_state    = nullptr;
    SerialManager     *m_serial   = nullptr;
    FlashManager      *m_flash    = nullptr;
    AnalysisManager   *m_analysis = nullptr;
    ToolDetector      *m_detector = nullptr;
    RegisterInspector *m_registers = nullptr;
    RegisterAdvisor   *m_advisor = nullptr;
    DebugLink         *m_debugLink = nullptr;
    VariableWatcher   *m_watcher = nullptr;
    TraceEventLog      m_eventLog;   // Faz 6 shared timeline; reset() on watch link open
    // Per-item smoothed plot Y-range (Bolum 9.3 "kucculme 1s yumusatmayla").
    // Keyed by WatchItem::id; grows instantly, shrinks exponentially.
    QMap<QString, QPair<double, double>> m_plotYRange;
    QElapsedTimer      m_plotFrameClock;   // dt between watchPlotFrame() calls, for the shrink smoothing
    QString            m_stlinkOwner;   // "" | "watch" (flash/pipeline/probe/register keep their own flags)
    int                m_registerViewSlot = 0;   // which slot registerModel shows

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
    double       m_n6LastCpuMhz = 600.0;
    QString      m_n6LastTextModel;
    QString      m_n6LastTextLabel;
    qint64       m_n6LastResetRequestMs = 0;

    // hardware simulation
    QTimer  *m_hwSimTimer = nullptr;
    bool     m_hwSimRunning = false;
    bool     m_hwSimPassiveCapture = false;
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
