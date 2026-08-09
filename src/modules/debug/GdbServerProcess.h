#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QProcess>
#include <QString>

class QTimer;

// ── GdbServerProcess ─────────────────────────────────────────────────────
// Owns the ST-LINK_gdbserver.exe child process: port selection, argument
// construction, readiness detection, and error-marker scanning. Lives on the
// main thread — QProcess is already async/signal based, so it does not need
// a worker thread (docs/variable_watcher_plan.md Bolum 2.2, 4.4).
//
// `-k` / `--halt` are never part of the argument list this class builds —
// there is no setter for them. That is the enforcement mechanism, not a
// runtime flag (see CLAUDE.md "Degisken Izleyici" decision record).
class GdbServerProcess : public QObject
{
    Q_OBJECT

public:
    explicit GdbServerProcess(QObject *parent = nullptr);
    ~GdbServerProcess() override;

    void setServerPath(const QString &path);
    void setCubeProgrammerBinDir(const QString &dir);
    void setStlinkSerial(const QString &sn);   // empty = let the server pick
    void setPreferredPort(quint16 port);       // 0 = auto (default)

    void start();     // emits ready(port) or failed(msg)
    void stop();       // graceful: terminate() -> 2s -> kill(), non-blocking
    void stopBlocking(int timeoutMs); // emergency path for qApp::aboutToQuit only
    bool isRunning() const;
    quint16 port() const { return m_port; }

signals:
    void ready(quint16 port);
    void failed(const QString &message);
    void logLine(const QString &line);   // surfaced in the UI log pane
    void crashed(const QString &message);

private slots:
    void onReadyReadStdOut();
    void onReadyReadStdErr();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onProcessError(QProcess::ProcessError error);
    void pollReadiness();

private:
    quint16 pickFreePort() const;
    void    spawn(quint16 port);
    void    scanLineForErrorMarkers(const QString &line);
    void    scanLineForReady(const QString &line);
    void    failOnce(const QString &message);
    void    teardownProcess();
    void    releaseProcess();

    QProcess      *m_process = nullptr;
    QString        m_serverPath;
    QString        m_cubeProgrammerBinDir;
    QString        m_stlinkSerial;
    quint16        m_preferredPort = 0;
    quint16        m_port          = 0;

    QTimer        *m_readinessTimer = nullptr;
    QElapsedTimer  m_startElapsed;
    bool           m_readyEmitted   = false;
    bool           m_failedEmitted  = false;
    bool           m_expectingExit  = false;   // true once stop()/stopBlocking() requested shutdown
    int            m_portRetries    = 0;
};
