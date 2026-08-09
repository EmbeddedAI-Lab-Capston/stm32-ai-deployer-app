#include "TestWatchProfile.h"
#include "modules/watcher/WatchProfile.h"

#include <QTest>

void TestWatchProfile::buildsOneRowPerItemWithFifteenCellsCorrectlyMapped()
{
    WatchItem it;
    it.label = "g_ai_infer_count";
    it.unit  = "count";
    it.role  = "heapEnd";

    WatchStats st;
    st.count = 1234;
    st.min = 1.0; st.max = 99.0; st.mean = 50.0; st.last = 98.0;
    // m2 chosen so stddev() = sqrt(m2/count) is a known value; not asserted
    // exactly here, just that the column round-trips through buildRows().

    WatchProfileInput session;
    session.model = "anomaly_cnn_int8";
    session.board = "STM32H7";
    session.rawSeriesPath = "D:/watch/session.csv";
    session.note = "demo run";

    const QList<QStringList> rows = WatchProfile::buildRows(session, {it}, {st}, 947.3, 30.021);

    QCOMPARE(rows.size(), 1);
    const QStringList &cells = rows.first();
    QCOMPARE(cells.size(), 15);

    QCOMPARE(cells.at(0), QStringLiteral("anomaly_cnn_int8"));   // c0 model
    QCOMPARE(cells.at(1), QStringLiteral("STM32H7"));            // c1 board
    QCOMPARE(cells.at(2), QStringLiteral("g_ai_infer_count"));   // c2 label
    QCOMPARE(cells.at(3), QStringLiteral("1234"));               // c3 sample count
    QCOMPARE(cells.at(4), QStringLiteral("947.30"));             // c4 actual Hz
    QCOMPARE(cells.at(5).toDouble(), 1.0);                       // c5 min
    QCOMPARE(cells.at(6).toDouble(), 99.0);                      // c6 max
    QCOMPARE(cells.at(7).toDouble(), 50.0);                      // c7 mean
    // c8 = stddev — just verify it parses as a non-negative number
    QVERIFY(cells.at(8).toDouble() >= 0.0);
    QCOMPARE(cells.at(9).toDouble(), 98.0);                      // c9 last
    QCOMPARE(cells.at(10), QStringLiteral("30.021"));            // c10 duration
    QCOMPARE(cells.at(11), QStringLiteral("count"));             // c11 unit
    QCOMPARE(cells.at(12), QStringLiteral("heapEnd"));           // c12 role
    QCOMPARE(cells.at(13), QStringLiteral("D:/watch/session.csv")); // c13 raw path
    QCOMPARE(cells.at(14), QStringLiteral("demo run"));          // c14 note
}

void TestWatchProfile::emptyItemListProducesNoRows()
{
    WatchProfileInput session;
    const QList<QStringList> rows = WatchProfile::buildRows(session, {}, {}, 0.0, 0.0);
    QVERIFY(rows.isEmpty());
}

// Regression for the Faz 8 unit-scale fix (docs/variable_watcher_findings.md
// 17.1): WatchStats holds RAW decoded values, so buildRows() must apply
// scale/offset before storing, or c5..c9 stay in raw counts while c11 claims a
// scaled unit. Mutation-tested: reverting the fix must fail this test.
void TestWatchProfile::scaleAndOffsetAreAppliedToStoredNumbers()
{
    WatchItem it;
    it.label = QStringLiteral("g_ai_last_inference_us");
    it.role  = QStringLiteral("inferenceUs");
    it.unit  = QStringLiteral("ms");
    it.scale = 0.001;      // microseconds -> milliseconds
    it.offset = 0.0;

    WatchStats st;
    st.push(1000.0);
    st.push(3000.0);
    st.push(2000.0);       // mean 2000 us, min 1000, max 3000

    WatchProfileInput in;
    in.board = QStringLiteral("STM32H7");
    const QList<QStringList> rows = WatchProfile::buildRows(in, {it}, {st}, 200.0, 10.0);

    QCOMPARE(rows.size(), 1);
    const QStringList &c = rows.first();
    QCOMPARE(c.at(5).toDouble(), 1.0);   // min  -> ms
    QCOMPARE(c.at(6).toDouble(), 3.0);   // max  -> ms
    QCOMPARE(c.at(7).toDouble(), 2.0);   // mean -> ms
    QCOMPARE(c.at(9).toDouble(), 2.0);   // last -> ms
    QCOMPARE(c.at(11), QStringLiteral("ms"));
    // stddev scales by |scale| only (offset must NOT shift a dispersion).
    // Tolerance is relative: cells are written with QString::number(..., 'g', 10).
    const double expectedStddev = st.stddev() * 0.001;
    QVERIFY(qAbs(c.at(8).toDouble() - expectedStddev) < qAbs(expectedStddev) * 1e-9);

    // An offset must shift the level but still not the stddev.
    WatchItem shifted = it;
    shifted.scale = 1.0;
    shifted.offset = 5.0;
    const QList<QStringList> rows2 = WatchProfile::buildRows(in, {shifted}, {st}, 200.0, 10.0);
    QCOMPARE(rows2.first().at(5).toDouble(), 1005.0);
    QVERIFY(qAbs(rows2.first().at(8).toDouble() - st.stddev()) < st.stddev() * 1e-9);
}
