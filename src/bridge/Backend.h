#pragma once

#include <QObject>
#include <QVariant>
#include <QVariantList>
#include <QStringList>

class AppState;
class SerialManager;
class FlashManager;
class AnalysisManager;
class ToolDetector;

// ── Backend ─────────────────────────────────────────────────────────────────
// Single QML-facing facade. Owns no business logic of its own; it forwards to
// the existing managers (AppState, SerialManager, FlashManager, AnalysisManager,
// ToolDetector) and adapts their data into QML-friendly types (QVariantList,
// Q_INVOKABLE methods, NOTIFY properties).
//
// Exposed to QML as the context property "backend".
class Backend : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QVariantList toolPaths   READ toolPaths   NOTIFY toolPathsChanged)
    Q_PROPERTY(QVariantList monitorLines READ monitorLines NOTIFY monitorLinesChanged)
    Q_PROPERTY(bool scanning READ isScanning NOTIFY scanningChanged)
    Q_PROPERTY(QVariantList flashLines READ flashLines NOTIFY flashLinesChanged)
    Q_PROPERTY(int  flashProgress READ flashProgress NOTIFY flashProgressChanged)
    Q_PROPERTY(bool flashBusy READ flashBusy NOTIFY flashBusyChanged)

public:
    explicit Backend(AppState        *state,
                     SerialManager   *serial,
                     FlashManager    *flash,
                     AnalysisManager *analysis,
                     QObject         *parent = nullptr);

    // ── Tool paths (Settings dialog) ──────────────────────────────────────
    QVariantList toolPaths() const;          // [{name, key, path, found}, ...]
    bool         isScanning() const { return m_scanning; }
    Q_INVOKABLE void scanTools();            // ToolDetector::detectAll()
    Q_INVOKABLE void setToolPath(const QString &key, const QString &path);

    // ── Serial (Board / Monitor screens) ──────────────────────────────────
    Q_INVOKABLE QStringList availablePorts() const;   // display strings
    Q_INVOKABLE QString     detectedStLinkPort() const;
    Q_INVOKABLE void        connectSerial(const QString &portName, int baud);
    Q_INVOKABLE void        disconnectSerial();

    // ── Monitor terminal ──────────────────────────────────────────────────
    QVariantList monitorLines() const { return m_monitorLines; }
    Q_INVOKABLE void clearMonitor();

    // ── Flash (Flash screen) ──────────────────────────────────────────────
    QVariantList flashLines()    const { return m_flashLines; }
    int          flashProgress() const { return m_flashProgress; }
    bool         flashBusy()     const { return m_flashBusy; }
    Q_INVOKABLE void flashFirmware(const QString &path, const QString &modelName,
                                   const QString &architecture, const QString &quantization,
                                   bool simulationMode);
    Q_INVOKABLE void cancelFlash();
    Q_INVOKABLE void clearFlashLog();
    Q_INVOKABLE QString fileInfo(const QString &path) const;   // "name · size"

signals:
    void toolPathsChanged();
    void monitorLinesChanged();
    void scanningChanged();
    void statusMessage(const QString &text);
    void flashLinesChanged();
    void flashProgressChanged();
    void flashBusyChanged();

private:
    void appendLine(const QString &text, const QString &type);
    void appendFlashLine(const QString &text, const QString &type);
    void wireSerial();
    void wireFlash();

    AppState        *m_state    = nullptr;
    SerialManager   *m_serial   = nullptr;
    FlashManager    *m_flash    = nullptr;
    AnalysisManager *m_analysis = nullptr;
    ToolDetector    *m_detector = nullptr;

    QVariantList m_monitorLines;
    bool         m_scanning = false;

    QVariantList m_flashLines;
    int          m_flashProgress = 0;
    bool         m_flashBusy = false;

    static constexpr int kMaxMonitorLines = 500;
};
