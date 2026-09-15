#pragma once

#include <QObject>
#include <QString>
#include <QVector>

#include "DebugLinkTypes.h"

class QProcess;

// Drives the stm32aid-memread sidecar: starts it, speaks MemReadProtocol over
// its stdin/stdout, and hands back the same MemoryRequest/MemoryReply types the
// gdbserver path uses, so a sampler can talk to either without caring which.
//
// Calls here BLOCK until the sidecar answers, which is only safe because this
// object is meant to live on a worker thread - never on the UI thread. A
// request/response exchange costs about 2 ms, and blocking keeps the sampling
// loop far simpler than the socket state machine the RSP path needs.
//
// The sidecar exits on its own when stdin closes, so stop() needs no signal, no
// port and no cleanup that could be skipped on a crash path.
class MemReadClient : public QObject
{
    Q_OBJECT

public:
    explicit MemReadClient(QObject *parent = nullptr);
    ~MemReadClient() override;

    // `sidecarPath` is stm32aid-memread.exe, normally beside the application.
    // `cubeProgrammerBinDir` is the folder holding STM32_Programmer_CLI.exe.
    void setPaths(const QString &sidecarPath, const QString &cubeProgrammerBinDir);
    void setStlinkSerial(const QString &serial);
    QString stlinkSerial() const { return m_serial; }

    bool    isRunning() const;
    bool    isConnected() const { return m_connected; }
    QString connectedBoard() const { return m_board; }
    QString lastError() const { return m_lastError; }

    // Largest single read handed to the sidecar. The API has no packet limit of
    // its own; this keeps one bad plan from asking for a huge allocation.
    static constexpr quint32 kMaxReadBytes = 65536;

    bool start(QString *error = nullptr);

    // Opens the ST-Link in hot-plug mode - a running target keeps running.
    bool connectTarget(QString *error = nullptr);

    // Executes every request and fills one reply per request, in order. Returns
    // false only when the exchange itself failed; a target-side read failure
    // surfaces as reply.ok == false so a partly good sample is still usable.
    bool readRanges(const QVector<MemoryRequest> &requests,
                    QVector<MemoryReply> &replies,
                    QString *error = nullptr);

    void stop();

signals:
    void logLine(const QString &line);

private:
    bool exchange(const QByteArray &payload, QByteArray &response, int timeoutMs, QString *error);
    void setError(const QString &message, QString *error);

    QProcess  *m_process = nullptr;
    QByteArray m_buffer;
    QString    m_sidecarPath;
    QString    m_binDir;
    QString    m_serial;
    QString    m_board;
    QString    m_lastError;
    bool       m_connected = false;
};
