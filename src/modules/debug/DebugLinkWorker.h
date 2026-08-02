#pragma once

#include "DebugLinkTypes.h"

#include <QAbstractSocket>
#include <QByteArray>
#include <QElapsedTimer>
#include <QList>
#include <QObject>
#include <QQueue>
#include <QVector>

class QTcpSocket;
class QTimer;

// ── DebugLinkWorker ──────────────────────────────────────────────────────
// Lives on a dedicated QThread (same pattern as SerialWorker). Owns the
// QTcpSocket to ST-LINK_gdbserver.exe, runs the RSP handshake, and executes
// memory-read requests strictly serially — one request in flight at a time,
// because gdbserver answers one connection synchronously.
//
// Every byte this class writes to the socket is gated through
// GdbRspCodec::isAllowedOutgoing() first. That is the observer-principle
// enforcement point (docs/variable_watcher_plan.md Bolum 4.3/4.5, CLAUDE.md
// "Degisken Izleyici" decision record) — nothing bypasses it.
//
// Faz 1 scope note: startSampling()/stopSampling() are implemented and
// functionally correct (continuous serial reads of the given plan, correct
// per-sample timestamp/skew contract per DebugLinkTypes.h), but they emit
// rawSamplesReady() once per completed sample rather than coalescing several
// samples into one cross-thread signal. The plan's Bolum 2.2 "worker never
// emits per sample" rule targets the eventual high-rate consumer
// (WatchSampler/TraceBuffer, Faz 4), which does not exist yet; that class is
// the natural place to add the <=30 Hz / 512-sample coalescing gate once it
// exists, matching the plan's own Bolum 4.6 scope note ("Faz 1'de DebugLink
// yalniz readRanges + handshake seviyesinde tamamlanir").
class DebugLinkWorker : public QObject
{
    Q_OBJECT

public:
    explicit DebugLinkWorker(QObject *parent = nullptr);
    ~DebugLinkWorker() override;

public slots:
    void connectToServer(quint16 port);
    void disconnectFromServer();

    // One-shot batch read (Register Inspector path). Replies arrive in the
    // same order as `requests`.
    void readRanges(quint32 batchId, const QVector<MemoryRequest> &requests);

    // Continuous sampling (Variable Watcher path). 0 Hz = back-to-back max rate.
    void startSampling(const QVector<MemoryRequest> &plan, int targetRateHz);
    void stopSampling();

signals:
    void socketConnected();   // TCP connected, RSP handshake about to start
    void handshakeSucceeded(quint32 maxReadBytes, quint32 dhcsrValue);
    void handshakeFailed(const QString &message);
    void socketClosed();

    void rangesRead(quint32 batchId, const QVector<MemoryReply> &replies);
    void rawSamplesReady(const QVector<MemoryReply> &replies, double t, double skewUs);
    void samplingStats(double actualRateHz, double rttMsAvg, quint32 dropped);

    void coreHalted();   // DHCSR S_HALT set -> sampling stops
    void coreReset();    // DHCSR S_RESET_ST set -> event only, sampling continues

    void logLine(const QString &line);

private slots:
    void onConnected();
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError error);
    void onDisconnected();
    void onSampleTimerTick();
    void onHealthTimerTick();

private:
    enum class HandshakeStep {
        Idle, AwaitQSupportedReply, AwaitNoAckReply,
        AwaitNonStopReply, AwaitVContReply, AwaitDhcsrReply, Done
    };

    enum class JobKind { Batch, Sample, Health };

    struct Job {
        JobKind kind = JobKind::Batch;
        quint32 batchId = 0;
        QVector<MemoryRequest> requests;
        QVector<MemoryReply>   replies;
        int     nextIndex   = 0;
        qint64  firstWriteNs = -1;
    };

    void processBuffer();
    void handlePacket(const QByteArray &rawPayload);
    void advanceHandshake(const QByteArray &rawPayload);
    void failHandshake(const QString &message);
    void sendRaw(const QByteArray &payload);
    void pumpNextRequest();
    void completeCurrentJob();
    void evaluateDhcsr(quint32 value, bool *haltedOut, bool *retiredOut, bool *resetOut);
    quint32 parsePacketSizeBytes(const QByteArray &qSupportedReply) const;
    quint32 computeMaxReadBytes() const;   // min(4096, (PacketSize-8)/2)
    QByteArray decodeMemoryReplyPayload(const QByteArray &rawPayload, bool *ok) const;

    QTcpSocket *m_socket = nullptr;
    QByteArray  m_rxBuffer;
    bool        m_ackMode = true;

    HandshakeStep m_step = HandshakeStep::Idle;
    quint32       m_packetSizeBytes = 0;

    QQueue<Job> m_jobQueue;
    bool        m_awaitingReply = false;
    QElapsedTimer m_requestRtt;

    QElapsedTimer m_sessionClock;

    QTimer *m_sampleTimer = nullptr;
    int     m_targetRateHz = 0;
    bool    m_sampling     = false;
    QVector<MemoryRequest> m_samplePlan;
    quint32 m_droppedSinceEmit = 0;
    double  m_rttSumMs = 0.0;
    int     m_rttCount = 0;
    QElapsedTimer m_statsWindow;
    int     m_samplesInWindow = 0;

    QTimer *m_healthTimer = nullptr;
    bool    m_coreRunning = true;

    quint16 m_port = 0;
};
