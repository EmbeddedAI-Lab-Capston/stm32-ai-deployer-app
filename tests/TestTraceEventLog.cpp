#include "TestTraceEventLog.h"
#include "modules/watcher/TraceEventLog.h"

#include <QTest>

void TestTraceEventLog::eventsAreTimeSortedByArrival()
{
    TraceEventLog log;
    log.reset();
    log.addEvent(QStringLiteral("boot"), QStringLiteral("first"));
    QTest::qSleep(2);
    log.addEvent(QStringLiteral("inference"), QStringLiteral("second"));
    QTest::qSleep(2);
    log.addEvent(QStringLiteral("sys"), QStringLiteral("third"));

    const QVector<TraceEvent> all = log.eventsBetween(-1.0, 1e9);
    QCOMPARE(all.size(), 3);
    QVERIFY(all.at(0).t <= all.at(1).t);
    QVERIFY(all.at(1).t <= all.at(2).t);
    QCOMPARE(all.at(0).text, QStringLiteral("first"));
    QCOMPARE(all.at(2).text, QStringLiteral("third"));
}

void TestTraceEventLog::eventsBetweenFiltersByRange()
{
    TraceEventLog log;
    log.reset();
    log.addEvent(QStringLiteral("boot"), QStringLiteral("a"));
    QTest::qSleep(10);
    const double mid = log.now();
    QTest::qSleep(10);
    log.addEvent(QStringLiteral("inference"), QStringLiteral("b"));

    // Range ending before the second event was added -> only "a".
    const QVector<TraceEvent> onlyA = log.eventsBetween(0.0, mid);
    QCOMPARE(onlyA.size(), 1);
    QCOMPARE(onlyA.at(0).text, QStringLiteral("a"));

    // Range starting after "a" -> only "b".
    const QVector<TraceEvent> onlyB = log.eventsBetween(mid, 1e9);
    QCOMPARE(onlyB.size(), 1);
    QCOMPARE(onlyB.at(0).text, QStringLiteral("b"));
}

void TestTraceEventLog::resetClearsAndRestartsClock()
{
    TraceEventLog log;
    log.reset();
    log.addEvent(QStringLiteral("boot"), QStringLiteral("before"));
    QCOMPARE(log.count(), 1);

    log.reset();
    QCOMPARE(log.count(), 0);
    QVERIFY(log.now() < 1.0);   // fresh clock, should read close to zero
}

void TestTraceEventLog::emptyLogReturnsEmptyRange()
{
    TraceEventLog log;
    const QVector<TraceEvent> out = log.eventsBetween(0.0, 100.0);
    QVERIFY(out.isEmpty());
}
