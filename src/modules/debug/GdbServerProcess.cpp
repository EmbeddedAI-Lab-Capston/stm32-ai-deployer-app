#include "GdbServerProcess.h"

#include <QHostAddress>
#include <QTcpServer>
#include <QTimer>

namespace {
constexpr int kReadinessPollMs   = 100;
constexpr int kReadinessTimeoutMs = 10000;
constexpr int kMaxPortRetries    = 3;
// How long to let the server shut itself down after we detach, before
// resorting to signals. Observed clean exit is well under 1 s.
constexpr int kGracefulExitMs    = 3000;

// Substrings ST-LINK_gdbserver.exe is known to print on failure. The CLI
// (and this server) can return exit code 0 even on connection failure, so
// exit code is never trusted — only these markers are (plan Bolum 4.4).
const char *const kErrorMarkers[] = {
    "ST-LINK error",
    "Error in initializing",
    "Cannot connect",
    "Cannot open",
    "DEV_CONNECT_ERR",
    "DEV_USB_COMM_ERR",
    "Target USB comms error",
    "Couldn't locate STM32CubeProgrammer",
    "already in use",
    "No ST-LINK detected",
};

// The probe's USB endpoint is wedged: no retry or restart fixes this, only a
// physical unplug/replug. Detected separately so the user gets an actionable
// message instead of a raw server string.
bool isUsbWedgedMarker(const QString &line)
{
    return line.contains(QStringLiteral("DEV_USB_COMM_ERR"), Qt::CaseInsensitive)
        || line.contains(QStringLiteral("Target USB comms error"), Qt::CaseInsensitive);
}

// Printed once the server is actually accepting GDB connections. Used instead
// of a throwaway TCP connect: without -e (persistent) the server exits as soon
// as a client disconnects, so probing the port would shut it down before the
// real client ever arrives.
const char *const kReadyMarker = "Waiting for debugger connection";
}

GdbServerProcess::GdbServerProcess(QObject *parent) : QObject(parent) {}

GdbServerProcess::~GdbServerProcess()
{
    if (m_process && m_process->state() != QProcess::NotRunning)
        m_process->kill();
}

void GdbServerProcess::setServerPath(const QString &path) { m_serverPath = path; }
void GdbServerProcess::setCubeProgrammerBinDir(const QString &dir) { m_cubeProgrammerBinDir = dir; }
void GdbServerProcess::setStlinkSerial(const QString &sn) { m_stlinkSerial = sn; }
void GdbServerProcess::setPreferredPort(quint16 port) { m_preferredPort = port; }

bool GdbServerProcess::isRunning() const
{
    return m_process && m_process->state() != QProcess::NotRunning;
}

quint16 GdbServerProcess::pickFreePort() const
{
    QTcpServer probe;
    if (!probe.listen(QHostAddress::LocalHost, 0))
        return 0;
    const quint16 p = probe.serverPort();
    probe.close();
    return p;
}

void GdbServerProcess::start()
{
    if (isRunning())
        return;

    m_readyEmitted  = false;
    m_failedEmitted = false;
    m_expectingExit = false;
    m_portRetries   = 0;

    if (m_serverPath.isEmpty()) {
        failOnce(tr("ST-LINK_gdbserver.exe yolu ayarlanmamış"));
        return;
    }

    const quint16 port = m_preferredPort != 0 ? m_preferredPort : pickFreePort();
    if (port == 0) {
        failOnce(tr("Boş port bulunamadı"));
        return;
    }
    spawn(port);
}

