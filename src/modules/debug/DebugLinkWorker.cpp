#include "DebugLinkWorker.h"
#include "GdbRspCodec.h"

#include <QDebug>
#include <QHostAddress>
#include <QTcpSocket>
#include <QTimer>

#include <utility>

namespace {
constexpr quint64 kDhcsrAddr = 0xE000EDF0u;
constexpr quint32 kDhcsrLen  = 4u;
constexpr int      kHealthIntervalMs = 250;   // 4 Hz
constexpr int      kStatsWindowMs    = 500;

quint32 bytesToU32Le(const QByteArray &d)
{
    return quint32(static_cast<unsigned char>(d[0]))
         | (quint32(static_cast<unsigned char>(d[1])) << 8)
         | (quint32(static_cast<unsigned char>(d[2])) << 16)
         | (quint32(static_cast<unsigned char>(d[3])) << 24);
}
}

DebugLinkWorker::DebugLinkWorker(QObject *parent) : QObject(parent)
{
    qRegisterMetaType<MemoryRequest>();
    qRegisterMetaType<MemoryReply>();
    qRegisterMetaType<QVector<MemoryRequest>>();
    qRegisterMetaType<QVector<MemoryReply>>();
    qRegisterMetaType<WatchSampleBatch>();
}

DebugLinkWorker::~DebugLinkWorker()
{
    if (m_socket && m_socket->state() != QAbstractSocket::UnconnectedState)
        m_socket->abort();
}

// ── Connection lifecycle ─────────────────────────────────────────────────

void DebugLinkWorker::connectToServer(quint16 port)
{
    m_port = port;
    m_rxBuffer.clear();
    m_ackMode = true;
    m_step = HandshakeStep::Idle;
    m_packetSizeBytes = 0;
    m_jobQueue.clear();
    m_awaitingReply = false;

    if (m_socket) {
        m_socket->disconnect(this);
        m_socket->deleteLater();
        m_socket = nullptr;
    }

    m_socket = new QTcpSocket(this);
    connect(m_socket, &QTcpSocket::connected,    this, &DebugLinkWorker::onConnected);
    connect(m_socket, &QTcpSocket::readyRead,    this, &DebugLinkWorker::onReadyRead);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &DebugLinkWorker::onSocketError);
    connect(m_socket, &QTcpSocket::disconnected, this, &DebugLinkWorker::onDisconnected);

    m_sessionClock.start();
    m_socket->connectToHost(QHostAddress::LocalHost, port);
}

void DebugLinkWorker::disconnectFromServer()
{
    if (m_sampleTimer) m_sampleTimer->stop();
    if (m_healthTimer) m_healthTimer->stop();
    m_sampling = false;
    m_jobQueue.clear();
    m_awaitingReply = false;

    if (m_socket) {
        // This function is the single source of the socketClosed() emission
        // below — disconnect onDisconnected() first so a disconnected()
        // signal delivered synchronously by waitForDisconnected() (the
        // common case) doesn't emit socketClosed() a second time.
        disconnect(m_socket, &QTcpSocket::disconnected, this, &DebugLinkWorker::onDisconnected);

        if (m_socket->state() == QAbstractSocket::ConnectedState) {
            sendRaw(QByteArrayLiteral("D"));   // detach — observer principle: this is the only teardown packet
            m_socket->flush();
            m_socket->disconnectFromHost();
            if (m_socket->state() != QAbstractSocket::UnconnectedState)
                m_socket->waitForDisconnected(500);
        }
        m_socket->deleteLater();
        m_socket = nullptr;
    }
    m_step = HandshakeStep::Idle;
    emit socketClosed();
}

void DebugLinkWorker::onConnected()
{
    emit socketConnected();
    m_step = HandshakeStep::AwaitQSupportedReply;
    sendRaw(QByteArrayLiteral("qSupported:swbreak+;hwbreak+"));
}

void DebugLinkWorker::onReadyRead()
{
    if (!m_socket) return;
    m_rxBuffer += m_socket->readAll();
    processBuffer();
}

void DebugLinkWorker::onSocketError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error);
    const QString msg = m_socket ? m_socket->errorString() : tr("Bilinmeyen soket hatasi");
    if (m_step != HandshakeStep::Done)
        failHandshake(msg);
    else
        emit logLine(tr("Soket hatasi: %1").arg(msg));
}

void DebugLinkWorker::onDisconnected()
{
    emit socketClosed();
}

// ── RSP byte stream ──────────────────────────────────────────────────────

