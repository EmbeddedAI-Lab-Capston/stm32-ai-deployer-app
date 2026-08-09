#pragma once
#include "TimeSeriesRuleModel.h"
#include "TraceBuffer.h"
#include "TraceEventLog.h"
#include "WatchModel.h"

#include <QByteArray>
#include <QList>
#include <QVector>

// ── TimeSeriesRuleEngine ──────────────────────────────────────────────────
// Pure: deterministic, windowed evaluation of TsRule against TraceBuffer
// data (plan docs/variable_watcher_plan.md Bolum 11.3). No ML/anomaly
// detection — every violation carries the arithmetic that produced it
// (detail: mean/stddev/z/slope/r2) so it can be justified, not just
// asserted. Caller (Backend) supplies the buffer and recent events; this
// class touches neither a file nor a clock of its own.
class TimeSeriesRuleEngine
{
public:
    static QVector<TsRule> loadRulesFromJson(const QByteArray &json, QString *errorOut = nullptr);

    static QVector<TsRuleViolation> evaluate(const QVector<TsRule> &rules,
                                              const QList<WatchItem> &items,
                                              const TraceBuffer &buffer,
                                              double now,
                                              const QVector<TraceEvent> &recentEvents);
};
