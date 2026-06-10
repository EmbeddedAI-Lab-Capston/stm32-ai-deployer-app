#include "FactorySimulator.h"

#include <QRandomGenerator>
#include <QtMath>

namespace {

qreal randUniform(qreal lo, qreal hi)
{
    return lo + (hi - lo) * QRandomGenerator::global()->generateDouble();
}

// Approximate gaussian noise via the central-limit trick (sum of uniforms).
qreal randNoise(qreal magnitude)
{
    qreal s = 0;
    for (int i = 0; i < 3; ++i)
        s += QRandomGenerator::global()->generateDouble() - 0.5;
    return (s / 1.5) * magnitude; // ~[-magnitude, +magnitude]
}

QString fmt(qreal v, int decimals)
{
    return QString::number(v, 'f', decimals);
}

} // namespace

// ── Construction ─────────────────────────────────────────────────────────────
FactorySimulator::FactorySimulator(QObject *parent)
    : QObject(parent)
{
    buildModel();
    recomputeKpis();

    m_timer.setInterval(m_intervalMs);
    connect(&m_timer, &QTimer::timeout, this, &FactorySimulator::advance);
}

int FactorySimulator::sensorCount() const
{
    int n = 0;
    for (const Node &node : m_nodes)
        n += node.sensors.size();
    return n;
}

void FactorySimulator::setIntervalMs(int ms)
{
    ms = qBound(120, ms, 5000);
    if (ms == m_intervalMs)
        return;
    m_intervalMs = ms;
    m_timer.setInterval(qMax(60, int(m_intervalMs / m_speed)));
    emit configChanged();
}

void FactorySimulator::setSpeed(qreal s)
{
    s = qBound(0.25, s, 8.0);
    if (qFuzzyCompare(s, m_speed))
        return;
    m_speed = s;
    m_timer.setInterval(qMax(60, int(m_intervalMs / m_speed)));
    emit configChanged();
}

void FactorySimulator::start()
{
    if (m_running)
        return;
    m_running = true;
    m_timer.start();
    emit runningChanged();
}

void FactorySimulator::stop()
{
    if (!m_running)
        return;
    m_running = false;
    m_timer.stop();
    emit runningChanged();
}

// ── Model construction ───────────────────────────────────────────────────────
void FactorySimulator::addNode(int zoneId, const QString &name, const QString &board,
                               const QString &model, qreal x, qreal y,
                               const QVector<Sensor> &sensors)
{
    Node n;
    n.id = m_nodes.size() + 1;
    n.zoneId = zoneId;
    n.name = name;
    n.board = board;
    n.model = model;
    n.x = x;
    n.y = y;
    n.sensors = sensors;

    // Inference budget + RAM scale with the MCU class.
    if (board == QStringLiteral("STM32N6"))      { n.baseInfUs = 1400; n.totalRamB = 4096 * 1024; }
    else if (board == QStringLiteral("STM32H7")) { n.baseInfUs = 3200; n.totalRamB = 1024 * 1024; }
    else                                         { n.baseInfUs = 8200; n.totalRamB = 192  * 1024; }

    n.infUs = n.baseInfUs;
    n.freeRamB = int(n.totalRamB * 0.55);
    n.uptimeS = QRandomGenerator::global()->bounded(3600, 864000);

    // Seed each sensor at its baseline with a random phase and its own swing
    // speed, so neighbouring sensors don't all rise and fall in lock-step.
    for (Sensor &s : n.sensors) {
        s.phase = randUniform(0, 6.28);
        s.freq  = randUniform(0.6, 1.5);
        s.value = s.baseline + randNoise(s.amplitude * 0.5); // start mid-band, not all equal
    }
    m_nodes.push_back(n);
}

