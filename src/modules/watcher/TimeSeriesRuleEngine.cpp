#include "TimeSeriesRuleEngine.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include <cmath>

namespace {

bool checkOp(double value, const QString &op, double threshold)
{
    if (op == QStringLiteral("<"))  return value < threshold;
    if (op == QStringLiteral(">"))  return value > threshold;
    if (op == QStringLiteral("<=")) return value <= threshold;
    if (op == QStringLiteral(">=")) return value >= threshold;
    if (op == QStringLiteral("==")) return qFuzzyCompare(value + 1.0, threshold + 1.0);
    if (op == QStringLiteral("!=")) return !qFuzzyCompare(value + 1.0, threshold + 1.0);
    return false;
}

bool itemMatchesRule(const WatchItem &item, const TsRule &rule)
{
    if (!rule.appliesToRole.isEmpty())
        return item.role == rule.appliesToRole;
    if (!rule.appliesToLabelRegex.isEmpty())
        return QRegularExpression(rule.appliesToLabelRegex).match(item.label).hasMatch();
    return false;
}

QString fmtNum(double v, int decimals = 2) { return QString::number(v, 'f', decimals); }

QString formatMessage(const QString &tmpl, const QMap<QString, QString> &vars)
{
    QString out = tmpl;
    for (auto it = vars.constBegin(); it != vars.constEnd(); ++it)
        out.replace(QStringLiteral("{%1}").arg(it.key()), it.value());
    return out;
}

bool gateAllows(const TsGate &gate, double t, const QVector<TraceEvent> &events)
{
    if (!gate.isValid()) return true;
    const double windowS = gate.withinMs / 1000.0;
    for (const TraceEvent &e : events) {
        if (e.kind != gate.eventKind) continue;
        if (std::abs(e.t - t) <= windowS) return true;
    }
    return false;
}

}   // namespace

QVector<TsRule> TimeSeriesRuleEngine::loadRulesFromJson(const QByteArray &json, QString *errorOut)
{
    QVector<TsRule> out;
    QJsonParseError perr;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &perr);
    if (perr.error != QJsonParseError::NoError) {
        if (errorOut) *errorOut = perr.errorString();
        return out;
    }

    const QJsonArray arr = doc.object().value(QStringLiteral("rules")).toArray();
    for (const QJsonValue &v : arr) {
        const QJsonObject o = v.toObject();
        TsRule r;
        r.id                 = o.value(QStringLiteral("id")).toString();
        if (r.id.isEmpty())
            continue;   // malformed entry — skip silently, never crash the whole load

        r.enabled             = o.value(QStringLiteral("enabled")).toBool(true);
        r.severity            = o.value(QStringLiteral("severity")).toString(QStringLiteral("info"));
        r.message              = o.value(QStringLiteral("message")).toString();
        r.appliesToRole        = o.value(QStringLiteral("appliesToRole")).toString();
        r.appliesToLabelRegex  = o.value(QStringLiteral("appliesToLabelRegex")).toString();
        r.windowMs             = o.value(QStringLiteral("windowMs")).toInt();
        r.type                 = tsConditionTypeFromString(o.value(QStringLiteral("type")).toString());
        r.op                    = o.value(QStringLiteral("op")).toString();
        r.value                 = o.value(QStringLiteral("value")).toDouble();
        r.k                     = o.value(QStringLiteral("k")).toDouble(4.0);
        r.minSlopePerSec       = o.value(QStringLiteral("minSlopePerSec")).toDouble();
        r.minR2                 = o.value(QStringLiteral("minR2")).toDouble(0.6);
        r.minSamples           = o.value(QStringLiteral("minSamples")).toInt(50);
        r.sustainMs            = o.value(QStringLiteral("sustainMs")).toInt();

        if (o.contains(QStringLiteral("gate"))) {
            const QJsonObject g = o.value(QStringLiteral("gate")).toObject();
            r.gate.eventKind = g.value(QStringLiteral("eventKind")).toString();
            r.gate.withinMs   = g.value(QStringLiteral("withinMs")).toInt();
        }

        out.append(r);
    }
    return out;
}