void DebugLinkWorker::processBuffer()
{
    QByteArray payload;
    for (;;) {
        const GdbRspCodec::Extract kind = GdbRspCodec::extract(m_rxBuffer, payload);
        switch (kind) {
        case GdbRspCodec::Extract::NeedMore:
            return;
        case GdbRspCodec::Extract::Ack:
        case GdbRspCodec::Extract::Nak:
            continue;   // protocol noise; the reply Packet itself drives state
        case GdbRspCodec::Extract::Notification:
            continue;   // '%Stop:' non-stop notifications — never a reply, always ignored
        case GdbRspCodec::Extract::Packet:
            if (m_ackMode && m_socket)
                m_socket->write("+");
            handlePacket(payload);
            continue;
        case GdbRspCodec::Extract::Garbage:
            continue;   // dropped by extract(); resync automatically
        }
    }
}

void DebugLinkWorker::handlePacket(const QByteArray &rawPayload)
{
    if (m_step != HandshakeStep::Done) {
        advanceHandshake(rawPayload);
        return;
    }

    if (!m_awaitingReply || m_jobQueue.isEmpty()) {
        emit logLine(tr("Beklenmeyen RSP paketi: %1").arg(QString::fromLatin1(rawPayload.left(64))));
        return;
    }

    Job &job = m_jobQueue.head();
    const MemoryRequest req = job.requests.at(job.nextIndex);
    const double rttMs = m_requestRtt.nsecsElapsed() / 1.0e6;

    MemoryReply reply;
    reply.id   = req.id;
    reply.addr = req.addr;

    QString errCode;
    if (GdbRspCodec::isErrorReply(rawPayload, &errCode)) {
        reply.ok    = false;
        reply.error = QStringLiteral("E%1").arg(errCode);
    } else {
        bool ok = false;
        reply.data = decodeMemoryReplyPayload(rawPayload, &ok);
        reply.ok   = ok;
        if (!ok)
            reply.error = QStringLiteral("decode error");
    }

    job.replies.append(reply);
    ++job.nextIndex;
    m_awaitingReply = false;

    m_rttSumMs += rttMs;
    ++m_rttCount;

    pumpNextRequest();
}

void DebugLinkWorker::advanceHandshake(const QByteArray &rawPayload)
{
    switch (m_step) {
    case HandshakeStep::Idle:
        // Nothing sent yet — a stray packet before qSupported went out.
        emit logLine(tr("Handshake oncesi beklenmeyen paket"));
        return;

    case HandshakeStep::AwaitQSupportedReply:
        m_packetSizeBytes = parsePacketSizeBytes(rawPayload);
        if (m_packetSizeBytes == 0)
            m_packetSizeBytes = 0x400; // conservative GDB default (1024) if the server omitted it
        m_step = HandshakeStep::AwaitNoAckReply;
        sendRaw(QByteArrayLiteral("QStartNoAckMode"));
        return;

    case HandshakeStep::AwaitNoAckReply:
        if (rawPayload != "OK") {
            failHandshake(tr("QStartNoAckMode reddedildi: %1").arg(QString::fromLatin1(rawPayload)));
            return;
        }
        // The '+' ack for this OK packet was already sent above (ack mode was
        // still on when it arrived) — from here on neither side acks.
        m_ackMode = false;
        m_step = HandshakeStep::AwaitNonStopReply;
        sendRaw(QByteArrayLiteral("QNonStop:1"));
        return;

    case HandshakeStep::AwaitNonStopReply:
        if (rawPayload != "OK") {
            failHandshake(tr("QNonStop:1 reddedildi: %1").arg(QString::fromLatin1(rawPayload)));
            return;
        }
        m_step = HandshakeStep::AwaitVContReply;
        sendRaw(QByteArrayLiteral("vCont;c"));
        return;

    case HandshakeStep::AwaitVContReply:
        if (rawPayload != "OK") {
            failHandshake(tr("vCont;c reddedildi: %1").arg(QString::fromLatin1(rawPayload)));
            return;
        }
        m_step = HandshakeStep::AwaitDhcsrReply;
        sendRaw(GdbRspCodec::memoryReadPacket(kDhcsrAddr, kDhcsrLen));
        return;

    case HandshakeStep::AwaitDhcsrReply: {
        bool ok = false;
        const QByteArray decoded = decodeMemoryReplyPayload(rawPayload, &ok);
        if (!ok || decoded.size() < 4) {
            failHandshake(tr("DHCSR okunamadi: %1").arg(QString::fromLatin1(rawPayload)));
            return;
        }
        const quint32 value = bytesToU32Le(decoded);
        bool halted = false, retired = false, resetSeen = false;
        evaluateDhcsr(value, &halted, &retired, &resetSeen);
        if (halted) {
            failHandshake(tr("Hedef durmus durumda (DHCSR S_HALT=1, deger=0x%1)")
                              .arg(value, 8, 16, QLatin1Char('0')));
            return;
        }
        m_step = HandshakeStep::Done;
        const double sessionElapsedS =
            m_sessionClock.isValid() ? double(m_sessionClock.nsecsElapsed()) / 1.0e9 : 0.0;
        emit handshakeSucceeded(computeMaxReadBytes(), value, sessionElapsedS);
        pumpNextRequest();   // release anything queued while handshaking
        return;
    }

    case HandshakeStep::Done:
        return; // unreachable — handlePacket() routes Done elsewhere
    }
}

