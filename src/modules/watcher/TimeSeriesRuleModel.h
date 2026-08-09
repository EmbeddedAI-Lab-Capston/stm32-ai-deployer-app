#pragma once

#include <QString>
#include <QVariantMap>

// ── TimeSeriesRuleModel ───────────────────────────────────────────────────
// Data model for the Faz 8 rule engine (plan docs/variable_watcher_plan.md
// Bolum 11.3). Deliberately NOT the same engine/schema as the existing
// src/modules/registers/RuleEngine.h (svd/rules.json) — that one judges a
// SINGLE snapshot's decoded register tree ("is the current state
// consistent?"); this one judges a WINDOW of time-series samples ("was
// recent behaviour normal?"). Different question, different data file
// (watch/watch_rules.json), never merged — see CLAUDE.md's ADR section.
enum class TsConditionType { Threshold, ZScore, Drift };

inline QString tsConditionTypeToString(TsConditionType t)
{
    switch (t) {
    case TsConditionType::Threshold: return QStringLiteral("threshold");
    case TsConditionType::ZScore:    return QStringLiteral("zscore");
    case TsConditionType::Drift:     return QStringLiteral("drift");
    }
    return QStringLiteral("threshold");
}

inline TsConditionType tsConditionTypeFromString(const QString &s)
{
    const QString v = s.trimmed().toLower();
    if (v == QStringLiteral("zscore")) return TsConditionType::ZScore;
    if (v == QStringLiteral("drift"))  return TsConditionType::Drift;
    return TsConditionType::Threshold;
}

// Optional event correlation gate: a violation that would otherwise fire is
// suppressed unless a matching event occurred within withinMs of it (plan
// 11.3) — e.g. only flag an inference-time outlier if it's actually
// adjacent to an "inference" event, not a stale/lagging read.
struct TsGate
{
    QString eventKind;
    int     withinMs = 0;
    bool    isValid() const { return !eventKind.isEmpty() && withinMs > 0; }
};

struct TsRule
{
    // false = parsed but never evaluated. Lets watch_rules.json ship a rule
    // whose preconditions do not exist yet without pretending it can fire.
    bool    enabled = true;

    QString id, severity, message;
    QString appliesToRole;          // matches WatchItem::role
    QString appliesToLabelRegex;    // or matches WatchItem::label
    int     windowMs = 0;           // ZScore/Drift window
    TsConditionType type = TsConditionType::Threshold;
    QString op;                     // "<" ">" "<=" ">=" "==" "!=" — Threshold only
    double  value = 0;              // Threshold constant
    double  k = 4.0;                // ZScore multiplier
    double  minSlopePerSec = 0;     // Drift — signed; negative means "falling faster than"
    double  minR2 = 0.6;            // Drift — rejects noise fit as a fake trend
    int     minSamples = 50;        // ZScore minimum window population
    int     sustainMs = 0;          // Threshold — 0 = check latest sample only
    TsGate  gate;
};

struct TsRuleViolation
{
    QString ruleId, severity, itemId, label, message;
    double  t = 0;         // when the violation was observed
    double  value = 0;
    QVariantMap detail;    // mean/stddev/z/slope/r2/threshold — the "why", for the UI
};
