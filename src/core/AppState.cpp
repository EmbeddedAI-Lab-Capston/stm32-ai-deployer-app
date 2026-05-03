#include "AppState.h"

AppState::AppState(QObject *parent)
    : QObject(parent)
{
    // Register BoardInfo as a Qt meta-type so it can travel through
    // QVariant and queued signal/slot connections.
    qRegisterMetaType<BoardInfo>("BoardInfo");
}

void AppState::setActiveBoard(const BoardInfo &board)
{
    if (m_activeBoard.name == board.name &&
        m_activeBoard.flashKb == board.flashKb)
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