void GdbServerProcess::spawn(quint16 port)
{
    m_port = port;

    // -g  = --attach       : attach to the running target, never reset it.
    // -d  = --swd           : SWD wire protocol.
    // NO -e (--persistent)  : persistent mode keeps the server listening after
    //   the client detaches, which means WE have to kill it — and on Windows
    //   QProcess::terminate() cannot reach a console process, so the fallback
    //   was TerminateProcess(). Hard-killing the server leaves the ST-Link's
    //   USB endpoint wedged (DEV_USB_COMM_ERR) for EVERY tool, recoverable
    //   only by physically replugging the board. Without -e the server exits
    //   by itself when we detach, closing the USB handle properly.
    QStringList args{
        QStringLiteral("-g"),
        QStringLiteral("-p"), QString::number(port),
        QStringLiteral("-d"),
    };
    if (!m_stlinkSerial.isEmpty())
        args << QStringLiteral("-i") << m_stlinkSerial;
    if (!m_cubeProgrammerBinDir.isEmpty())
        args << QStringLiteral("-cp") << m_cubeProgrammerBinDir;   // empty value makes the server abort

    // Enforcement, not convention: this argument list is built entirely by
    // this function and never touched externally, so these can never appear —
    // asserted here so a future edit that adds them fails loudly in debug
    // builds (see CLAUDE.md "Degisken Izleyici" decision record).
    Q_ASSERT(!args.contains(QStringLiteral("-k")));
    Q_ASSERT(!args.contains(QStringLiteral("--halt")));

    m_process = new QProcess(this);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &GdbServerProcess::onReadyReadStdOut);
    connect(m_process, &QProcess::readyReadStandardError,  this, &GdbServerProcess::onReadyReadStdErr);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &GdbServerProcess::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred, this, &GdbServerProcess::onProcessError);

    m_startElapsed.start();
    m_process->start(m_serverPath, args);

    if (!m_readinessTimer) {
        m_readinessTimer = new QTimer(this);
        m_readinessTimer->setInterval(kReadinessPollMs);
        connect(m_readinessTimer, &QTimer::timeout, this, &GdbServerProcess::pollReadiness);
    }
    m_readinessTimer->start();
}

// Only enforces the overall timeout now — readiness itself comes from the
// server's own "waiting for connection" banner (see scanLineForReady).
void GdbServerProcess::pollReadiness()
{
    if (m_readyEmitted || m_failedEmitted)
        return;

    if (m_startElapsed.elapsed() > kReadinessTimeoutMs) {
        m_readinessTimer->stop();
        failOnce(tr("gdbserver %1 saniye içinde hazır olmadı").arg(kReadinessTimeoutMs / 1000));
    }
}

void GdbServerProcess::scanLineForReady(const QString &line)
{
    if (m_readyEmitted || m_failedEmitted)
        return;
    if (!line.contains(QString::fromLatin1(kReadyMarker), Qt::CaseInsensitive))
        return;

    m_readyEmitted = true;
    if (m_readinessTimer)
        m_readinessTimer->stop();
    emit ready(m_port);
}

void GdbServerProcess::scanLineForErrorMarkers(const QString &line)
{
    if (m_readyEmitted || m_failedEmitted)
        return;

    for (const char *marker : kErrorMarkers) {
        if (line.contains(QString::fromLatin1(marker), Qt::CaseInsensitive)) {
            if (m_readinessTimer)
                m_readinessTimer->stop();

            // "already in use" during port bind means the picked port raced
            // with something else — retry on a fresh port a few times before
            // giving up (plan Bolum 4.4 point 4).
            if (line.contains(QStringLiteral("already in use"), Qt::CaseInsensitive)
                && m_portRetries < kMaxPortRetries) {
                ++m_portRetries;
                teardownProcess();
                const quint16 newPort = pickFreePort();
                if (newPort != 0) {
                    spawn(newPort);
                    return;
                }
            }

            failOnce(isUsbWedgedMarker(line)
                          ? tr("ST-Link USB baglantisi kilitlendi (%1). "
                               "Kartin USB kablosunu cikarip tekrar takin - "
                               "bu durumdan yazilimla cikilamaz.").arg(line.trimmed())
                          : line);
            return;
        }
    }
}

void GdbServerProcess::onReadyReadStdOut()
{
    while (m_process && m_process->canReadLine()) {
        const QString line = QString::fromLocal8Bit(m_process->readLine()).trimmed();
        if (line.isEmpty()) continue;
        emit logLine(line);
        scanLineForErrorMarkers(line);
        scanLineForReady(line);
    }
}

void GdbServerProcess::onReadyReadStdErr()
{
    while (m_process && m_process->canReadLine()) {
        const QString line = QString::fromLocal8Bit(m_process->readLine()).trimmed();
        if (line.isEmpty()) continue;
        emit logLine(line);
        scanLineForErrorMarkers(line);
        scanLineForReady(line);
    }
}

