#include "DebugLink.h"
#include "DebugLinkWorker.h"
#include "GdbServerProcess.h"
#include "MemReadWorker.h"

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

    buildGdbBackend();
}

void DebugLink::setBackend(Backend backend)
{
    if (m_backend == backend)
        return;
    // Switching transports while owners hold the link would strand them on a
    // worker that is about to disappear.
    Q_ASSERT(m_refCount == 0);
    if (m_refCount != 0)
        return;

    m_backend = backend;
    if (backend == Backend::MemRead)
        buildMemReadBackend();
    else
        buildGdbBackend();
}

void DebugLink::setMemReadPaths(const QString &sidecarPath, const QString &cubeProgrammerApiDir)
{
    m_sidecarPath = sidecarPath;
    m_apiDir = cubeProgrammerApiDir;
    if (m_memWorker)
        m_memWorker->setPaths(m_sidecarPath, m_apiDir);
}

void DebugLink::buildMemReadBackend()
{
    if (m_memWorker)
        return;
    if (m_thread) {
        m_thread->quit();
        m_thread->wait(3000);
        delete m_thread;
        m_thread = nullptr;
        m_worker = nullptr;      // deleted by the thread's finished() hookup
    }
    delete m_gdbProcess;
    m_gdbProcess = nullptr;

    m_thread = new QThread(this);
    m_memWorker = new MemReadWorker();
    m_memWorker->setPaths(m_sidecarPath, m_apiDir);
    m_memWorker->setStlinkSerial(m_stlinkSerial);
    m_memWorker->moveToThread(m_thread);

    // The two workers report the same events, so the existing handlers carry
    // over unchanged and DebugLink's public surface does not move.
    connect(m_memWorker, &MemReadWorker::opened, this, &DebugLink::onWorkerHandshakeSucceeded);
    connect(m_memWorker, &MemReadWorker::failed, this, &DebugLink::onWorkerHandshakeFailed);
    connect(m_memWorker, &MemReadWorker::closed, this, &DebugLink::onWorkerSocketClosed);
    connect(m_memWorker, &MemReadWorker::logLine, this, &DebugLink::logLine);
    connect(m_memWorker, &MemReadWorker::rangesRead, this, &DebugLink::rangesRead);
    connect(m_memWorker, &MemReadWorker::rawSamplesReady, this, &DebugLink::rawSamplesReady);
    connect(m_memWorker, &MemReadWorker::samplingStats, this, &DebugLink::samplingStats);
    connect(m_memWorker, &MemReadWorker::coreHalted, this, [this]() {
        m_coreRunning = false;
        emit coreHalted();
    });
    connect(m_memWorker, &MemReadWorker::coreReset, this, &DebugLink::coreReset);

    connect(this, &DebugLink::requestOpenMemLink,   m_memWorker, &MemReadWorker::openLink,       Qt::QueuedConnection);
    connect(this, &DebugLink::requestCloseMemLink,  m_memWorker, &MemReadWorker::closeLink,      Qt::QueuedConnection);
    connect(this, &DebugLink::requestReadRanges,    m_memWorker, &MemReadWorker::readRanges,     Qt::QueuedConnection);
    connect(this, &DebugLink::requestStartSampling, m_memWorker, &MemReadWorker::startSampling,  Qt::QueuedConnection);
    connect(this, &DebugLink::requestStopSampling,  m_memWorker, &MemReadWorker::stopSampling,   Qt::QueuedConnection);

    connect(m_thread, &QThread::finished, m_memWorker, &QObject::deleteLater);
    m_thread->start();
}

void DebugLink::buildGdbBackend()
{
    if (m_gdbProcess)
        return;

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
    // Kept whichever backend is active, so switching later does not lose them.
    m_cubeProgrammerBinDir = cubeProgrammerBinDir;
    if (m_gdbProcess) {
        m_gdbProcess->setServerPath(gdbServerPath);
        m_gdbProcess->setCubeProgrammerBinDir(cubeProgrammerBinDir);
    }
}

void DebugLink::setStlinkSerial(const QString &sn)
{
    m_stlinkSerial = sn;
    if (m_gdbProcess)
        m_gdbProcess->setStlinkSerial(sn);
    if (m_memWorker)
        m_memWorker->setStlinkSerial(sn);
}

// ── retain() / release() — see header for the binding contract ─────────────

void DebugLink::retain()
{
    ++m_refCount;

    if (m_refCount == 1) {
        m_lastError.clear();
        m_pendingClose = false;
        if (m_backend == Backend::MemRead) {
            // No server process and no port to wait for; the sidecar is
            // launched by the worker itself as part of opening.
            setState(DebugLinkState::Connecting);
            emit requestOpenMemLink();
        } else {
            setState(DebugLinkState::StartingServer);
            if (m_gdbProcess)
                m_gdbProcess->start();
        }
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
        if (m_state == DebugLinkState::Closed) {
            // Benign, not a bug: the link already tore itself down on its own
            // (onWorkerSocketClosed() force-resets the counter when the socket
            // drops unexpectedly — cable pull, gdbserver crash) and this
            // release() is an owner's normal teardown arriving after the fact.
            // Every real caller (Backend::closeWatchLink(), GdbServerReader)
            // calls release() unconditionally from its own cleanup with no way
            // to know in advance that the link already died — that is
            // literally what DebugLink::closed() is for. Asserting here used
            // to crash any Debug build the first time a session dropped mid-use.
            return;
        }
        // refCount already <=0 but the link still thinks it's open/opening —
        // THIS is the actual double-release-without-matching-retain() bug.
        qWarning() << "DebugLink::release() called with refCount <= 0 while state is not Closed — programming error, ignored";
        Q_ASSERT(false);
        return;
    }

    --m_refCount;
    if (m_refCount == 0)
        closeInternal();
}

void DebugLink::shutdownNow()
{
    if (m_thread && m_thread->isRunning()) {
        if (m_backend == Backend::MemRead && m_memWorker)
            QMetaObject::invokeMethod(m_memWorker, &MemReadWorker::closeLink, Qt::BlockingQueuedConnection);
        else if (m_worker)
            QMetaObject::invokeMethod(m_worker, &DebugLinkWorker::disconnectFromServer, Qt::BlockingQueuedConnection);
    }

    if (m_gdbProcess)
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
    if (m_backend == Backend::MemRead)
        emit requestCloseMemLink();   // worker stops the sidecar, emits closed()
    else
        emit requestDisconnect();     // worker sends 'D', closes the socket, emits socketClosed()
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

void DebugLink::onWorkerHandshakeSucceeded(quint32 maxReadBytes, quint32 dhcsrValue, double sessionElapsedS)
{
    m_maxReadBytes         = maxReadBytes;
    m_sessionElapsedAtOpen = sessionElapsedS;
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
    // Null under the MemRead backend, which has no server process to stop -
    // the sidecar shuts itself down when the worker closes its pipe.
    if (m_gdbProcess)
        m_gdbProcess->stop();
    failOpenAttempt(message);
}

void DebugLink::onWorkerSocketClosed()
{
    if (m_gdbProcess)
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