void DebugLinkWorker::failHandshake(const QString &message)
{
    if (m_step == HandshakeStep::Done) return;
    m_step = HandshakeStep::Idle;
    if (m_socket) m_socket->abort();
    emit handshakeFailed(message);
}

void DebugLinkWorker::sendRaw(const QByteArray &payload)
{
    if (!GdbRspCodec::isAllowedOutgoing(payload)) {
        qWarning() << "BLOCKED outgoing RSP packet:" << payload;   // never sent — observer guarantee
        return;
    }
    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState)
        return;
    m_socket->write(GdbRspCodec::frame(payload));
}

// ── Request pump (strictly serial — one in-flight request at a time) ────

void DebugLinkWorker::readRanges(quint32 batchId, const QVector<MemoryRequest> &requests)
{
    Job job;
    job.kind     = JobKind::Batch;
    job.batchId  = batchId;
    job.requests = requests;
    m_jobQueue.enqueue(job);
    pumpNextRequest();
}

void DebugLinkWorker::pumpNextRequest()
{
    if (m_awaitingReply) return;
    if (m_step != HandshakeStep::Done) return;
    if (m_jobQueue.isEmpty()) return;

    Job &job = m_jobQueue.head();
    if (job.nextIndex >= job.requests.size()) {
        completeCurrentJob();
        return;
    }

    const MemoryRequest &req = job.requests.at(job.nextIndex);
    if (job.nextIndex == 0)
        job.firstWriteNs = m_sessionClock.isValid() ? m_sessionClock.nsecsElapsed() : 0;

    m_requestRtt.start();
    m_awaitingReply = true;
    sendRaw(GdbRspCodec::memoryReadPacket(req.addr, req.len));
}

void DebugLinkWorker::completeCurrentJob()
{
    if (m_jobQueue.isEmpty()) return;
    Job job = m_jobQueue.dequeue();

    switch (job.kind) {
    case JobKind::Batch:
        emit rangesRead(job.batchId, job.replies);
        break;

    case JobKind::Sample: {
        const double t = job.firstWriteNs / 1.0e9;
        const qint64 nowNs = m_sessionClock.nsecsElapsed();
        const double skewUs = (nowNs - job.firstWriteNs) / 1.0e3;
        emit rawSamplesReady(job.replies, t, skewUs);

        ++m_samplesInWindow;
        if (!m_statsWindow.isValid() || m_statsWindow.elapsed() >= kStatsWindowMs) {
            const qint64 elapsedMs = m_statsWindow.isValid() ? qMax<qint64>(1, m_statsWindow.elapsed()) : 1;
            const double actualHz = m_samplesInWindow * 1000.0 / elapsedMs;
            const double rttAvg = m_rttCount > 0 ? m_rttSumMs / m_rttCount : 0.0;
            emit samplingStats(actualHz, rttAvg, m_droppedSinceEmit);
            m_droppedSinceEmit = 0;
            m_rttSumMs = 0.0;
            m_rttCount = 0;
            m_samplesInWindow = 0;
            m_statsWindow.restart();
        }

        if (m_sampling && m_targetRateHz == 0)
            onSampleTimerTick();   // back-to-back max-rate re-arm
        break;
    }

    case JobKind::Health: {
        if (!job.replies.isEmpty() && job.replies.first().ok && job.replies.first().data.size() >= 4) {
            const quint32 value = bytesToU32Le(job.replies.first().data);
            bool halted = false, retired = false, resetSeen = false;
            evaluateDhcsr(value, &halted, &retired, &resetSeen);
            if (halted)    emit coreHalted();
            if (resetSeen) emit coreReset();
        }
        break;
    }
    }

    pumpNextRequest();
}

// ── Sampling ──────────────────────────────────────────────────────────────

