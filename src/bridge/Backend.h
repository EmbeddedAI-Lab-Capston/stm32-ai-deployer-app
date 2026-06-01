#pragma once

#include <QObject>
#include <QVariant>
#include <QVariantList>
#include <QStringList>
#include <QTimer>

class AppState;
class SerialManager;
class FlashManager;
class AnalysisManager;
class ToolDetector;
class PacketParser;
class SerialSimulator;

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

    // Flash
    Q_PROPERTY(QVariantList flashLines   READ flashLines   NOTIFY flashLinesChanged)
    Q_PROPERTY(int  flashProgress        READ flashProgress NOTIFY flashProgressChanged)
    Q_PROPERTY(bool flashBusy            READ flashBusy     NOTIFY flashBusyChanged)

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

    // ── Monitor terminal ──────────────────────────────────────────────────
    QVariantList monitorLines() const { return m_monitorLines; }
    Q_INVOKABLE void clearMonitor();

    // ── Simulation (Monitor screen) ───────────────────────────────────────
    bool simRunning() const { return m_simRunning; }
    Q_INVOKABLE void startSimulation(int intervalMs, double minVal, double maxVal);
    Q_INVOKABLE void stopSimulation();

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

    // ── Analysis (read) ───────────────────────────────────────────────────
    QVariantList benchmarkRecords()  const;
    QVariantList simulationRecords() const;
    QVariantList sensorRecords()     const;
    QVariantList compiledRecords()   const;
    Q_INVOKABLE void deleteAnalysisRecord(int id);

signals:
    void toolPathsChanged();
    void scanningChanged();
    void monitorLinesChanged();
    void simRunningChanged();
    void flashLinesChanged();
    void flashProgressChanged();
    void flashBusyChanged();
    void analysisChanged();
    void statusMessage(const QString &text);

private:
    void appendMonitorLine(const QString &text, const QString &type);
    void appendFlashLine(const QString &text, const QString &type);
    QVariantList recordsForKind(const QString &kind) const;
    void wireSerial();
    void wireFlash();
    void wireAnalysis();

    // simulation helpers
    void tickSimulation();

    AppState        *m_state    = nullptr;
    SerialManager   *m_serial   = nullptr;
    FlashManager    *m_flash    = nullptr;
    AnalysisManager *m_analysis = nullptr;
    ToolDetector    *m_detector = nullptr;

    // monitor
    QVariantList m_monitorLines;
    bool         m_scanning    = false;

    // simulation
    QTimer       *m_simTimer   = nullptr;
    PacketParser *m_simParser  = nullptr;
    bool          m_simRunning = false;
    double        m_simMin     = 0.0;
    double        m_simMax     = 1.0;
    quint32       m_simUptime  = 0;
    QStringList   m_simLabels  = {"normal", "anomaly", "walking", "running", "idle"};

    // flash
    QVariantList m_flashLines;
    int          m_flashProgress = 0;
    bool         m_flashBusy     = false;

    static constexpr int kMaxMonitorLines = 500;
};
