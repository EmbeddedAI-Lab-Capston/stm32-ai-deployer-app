#include "TestTraceRecorderPlayer.h"
#include "modules/watcher/TraceRecorder.h"
#include "modules/watcher/TracePlayer.h"

#include <QTemporaryDir>
#include <QTest>

namespace {
QList<WatchItem> makeItems()
{
    WatchItem a;
    a.label = "g_ai_infer_count"; a.address = 0x24000123; a.type = WatchValueType::U32;
    a.format = DisplayFormat::Dec; a.scale = 1.0; a.offset = 0.0; a.unit = ""; a.role = "";

    WatchItem b;
    b.label = "g_ai_last_inference_us"; b.address = 0x24000127; b.type = WatchValueType::U32;
    b.format = DisplayFormat::Dec; b.scale = 0.001; b.offset = 0.0; b.unit = "ms"; b.role = "";

    WatchItem c;
    c.label = "heapEnd"; c.address = 0x2400a000; c.type = WatchValueType::U32;
    c.format = DisplayFormat::Hex; c.scale = 1.0; c.offset = 0.0; c.unit = ""; c.role = "heapEnd";

    return {a, b, c};
}
}

void TestTraceRecorderPlayer::roundTripPreservesTimesAndValues()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("roundtrip.csv");

    const QList<WatchItem> items = makeItems();
    TraceRecorder rec;
    QVERIFY(rec.start(path, "STM32H7", "D:/x/app.elf", "anomaly_cnn_int8", 1000, items));

    const int n = 10000;
    WatchSampleBatch batch;
    batch.series.resize(items.size());
    for (int i = 0; i < n; ++i) {
        batch.times.append(i * 0.001);
        batch.series[0].append(double(i % 1000));
        batch.series[1].append(double((i * 7) % 5000));
        batch.series[2].append(double(603979776 + i));
    }
    rec.appendBatch(batch);
    rec.stop();
    QCOMPARE(rec.sampleCount(), quint64(n));

    TracePlayer player;
    QVERIFY2(player.load(path), qPrintable(player.lastError()));

    QCOMPARE(player.board(), QStringLiteral("STM32H7"));
    QCOMPARE(player.model(), QStringLiteral("anomaly_cnn_int8"));
    QCOMPARE(player.targetRateHz(), 1000);
    QCOMPARE(player.items().size(), items.size());
    QCOMPARE(player.times().size(), n);
    QCOMPARE(player.series().size(), items.size());

    for (int i = 0; i < n; ++i) {
        QVERIFY(qAbs(player.times().at(i) - i * 0.001) < 1e-6);
        QCOMPARE(player.series().at(0).at(i), double(i % 1000));
        QCOMPARE(player.series().at(1).at(i), double((i * 7) % 5000));
        QCOMPARE(player.series().at(2).at(i), double(603979776 + i));
    }

    // actualHz/duration come from the trailing summary line, written by stop().
    QVERIFY(player.actualHz() > 0.0);
    QVERIFY(player.durationS() > 0.0);
}

void TestTraceRecorderPlayer::headerRestoresItemMetadata()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("header.csv");

    const QList<WatchItem> items = makeItems();
    TraceRecorder rec;
    QVERIFY(rec.start(path, "STM32H7", "D:/x/app.elf", "anomaly_cnn_int8", 500, items));
    WatchSampleBatch batch;
    batch.times << 0.0;
    batch.series = { {1.0}, {2.0}, {3.0} };
    rec.appendBatch(batch);
    rec.stop();

    TracePlayer player;
    QVERIFY(player.load(path));
    QCOMPARE(player.items().size(), 3);

    const LoadedTraceItem &b = player.items().at(1);
    QCOMPARE(b.label, QStringLiteral("g_ai_last_inference_us"));
    QCOMPARE(b.address, quint64(0x24000127));
    QCOMPARE(b.type, WatchValueType::U32);
    QCOMPARE(b.format, DisplayFormat::Dec);
    QCOMPARE(b.scale, 0.001);
    QCOMPARE(b.unit, QStringLiteral("ms"));

    const LoadedTraceItem &c = player.items().at(2);
    QCOMPARE(c.label, QStringLiteral("heapEnd"));
    QCOMPARE(c.format, DisplayFormat::Hex);
    QCOMPARE(c.role, QStringLiteral("heapEnd"));
}

void TestTraceRecorderPlayer::eventsRoundTrip()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("events.csv");

    const QList<WatchItem> items = makeItems();
    TraceRecorder rec;
    QVERIFY(rec.start(path, "STM32H7", "D:/x/app.elf", "m", 200, items));
    WatchSampleBatch batch;
    batch.times << 0.0 << 1.0;
    batch.series = { {1.0, 2.0}, {1.0, 2.0}, {1.0, 2.0} };
    rec.appendBatch(batch);
    rec.addEvent(0.418, QStringLiteral("inference"), QStringLiteral("MLP_INT8 8200us, \"walking\""), QStringLiteral("info"));
    rec.addEvent(0.9, QStringLiteral("targetReset"), QStringLiteral("reset"), QStringLiteral("warning"));
    rec.stop();

    TracePlayer player;
    QVERIFY(player.load(path));
    QCOMPARE(player.events().size(), 2);
    QCOMPARE(player.events().at(0).kind, QStringLiteral("inference"));
    QVERIFY(player.events().at(0).text.contains(QStringLiteral("walking")));
    QCOMPARE(player.events().at(1).kind, QStringLiteral("targetReset"));
    QCOMPARE(player.events().at(1).severity, QStringLiteral("warning"));
}

void TestTraceRecorderPlayer::corruptFileReturnsErrorNoCrash()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("corrupt.csv");

    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream out(&f);
    out << "this is not a valid trace file\n";
    out << "garbage,,,\n";
    f.close();

    TracePlayer player;
    const bool ok = player.load(path);
    QVERIFY(!ok);
    QVERIFY(!player.lastError().isEmpty());
}

void TestTraceRecorderPlayer::missingFileReturnsErrorNoCrash()
{
    TracePlayer player;
    QVERIFY(!player.load(QStringLiteral("D:/this/path/does/not/exist_at_all.csv")));
    QVERIFY(!player.lastError().isEmpty());
}
