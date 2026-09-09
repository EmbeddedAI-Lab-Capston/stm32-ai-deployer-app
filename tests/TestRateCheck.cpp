#include "TestRateCheck.h"
#include "modules/watcher/RateCheck.h"

#include <QTest>

void TestRateCheck::noCountSamplesReportsNotOk()
{
    RateCheckInput in;
    in.haveCountSamples = false;
    const RateCheckResult r = computeRateCheck(in);
    QVERIFY(!r.ok);
    QVERIFY(!r.detail.isEmpty());
}

void TestRateCheck::zeroCountDeltaReportsNotOk()
{
    RateCheckInput in;
    in.haveCountSamples = true;
    in.firstT = 0.0; in.lastT = 10.0;
    in.firstCount = 500.0; in.lastCount = 500.0;   // no progress in the window
    in.infUsKnown = true; in.reportedInfUs = 125.0;
    const RateCheckResult r = computeRateCheck(in);
    QVERIFY(!r.ok);
    QVERIFY(!r.detail.isEmpty());
}

void TestRateCheck::missingInfUsReportsNotOk()
{
    RateCheckInput in;
    in.haveCountSamples = true;
    in.firstT = 0.0; in.lastT = 10.0;
    in.firstCount = 0.0; in.lastCount = 600.0;
    in.infUsKnown = false;
    const RateCheckResult r = computeRateCheck(in);
    QVERIFY(!r.ok);
    QVERIFY(!r.detail.isEmpty());
}

// Firmware loop paces itself with HAL_Delay(16) -> ~60 Hz, well under the
// theoretical max implied by a 125 us inference (8000 Hz) — matches the
// real F4 numbers measured live in Faz 10.1 (memInferUs == 125 us).
void TestRateCheck::consistentCaseComputesExpectedHz()
{
    RateCheckInput in;
    in.haveCountSamples = true;
    in.firstT = 0.0; in.lastT = 10.0;
    in.firstCount = 0.0; in.lastCount = 600.0;   // 60 Hz over 10 s
    in.infUsKnown = true; in.reportedInfUs = 125.0;

    const RateCheckResult r = computeRateCheck(in);
    QVERIFY(r.ok);
    QCOMPARE(r.observedHz, 60.0);
    QCOMPARE(r.theoreticalMaxHz, 8000.0);
    QVERIFY(r.consistent);
    QVERIFY2(r.detail.contains(QStringLiteral("tutarlı")) && !r.detail.contains(QStringLiteral("TUTARSIZ")),
              "consistent result must say 'tutarli', never 'dogrulandi' or 'TUTARSIZ'");
}

// A claimed 125 us inference caps throughput at 8000 Hz — observing 9000 Hz
// is a provable contradiction of the firmware's own claim, must be flagged.
void TestRateCheck::observedRateExceedingTheoreticalMaxIsInconsistent()
{
    RateCheckInput in;
    in.haveCountSamples = true;
    in.firstT = 0.0; in.lastT = 1.0;
    in.firstCount = 0.0; in.lastCount = 9000.0;   // 9000 Hz observed
    in.infUsKnown = true; in.reportedInfUs = 125.0;   // implies max 8000 Hz

    const RateCheckResult r = computeRateCheck(in);
    QVERIFY(r.ok);   // the numbers ARE computed — the contradiction IS the result
    QVERIFY(!r.consistent);
    QVERIFY(r.detail.contains(QStringLiteral("TUTARSIZ")));
}
