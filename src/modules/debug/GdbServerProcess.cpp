#include "GdbServerProcess.h"

#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

namespace {
constexpr int kReadinessPollMs   = 100;
constexpr int kReadinessTimeoutMs = 10000;
constexpr int kMaxPortRetries    = 3;

// Substrings ST-LINK_gdbserver.exe is known to print on failure. The CLI
// (and this server) can return exit code 0 even on connection failure, so
// exit code is never trusted — only these markers are (plan Bolum 4.4).
const char *const kErrorMarkers[] = {
    "ST-LINK error",
    "Error in initializing",
    "Cannot",
    "DEV_CONNECT_ERR",
    "already in use",
    "No ST-LINK detected",
};
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

    QStringList args{
        QStringLiteral("-g"),
        QStringLiteral("-e"),
        QStringLiteral("-p"), QString::number(port),
        QStringLiteral("-d"),
    };
    if (!m_stlinkSerial.isEmpty())
        args << QStringLiteral("-i") << m_stlinkSerial;
    args << QStringLiteral("-cp") << m_cubeProgrammerBinDir;

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

void GdbServerProcess::pollReadiness()
{
    if (m_readyEmitted || m_failedEmitted)
        return;

    if (m_startElapsed.elapsed() > kReadinessTimeoutMs) {
        m_readinessTimer->stop();
        failOnce(tr("gdbserver %1 saniye içinde hazır olmadı").arg(kReadinessTimeoutMs / 1000));
        return;
    }

    auto *probe = new QTcpSocket(this);
    connect(probe, &QTcpSocket::connected, this, [this, probe]() {
        probe->disconnectFromHost();
        probe->deleteLater();
        if (!m_readyEmitted && !m_failedEmitted) {
            m_readyEmitted = true;
            m_readinessTimer->stop();
            emit ready(m_port);
        }
    });
    connect(probe, &QTcpSocket::errorOccurred, this, [probe](QAbstractSocket::SocketError) {
        probe->deleteLater();
    });
    probe->connectToHost(QHostAddress::LocalHost, m_port);
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

            failOnce(line);
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
    }
}

void GdbServerProcess::onReadyReadStdErr()
{
    while (m_process && m_process->canReadLine()) {
        const QString line = QString::fromLocal8Bit(m_process->readLine()).trimmed();
        if (line.isEmpty()) continue;
        emit logLine(line);
        scanLineForErrorMarkers(line);
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
    m_process->deleteLater();
    m_process = nullptr;
    m_expectingExit = false;
}

void GdbServerProcess::stop()
{
    if (m_readinessTimer)
        m_readinessTimer->stop();

    if (!isRunning()) {
        if (m_process) { m_process->deleteLater(); m_process = nullptr; }
        return;
    }

    m_expectingExit = true;
    m_process->terminate();

    auto *killTimer = new QTimer(this);
    killTimer->setSingleShot(true);
    connect(killTimer, &QTimer::timeout, this, [this, killTimer]() {
        if (m_process && m_process->state() != QProcess::NotRunning)
            m_process->kill();
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
        if (m_process) { m_process->deleteLater(); m_process = nullptr; }
        return;
    }

    m_expectingExit = true;
    m_process->terminate();
    if (!m_process->waitForFinished(timeoutMs))
        m_process->kill();
    m_process->deleteLater();
    m_process = nullptr;
}