void GdbServerProcess::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    Q_UNUSED(exitCode);
    Q_UNUSED(status);

    if (m_readinessTimer)
        m_readinessTimer->stop();

    if (!m_expectingExit && !m_failedEmitted) {
        // The process died without us asking it to — either a residual
        // "already in use" retry already handled it above, or a genuine crash.
        emit crashed(tr("gdbserver beklenmedik şekilde kapandı (exit %1)").arg(exitCode));
    }

    if (m_process) {
        m_process->deleteLater();
        m_process = nullptr;
    }
}

void GdbServerProcess::onProcessError(QProcess::ProcessError error)
{
    if (error == QProcess::FailedToStart)
        failOnce(tr("gdbserver başlatılamadı — yol doğru mu? (%1)").arg(m_serverPath));
    // Other QProcess::ProcessError values are followed by ::finished(), which
    // onProcessFinished() already handles.
}

void GdbServerProcess::failOnce(const QString &message)
{
    if (m_failedEmitted) return;
    m_failedEmitted = true;
    emit failed(message);
}

void GdbServerProcess::teardownProcess()
{
    if (!m_process) return;
    m_expectingExit = true;
    disconnect(m_process, nullptr, this, nullptr);
    if (m_process->state() != QProcess::NotRunning)
        m_process->kill();
    releaseProcess();
    m_expectingExit = false;
}

// Drops our reference to the child process if onProcessFinished() has not
// already done it. Safe to call when m_process is already null.
void GdbServerProcess::releaseProcess()
{
    if (!m_process)
        return;
    m_process->deleteLater();
    m_process = nullptr;
}

void GdbServerProcess::stop()
{
    if (m_readinessTimer)
        m_readinessTimer->stop();

    if (!isRunning()) {
        releaseProcess();
        return;
    }

    m_expectingExit = true;

    // Ordering matters for the ST-Link's health. We are called after the RSP
    // 'D' (detach) has gone out and the socket has closed, so a NON-persistent
    // server is already on its way out and will close its USB handle cleanly.
    // Give it that chance first. terminate() cannot reach a Windows console
    // process and kill() is TerminateProcess(), which strands the USB endpoint
    // (DEV_USB_COMM_ERR until the board is physically replugged) — so the hard
    // kill is a last resort, not the first move.
    //
    // NOTE: waitForFinished() dispatches QProcess::finished SYNCHRONOUSLY, so
    // onProcessFinished() can null m_process out from under us before the call
    // returns. Always re-check the member afterwards; dereferencing it blindly
    // is a segfault on the normal shutdown path.
    if (m_process->waitForFinished(kGracefulExitMs)) {
        releaseProcess();
        m_expectingExit = false;
        return;
    }

    m_process->terminate();

    auto *killTimer = new QTimer(this);
    killTimer->setSingleShot(true);
    connect(killTimer, &QTimer::timeout, this, [this, killTimer]() {
        if (m_process && m_process->state() != QProcess::NotRunning) {
            emit logLine(tr("gdbserver kendiliginden kapanmadi, zorla sonlandiriliyor - "
                            "ST-Link'in yeniden takilmasi gerekebilir"));
            m_process->kill();
        }
        killTimer->deleteLater();
    });
    killTimer->start(2000);
}

void GdbServerProcess::stopBlocking(int timeoutMs)
{
    // Emergency, synchronous shutdown for qApp::aboutToQuit only — the app is
    // exiting regardless, and process cleanup at exit must be unconditional
    // so a leaked ST-Link handle doesn't survive the app (plan Bolum 4.6).
    if (m_readinessTimer)
        m_readinessTimer->stop();

    if (!isRunning()) {
        releaseProcess();
        return;
    }

    m_expectingExit = true;
    // Same ordering rationale as stop(): natural exit first, hard kill last.
    // Every waitForFinished() below can dispatch finished() synchronously and
    // clear m_process, so each step re-checks it (see the note in stop()).
    if (m_process && !m_process->waitForFinished(qMin(timeoutMs, kGracefulExitMs))) {
        if (m_process) m_process->terminate();
        if (m_process && !m_process->waitForFinished(timeoutMs))
            m_process->kill();
    }
    releaseProcess();
}
