#include "TestWatchModel.h"
#include "modules/watcher/WatchModel.h"

#include <QRandomGenerator>
#include <QTest>

#include <cmath>
#include <vector>

void TestWatchModel::statsMatchTwoPassReferenceWithinTolerance()
{
    QRandomGenerator rng(42);   // fixed seed — deterministic test
    std::vector<double> values;
    values.reserve(1000);
    for (int i = 0; i < 1000; ++i)
        values.push_back(rng.generateDouble() * 1000.0 - 500.0);

    WatchStats stats;
    for (double v : values)
        stats.push(v);

    double sum = 0.0;
    for (double v : values) sum += v;
    const double refMean = sum / double(values.size());

    double sqSum = 0.0;
    for (double v : values) sqSum += (v - refMean) * (v - refMean);
    const double refStddev = std::sqrt(sqSum / double(values.size()));

    QCOMPARE(stats.count, quint64(1000));
    QVERIFY(std::fabs(stats.mean - refMean) <= 1e-9);
    QVERIFY(std::fabs(stats.stddev() - refStddev) <= 1e-9);
}

void TestWatchModel::statsResetClearsEverything()
{
    WatchStats stats;
    stats.push(1.0);
    stats.push(2.0);
    stats.push(3.0);
    stats.reset();

    QCOMPARE(stats.count, quint64(0));
    QCOMPARE(stats.min, 0.0);
    QCOMPARE(stats.max, 0.0);
    QCOMPARE(stats.last, 0.0);
    QCOMPARE(stats.mean, 0.0);
    QCOMPARE(stats.stddev(), 0.0);
}

void TestWatchModel::statsTrackMinMaxLast()
{
    WatchStats stats;
    stats.push(5.0);
    stats.push(-3.0);
    stats.push(9.0);
    stats.push(1.0);

    QCOMPARE(stats.min, -3.0);
    QCOMPARE(stats.max, 9.0);
    QCOMPARE(stats.last, 1.0);
}