QVector<TsRuleViolation> TimeSeriesRuleEngine::evaluate(const QVector<TsRule> &rules,
                                                         const QList<WatchItem> &items,
                                                         const TraceBuffer &buffer,
                                                         double now,
                                                         const QVector<TraceEvent> &recentEvents)
{
    QVector<TsRuleViolation> out;

    for (const TsRule &rule : rules) {
        if (!rule.enabled)
            continue;

        for (int i = 0; i < items.size(); ++i) {
            const WatchItem &item = items.at(i);
            if (!item.enabled || !itemMatchesRule(item, rule))
                continue;

            switch (rule.type) {
            case TsConditionType::Threshold: {
                double t = now, value = 0.0;
                bool satisfied = false;

                if (rule.sustainMs <= 0) {
                    const WatchStats &st = buffer.stats(i);
                    if (st.count == 0) continue;
                    value = st.last;
                    satisfied = checkOp(value, rule.op, rule.value);
                } else {
                    const double t0 = now - rule.sustainMs / 1000.0;
                    const QVector<RawSample> win = buffer.rawWindow(i, t0, now);
                    if (win.isEmpty())
                        continue;
                    // Require history actually reaching back to (near) t0 —
                    // otherwise a single recent sample under threshold would
                    // false-positive as "sustained" on no evidence at all.
                    if (win.first().t > t0 + 0.001)
                        continue;
                    satisfied = true;
                    for (const RawSample &s : win) {
                        if (!checkOp(s.v, rule.op, rule.value)) { satisfied = false; break; }
                    }
                    value = win.last().v;
                    t     = win.last().t;
                }
                if (!satisfied || !gateAllows(rule.gate, t, recentEvents))
                    continue;

                TsRuleViolation viol;
                viol.ruleId = rule.id; viol.severity = rule.severity;
                viol.itemId = item.id; viol.label = item.label;
                viol.t = t; viol.value = value;
                viol.detail[QStringLiteral("threshold")] = rule.value;
                viol.message = formatMessage(rule.message, {
                    {QStringLiteral("label"), item.label},
                    {QStringLiteral("value"), fmtNum(value)},
                    {QStringLiteral("threshold"), fmtNum(rule.value)},
                });
                out.append(viol);
                break;
            }
            case TsConditionType::ZScore: {
                const QVector<RawSample> win = buffer.rawWindow(i, now - rule.windowMs / 1000.0, now);
                if (win.size() < rule.minSamples)
                    continue;

                double sum = 0.0;
                for (const RawSample &s : win) sum += s.v;
                const double mean = sum / win.size();

                double sq = 0.0;
                for (const RawSample &s : win) sq += (s.v - mean) * (s.v - mean);
                const double stddev = std::sqrt(sq / win.size());
                if (stddev <= 0.0)
                    continue;   // perfectly flat window — no meaningful z-score

                const RawSample &latest = win.last();
                const double z = std::abs(latest.v - mean) / stddev;
                if (z <= rule.k || !gateAllows(rule.gate, latest.t, recentEvents))
                    continue;

                TsRuleViolation viol;
                viol.ruleId = rule.id; viol.severity = rule.severity;
                viol.itemId = item.id; viol.label = item.label;
                viol.t = latest.t; viol.value = latest.v;
                viol.detail[QStringLiteral("mean")]   = mean;
                viol.detail[QStringLiteral("stddev")] = stddev;
                viol.detail[QStringLiteral("z")]      = z;
                viol.message = formatMessage(rule.message, {
                    {QStringLiteral("label"), item.label},
                    {QStringLiteral("value"), fmtNum(latest.v)},
                    {QStringLiteral("mean"), fmtNum(mean)},
                    {QStringLiteral("z"), fmtNum(z)},
                });
                out.append(viol);
                break;
            }
            case TsConditionType::Drift: {
                const QVector<RawSample> win = buffer.rawWindow(i, now - rule.windowMs / 1000.0, now);
                if (win.size() < 2)
                    continue;

                double sumT = 0.0, sumV = 0.0;
                for (const RawSample &s : win) { sumT += s.t; sumV += s.v; }
                const double meanT = sumT / win.size(), meanV = sumV / win.size();

                double sxx = 0.0, syy = 0.0, sxy = 0.0;
                for (const RawSample &s : win) {
                    const double dt = s.t - meanT, dv = s.v - meanV;
                    sxx += dt * dt; syy += dv * dv; sxy += dt * dv;
                }
                if (sxx <= 0.0)
                    continue;   // all samples at the same timestamp — no slope defined

                const double slope = sxy / sxx;
                const double r2 = (syy > 0.0) ? (sxy * sxy) / (sxx * syy) : 0.0;

                const bool trendMatches = (rule.minSlopePerSec >= 0.0)
                    ? (slope >= rule.minSlopePerSec)
                    : (slope <= rule.minSlopePerSec);
                if (!trendMatches || r2 < rule.minR2)
                    continue;

                const RawSample &latest = win.last();
                if (!gateAllows(rule.gate, latest.t, recentEvents))
                    continue;

                TsRuleViolation viol;
                viol.ruleId = rule.id; viol.severity = rule.severity;
                viol.itemId = item.id; viol.label = item.label;
                viol.t = latest.t; viol.value = latest.v;
                viol.detail[QStringLiteral("slope")] = slope;
                viol.detail[QStringLiteral("r2")]     = r2;
                viol.message = formatMessage(rule.message, {
                    {QStringLiteral("label"), item.label},
                    {QStringLiteral("slope"), fmtNum(slope)},
                    {QStringLiteral("r2"), fmtNum(r2, 3)},
                    {QStringLiteral("windowSec"), fmtNum(rule.windowMs / 1000.0, 0)},
                });
                out.append(viol);
                break;
            }
            }
        }
    }
    return out;
}
