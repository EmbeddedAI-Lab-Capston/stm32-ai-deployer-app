#include "MemReadWorker.h"

#include <QTimer>
#include <QtMath>

#include "MemReadClient.h"

namespace {

// Cortex-M architectural addresses, not family-specific, so they belong in code
// rather than in boards.json (CLAUDE.md).
constexpr quint64 kDhcsrAddr = 0xE000EDF0ull;
constexpr quint32 kDhcsrLen  = 4u;

constexpr int kHealthIntervalMs = 250;   // 4 Hz, same as the RSP path
constexpr int kStatsWindowMs    = 500;

// Deliberately the same ceiling the gdbserver path ends up with, so
// WatchPlanBuilder splits a plan into identical blocks on either transport. The
// sidecar could serve far larger reads; raising this would change proven plan
// shapes for a gain nobody has measured yet.
constexpr quint32 kMaxReadBytes = 4096u;

quint32 bytesToU32Le(const QByteArray &bytes)
{
    if (bytes.size() < 4)
        return 0;
    return quint8(bytes[0]) | (quint8(bytes[1]) << 8)
           | (quint8(bytes[2]) << 16) | (quint32(quint8(bytes[3])) << 24);
}

} // namespace

MemReadWorker::MemReadWorker(QObject *parent) : QObject(parent)
{
    m_client = new MemReadClient(this);
    connect(m_client, &MemReadClient::logLine, this, &MemReadWorker::logLine);
}

MemReadWorker::~MemReadWorker()
{
    closeLink();
}

void MemReadWorker::setPaths(const QString &sidecarPath, const QString &cubeProgrammerBinDir)
{
    m_client->setPaths(sidecarPath, cubeProgrammerBinDir);
}

void MemReadWorker::setStlinkSerial(const QString &serial)
{
    m_client->setStlinkSerial(serial);
}

void MemReadWorker::openLink()
{
    if (m_open)
        return;

    // Started before connecting, so the origin covers the whole session the
    // same way the RSP clock does - see the note on opened().
    m_sessionClock.start();

    QString error;
    if (!m_client->start(&error) || !m_client->connectTarget(&error)) {
        m_client->stop();
        emit failed(error);
        return;
    }

    // An initial DHCSR read doubles as proof that reads work before anything
    // downstream starts trusting this link.
    quint32 dhcsr = 0;
    if (!readDhcsr(dhcsr)) {
        m_client->stop();
        emit failed(QStringLiteral("DHCSR okunamadi - hedef erisilebilir degil."));
        return;
    }

    m_open = true;
    evaluateDhcsr(dhcsr);
    emit opened(kMaxReadBytes, dhcsr, double(m_sessionClock.nsecsElapsed()) / 1.0e9);
}

void MemReadWorker::closeLink()
{
    stopSampling();
    const bool wasOpen = m_open;
    m_open = false;
    if (m_client)
        m_client->stop();
    if (wasOpen)
        emit closed();
}

bool MemReadWorker::readDhcsr(quint32 &value)
{
    QVector<MemoryRequest> request{ MemoryRequest{0, kDhcsrAddr, kDhcsrLen} };
    QVector<MemoryReply> replies;
    if (!m_client->readRanges(request, replies, nullptr) || replies.isEmpty() || !replies.first().ok)
        return false;
    value = bytesToU32Le(replies.first().data);
    return true;
}

void MemReadWorker::evaluateDhcsr(quint32 value)
{
    // Bit 24 S_RETIRE_ST is the liveness proof; bit 17 S_HALT means stopped;
    // bit 25 S_RESET_ST means the target reset under us. All three are sticky
    // and clear on read.
    const bool halted  = (value & (1u << 17)) != 0;
    const bool resetSt = (value & (1u << 25)) != 0;
    if (halted)
        emit coreHalted();
    if (resetSt)
        emit coreReset();
}

void MemReadWorker::readRanges(quint32 batchId, const QVector<MemoryRequest> &requests)
{
    if (!m_open) {
        emit rangesRead(batchId, {});
        return;
    }

    QVector<MemoryReply> replies;
    QString error;
    if (!m_client->readRanges(requests, replies, &error)) {
        handleTransportLoss(error);
        emit rangesRead(batchId, {});
        return;
    }
    emit rangesRead(batchId, replies);
}

