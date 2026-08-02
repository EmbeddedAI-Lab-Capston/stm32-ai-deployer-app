#include "DebugLink.h"
#include "DebugLinkWorker.h"
#include "GdbServerProcess.h"

#include <QDebug>
#include <QMetaObject>
#include <QThread>
#include <QTimer>

DebugLink::DebugLink(QObject *parent) : QObject(parent)
{
    qRegisterMetaType<MemoryRequest>();
    qRegisterMetaType<MemoryReply>();
    qRegisterMetaType<QVector<MemoryRequest>>();
    qRegisterMetaType<QVector<MemoryReply>>();

    m_gdbProcess = new GdbServerProcess(this);
    connect(m_gdbProcess, &GdbServerProcess::ready,   this, &DebugLink::onServerReady);
    connect(m_gdbProcess, &GdbServerProcess::failed,  this, &DebugLink::onServerFailed);
    connect(m_gdbProcess, &GdbServerProcess::logLine, this, &DebugLink::logLine);
    connect(m_gdbProcess, &GdbServerProcess::crashed, this, &DebugLink::onServerCrashed);

    m_thread = new QThread(this);
    m_worker = new DebugLinkWorker();
    m_worker->moveToThread(m_thread);

    connect(m_worker, &DebugLinkWorker::socketConnected,     this, &DebugLink::onWorkerSocketConnected);
    connect(m_worker, &DebugLinkWorker::handshakeSucceeded,  this, &DebugLink::onWorkerHandshakeSucceeded);
    connect(m_worker, &DebugLinkWorker::handshakeFailed,     this, &DebugLink::onWorkerHandshakeFailed);
    connect(m_worker, &DebugLinkWorker::socketClosed,        this, &DebugLink::onWorkerSocketClosed);
    connect(m_worker, &DebugLinkWorker::logLine,             this, &DebugLink::logLine);
    connect(m_worker, &DebugLinkWorker::rangesRead,          this, &DebugLink::rangesRead);
    connect(m_worker, &DebugLinkWorker::rawSamplesReady,     this, &DebugLink::rawSamplesReady);
    connect(m_worker, &DebugLinkWorker::samplingStats,       this, &DebugLink::samplingStats);
    connect(m_worker, &DebugLinkWorker::coreHalted, this, [this]() {
        m_coreRunning = false;
        emit coreHalted();
    });
    connect(m_worker, &DebugLinkWorker::coreReset, this, &DebugLink::coreReset);

    connect(this, &DebugLink::requestConnect,       m_worker, &DebugLinkWorker::connectToServer,      Qt::QueuedConnection);
    connect(this, &DebugLink::requestDisconnect,    m_worker, &DebugLinkWorker::disconnectFromServer,  Qt::QueuedConnection);
    connect(this, &DebugLink::requestReadRanges,    m_worker, &DebugLinkWorker::readRanges,            Qt::QueuedConnection);
    connect(this, &DebugLink::requestStartSampling, m_worker, &DebugLinkWorker::startSampling,         Qt::QueuedConnection);
    connect(this, &DebugLink::requestStopSampling,  m_worker, &DebugLinkWorker::stopSampling,          Qt::QueuedConnection);

    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);

    m_thread->start();
}

DebugLink::~DebugLink()
{
    m_thread->quit();
    m_thread->wait(3000);
}

void DebugLink::setPaths(const QString &gdbServerPath, const QString &cubeProgrammerBinDir)
{
    m_gdbProcess->setServerPath(gdbServerPath);
    m_gdbProcess->setCubeProgrammerBinDir(cubeProgrammerBinDir);
}

void DebugLink::setStlinkSerial(const QString &sn)
{
    m_gdbProcess->setStlinkSerial(sn);
}

// ── retain() / release() — see header for the binding contract ─────────────

void DebugLink::retain()
{
    ++m_refCount;

    if (m_refCount == 1) {
        m_lastError.clear();
        m_pendingClose = false;
        setState(DebugLinkState::StartingServer);
        m_gdbProcess->start();
        return;
    }

    if (m_state == DebugLinkState::Open) {
        // Already open — tell this caller so it doesn't have to special-case
        // "was it already open" vs "just opened".
        QTimer::singleShot(0, this, [this]() { emit opened(); });
    }
    // Otherwise an open attempt is already in flight (StartingServer /
    // Connecting / Handshaking); this caller's opened()/failed() arrives when
    // that attempt resolves.
}