// Sensor factory shorthand.
FactorySimulator::Sensor FactorySimulator::mkSensor(const QString &name, const QString &type,
                                                    const QString &unit, qreal nmin, qreal nmax,
                                                    qreal noise, int decimals)
{
    Sensor s;
    s.name = name;
    s.type = type;
    s.unit = unit;
    s.normalMin = nmin;
    s.normalMax = nmax;
    s.baseline  = (nmin + nmax) * 0.5;
    s.amplitude = (nmax - nmin) * 0.30;
    s.noise     = noise;
    s.decimals  = decimals;
    return s;
}

void FactorySimulator::buildModel()
{
    using S = Sensor;
    m_zones = {
        { 1, QStringLiteral("Çevresel Güvenlik & Erken Uyarı"), QStringLiteral("cyan"),
          QStringLiteral("Gaz, duman, sıcaklık/nem izleme — LoRa") },
        { 2, QStringLiteral("Sıvı/Gaz Akış & Basınç Kontrol"),  QStringLiteral("primary"),
          QStringLiteral("Tank seviyesi, debi ve basınç otomasyonu") },
        { 3, QStringLiteral("Kestirimci Bakım & Ağır Makine"),   QStringLiteral("warning"),
          QStringLiteral("Titreşim, akım ve sıcaklık ile arıza tahmini") },
        { 4, QStringLiteral("Uç Yapay Zeka & Kalite Kontrol"),   QStringLiteral("purple"),
          QStringLiteral("Kamera/TOF üzerinde TinyML nesne tespiti") },
        { 5, QStringLiteral("Merkezi Ağ Geçitleri (Gateway)"),   QStringLiteral("success"),
          QStringLiteral("Veri toplama ve buluta aktarım") },
    };

    // ── Zone 1 — Çevresel Güvenlik (5× F4, 4 sensör/node = 20) ──────────────
    auto envSensors = []() -> QVector<S> {
        return {
            mkSensor(QStringLiteral("Gaz (MQ-135)"),   QStringLiteral("gas"),  QStringLiteral("ppm"), 60, 420, 18, 0),
            mkSensor(QStringLiteral("Duman (MQ-2)"),   QStringLiteral("gas"),  QStringLiteral("ppm"), 20, 300, 15, 0),
            mkSensor(QStringLiteral("Sıcaklık"),       QStringLiteral("temp"), QStringLiteral("°C"),  18, 30,  0.6),
            mkSensor(QStringLiteral("Nem"),            QStringLiteral("hum"),  QStringLiteral("%"),   30, 65,  1.5),
        };
    };
    const char *envNames[] = { "Kuzey Depo", "Boya Hattı", "Kimyasal Oda", "Yükleme Rampası", "Jeneratör" };
    for (int i = 0; i < 5; ++i)
        addNode(1, QStringLiteral("ENV-%1 · %2").arg(i + 1).arg(envNames[i]),
                QStringLiteral("STM32F4"), QStringLiteral("MLP_INT8"),
                0.10 + 0.20 * i, 0.30 + (i % 2 ? 0.34 : 0.0), envSensors());

    // ── Zone 2 — Akış & Basınç (5× F4/H7, 3 sensör/node = 15) ───────────────
    auto flowSensors = []() -> QVector<S> {
        return {
            mkSensor(QStringLiteral("Basınç"),  QStringLiteral("pressure"), QStringLiteral("bar"),   2.0, 8.0, 0.25, 2),
            mkSensor(QStringLiteral("Debi"),    QStringLiteral("flow"),     QStringLiteral("L/dk"),  10,  90,  3.0,  0),
            mkSensor(QStringLiteral("Seviye"),  QStringLiteral("level"),    QStringLiteral("%"),     20,  95,  2.0,  0),
        };
    };
    const char *flowNames[] = { "Ana Tank", "Pompa-1", "Pompa-2", "Boru Hattı A", "Boru Hattı B" };
    for (int i = 0; i < 5; ++i)
        addNode(2, QStringLiteral("FLW-%1 · %2").arg(i + 1).arg(flowNames[i]),
                (i % 2 ? QStringLiteral("STM32H7") : QStringLiteral("STM32F4")),
                QStringLiteral("CNN1D_INT8"),
                0.12 + 0.19 * i, 0.55 + (i % 2 ? -0.18 : 0.10), flowSensors());

    // ── Zone 3 — Kestirimci Bakım (4× H7, 4 sensör/node = 16) ───────────────
    auto vibSensors = []() -> QVector<S> {
        return {
            mkSensor(QStringLiteral("Titreşim (RMS)"), QStringLiteral("vib"),  QStringLiteral("mm/s"), 0.5, 4.5, 0.35, 2),
            mkSensor(QStringLiteral("Sıcaklık"),       QStringLiteral("temp"), QStringLiteral("°C"),   35,  75,  1.2),
            mkSensor(QStringLiteral("Akım (ACS712)"),  QStringLiteral("amp"),  QStringLiteral("A"),    3,   18,  0.8),
            mkSensor(QStringLiteral("Devir"),          QStringLiteral("rpm"),  QStringLiteral("RPM"),  1420, 1490, 8, 0),
        };
    };
    const char *vibNames[] = { "Motor-1", "Konveyör", "Kompresör", "Fan Ünitesi" };
    for (int i = 0; i < 4; ++i)
        addNode(3, QStringLiteral("MNT-%1 · %2").arg(i + 1).arg(vibNames[i]),
                QStringLiteral("STM32H7"), QStringLiteral("CNN1D_INT8"),
                0.16 + 0.22 * i, 0.40 + (i % 2 ? 0.28 : 0.0), vibSensors());

    // ── Zone 4 — Uç Yapay Zeka (4× H7/N6, 3 sensör/node = 12) ───────────────
    auto aiSensors = []() -> QVector<S> {
        return {
            mkSensor(QStringLiteral("Güven"),       QStringLiteral("conf"), QStringLiteral("%"),  82, 99, 2.0, 0),
            mkSensor(QStringLiteral("Tespit/dk"),   QStringLiteral("rate"), QStringLiteral("adet"),12, 60, 4.0, 0),
            mkSensor(QStringLiteral("Mesafe (TOF)"),QStringLiteral("tof"),  QStringLiteral("mm"), 120,1800,60,0),
        };
    };
    const char *aiNames[] = { "Bant Kamera-1", "Bant Kamera-2", "Ambalaj QC", "Etiket QC" };
    for (int i = 0; i < 4; ++i)
        addNode(4, QStringLiteral("AI-%1 · %2").arg(i + 1).arg(aiNames[i]),
                (i < 2 ? QStringLiteral("STM32N6") : QStringLiteral("STM32H7")),
                QStringLiteral("YOLO-Nano INT8"),
                0.18 + 0.22 * i, 0.32 + (i % 2 ? 0.30 : 0.0), aiSensors());

    // ── Zone 5 — Gateway (2× H7, ~3 sensör/node = 5..6) ─────────────────────
    addNode(5, QStringLiteral("GW-1 · Saha Toplayıcı"), QStringLiteral("STM32H7"),
            QStringLiteral("AggregatorFW"), 0.30, 0.45, {
        mkSensor(QStringLiteral("Ağ Yükü"),  QStringLiteral("net"),  QStringLiteral("%"),   20, 80, 4.0, 0),
        mkSensor(QStringLiteral("Kuyruk"),   QStringLiteral("queue"),QStringLiteral("msg"),  0, 400, 30, 0),
        mkSensor(QStringLiteral("SD Kullanım"),QStringLiteral("sd"), QStringLiteral("%"),   10, 88, 1.5, 0),
    });
    addNode(5, QStringLiteral("GW-2 · Bulut Köprüsü"), QStringLiteral("STM32H7"),
            QStringLiteral("AggregatorFW"), 0.66, 0.55, {
        mkSensor(QStringLiteral("Ağ Yükü"),  QStringLiteral("net"),  QStringLiteral("%"),   25, 85, 4.0, 0),
        mkSensor(QStringLiteral("Gecikme"),  QStringLiteral("lat"),  QStringLiteral("ms"),  12, 90, 6.0, 0),
    });
}

