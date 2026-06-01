#include "AppState.h"

AppState::AppState(QObject *parent)
    : QObject(parent)
{
    // Register BoardInfo as a Qt meta-type so it can travel through
    // QVariant and queued signal/slot connections.
    qRegisterMetaType<BoardInfo>("BoardInfo");
}

// ── QML helper getters ─────────────────────────────────────────────────────
QVariantList AppState::boardInfoRows() const
{
    const BoardInfo &b = m_activeBoard;
    auto row = [](const QString &k, const QString &v) {
        return QVariant(QVariantList{ k, v.isEmpty() ? QStringLiteral("—") : v });
    };
    QVariantList rows;
    rows << row("Model", b.name);
    rows << row("Flash", b.flashKb > 0 ? QString("%1 KB").arg(b.flashKb) : QString());
    rows << row("RAM",   b.ramKb   > 0 ? QString("%1 KB").arg(b.ramKb)   : QString());
    rows << row("Hız",   b.clockMhz > 0 ? QString("%1 MHz").arg(b.clockMhz) : QString());
    rows << row("COM",        b.portName);
    rows << row("Board",      b.probeBoardName);
    rows << row("Device ID",  b.deviceId);
    rows << row("Revision",   b.revisionId);
    rows << row("Device",     b.deviceName);
    rows << row("NVM",        b.nvmSize);
    rows << row("CPU",        b.deviceCpu);
    rows << row("ST-LINK",    b.stlinkSn.isEmpty() ? b.stlinkFw
                                                   : (b.stlinkSn + "  ·  " + b.stlinkFw));
    rows << row("Voltage",    b.voltage);
    return rows;
}

void AppState::setActiveBoard(const BoardInfo &board)
{
    if (m_activeBoard.name == board.name &&
        m_activeBoard.flashKb == board.flashKb &&
        m_activeBoard.ramKb == board.ramKb &&
        m_activeBoard.clockMhz == board.clockMhz &&
        m_activeBoard.portName == board.portName &&
        m_activeBoard.probeBoardName == board.probeBoardName &&
        m_activeBoard.deviceId == board.deviceId &&
        m_activeBoard.revisionId == board.revisionId &&
        m_activeBoard.deviceName == board.deviceName &&
        m_activeBoard.nvmSize == board.nvmSize &&
        m_activeBoard.deviceCpu == board.deviceCpu &&
        m_activeBoard.stlinkSn == board.stlinkSn &&
        m_activeBoard.stlinkFw == board.stlinkFw &&
        m_activeBoard.voltage == board.voltage)
        return;
    m_activeBoard = board;
    emit activeBoardChanged(m_activeBoard);
}

void AppState::setActivePort(const QString &port)
{
    if (m_activePort == port) return;
    m_activePort = port;
    emit activePortChanged(m_activePort);
}

void AppState::setActiveBaud(qint32 baud)
{
    if (m_activeBaud == baud) return;
    m_activeBaud = baud;
    emit activeBaudChanged(m_activeBaud);
}

void AppState::setConnected(bool connected, const QString &info)
{
    m_connected = connected;
    m_connInfo  = info;
    emit connectionChanged(connected, info);
}

void AppState::setLastModel(const QString &name, double infMs, quint8 acc)
{
    m_lastModelName    = name;
    m_lastInferenceMs  = infMs;
    m_lastAccuracy     = acc;
    emit lastModelChanged(name, infMs, acc);
}

void AppState::setLiveMetrics(const QString &model, double infMs, quint8 acc,
                              double ramKb, const QString &label)
{
    m_lastModelName   = model;
    m_lastInferenceMs = infMs;
    m_lastAccuracy    = acc;
    m_lastRamKb       = ramKb;
    m_lastLabel       = label;
    emit lastModelChanged(model, infMs, acc);
    emit liveMetricsChanged();
}

void AppState::setSystemMetrics(int uptime, double tempC, double freeRamKb)
{
    m_lastUptime    = uptime;
    m_lastTempC     = tempC;
    m_lastFreeRamKb = freeRamKb;
    emit systemMetricsChanged();
}
