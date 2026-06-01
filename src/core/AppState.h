#pragma once
#include <QObject>
#include <QString>
#include <QVariant>
#include <QVariantList>
#include "modules/board/BoardPresets.h"

// ── AppState ──────────────────────────────────────────────────────────────
// Single source of truth for the application's runtime state.
// Lives in MainWindow (single instance). All tabs receive a pointer to it.
// Tabs READ state via getters and REACT via signals. They WRITE via slots.
class AppState : public QObject
{
    Q_OBJECT

    // QML-accessible properties
    Q_PROPERTY(bool connected       READ isConnected    NOTIFY connectionChanged)
    Q_PROPERTY(QString connInfo     READ connectionInfo NOTIFY connectionChanged)
    Q_PROPERTY(QString boardName    READ boardName      NOTIFY activeBoardChanged)
    Q_PROPERTY(QString boardSpec    READ boardSpec      NOTIFY activeBoardChanged)
    Q_PROPERTY(int boardFlashKb     READ boardFlashKb   NOTIFY activeBoardChanged)
    Q_PROPERTY(int boardRamKb       READ boardRamKb     NOTIFY activeBoardChanged)
    Q_PROPERTY(int boardClockMhz    READ boardClockMhz  NOTIFY activeBoardChanged)
    Q_PROPERTY(QString activePort   READ activePort     NOTIFY activePortChanged)
    Q_PROPERTY(qint32 activeBaud    READ activeBaud     NOTIFY activeBaudChanged)
    Q_PROPERTY(QString lastModel    READ lastModelName  NOTIFY lastModelChanged)
    Q_PROPERTY(double lastInfMs     READ lastInferenceMs NOTIFY lastModelChanged)
    Q_PROPERTY(int lastAcc          READ lastAccuracyPct NOTIFY lastModelChanged)
    Q_PROPERTY(double lastRamKb     READ lastRamKb      NOTIFY liveMetricsChanged)
    Q_PROPERTY(QString lastLabel    READ lastLabel      NOTIFY liveMetricsChanged)
    Q_PROPERTY(double lastTempC     READ lastTempC      NOTIFY systemMetricsChanged)
    Q_PROPERTY(int lastUptime       READ lastUptime     NOTIFY systemMetricsChanged)
    Q_PROPERTY(double lastFreeRamKb READ lastFreeRamKb  NOTIFY systemMetricsChanged)
    Q_PROPERTY(QVariantList boardInfoRows READ boardInfoRows NOTIFY activeBoardChanged)

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
    double     lastRamKb()         const { return m_lastRamKb; }
    QString    lastLabel()         const { return m_lastLabel; }
    double     lastTempC()         const { return m_lastTempC; }
    int        lastUptime()        const { return m_lastUptime; }
    double     lastFreeRamKb()     const { return m_lastFreeRamKb; }

    // QML helper getters (flat types)
    QString boardName()    const { return m_activeBoard.name; }
    int     boardFlashKb() const { return m_activeBoard.flashKb; }
    int     boardRamKb()   const { return m_activeBoard.ramKb; }
    int     boardClockMhz()const { return m_activeBoard.clockMhz; }
    int     lastAccuracyPct() const { return static_cast<int>(m_lastAccuracy); }
    QString boardSpec() const {
        if (m_activeBoard.isNull()) return QString();
        return QString("%1 KB Flash  |  %2 KB RAM  |  %3 MHz")
               .arg(m_activeBoard.flashKb)
               .arg(m_activeBoard.ramKb)
               .arg(m_activeBoard.clockMhz);
    }
    QVariantList boardInfoRows() const;   // [[label,value], ...] for Board screen

public slots:
    void setActiveBoard(const BoardInfo &board);
    void setActivePort(const QString &port);
    void setActiveBaud(qint32 baud);
    void setConnected(bool connected, const QString &info = QString());
    void setLastModel(const QString &name, double infMs, quint8 acc);
    void setLiveMetrics(const QString &model, double infMs, quint8 acc,
                        double ramKb, const QString &label);
    void setSystemMetrics(int uptime, double tempC, double freeRamKb);

signals:
    void activeBoardChanged(const BoardInfo &board);
    void activePortChanged(const QString &port);
    void activeBaudChanged(qint32 baud);
    void connectionChanged(bool connected, const QString &info);
    void lastModelChanged(const QString &name, double infMs, quint8 acc);
    void liveMetricsChanged();
    void systemMetricsChanged();

private:
    BoardInfo  m_activeBoard      { BoardPresets::defaultBoard() };
    QString    m_activePort;
    qint32     m_activeBaud       = 115200;
    bool       m_connected        = false;
    QString    m_connInfo;
    QString    m_lastModelName;
    double     m_lastInferenceMs  = 0.0;
    quint8     m_lastAccuracy     = 0;
    double     m_lastRamKb        = 0.0;
    QString    m_lastLabel;
    double     m_lastTempC        = 0.0;
    int        m_lastUptime       = 0;
    double     m_lastFreeRamKb    = 0.0;
};