// ── Per-tick simulation ──────────────────────────────────────────────────────
QString FactorySimulator::worseStatus(const QString &a, const QString &b)
{
    auto rank = [](const QString &s) {
        if (s == QStringLiteral("error") || s == QStringLiteral("critical")) return 2;
        if (s == QStringLiteral("warning")) return 1;
        return 0;
    };
    return rank(a) >= rank(b) ? a : b;
}

void FactorySimulator::advance()
{
    ++m_tickCount;
    // Moderate time base — fast enough that each sensor visibly rises AND falls
    // over a short window, slow enough that it doesn't look twitchy.
    const qreal t = m_tickCount * 0.12;

    for (Node &n : m_nodes) {
        n.uptimeS += qMax(1, int(m_intervalMs / 1000));

        // Decay an active fault, or roll a small chance to start one.
        // Faults last longer so the anomaly is sustained, not a single spike.
        if (n.faultTicks > 0) {
            --n.faultTicks;
        } else if (QRandomGenerator::global()->bounded(1000) < 4) { // ~0.4%/tick
            n.faultTicks = QRandomGenerator::global()->bounded(8, 22);
        }
        const bool faulting = n.faultTicks > 0;

        QString nodeStatus = QStringLiteral("ok");
        for (Sensor &s : n.sensors) {
            // 1) Where the value "wants" to be: baseline + a slow primary swing
            //    plus a smaller, faster harmonic so the curve isn't a clean sine.
            const qreal swing = s.amplitude * (0.78 * qSin(t * s.freq + s.phase)
                                             + 0.22 * qSin(t * s.freq * 2.3 + s.phase * 1.7));
            qreal target = s.baseline + swing;

            // 2) Fault severity ramps up while active and decays slowly afterwards,
            //    so out-of-range values persist and then recover gradually instead
            //    of snapping back to normal on the very next tick.
            const qreal desired = faulting ? 1.0 : 0.0;
            s.faultLevel += (desired - s.faultLevel) * (faulting ? 0.22 : 0.06);
            if (s.faultLevel > 0.01) {
                if (s.faultDir == 0)
                    s.faultDir = QRandomGenerator::global()->bounded(2) ? 1 : -1;
                const qreal over = (s.normalMax - s.normalMin) * 0.55;
                const qreal extreme = (s.faultDir > 0) ? s.normalMax + over
                                                       : s.normalMin - over * 0.4;
                target += (extreme - target) * s.faultLevel;
            } else {
                s.faultDir = 0;
            }

            // 3) Mean-reverting random walk — each step nudges off the previous one,
            //    so the jitter looks organic instead of independent per-tick spikes.
            s.noiseState = s.noiseState * 0.82 + randNoise(s.noise * 0.6);
            target += s.noiseState;

            // 4) Low-pass filter the displayed value toward the target so it moves
            //    smoothly rather than jumping.
            s.value += (target - s.value) * 0.18;
            s.value = qMax<qreal>(0, s.value);
            const qreal v = s.value;

            const QString prev = s.status;
            const qreal margin = (s.normalMax - s.normalMin) * 0.12;
            if (v > s.normalMax + margin || v < s.normalMin - margin)
                s.status = QStringLiteral("error");
            else if (v > s.normalMax || v < s.normalMin)
                s.status = QStringLiteral("warning");
            else
                s.status = QStringLiteral("ok");

            nodeStatus = worseStatus(nodeStatus, s.status);

            // Raise an alarm on a transition into an out-of-range state.
            if (s.status != prev && s.status != QStringLiteral("ok")) {
                Alarm a;
                a.time = QDateTime::currentDateTime();
                a.nodeId = n.id;
                a.nodeName = n.name;
                a.zoneName = zoneName(n.zoneId);
                a.severity = (s.status == QStringLiteral("error")) ? QStringLiteral("critical")
                                                                   : QStringLiteral("warning");
                a.message = QStringLiteral("%1: %2 %3 (eşik %4–%5)")
                                .arg(s.name, fmt(v, s.decimals), s.unit,
                                     fmt(s.normalMin, s.decimals), fmt(s.normalMax, s.decimals));
                m_alarms.prepend(a);
                if (m_alarms.size() > 200)
                    m_alarms.removeLast();
                emit alarmRaised(sensorToMap(s)); // payload kept lightweight
            }
        }

        // Inference time and free RAM also drift smoothly toward a target rather
        // than jumping every tick; both degrade a little while faulting.
        const qreal jitter = randUniform(0.97, 1.05) * (faulting ? 1.15 : 1.0);
        const int targetInf = int(n.baseInfUs * jitter);
        n.infUs += int((targetInf - n.infUs) * 0.22);

        const qreal ramTarget = faulting ? randUniform(0.40, 0.50) : randUniform(0.50, 0.60);
        const int targetRam = int(n.totalRamB * ramTarget);
        n.freeRamB += int((targetRam - n.freeRamB) * 0.18);
        n.status = nodeStatus;
    }

    recomputeKpis();
    emit tick();
}

