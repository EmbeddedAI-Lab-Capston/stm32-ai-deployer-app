#include "TestTimeSeriesRuleEngine.h"
#include "modules/watcher/TimeSeriesRuleEngine.h"
#include "modules/watcher/TraceBuffer.h"

#include <QTest>
#include <cmath>

namespace {

WatchItem makeItem(const QString &id, const QString &label, const QString &role)
{
    WatchItem it;
    it.id = id;
    it.label = label;
    it.role = role;
    it.enabled = true;
    return it;
}

void appendSample(TraceBuffer &buf, double t, double v)
{
    WatchSampleBatch b;
    b.times << t;
    b.series << QVector<double>{v};
    buf.append(b);
}

}

void TestTimeSeriesRuleEngine::thresholdSingleSampleUnderSustainMsIsNotAViolation()
{
    TraceBuffer buf;
    buf.configure(1, 1000);
    appendSample(buf, 10.0, 100.0);   // one lone sample, well under 512

    TsRule rule;
    rule.id = "stack_headroom_critical";
    rule.severity = "error";
    rule.appliesToRole = "stackWatermark";
    rule.type = TsConditionType::Threshold;
    rule.op = "<";
    rule.value = 512;
    rule.sustainMs = 1000;
    rule.message = "{label}: {value} < {threshold}";

    const QList<WatchItem> items = { makeItem("i1", "stackFree", "stackWatermark") };
    const auto violations = TimeSeriesRuleEngine::evaluate({rule}, items, buf, buf.lastTime(), {});
    QCOMPARE(violations.size(), 0);
}

void TestTimeSeriesRuleEngine::thresholdSustainedOverWindowIsAViolationWithCorrectTime()
{
    TraceBuffer buf;
    buf.configure(1, 10000);
    // 1.5s of samples, all under 512, at 100 Hz.
    for (int i = 0; i <= 150; ++i)
        appendSample(buf, i * 0.01, 100.0);

    TsRule rule;
    rule.id = "stack_headroom_critical";
    rule.appliesToRole = "stackWatermark";
    rule.type = TsConditionType::Threshold;
    rule.op = "<";
    rule.value = 512;
    rule.sustainMs = 1000;
    rule.message = "{label}: {value} < {threshold}";

    const QList<WatchItem> items = { makeItem("i1", "stackFree", "stackWatermark") };
    const double now = buf.lastTime();
    const auto violations = TimeSeriesRuleEngine::evaluate({rule}, items, buf, now, {});
    QCOMPARE(violations.size(), 1);
    QCOMPARE(violations.first().t, now);
    QVERIFY(violations.first().message.contains("stackFree"));
}

void TestTimeSeriesRuleEngine::zscoreSpikeIsDetectedWithExpectedZ()
{
    TraceBuffer buf;
    buf.configure(1, 1000);
    for (int i = 0; i < 199; ++i)
        appendSample(buf, i * 0.001, 100.0);
    appendSample(buf, 0.199, 1000.0);   // spike as the latest sample

    // Expected mean/stddev/z computed the same way the engine does.
    double sum = 199 * 100.0 + 1000.0;
    const double mean = sum / 200.0;
    double sq = 199 * (100.0 - mean) * (100.0 - mean) + (1000.0 - mean) * (1000.0 - mean);
    const double stddev = std::sqrt(sq / 200.0);
    const double expectedZ = std::abs(1000.0 - mean) / stddev;

    TsRule rule;
    rule.id = "inference_time_outlier";
    rule.appliesToLabelRegex = "inference_us|elapsed_us|inf_us";
    rule.type = TsConditionType::ZScore;
    rule.windowMs = 5000;
    rule.k = 4.0;
    rule.minSamples = 200;
    rule.message = "{label}: {value} mean={mean} z={z}";

    const QList<WatchItem> items = { makeItem("i1", "g_ai_last_inference_us", "") };
    const auto violations = TimeSeriesRuleEngine::evaluate({rule}, items, buf, buf.lastTime(), {});
    QCOMPARE(violations.size(), 1);
    QVERIFY(qAbs(violations.first().detail.value("z").toDouble() - expectedZ) < 1e-6);
}

void TestTimeSeriesRuleEngine::zscoreBelowMinSamplesIsNotAViolation()
{
    TraceBuffer buf;
    buf.configure(1, 1000);
    for (int i = 0; i < 49; ++i)
        appendSample(buf, i * 0.001, 100.0);
    appendSample(buf, 0.049, 1000.0);   // only 50 samples total, spike included

    TsRule rule;
    rule.id = "inference_time_outlier";
    rule.appliesToLabelRegex = "inference_us";
    rule.type = TsConditionType::ZScore;
    rule.windowMs = 5000;
    rule.k = 4.0;
    rule.minSamples = 200;   // more than the 50 available
    rule.message = "x";

    const QList<WatchItem> items = { makeItem("i1", "inference_us", "") };
    const auto violations = TimeSeriesRuleEngine::evaluate({rule}, items, buf, buf.lastTime(), {});
    QCOMPARE(violations.size(), 0);
}

