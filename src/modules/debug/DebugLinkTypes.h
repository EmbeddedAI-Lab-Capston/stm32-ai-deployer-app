#pragma once

#include <QByteArray>
#include <QMetaType>
#include <QString>
#include <QVector>

// ── DebugLinkState ───────────────────────────────────────────────────────
enum class DebugLinkState {
    Closed, StartingServer, Connecting, Handshaking, Open, Failed
};

// One contiguous read request (worker executes them strictly serially).
struct MemoryRequest
{
    quint32 id   = 0;
    quint64 addr = 0;
    quint32 len  = 0;
};

struct MemoryReply
{
    quint32    id   = 0;
    quint64    addr = 0;
    QByteArray data;        // raw bytes, memory order
    bool       ok   = false;
    QString    error;       // e.g. "E01" or a transport message
};

// Column-major sample batch: series[i][k] is item i's value at times[k].
// One allocation per item per emit instead of one per sample.
//
// ── Timestamp contract (binding — see docs/variable_watcher_plan.md Bolum 4.2) ──
// times[k]: taken from the session monotonic clock immediately BEFORE the
//           FIRST 'm' packet of that sample is written to the socket.
// skewUs:   the span that starts at times[k] and ends when the LAST reply of
//           that same sample has been fully parsed, in microseconds.
// Meaning:  "these values were read within the window [t, t + skewUs/1e6]."
//           For single-round-trip samples skewUs is simply that read's RTT.
// This is why a sample with many read blocks is less simultaneous, not just
// slower — the UI reports skewUs so the user can see it.
struct WatchSampleBatch
{
    QVector<double>          times;      // see timestamp contract above
    QVector<QVector<double>> series;     // size == watch item count
    quint32 dropped      = 0;            // missed deadlines since last emit
    double  actualRateHz = 0.0;
    double  rttMsAvg     = 0.0;
    double  skewUs        = 0.0;         // t -> last reply parsed, per sample
    bool    coreRunning  = true;         // DHCSR S_RETIRE_ST==1 && S_HALT==0
    bool    targetReset  = false;        // DHCSR S_RESET_ST seen since last check
};

Q_DECLARE_METATYPE(MemoryRequest)
Q_DECLARE_METATYPE(MemoryReply)
Q_DECLARE_METATYPE(QVector<MemoryRequest>)
Q_DECLARE_METATYPE(QVector<MemoryReply>)
Q_DECLARE_METATYPE(WatchSampleBatch)