void MemReadWorker::startSampling(const QVector<MemoryRequest> &plan, int targetRateHz)
{
    if (!m_open)
        return;

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
        connect(m_sampleTimer, &QTimer::timeout, this, &MemReadWorker::onSampleTimerTick);
    }
    if (!m_healthTimer) {
        m_healthTimer = new QTimer(this);
        m_healthTimer->setInterval(kHealthIntervalMs);
        connect(m_healthTimer, &QTimer::timeout, this, &MemReadWorker::onHealthTimerTick);
    }

    if (targetRateHz > 0) {
        // Round to the nearest millisecond rather than truncating: truncation
        // turns 600 Hz into 1000/1 = 1000 Hz, further from the request than
        // 1000/2 = 500 Hz, and collapses everything above 500 Hz onto 1000.
        m_sampleTimer->start(qMax(1, qRound(1000.0 / double(targetRateHz))));
    } else {
        m_sampleTimer->stop();
        QTimer::singleShot(0, this, &MemReadWorker::onSampleTimerTick);
    }
    m_healthTimer->start();
}

void MemReadWorker::stopSampling()
{
    m_sampling = false;
    if (m_sampleTimer) m_sampleTimer->stop();
    if (m_healthTimer) m_healthTimer->stop();
}

void MemReadWorker::onSampleTimerTick()
{
    if (!m_sampling || m_samplePlan.isEmpty() || !m_open)
        return;
    if (m_inSample) {
        // The previous sample has not returned, so this deadline is missed.
        ++m_droppedSinceEmit;
        return;
    }

    m_inSample = true;

    // Timestamp contract (DebugLinkTypes.h): t is taken immediately BEFORE the
    // first read of this sample, and skewUs spans until its last reply is in
    // hand - "these values were read within [t, t + skewUs/1e6]".
    const qint64 startNs = m_sessionClock.nsecsElapsed();

    QVector<MemoryReply> replies;
    QString error;
    const bool ok = m_client->readRanges(m_samplePlan, replies, &error);

    const qint64 endNs = m_sessionClock.nsecsElapsed();
    m_inSample = false;

    if (!ok) {
        handleTransportLoss(error);
        return;
    }

    const double t = double(startNs) / 1.0e9;
    const double skewUs = double(endNs - startNs) / 1.0e3;
    emit rawSamplesReady(replies, t, skewUs);

    m_rttSumMs += double(endNs - startNs) / 1.0e6;
    ++m_rttCount;
    ++m_samplesInWindow;
    reportStatsIfDue();

    if (m_sampling && m_targetRateHz == 0) {
        // Back-to-back mode re-arms through the event loop rather than
        // recursing, so stopSampling() and closeLink() still get through.
        QTimer::singleShot(0, this, &MemReadWorker::onSampleTimerTick);
    }
}

void MemReadWorker::reportStatsIfDue()
{
    if (m_statsWindow.isValid() && m_statsWindow.elapsed() < kStatsWindowMs)
        return;

    const qint64 elapsedMs = m_statsWindow.isValid() ? qMax<qint64>(1, m_statsWindow.elapsed()) : 1;
    const double actualHz = m_samplesInWindow * 1000.0 / double(elapsedMs);
    const double rttAvg = m_rttCount > 0 ? m_rttSumMs / m_rttCount : 0.0;
    emit samplingStats(actualHz, rttAvg, m_droppedSinceEmit);

    m_droppedSinceEmit = 0;
    m_rttSumMs = 0.0;
    m_rttCount = 0;
    m_samplesInWindow = 0;
    m_statsWindow.restart();
}

void MemReadWorker::onHealthTimerTick()
{
    if (!m_sampling || !m_open || m_inSample)
        return;

    quint32 value = 0;
    if (!readDhcsr(value)) {
        handleTransportLoss(QStringLiteral("DHCSR okunamadi."));
        return;
    }
    evaluateDhcsr(value);
}

void MemReadWorker::handleTransportLoss(const QString &reason)
{
    // A failed read is not automatically a dead link - the target can refuse an
    // address. Only a sidecar that is gone ends the session; anything else is
    // reported and sampling carries on, so one bad item cannot silently stop
    // the whole screen.
    if (m_client->isRunning() && m_client->isConnected()) {
        emit logLine(QStringLiteral("[memread] okuma hatasi: %1").arg(reason));
        return;
    }

    emit logLine(QStringLiteral("[memread] baglanti kayboldu: %1").arg(reason));
    stopSampling();
    const bool wasOpen = m_open;
    m_open = false;
    m_client->stop();
    if (wasOpen) {
        emit failed(reason);
        emit closed();
    }
}