void FactorySimulator::recomputeKpis()
{
    m_okCount = m_warningCount = m_criticalCount = 0;
    qint64 infSum = 0;
    for (const Node &n : m_nodes) {
        if (n.status == QStringLiteral("error") || n.status == QStringLiteral("critical"))
            ++m_criticalCount;
        else if (n.status == QStringLiteral("warning"))
            ++m_warningCount;
        else
            ++m_okCount;
        infSum += n.infUs;
    }
    m_avgInfUs = m_nodes.isEmpty() ? 0 : int(infSum / m_nodes.size());
    m_activeAlarms = m_warningCount + m_criticalCount;

    // Rough factory throughput proxy (inferences/sec across all nodes).
    qreal tp = 0;
    for (const Node &n : m_nodes)
        tp += n.infUs > 0 ? (1e6 / n.infUs) : 0;
    m_throughput = tp;

    emit kpisChanged();
}

QString FactorySimulator::zoneName(int zoneId) const
{
    for (const Zone &z : m_zones)
        if (z.id == zoneId)
            return z.name;
    return QString();
}

// ── QML conversion ───────────────────────────────────────────────────────────
QVariantMap FactorySimulator::sensorToMap(const Sensor &s) const
{
    QVariantMap m;
    m[QStringLiteral("name")]      = s.name;
    m[QStringLiteral("type")]      = s.type;
    m[QStringLiteral("unit")]      = s.unit;
    m[QStringLiteral("value")]     = s.value;
    m[QStringLiteral("valueText")] = fmt(s.value, s.decimals);
    m[QStringLiteral("normalMin")] = s.normalMin;
    m[QStringLiteral("normalMax")] = s.normalMax;
    m[QStringLiteral("status")]    = s.status;
    // Fraction of the normal band the value currently sits at (clamped 0..1.3).
    const qreal span = qMax<qreal>(0.0001, s.normalMax - s.normalMin);
    m[QStringLiteral("fraction")]  = qBound<qreal>(0, (s.value - s.normalMin) / span, 1.3);
    return m;
}

