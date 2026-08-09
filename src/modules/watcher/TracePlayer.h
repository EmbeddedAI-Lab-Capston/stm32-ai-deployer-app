#pragma once
#include "WatchModel.h"

#include <QList>
#include <QString>
#include <QVector>

// ── TracePlayer ───────────────────────────────────────────────────────────
// Pure (no QObject/QTimer): parses a TraceRecorder CSV file fully into
// memory (plan docs/variable_watcher_plan.md Bolum 10.1/10.3). The actual
// "play it back over time" driving (a virtual clock, speed multiplier,
// single-step) lives in VariableWatcher, which already owns the sample
// pipeline and TraceBuffer — this class only has to answer "what data is in
// this file", which keeps it directly unit-testable without a running
// event loop.
struct LoadedTraceItem
{
    QString        label;
    quint64        address = 0;
    WatchValueType type = WatchValueType::U32;
    DisplayFormat  format = DisplayFormat::Dec;
    double         scale = 1.0;
    double         offset = 0.0;
    QString        unit;
    QString        role;
};

struct LoadedTraceEvent
{
    double  t = 0.0;
    QString kind;
    QString text;
    QString severity;
};

class TracePlayer
{
public:
    bool load(const QString &path);
    QString lastError() const { return m_lastError; }
    // Non-fatal problems from a load() that still SUCCEEDED (e.g. dropped
    // malformed rows). Empty when the file parsed cleanly.
    QString loadWarning() const { return m_loadWarning; }
    int     skippedRows() const { return m_skippedRows; }

    QString board() const { return m_board; }
    QString elfPath() const { return m_elfPath; }
    QString model() const { return m_model; }
    QString started() const { return m_started; }
    int     targetRateHz() const { return m_targetRateHz; }
    double  actualHz() const { return m_actualHz; }
    double  durationS() const { return m_durationS; }

    const QList<LoadedTraceItem> &items() const { return m_items; }
    const QVector<double> &times() const { return m_times; }
    // series[itemIndex][k] — parallel to times()
    const QVector<QVector<double>> &series() const { return m_series; }
    const QVector<LoadedTraceEvent> &events() const { return m_events; }

private:
    QString m_lastError;
    QString m_loadWarning;
    int     m_skippedRows = 0;
    QString m_board, m_elfPath, m_model, m_started;
    int     m_targetRateHz = 0;
    double  m_actualHz = 0.0;
    double  m_durationS = 0.0;

    QList<LoadedTraceItem>   m_items;
    QVector<double>          m_times;
    QVector<QVector<double>> m_series;
    QVector<LoadedTraceEvent> m_events;
};
