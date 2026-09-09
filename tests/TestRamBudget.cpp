#include "TestRamBudget.h"
#include "modules/watcher/RamBudget.h"

#include <QTest>

namespace {
// A self-consistent "normal" layout, chosen with round decimal numbers so
// the expected results can be hand-verified without hex arithmetic:
//   ramTop = 1,000,000   ramTotal = 100,000  -> ramBase = 900,000
//   staticEnd = 910,000  (staticUsed = 10,000)
//   heapTop   = 920,000  (heapUsed   = 10,000)
//   stackBase = 995,000, headroom = 2,000    -> stackDip = 997,000
//   stackUsed = ramTop - stackDip = 3,000
//   freeBytes = stackDip - max(staticEnd, heapTop) = 997,000 - 920,000 = 77,000
// 10,000 + 10,000 + 3,000 + 77,000 == 100,000 (ramTotal) — sums to the whole.
RamBudgetInput normalInput()
{
    RamBudgetInput in;
    in.ramTotalBytes = 100000;
    in.ramTopKnown = true;
    in.ramTopAddr  = 1000000;
    in.staticEndKnown = true;
    in.staticEndAddr  = 910000;
    in.heapKnown   = true;
    in.heapTopAddr = 920000;
    in.stackKnown         = true;
    in.stackBaseAddr      = 995000;
    in.stackHeadroomBytes = 2000;
    return in;
}
}

void TestRamBudget::missingRamTotalReportsNotOk()
{
    RamBudgetInput in = normalInput();
    in.ramTotalBytes = 0;
    const RamBudgetResult r = computeRamBudget(in);
    QVERIFY(!r.ok);
    QVERIFY(!r.warning.isEmpty());
}

void TestRamBudget::missingEstackReportsNotOk()
{
    RamBudgetInput in = normalInput();
    in.ramTopKnown = false;
    const RamBudgetResult r = computeRamBudget(in);
    QVERIFY(!r.ok);
    QVERIFY(!r.warning.isEmpty());
}

void TestRamBudget::missingStackWatermarkReportsNotOk()
{
    RamBudgetInput in = normalInput();
    in.stackKnown = false;
    const RamBudgetResult r = computeRamBudget(in);
    QVERIFY(!r.ok);
    QVERIFY(!r.warning.isEmpty());
}

void TestRamBudget::normalCaseComputesExpectedBreakdown()
{
    const RamBudgetResult r = computeRamBudget(normalInput());
    QVERIFY(r.ok);
    QVERIFY(r.warning.isEmpty());

    QCOMPARE(r.ramBase, quint64(900000));
    QCOMPARE(r.staticEnd, quint64(910000));
    QCOMPARE(r.heapTop, quint64(920000));
    QCOMPARE(r.stackDip, quint64(997000));

    QCOMPARE(r.staticUsed, quint64(10000));
    QCOMPARE(r.heapUsed, quint64(10000));
    QCOMPARE(r.stackUsed, quint64(3000));
    QCOMPARE(r.freeBytes, qint64(77000));

    // The four bands must account for the whole of ramTotal.
    QCOMPARE(r.staticUsed + r.heapUsed + r.stackUsed + quint64(r.freeBytes), r.ramTotal);
    QVERIFY(qFuzzyCompare(r.usedPct, 23.0));
}

// __sbrk_heap_end not being watched (or never having produced a value yet)
// must be treated as "heap isn't in use", not as a missing/failed reading —
// heapTop folds to 0 and the static end alone bounds the free region.
void TestRamBudget::heapNotWatchedTreatsHeapAsUnused()
{
    RamBudgetInput in = normalInput();
    in.heapKnown = false;
    const RamBudgetResult r = computeRamBudget(in);
    QVERIFY(r.ok);
    QCOMPARE(r.heapTop, quint64(0));
    QCOMPARE(r.heapUsed, quint64(0));
    // Free region now bounded by staticEnd (910,000) instead of heapTop:
    // 997,000 - 910,000 = 87,000.
    QCOMPARE(r.freeBytes, qint64(87000));
}

// The stack's deepest recorded point landing BELOW static/heap end is a real
// collision (memory corruption risk), not a rounding artefact — must be
// reported (negative freeBytes + non-empty warning) but not treated as an
// unusable/"ok=false" result, since the numbers themselves are still valid.
void TestRamBudget::stackPastStaticEndReportsCollisionButStillOk()
{
    RamBudgetInput in = normalInput();
    in.stackBaseAddr      = 905000;
    in.stackHeadroomBytes = 1000;   // stackDip = 906000, well under heapTop=920000

    const RamBudgetResult r = computeRamBudget(in);
    QVERIFY(r.ok);
    QVERIFY2(!r.warning.isEmpty(), "a RAM collision must not pass silently");
    QCOMPARE(r.stackDip, quint64(906000));
    QCOMPARE(r.freeBytes, qint64(-14000));
}