QVariantMap FactorySimulator::nodeToMap(const Node &n, bool withSensors) const
{
    QVariantMap m;
    m[QStringLiteral("id")]        = n.id;
    m[QStringLiteral("zoneId")]    = n.zoneId;
    m[QStringLiteral("zoneName")]  = zoneName(n.zoneId);
    m[QStringLiteral("name")]      = n.name;
    m[QStringLiteral("board")]     = n.board;
    m[QStringLiteral("model")]     = n.model;
    m[QStringLiteral("infUs")]     = n.infUs;
    m[QStringLiteral("infMs")]     = n.infUs / 1000.0;
    m[QStringLiteral("freeRamKb")] = n.freeRamB / 1024.0;
    m[QStringLiteral("totalRamKb")]= n.totalRamB / 1024.0;
    m[QStringLiteral("uptimeS")]   = n.uptimeS;
    m[QStringLiteral("status")]    = n.status;
    m[QStringLiteral("x")]         = n.x;
    m[QStringLiteral("y")]         = n.y;
    m[QStringLiteral("sensorCount")] = n.sensors.size();

    int warn = 0;
    for (const Sensor &s : n.sensors)
        if (s.status != QStringLiteral("ok"))
            ++warn;
    m[QStringLiteral("alarmCount")] = warn;

    if (withSensors) {
        QVariantList sl;
        for (const Sensor &s : n.sensors)
            sl.push_back(sensorToMap(s));
        m[QStringLiteral("sensors")] = sl;
    }
    return m;
}

