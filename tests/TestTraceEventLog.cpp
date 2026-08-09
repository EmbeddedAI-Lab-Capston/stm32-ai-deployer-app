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

// Regression: sample timestamps run on DebugLinkWorker's clock, which starts
// at socket-connect — earlier than link-open by the whole handshake. reset()
// must be seedable with that clock's reading so both live on ONE axis; before
// this, events were shifted by the handshake duration (~61 ms measured on a
// NUCLEO-H723ZG), which alone defeated the +/-50 ms "inference" event gate.
void TestTraceEventLog::resetWithOriginPutsEventsOnTheSampleClockAxis()
{
    TraceEventLog log;
    log.reset(12.5);                       // link opened 12.5 s into the session
    QVERIFY(log.now() >= 12.5);
    QVERIFY(log.now() < 12.5 + 5.0);       // generous upper bound, not a timing test

    log.addEvent(QStringLiteral("inference"), QStringLiteral("x"));
    const QVector<TraceEvent> all = log.eventsBetween(0.0, 1e9);
    QCOMPARE(all.size(), 1);
    QVERIFY2(all.first().t >= 12.5,
             "event stamped before the session origin -> two-clock regression");

    // A default reset() keeps the old zero-based behaviour.
    TraceEventLog zeroed;
    zeroed.reset();
    QVERIFY(zeroed.now() < 1.0);
}

// Regression: the event list used to grow without bound (one event per
// inference packet) while eventsBetween() scans it linearly at plot rate.
void TestTraceEventLog::eventListIsCappedAndKeepsNewest()
{
    TraceEventLog log;
    log.reset();
    for (int i = 0; i < TraceEventLog::kMaxEvents + 250; ++i)
        log.addEvent(QStringLiteral("inference"), QString::number(i));

    QCOMPARE(log.count(), TraceEventLog::kMaxEvents);
    const QVector<TraceEvent> all = log.eventsBetween(0.0, 1e9);
    QCOMPARE(all.size(), TraceEventLog::kMaxEvents);
    QCOMPARE(all.first().text, QString::number(250));                       // oldest dropped
    QCOMPARE(all.last().text, QString::number(TraceEventLog::kMaxEvents + 249));
}
