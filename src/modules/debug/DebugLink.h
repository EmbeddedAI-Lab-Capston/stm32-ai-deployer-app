#pragma once

#include "DebugLinkTypes.h"

#include <QObject>
#include <QString>
#include <QVector>

class QThread;
class GdbServerProcess;
class DebugLinkWorker;

// ── DebugLink ─────────────────────────────────────────────────────────────
// Main-thread facade over GdbServerProcess (main thread) + DebugLinkWorker
// (worker QThread) — the same "manager owns thread+worker" shape as
// SerialManager/SerialWorker. This is the ONE ST-Link connection Register
// Inspector and Variable Watcher share; a single instance is created in
// main.cpp and handed to both (docs/variable_watcher_plan.md Bolum 2.1, 4.6).
//
// ── Reference-counted session (binding contract, plan Bolum 4.6) ─────────
// The counter going 0->1 opens the link; 1->0 closes it. Every retain() MUST
// be matched by exactly one release() — error and cancel paths included.
// There is deliberately no public open()/close(): two owners sharing one
// ST-Link means a raw close() from one would yank the link out from under
// the other. A FAILED retain() (failed() signal fires) does NOT consume the
// counter — the caller must NOT call release() in that case; the counter is
// reset to 0 by the failure handler itself.
class DebugLink : public QObject
{
    Q_OBJECT

public:
    explicit DebugLink(QObject *parent = nullptr);
    ~DebugLink() override;

    void setPaths(const QString &gdbServerPath, const QString &cubeProgrammerBinDir);
    void setStlinkSerial(const QString &sn);

    DebugLinkState state() const { return m_state; }
    QString  lastError() const { return m_lastError; }
    bool     isOpen() const { return m_state == DebugLinkState::Open; }
    quint32  maxReadBytes() const { return m_maxReadBytes; }   // min(4096, (PacketSize-8)/2)
    // Reading of the sampling clock at the moment the link finished opening.
    // A main-thread timeline that wants to share one axis with sample times
    // must start at THIS value, not at zero (see TraceEventLog::reset()).
    double   sessionElapsedAtOpen() const { return m_sessionElapsedAtOpen; }
    bool     coreRunning() const { return m_coreRunning; }
    bool     isSampling() const { return m_sampling; }

    void retain();          // async; emits opened() immediately if already open
    void release();
    int  refCount() const { return m_refCount; }

    // Emergency teardown that IGNORES the reference count. The only caller is
    // qApp::aboutToQuit — process cleanup at exit must be unconditional, or a
    // leaked retain() would strand a gdbserver holding the ST-Link.
    void shutdownNow();

    // One-shot batch read (Register Inspector path). Replies arrive in order.
    void readRanges(quint32 batchId, const QVector<MemoryRequest> &requests);

    // Continuous sampling (Variable Watcher path).
    void startSampling(const QVector<MemoryRequest> &plan, int targetRateHz); // 0 = max
    void stopSampling();

signals:
    void stateChanged();
    void opened();
    void closed();
    void failed(const QString &message);
    void logLine(const QString &line);
    void rangesRead(quint32 batchId, const QVector<MemoryReply> &replies);
    void rawSamplesReady(const QVector<MemoryReply> &replies, double t, double skewUs);
    void samplingStats(double actualRateHz, double rttMsAvg, quint32 dropped);
    void coreHalted();     // S_HALT set -> sampling stopped
    void coreReset();      // S_RESET_ST set -> event logged, sampling continues

    // Internal — cross-thread command relays into the worker (queued).
    void requestConnect(quint16 port);
    void requestDisconnect();
    void requestReadRanges(quint32 batchId, const QVector<MemoryRequest> &requests);
    void requestStartSampling(const QVector<MemoryRequest> &plan, int targetRateHz);
    void requestStopSampling();

private slots:
    void onServerReady(quint16 port);
    void onServerFailed(const QString &message);
    void onServerCrashed(const QString &message);

    void onWorkerSocketConnected();
    void onWorkerHandshakeSucceeded(quint32 maxReadBytes, quint32 dhcsrValue, double sessionElapsedS);
    void onWorkerHandshakeFailed(const QString &message);
    void onWorkerSocketClosed();

private:
    void setState(DebugLinkState s);
    void failOpenAttempt(const QString &message);
    void closeInternal();

    GdbServerProcess *m_gdbProcess = nullptr;
    QThread          *m_thread     = nullptr;
    DebugLinkWorker  *m_worker     = nullptr;

    DebugLinkState m_state       = DebugLinkState::Closed;
    QString        m_lastError;
    int            m_refCount    = 0;
    quint32        m_maxReadBytes = 0;
    double         m_sessionElapsedAtOpen = 0.0;
    bool           m_coreRunning = true;
    bool           m_sampling    = false;
    bool           m_pendingClose = false;   // release() -> refCount 0 while gdbserver teardown is in flight
};
