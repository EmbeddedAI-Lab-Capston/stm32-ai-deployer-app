#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QVector>

#include "DebugLinkTypes.h"

class MemReadClient;
class QTimer;

// ── MemReadWorker ────────────────────────────────────────────────────────
// Lives on a dedicated QThread and samples a target through MemReadClient, the
// way DebugLinkWorker does through the RSP socket. It exists because
// ST-LINK_gdbserver cannot create a session on the STM32N6 at all, and because
// launching STM32_Programmer_CLI per sample caps the rate at ~3 Hz.
//
// The two workers deliberately emit the same signals with the same meanings, so
// DebugLink can own either one without its public surface - and therefore
// VariableWatcher and Backend - changing at all.
//
// The transport here is blocking, which removes the whole in-flight/queue state
// machine RSP needs: a sample is one call that returns in under 2 ms. Blocking
// is only safe off the UI thread, which is exactly where this object lives.
//
// The observer principle is enforced one level down: the sidecar never resolves
// a symbol that could write to or reset a target, so nothing this class does
// can disturb one.
class MemReadWorker : public QObject
{
    Q_OBJECT

public:
    explicit MemReadWorker(QObject *parent = nullptr);
    ~MemReadWorker() override;

    // Must be called before openLink(), from the owning thread.
    void setPaths(const QString &sidecarPath, const QString &cubeProgrammerBinDir);
    void setStlinkSerial(const QString &serial);

public slots:
    void openLink();
    void closeLink();

    // One-shot batch read (Register Inspector path). Replies arrive in the same
    // order as `requests`.
    void readRanges(quint32 batchId, const QVector<MemoryRequest> &requests);

    // Continuous sampling (Variable Watcher path). 0 Hz = back-to-back max rate.
    void startSampling(const QVector<MemoryRequest> &plan, int targetRateHz);
    void stopSampling();

signals:
    // sessionElapsedS is the sample clock's reading at the moment the link
    // opened. Sample timestamps are measured against that same clock, which
    // starts in openLink() - BEFORE the connection completes. Main-thread
    // consumers need this to put their event timeline on the same origin
    // instead of starting a second, later-zeroed clock; getting that wrong once
    // shifted the two axes by 61 ms and silently disabled every event-gated
    // rule (CLAUDE.md, review K-2).
    void opened(quint32 maxReadBytes, quint32 dhcsrValue, double sessionElapsedS);
    void failed(const QString &message);
    void closed();

    void rangesRead(quint32 batchId, const QVector<MemoryReply> &replies);
    void rawSamplesReady(const QVector<MemoryReply> &replies, double t, double skewUs);
    void samplingStats(double actualRateHz, double rttMsAvg, quint32 dropped);

    void coreHalted();   // DHCSR S_HALT set -> sampling stops
    void coreReset();    // DHCSR S_RESET_ST set -> event only, sampling continues

    void logLine(const QString &line);

private slots:
    void onSampleTimerTick();
    void onHealthTimerTick();

private:
    bool readDhcsr(quint32 &value);
    void evaluateDhcsr(quint32 value);
    void reportStatsIfDue();
    void handleTransportLoss(const QString &reason);

    MemReadClient *m_client = nullptr;
    QTimer        *m_sampleTimer = nullptr;
    QTimer        *m_healthTimer = nullptr;

    QVector<MemoryRequest> m_samplePlan;
    int     m_targetRateHz = 0;
    bool    m_sampling = false;
    bool    m_inSample = false;
    bool    m_open = false;

    QElapsedTimer m_sessionClock;
    QElapsedTimer m_statsWindow;
    quint32 m_droppedSinceEmit = 0;
    double  m_rttSumMs = 0.0;
    int     m_rttCount = 0;
    int     m_samplesInWindow = 0;
};