QVariantList FactorySimulator::zones() const
{
    QVariantList out;
    for (const Zone &z : m_zones) {
        int nodes = 0, sensors = 0, warn = 0, crit = 0;
        for (const Node &n : m_nodes) {
            if (n.zoneId != z.id)
                continue;
            ++nodes;
            sensors += n.sensors.size();
            if (n.status == QStringLiteral("error") || n.status == QStringLiteral("critical")) ++crit;
            else if (n.status == QStringLiteral("warning")) ++warn;
        }
        QString status = crit > 0 ? QStringLiteral("error")
                                  : (warn > 0 ? QStringLiteral("warning") : QStringLiteral("ok"));
        QVariantMap m;
        m[QStringLiteral("id")]          = z.id;
        m[QStringLiteral("name")]        = z.name;
        m[QStringLiteral("accent")]      = z.accent;
        m[QStringLiteral("hint")]        = z.hint;
        m[QStringLiteral("nodeCount")]   = nodes;
        m[QStringLiteral("sensorCount")] = sensors;
        m[QStringLiteral("warningCount")]= warn;
        m[QStringLiteral("criticalCount")]= crit;
        m[QStringLiteral("status")]      = status;
        out.push_back(m);
    }
    return out;
}

QVariantList FactorySimulator::nodes() const
{
    QVariantList out;
    for (const Node &n : m_nodes)
        out.push_back(nodeToMap(n, false));
    return out;
}

QVariantList FactorySimulator::nodesInZone(int zoneId) const
{
    QVariantList out;
    for (const Node &n : m_nodes)
        if (n.zoneId == zoneId)
            out.push_back(nodeToMap(n, true));
    return out;
}

QVariantMap FactorySimulator::node(int nodeId) const
{
    for (const Node &n : m_nodes)
        if (n.id == nodeId)
            return nodeToMap(n, true);
    return QVariantMap();
}

QVariantList FactorySimulator::sensorsOf(int nodeId) const
{
    for (const Node &n : m_nodes)
        if (n.id == nodeId)
            return nodeToMap(n, true).value(QStringLiteral("sensors")).toList();
    return QVariantList();
}

QVariantList FactorySimulator::alarms(int limit) const
{
    QVariantList out;
    const int count = qMin(limit, m_alarms.size());
    for (int i = 0; i < count; ++i) {
        const Alarm &a = m_alarms.at(i);
        QVariantMap m;
        m[QStringLiteral("time")]     = a.time.toString(QStringLiteral("HH:mm:ss"));
        m[QStringLiteral("nodeId")]   = a.nodeId;
        m[QStringLiteral("nodeName")] = a.nodeName;
        m[QStringLiteral("zoneName")] = a.zoneName;
        m[QStringLiteral("severity")] = a.severity;
        m[QStringLiteral("message")]  = a.message;
        out.push_back(m);
    }
    return out;
}

QVariantMap FactorySimulator::zoneSummary(int zoneId) const
{
    for (const QVariant &v : zones()) {
        const QVariantMap m = v.toMap();
        if (m.value(QStringLiteral("id")).toInt() == zoneId)
            return m;
    }
    return QVariantMap();
}
