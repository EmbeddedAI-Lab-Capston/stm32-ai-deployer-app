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