void TestTimeSeriesRuleEngine::driftRisingSeriesIsDetectedWithSlopeAndR2()
{
    TraceBuffer buf;
    buf.configure(1, 10000);
    // Perfectly linear: v = 10 * t (B/s = 10), 30s @ 10 Hz.
    for (int i = 0; i <= 300; ++i) {
        const double t = i * 0.1;
        appendSample(buf, t, 10.0 * t);
    }

    TsRule rule;
    rule.id = "heap_leak_drift";
    rule.appliesToRole = "heapEnd";
    rule.type = TsConditionType::Drift;
    rule.windowMs = 30000;
    rule.minSlopePerSec = 8.0;
    rule.minR2 = 0.6;
    rule.message = "{label}: slope={slope} r2={r2}";

    const QList<WatchItem> items = { makeItem("i1", "heapEnd", "heapEnd") };
    const auto violations = TimeSeriesRuleEngine::evaluate({rule}, items, buf, buf.lastTime(), {});
    QCOMPARE(violations.size(), 1);
    QVERIFY(qAbs(violations.first().detail.value("slope").toDouble() - 10.0) < 0.01);
    QVERIFY(violations.first().detail.value("r2").toDouble() > 0.999);
}

void TestTimeSeriesRuleEngine::driftNoisyFlatSeriesIsRejectedByR2Gate()
{
    TraceBuffer buf;
    buf.configure(1, 10000);
    // Constant mean with alternating jitter — no real trend, low R^2.
    for (int i = 0; i <= 300; ++i) {
        const double t = i * 0.1;
        const double noise = (i % 2 == 0) ? 5.0 : -5.0;
        appendSample(buf, t, 1000.0 + noise);
    }

    TsRule rule;
    rule.id = "heap_leak_drift";
    rule.appliesToRole = "heapEnd";
    rule.type = TsConditionType::Drift;
    rule.windowMs = 30000;
    rule.minSlopePerSec = 8.0;
    rule.minR2 = 0.6;
    rule.message = "x";

    const QList<WatchItem> items = { makeItem("i1", "heapEnd", "heapEnd") };
    const auto violations = TimeSeriesRuleEngine::evaluate({rule}, items, buf, buf.lastTime(), {});
    QCOMPARE(violations.size(), 0);
}

void TestTimeSeriesRuleEngine::gateSuppressesViolationWithoutMatchingEvent()
{
    TraceBuffer buf;
    buf.configure(1, 1000);
    for (int i = 0; i < 199; ++i)
        appendSample(buf, i * 0.001, 100.0);
    appendSample(buf, 0.199, 1000.0);

    TsRule rule;
    rule.id = "inference_time_outlier";
    rule.appliesToLabelRegex = "inference_us";
    rule.type = TsConditionType::ZScore;
    rule.windowMs = 5000;
    rule.k = 4.0;
    rule.minSamples = 200;
    rule.gate.eventKind = "inference";
    rule.gate.withinMs = 50;
    rule.message = "x";

    const QList<WatchItem> items = { makeItem("i1", "inference_us", "") };
    const auto violations = TimeSeriesRuleEngine::evaluate({rule}, items, buf, buf.lastTime(), {});
    QCOMPARE(violations.size(), 0);   // condition met, but no event -> suppressed
}

void TestTimeSeriesRuleEngine::gateAllowsViolationWithMatchingEventWithinWindow()
{
    TraceBuffer buf;
    buf.configure(1, 1000);
    for (int i = 0; i < 199; ++i)
        appendSample(buf, i * 0.001, 100.0);
    appendSample(buf, 0.199, 1000.0);

    TsRule rule;
    rule.id = "inference_time_outlier";
    rule.appliesToLabelRegex = "inference_us";
    rule.type = TsConditionType::ZScore;
    rule.windowMs = 5000;
    rule.k = 4.0;
    rule.minSamples = 200;
    rule.gate.eventKind = "inference";
    rule.gate.withinMs = 50;
    rule.message = "x";

    TraceEvent ev;
    ev.t = 0.20;   // 1ms after the spike, well within 50ms
    ev.kind = "inference";
    ev.text = "test";
    ev.severity = "info";

    const QList<WatchItem> items = { makeItem("i1", "inference_us", "") };
    const auto violations = TimeSeriesRuleEngine::evaluate({rule}, items, buf, buf.lastTime(), {ev});
    QCOMPARE(violations.size(), 1);
}

void TestTimeSeriesRuleEngine::loadRulesFromJsonParsesKnownFields()
{
    const QByteArray json = R"({
        "rules": [
            { "id": "r1", "severity": "error", "appliesToRole": "stackWatermark",
              "type": "threshold", "op": "<", "value": 512, "sustainMs": 1000,
              "message": "m" },
            { "id": "r2", "type": "zscore", "appliesToLabelRegex": "inf_us",
              "windowMs": 5000, "k": 4.0, "minSamples": 200,
              "gate": { "eventKind": "inference", "withinMs": 50 } }
        ]
    })";

    QString err;
    const QVector<TsRule> rules = TimeSeriesRuleEngine::loadRulesFromJson(json, &err);
    QVERIFY2(err.isEmpty(), qPrintable(err));
    QCOMPARE(rules.size(), 2);
    QCOMPARE(rules.at(0).type, TsConditionType::Threshold);
    QCOMPARE(rules.at(0).value, 512.0);
    QCOMPARE(rules.at(0).sustainMs, 1000);
    QCOMPARE(rules.at(1).type, TsConditionType::ZScore);
    QCOMPARE(rules.at(1).gate.eventKind, QStringLiteral("inference"));
    QCOMPARE(rules.at(1).gate.withinMs, 50);
}

void TestTimeSeriesRuleEngine::loadRulesFromJsonSkipsEntriesMissingId()
{
    const QByteArray json = R"({
        "rules": [
            { "severity": "error", "type": "threshold" },
            { "id": "ok", "type": "threshold" }
        ]
    })";
    const QVector<TsRule> rules = TimeSeriesRuleEngine::loadRulesFromJson(json);
    QCOMPARE(rules.size(), 1);
    QCOMPARE(rules.first().id, QStringLiteral("ok"));
}
