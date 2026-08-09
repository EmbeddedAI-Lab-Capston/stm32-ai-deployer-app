#pragma once
#include <QElapsedTimer>
#include <QString>
#include <QVector>

// ── TraceEventLog ─────────────────────────────────────────────────────────
// Pure (no QObject): common timeline for events that correlate with the
// plotted trace (plan docs/variable_watcher_plan.md Bolum 9.5) — inference
// packets, sys/boot/error UART lines, register snapshots, target resets,
// rule violations (Faz 8). All timestamped in seconds against ONE monotonic
// clock shared with the session (reset() called when the watch link opens),
// so event markers line up with TraceBuffer sample times on the same axis.
//
// Honesty rule (plan Bolum 9.5): UART-sourced events are stamped on ARRIVAL
// at the main thread, not with a firmware timestamp — they can lag the real
// occurrence by a few ms (USB-CDC + DMA). Callers must render these as
// approximate (dashed), not measured.
struct TraceEvent
{
    double  t = 0.0;
    QString kind;       // "inference" | "sys" | "boot" | "uartError" | "snapshot" |
                         // "ruleViolation" | "flash" | "note" | "targetReset"
    QString text;
    QString severity = QStringLiteral("info");   // "info" | "warning" | "error"
};

class TraceEventLog
{
public:
    // Clears all events and restarts the session clock. Call when the watch
    // link opens so event times share the same origin as sample times.
    //
    // originS MATTERS: sample timestamps are measured against DebugLinkWorker's
    // clock, which starts at socket-connect — earlier than link-open by the
    // whole handshake (measured: ~61 ms on a NUCLEO-H723ZG). Starting this log
    // at 0 therefore put events on an axis shifted by that amount, which was
    // enough on its own to make the ±50 ms "inference" gate in
    // watch/watch_rules.json reject every genuine correlation. Pass
    // DebugLink::sessionElapsedAtOpen() so both axes share one origin.
    void reset(double originS = 0.0);

    double now() const;   // seconds on the shared session axis; 0 if never reset
    void addEvent(const QString &kind, const QString &text,
                  const QString &severity = QStringLiteral("info"));

    // Events with t in [t0, t1], oldest first (insertion order is already
    // time-sorted since addEvent() always stamps with the current now()).
    QVector<TraceEvent> eventsBetween(double t0, double t1) const;

    void clear();
    int  count() const { return m_events.size(); }

    // Hard cap on retained events. UART-sourced events arrive per inference
    // packet, so an unbounded list grows without limit over a long session
    // while eventsBetween() scans it linearly at plot rate. Oldest events are
    // dropped once the cap is hit.
    static constexpr int kMaxEvents = 20000;

private:
    QElapsedTimer        m_clock;
    double               m_originS = 0.0;
    QVector<TraceEvent>  m_events;
};
