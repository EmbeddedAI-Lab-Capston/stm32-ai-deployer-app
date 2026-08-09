#include "TestTraceBuffer.h"
#include "modules/watcher/TraceBuffer.h"

#include <QElapsedTimer>
#include <QTest>

#include <cmath>

namespace {
WatchSampleBatch makeBatch(int count, double tStart, double dt, double valueStart = 0.0)
{
    WatchSampleBatch b;
    b.times.reserve(count);
    QVector<double> series;
    series.reserve(count);
    for (int i = 0; i < count; ++i) {
        b.times.append(tStart + dt * i);
        series.append(valueStart + double(i));
    }
    b.series.append(series);
    return b;
}
}

void TestTraceBuffer::appendAndDecimateProduceRequestedColumnCount()
{
    TraceBuffer buf;
    buf.configure(1, 20000);
    buf.append(makeBatch(10000, 0.0, 0.001));

    const QVector<PlotColumn> cols = buf.decimate(0, buf.firstTime(), buf.lastTime(), 100);
    QCOMPARE(cols.size(), 100);
    for (const PlotColumn &c : cols) {
        if (c.hasData)
            QVERIFY(c.vmin <= c.vmax);
    }
}

void TestTraceBuffer::overflowCapsRingButStatsKeepGrowing()
{
    TraceBuffer buf;
    const int capacity = 1000;
    buf.configure(1, capacity);
    buf.append(makeBatch(capacity * 2, 0.0, 0.001));

    QCOMPARE(buf.sampleCount(), quint64(capacity));
    QCOMPARE(buf.stats(0).count, quint64(capacity * 2));
}

void TestTraceBuffer::decimateOutsideDataRangeHasNoDataNoCrash()
{
    TraceBuffer buf;
    buf.configure(1, 1000);
    buf.append(makeBatch(500, 0.0, 0.001));   // t in [0, 0.499]

    const QVector<PlotColumn> cols = buf.decimate(0, 1000.0, 1001.0, 50);   // way outside
    QCOMPARE(cols.size(), 50);
    for (const PlotColumn &c : cols)
        QVERIFY(!c.hasData);
}

void TestTraceBuffer::decimateOnEmptyBufferDoesNotCrash()
{
    TraceBuffer buf;
    buf.configure(2, 1000);
    const QVector<PlotColumn> cols = buf.decimate(0, 0.0, 1.0, 20);
    QCOMPARE(cols.size(), 20);
    for (const PlotColumn &c : cols)
        QVERIFY(!c.hasData);
}

void TestTraceBuffer::valueAtFindsClosestSample()
{
    TraceBuffer buf;
    buf.configure(1, 1000);
    buf.append(makeBatch(1000, 0.0, 0.01));   // t = 0, 0.01, 0.02, ... value = t/0.01

    // Exact hit
    QCOMPARE(buf.valueAt(0, 5.0 * 0.01), 5.0);
    // Closer to sample 5 (t=0.05) than sample 6 (t=0.06)
    QCOMPARE(buf.valueAt(0, 0.054), 5.0);
    // Closer to sample 6
    QCOMPARE(buf.valueAt(0, 0.056), 6.0);
    // Clamped to the nearest edge sample when querying outside the range
    QCOMPARE(buf.valueAt(0, -10.0), 0.0);
    QCOMPARE(buf.valueAt(0, 1000.0), 999.0);
}

void TestTraceBuffer::valueAtOnEmptyBufferReturnsNan()
{
    TraceBuffer buf;
    buf.configure(1, 1000);
    QVERIFY(std::isnan(buf.valueAt(0, 0.0)));

    TraceBuffer unconfigured;
    QVERIFY(std::isnan(unconfigured.valueAt(0, 0.0)));
}

void TestTraceBuffer::decimatePerfUnder20MsFor1eSamples800Columns()
{
    TraceBuffer buf;
    const int n = 1000000;
    buf.configure(1, n);
    buf.append(makeBatch(n, 0.0, 0.000001));

    QElapsedTimer timer;
    timer.start();
    const QVector<PlotColumn> cols = buf.decimate(0, buf.firstTime(), buf.lastTime(), 800);
    const qint64 elapsedMs = timer.elapsed();

    QCOMPARE(cols.size(), 800);
    QVERIFY2(elapsedMs < 20, qPrintable(QString("decimate() took %1 ms, expected < 20 ms").arg(elapsedMs)));
}
