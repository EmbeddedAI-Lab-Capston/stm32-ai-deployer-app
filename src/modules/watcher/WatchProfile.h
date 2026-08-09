#pragma once
#include "WatchModel.h"

#include <QList>
#include <QString>
#include <QStringList>

// ── WatchProfile ──────────────────────────────────────────────────────────
// Pure: builds analysis_records rows for a finished watch session (plan
// docs/variable_watcher_plan.md Bolum 10.4) — one row per item, kind =
// "watch_profile". The raw per-sample series is NEVER written to the DB
// (1s-resolution summary only); Backend owns the actual
// AnalysisManager::addRecord() call, this class only knows the column
// mapping so it's independently testable.
struct WatchProfileInput
{
    QString model;
    QString board;
    QString rawSeriesPath;   // path to the CSV recording, or empty if not recorded
    QString note;
};

class WatchProfile
{
public:
    // c0=model c1=board c2=item label c3=sample count c4=actual Hz c5=min
    // c6=max c7=mean c8=stddev c9=last c10=duration(s) c11=unit c12=role
    // c13=raw series path c14=note
    static QList<QStringList> buildRows(const WatchProfileInput &session,
                                         const QList<WatchItem> &items,
                                         const QList<WatchStats> &statsPerItem,
                                         double actualHz, double durationS);
};
