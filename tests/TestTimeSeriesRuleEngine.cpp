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

// appliesToRole is the ONLY link between watch/watch_rules.json and a watch
// item. VariableWatcher::updateItem() used to silently drop "role", which left
// every role-based rule permanently unmatched (and profile comparison empty)
// while nothing failed. Pin both directions of the match here.
void TestTimeSeriesRuleEngine::ruleDoesNotMatchAnItemWithADifferentOrEmptyRole()
{
    TraceBuffer buf;
    buf.configure(1, 1000);
    for (int i = 0; i <= 150; ++i)
        appendSample(buf, i * 0.01, 100.0);

    TsRule rule;
    rule.id = "stack_headroom_critical";
    rule.appliesToRole = "stackWatermark";
    rule.type = TsConditionType::Threshold;
    rule.op = "<";
    rule.value = 512;
    rule.sustainMs = 1000;
    rule.message = "m";

    const double now = buf.lastTime();

    // Empty role -> no match (the regression that made the rule dead).
    const QList<WatchItem> noRole = { makeItem("i1", "stackFree", "") };
    QCOMPARE(TimeSeriesRuleEngine::evaluate({rule}, noRole, buf, now, {}).size(), 0);

    // Different role -> no match.
    const QList<WatchItem> otherRole = { makeItem("i1", "stackFree", "heapEnd") };
    QCOMPARE(TimeSeriesRuleEngine::evaluate({rule}, otherRole, buf, now, {}).size(), 0);

    // Correct role -> match.
    const QList<WatchItem> right = { makeItem("i1", "stackFree", "stackWatermark") };
    QCOMPARE(TimeSeriesRuleEngine::evaluate({rule}, right, buf, now, {}).size(), 1);
}

void TestTimeSeriesRuleEngine::thresholdBoundaryIsExclusiveForStrictLessThan()
{
    TraceBuffer buf;
    buf.configure(1, 1000);
    for (int i = 0; i <= 150; ++i)
        appendSample(buf, i * 0.01, 512.0);   // exactly AT the threshold

    TsRule rule;
    rule.id = "stack_headroom_critical";
    rule.appliesToRole = "stackWatermark";
    rule.type = TsConditionType::Threshold;
    rule.op = "<";
    rule.value = 512;
    rule.sustainMs = 1000;
    rule.message = "m";

    const QList<WatchItem> items = { makeItem("i1", "stackFree", "stackWatermark") };
    QCOMPARE(TimeSeriesRuleEngine::evaluate({rule}, items, buf, buf.lastTime(), {}).size(), 0);

    TsRule le = rule;
    le.op = "<=";
    QCOMPARE(TimeSeriesRuleEngine::evaluate({le}, items, buf, buf.lastTime(), {}).size(), 1);
}

// The gate window is symmetric (|e.t - t| <= withinMs): an event just BEFORE
// the sample correlates just as well as one just after.
void TestTimeSeriesRuleEngine::gateRejectsAnEventOutsideTheWindowOnEitherSide()
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
    auto evAt = [](double t) {
        TraceEvent e; e.t = t; e.kind = "inference"; e.severity = "info"; return e;
    };

    // 20 ms BEFORE the spike -> inside the window, must be allowed.
    QCOMPARE(TimeSeriesRuleEngine::evaluate({rule}, items, buf, buf.lastTime(), {evAt(0.179)}).size(), 1);
    // 20 ms after -> also allowed.
    QCOMPARE(TimeSeriesRuleEngine::evaluate({rule}, items, buf, buf.lastTime(), {evAt(0.219)}).size(), 1);
    // 200 ms before -> outside on the early side, must be rejected.
    QCOMPARE(TimeSeriesRuleEngine::evaluate({rule}, items, buf, buf.lastTime(), {evAt(-0.001)}).size(), 0);
    // 200 ms after -> outside on the late side, rejected.
    QCOMPARE(TimeSeriesRuleEngine::evaluate({rule}, items, buf, buf.lastTime(), {evAt(0.399)}).size(), 0);
}

// A rule marked "enabled": false in watch_rules.json must parse but never
// produce a violation — that is how a rule whose preconditions do not exist
// yet (e.g. the stackWatermark rules, which need RegionScan sampling) can ship
// without pretending it can fire.
void TestTimeSeriesRuleEngine::disabledRuleIsParsedButNeverEvaluated()
{
    const QByteArray json = R"({
        "rules": [
            { "id": "off", "enabled": false, "appliesToRole": "stackWatermark",
              "type": "threshold", "op": "<", "value": 512, "message": "m" },
            { "id": "on", "appliesToRole": "stackWatermark",
              "type": "threshold", "op": "<", "value": 512, "message": "m" }
        ]
    })";
    const QVector<TsRule> rules = TimeSeriesRuleEngine::loadRulesFromJson(json);
    QCOMPARE(rules.size(), 2);              // both parsed
    QVERIFY(!rules.at(0).enabled);
    QVERIFY(rules.at(1).enabled);

    TraceBuffer buf;
    buf.configure(1, 100);
    appendSample(buf, 0.0, 100.0);          // under the threshold

    const QList<WatchItem> items = { makeItem("i1", "stackFree", "stackWatermark") };
    const auto violations = TimeSeriesRuleEngine::evaluate(rules, items, buf, buf.lastTime(), {});
    QCOMPARE(violations.size(), 1);         // only the enabled one fired
    QCOMPARE(violations.first().ruleId, QStringLiteral("on"));
}
