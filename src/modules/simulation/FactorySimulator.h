#pragma once

#include <QObject>
#include <QTimer>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>
#include <QString>
#include <QDateTime>

// ── FactorySimulator ─────────────────────────────────────────────────────────
// Self-contained synthetic data engine for the "Fabrika Simülasyonu" mode.
// Models a large smart factory (5 zones, 20 STM32 nodes, ~68 sensors) and emits
// realistic live values, drift and occasional anomalies/alarms on a timer.
// Exposed to QML as the context property "factorySim". No real hardware is used.
class FactorySimulator : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool    running          READ running          NOTIFY runningChanged)
    Q_PROPERTY(int     intervalMs       READ intervalMs       WRITE setIntervalMs NOTIFY configChanged)
    Q_PROPERTY(qreal   speed            READ speed            WRITE setSpeed      NOTIFY configChanged)
    Q_PROPERTY(int     tickCount        READ tickCount        NOTIFY tick)

    // Aggregate KPIs (recomputed every tick).
    Q_PROPERTY(int     nodeCount        READ nodeCount        CONSTANT)
    Q_PROPERTY(int     sensorCount      READ sensorCount      CONSTANT)
    Q_PROPERTY(int     zoneCount        READ zoneCount        CONSTANT)
    Q_PROPERTY(int     okCount          READ okCount          NOTIFY kpisChanged)
    Q_PROPERTY(int     warningCount     READ warningCount     NOTIFY kpisChanged)
    Q_PROPERTY(int     criticalCount    READ criticalCount    NOTIFY kpisChanged)
    Q_PROPERTY(int     activeAlarmCount READ activeAlarmCount NOTIFY kpisChanged)
    Q_PROPERTY(int     avgInfUs         READ avgInfUs         NOTIFY kpisChanged)
    Q_PROPERTY(qreal   throughput       READ throughput       NOTIFY kpisChanged)

public:
    explicit FactorySimulator(QObject *parent = nullptr);

    bool  running() const     { return m_running; }
    int   intervalMs() const  { return m_intervalMs; }
    qreal speed() const       { return m_speed; }
    int   tickCount() const   { return m_tickCount; }

    int   nodeCount() const   { return m_nodes.size(); }
    int   sensorCount() const;
    int   zoneCount() const   { return m_zones.size(); }
    int   okCount() const     { return m_okCount; }
    int   warningCount() const{ return m_warningCount; }
    int   criticalCount() const { return m_criticalCount; }
    int   activeAlarmCount() const { return m_activeAlarms; }
    int   avgInfUs() const    { return m_avgInfUs; }
    qreal throughput() const  { return m_throughput; }

    void setIntervalMs(int ms);
    void setSpeed(qreal s);

public slots:
    void start();
    void stop();

    // ── QML data accessors ─────────────────────────────────────────────────
    QVariantList zones() const;                       // [{id,name,color,nodeCount,status,...}]
    QVariantList nodes() const;                        // all nodes (map view)
    QVariantList nodesInZone(int zoneId) const;
    QVariantMap  node(int nodeId) const;               // single node + sensors
    QVariantList sensorsOf(int nodeId) const;
    QVariantList alarms(int limit = 50) const;         // newest first
    QVariantMap  zoneSummary(int zoneId) const;

signals:
    void tick();
    void runningChanged();
    void configChanged();
    void kpisChanged();
    void alarmRaised(QVariantMap alarm);

private:
    // ── Internal model ──────────────────────────────────────────────────────
    struct Sensor {
        QString name;
        QString type;
        QString unit;
        qreal   baseline = 0;     // centre of the normal band
        qreal   amplitude = 0;    // slow sinusoidal swing
        qreal   noise = 0;        // random jitter magnitude
        qreal   normalMin = 0;
        qreal   normalMax = 0;
        qreal   phase = 0;        // sinusoid phase offset
        qreal   freq = 1.0;       // per-sensor swing speed (de-synchronises nodes)
        int     decimals = 1;
        qreal   value = 0;
        qreal   noiseState = 0;   // mean-reverting random-walk jitter
        qreal   faultLevel = 0;   // 0..1 — how far into the fault the value is pushed
        int     faultDir = 0;     // -1 below range, +1 above range, 0 idle
        QString status = QStringLiteral("ok");
    };

    struct Node {
        int     id = 0;
        int     zoneId = 0;
        QString name;
        QString board;            // STM32F4 / STM32H7 / STM32N6
        QString model;
        int     baseInfUs = 0;
        int     infUs = 0;
        int     freeRamB = 0;
        int     totalRamB = 0;
        int     uptimeS = 0;
        qreal   x = 0;            // relative position within zone [0..1]
        qreal   y = 0;
        QString status = QStringLiteral("ok");
        int     faultTicks = 0;   // >0 while a forced anomaly is active
        QVector<Sensor> sensors;
    };

    struct Zone {
        int     id = 0;
        QString name;
        QString accent;           // theme accent name (hex handled in QML)
        QString hint;             // short descriptive subtitle
    };

    struct Alarm {
        QDateTime time;
        int       nodeId = 0;
        QString   nodeName;
        QString   zoneName;
        QString   severity;       // info / warning / critical
        QString   message;
    };

    void buildModel();
    static Sensor mkSensor(const QString &name, const QString &type, const QString &unit,
                           qreal nmin, qreal nmax, qreal noise, int decimals = 1);
    void addNode(int zoneId, const QString &name, const QString &board,
                 const QString &model, qreal x, qreal y,
                 const QVector<Sensor> &sensors);
    void advance();
    void recomputeKpis();
    static QString worseStatus(const QString &a, const QString &b);
    QString zoneName(int zoneId) const;

    QVariantMap nodeToMap(const Node &n, bool withSensors) const;
    QVariantMap sensorToMap(const Sensor &s) const;

    QTimer  m_timer;
    bool    m_running = false;
    int     m_intervalMs = 1000;
    qreal   m_speed = 1.0;
    int     m_tickCount = 0;

    QVector<Zone> m_zones;
    QVector<Node> m_nodes;
    QList<Alarm>  m_alarms;       // newest appended at front

    int   m_okCount = 0;
    int   m_warningCount = 0;
    int   m_criticalCount = 0;
    int   m_activeAlarms = 0;
    int   m_avgInfUs = 0;
    qreal m_throughput = 0;
};
