#pragma once
#include <QObject>
#include <QString>
#include "modules/board/BoardPresets.h"

// ── AppState ──────────────────────────────────────────────────────────────
// Single source of truth for the application's runtime state.
// Lives in MainWindow (single instance). All tabs receive a pointer to it.
// Tabs READ state via getters and REACT via signals. They WRITE via slots.
class AppState : public QObject
{
    Q_OBJECT

public:
    explicit AppState(QObject *parent = nullptr);

    // ── Getters ──────────────────────────────────────────────────────────
    BoardInfo  activeBoard()       const { return m_activeBoard; }
    QString    activePort()        const { return m_activePort; }
    qint32     activeBaud()        const { return m_activeBaud; }
    bool       isConnected()       const { return m_connected; }
    QString    connectionInfo()    const { return m_connInfo; }
    QString    lastModelName()     const { return m_lastModelName; }
    double     lastInferenceMs()   const { return m_lastInferenceMs; }
    quint8     lastAccuracy()      const { return m_lastAccuracy; }

public slots:
    void setActiveBoard(const BoardInfo &board);
    void setActivePort(const QString &port);
    void setActiveBaud(qint32 baud);
    void setConnected(bool connected, const QString &info = QString());
    void setLastModel(const QString &name, double infMs, quint8 acc);

signals:
    void activeBoardChanged(const BoardInfo &board);
    void activePortChanged(const QString &port);
    void activeBaudChanged(qint32 baud);
    void connectionChanged(bool connected, const QString &info);
    void lastModelChanged(const QString &name, double infMs, quint8 acc);

private:
    BoardInfo  m_activeBoard      { BoardPresets::defaultBoard() };
    QString    m_activePort;
    qint32     m_activeBaud       = 115200;
    bool       m_connected        = false;
    QString    m_connInfo;
    QString    m_lastModelName;
    double     m_lastInferenceMs  = 0.0;
    quint8     m_lastAccuracy     = 0;
};