void DebugLinkWorker::startSampling(const QVector<MemoryRequest> &plan, int targetRateHz)
{
    m_samplePlan   = plan;
    m_targetRateHz = targetRateHz;
    m_sampling     = true;
    m_droppedSinceEmit = 0;
    m_rttSumMs = 0.0;
    m_rttCount = 0;
    m_samplesInWindow = 0;
    m_statsWindow.start();

    if (!m_sampleTimer) {
        m_sampleTimer = new QTimer(this);
        connect(m_sampleTimer, &QTimer::timeout, this, &DebugLinkWorker::onSampleTimerTick);
    }
    if (!m_healthTimer) {
        m_healthTimer = new QTimer(this);
        m_healthTimer->setInterval(kHealthIntervalMs);
        connect(m_healthTimer, &QTimer::timeout, this, &DebugLinkWorker::onHealthTimerTick);
    }

    if (targetRateHz > 0) {
        // QTimer resolution is whole milliseconds, so the achievable rate is
        // quantised to 1000/N Hz. Round to the NEAREST millisecond rather than
        // truncating: truncation turned e.g. 600 Hz into 1000/1 = 1000 Hz
        // (further from the request than 1000/2 = 500 Hz) and made every
        // request above 500 Hz collapse onto 1000 Hz.
        m_sampleTimer->start(qMax(1, qRound(1000.0 / double(targetRateHz))));
    } else {
        m_sampleTimer->stop();
        onSampleTimerTick();   // kick off back-to-back mode; re-armed from completeCurrentJob()
    }
    m_healthTimer->start();
}

void DebugLinkWorker::stopSampling()
{
    m_sampling = false;
    if (m_sampleTimer) m_sampleTimer->stop();
    if (m_healthTimer) m_healthTimer->stop();

    // Drop queued-but-not-yet-started Sample/Health jobs; let an in-flight
    // one finish so the RSP request/reply stream doesn't desync.
    QQueue<Job> kept;
    for (int i = 0; i < m_jobQueue.size(); ++i) {
        const Job &job = m_jobQueue.at(i);
        const bool isHeadInFlight = (i == 0) && (job.nextIndex > 0 || m_awaitingReply);
        if (job.kind == JobKind::Batch || isHeadInFlight)
            kept.enqueue(job);
    }
    m_jobQueue = kept;
}

void DebugLinkWorker::onSampleTimerTick()
{
    if (!m_sampling || m_samplePlan.isEmpty()) return;

    for (const Job &job : std::as_const(m_jobQueue)) {
        if (job.kind == JobKind::Sample) {
            ++m_droppedSinceEmit;   // previous sample still queued/in-flight — missed deadline
            return;
        }
    }

    Job job;
    job.kind     = JobKind::Sample;
    job.requests = m_samplePlan;
    m_jobQueue.enqueue(job);
    pumpNextRequest();
}

void DebugLinkWorker::onHealthTimerTick()
{
    if (!m_sampling) return;

    Job job;
    job.kind = JobKind::Health;
    job.requests = { MemoryRequest{ 0, kDhcsrAddr, kDhcsrLen } };
    m_jobQueue.enqueue(job);
    pumpNextRequest();
}

// ── Decoding helpers ──────────────────────────────────────────────────────

void DebugLinkWorker::evaluateDhcsr(quint32 value, bool *haltedOut, bool *retiredOut, bool *resetOut)
{
    const bool halted  = (value & (1u << 17)) != 0;
    const bool retired = (value & (1u << 24)) != 0;
    const bool resetSt = (value & (1u << 25)) != 0;
    if (haltedOut)  *haltedOut  = halted;
    if (retiredOut) *retiredOut = retired;
    if (resetOut)   *resetOut   = resetSt;
    m_coreRunning = !halted;
}

quint32 DebugLinkWorker::parsePacketSizeBytes(const QByteArray &qSupportedReply) const
{
    const QList<QByteArray> parts = qSupportedReply.split(';');
    for (const QByteArray &part : parts) {
        if (part.startsWith("PacketSize=")) {
            bool ok = false;
            const quint32 v = part.mid(11).toUInt(&ok, 16);
            if (ok) return v;
        }
    }
    return 0;
}

quint32 DebugLinkWorker::computeMaxReadBytes() const
{
    if (m_packetSizeBytes <= 8) return 0;
    const quint32 fromPacketSize = (m_packetSizeBytes - 8) / 2;
    return qMin<quint32>(4096u, fromPacketSize);
}

QByteArray DebugLinkWorker::decodeMemoryReplyPayload(const QByteArray &rawPayload, bool *ok) const
{
    // Order matters: escaping produces the logical byte stream first, then
    // run-length compression (which repeats "the preceding character") is
    // expanded on top of that already-unescaped stream.
    const QByteArray unescaped = GdbRspCodec::unescape(rawPayload);
    bool rleOk = false;
    const QByteArray expanded = GdbRspCodec::expandRunLength(unescaped, &rleOk);
    if (!rleOk) {
        if (ok) *ok = false;
        return QByteArray();
    }
    bool hexOk = false;
    const QByteArray decoded = GdbRspCodec::hexDecode(expanded, &hexOk);
    if (ok) *ok = hexOk;
    return decoded;
}