void DebugLink::release()
{
    if (m_refCount <= 0) {
        qWarning() << "DebugLink::release() called with refCount <= 0 — programming error, ignored";
        Q_ASSERT(false);
        return;
    }

    --m_refCount;
    if (m_refCount == 0)
        closeInternal();
}

void DebugLink::shutdownNow()
{
    if (m_thread && m_thread->isRunning())
        QMetaObject::invokeMethod(m_worker, &DebugLinkWorker::disconnectFromServer, Qt::BlockingQueuedConnection);

    m_gdbProcess->stopBlocking(2000);

    m_refCount     = 0;
    m_pendingClose = false;
    m_sampling     = false;
    setState(DebugLinkState::Closed);
}

void DebugLink::closeInternal()
{
    if (m_state == DebugLinkState::Closed)
        return;

    if (m_state == DebugLinkState::StartingServer ||
        m_state == DebugLinkState::Connecting ||
        m_state == DebugLinkState::Handshaking) {
        // Still opening — the in-flight attempt tears itself down as soon as
        // it resolves (see onWorkerHandshakeSucceeded / failOpenAttempt).
        m_pendingClose = true;
        return;
    }

    if (m_sampling) {
        m_sampling = false;
        emit requestStopSampling();
    }
    emit requestDisconnect();   // worker sends 'D', closes the socket, emits socketClosed()
}

// ── readRanges / sampling passthrough ───────────────────────────────────

void DebugLink::readRanges(quint32 batchId, const QVector<MemoryRequest> &requests)
{
    emit requestReadRanges(batchId, requests);
}

void DebugLink::startSampling(const QVector<MemoryRequest> &plan, int targetRateHz)
{
    m_sampling = true;
    emit requestStartSampling(plan, targetRateHz);
}

void DebugLink::stopSampling()
{
    m_sampling = false;
    emit requestStopSampling();
}

// ── GdbServerProcess events ─────────────────────────────────────────────

void DebugLink::onServerReady(quint16 port)
{
    setState(DebugLinkState::Connecting);
    emit requestConnect(port);
}

void DebugLink::onServerFailed(const QString &message)
{
    failOpenAttempt(message);
}

void DebugLink::onServerCrashed(const QString &message)
{
    m_lastError = message;
    emit logLine(tr("gdbserver beklenmedik sekilde kapandi: %1").arg(message));
    if (m_state != DebugLinkState::Closed)
        emit requestDisconnect();   // let the worker clean up; onWorkerSocketClosed() finishes teardown
}

// ── DebugLinkWorker events ──────────────────────────────────────────────

void DebugLink::onWorkerSocketConnected()
{
    setState(DebugLinkState::Handshaking);
}

void DebugLink::onWorkerHandshakeSucceeded(quint32 maxReadBytes, quint32 dhcsrValue)
{
    m_maxReadBytes = maxReadBytes;
    m_coreRunning  = true;

    emit logLine(tr("Handshake tamamlandi. DHCSR=0x%1 maxReadBytes=%2")
                     .arg(dhcsrValue, 8, 16, QLatin1Char('0'))
                     .arg(maxReadBytes));

    if (m_pendingClose) {
        // release() arrived while we were still opening — tear down now
        // instead of surfacing an opened() no one asked for anymore.
        m_pendingClose = false;
        emit requestDisconnect();
        return;
    }

    setState(DebugLinkState::Open);
    emit opened();
}

void DebugLink::onWorkerHandshakeFailed(const QString &message)
{
    m_gdbProcess->stop();
    failOpenAttempt(message);
}

void DebugLink::onWorkerSocketClosed()
{
    m_gdbProcess->stop();

    const bool wasPendingClose = m_pendingClose;
    m_pendingClose = false;
    m_sampling     = false;

    if (!wasPendingClose && m_refCount > 0) {
        // Unexpected: the socket dropped while still retained (e.g. gdbserver
        // crashed). The link is gone regardless of how many owners it had.
        m_refCount  = 0;
        m_lastError = tr("Baglanti beklenmedik sekilde kapandi");
    }

    setState(DebugLinkState::Closed);
    emit closed();
}

// ── helpers ──────────────────────────────────────────────────────────────

void DebugLink::failOpenAttempt(const QString &message)
{
    m_lastError    = message;
    m_refCount     = 0;   // a failed retain() never consumes the counter (plan Bolum 4.6 point 4)
    m_pendingClose = false;
    m_sampling     = false;
    setState(DebugLinkState::Failed);
    emit failed(message);
}

void DebugLink::setState(DebugLinkState s)
{
    if (m_state == s) return;
    m_state = s;
    emit stateChanged();
}
