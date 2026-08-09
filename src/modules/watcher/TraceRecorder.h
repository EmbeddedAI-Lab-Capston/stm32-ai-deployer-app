#pragma once
#include "WatchModel.h"
#include "modules/debug/DebugLinkTypes.h"

#include <QFile>
#include <QList>
#include <QString>
#include <QTextStream>
#include <QVector>

// ── TraceRecorder ─────────────────────────────────────────────────────────
// Streams live samples to a CSV trace file (plan docs/variable_watcher_plan.md
// Bolum 10.1/10.2), independent of TraceBuffer's memory-capped ring so a long
// recording is never silently truncated by ring eviction.
//
// actualHz and the event list can only be known once the session ends, so —
// unlike the plan's illustrative example, which shows them in the top
// header — this writes item/board/model metadata up front (start()), then
// streams data rows as batches arrive, then appends a trailing "# summary"
// comment plus one "# event" comment per collected event at stop(). This
// keeps the header truthful (no forward-guessed numbers) and keeps the data
// block contiguous (no comment lines interrupting the CSV table), which
// TracePlayer relies on. Uses Qt's own buffered QTextStream/QFile (no manual
// flush per sample — plan's "64 KB tamponlu, ornek basina flush yok").
class TraceRecorder
{
public:
    bool isRecording() const { return m_file.isOpen(); }
    QString lastError() const { return m_lastError; }
    QString path() const { return m_file.fileName(); }

    bool start(const QString &path, const QString &board, const QString &elfPath,
               const QString &model, int targetRateHz, const QList<WatchItem> &items);

    void appendBatch(const WatchSampleBatch &batch);
    void addEvent(double t, const QString &kind, const QString &text, const QString &severity);

    void stop();   // no-op if not currently recording

    quint64 sampleCount() const { return m_sampleCount; }

private:
    QFile        m_file;
    QTextStream  m_stream;
    QString      m_lastError;
    quint64      m_sampleCount = 0;
    double       m_firstT = 0.0;
    double       m_lastT  = 0.0;
    bool         m_haveFirst = false;

    struct PendingEvent { double t; QString kind, text, severity; };
    QVector<PendingEvent> m_events;
};
