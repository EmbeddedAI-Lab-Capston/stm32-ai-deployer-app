#include "TraceEventLog.h"

void TraceEventLog::reset()
{
    m_events.clear();
    m_clock.start();
}

double TraceEventLog::now() const
{
    return m_clock.isValid() ? double(m_clock.nsecsElapsed()) / 1.0e9 : 0.0;
}

void TraceEventLog::addEvent(const QString &kind, const QString &text, const QString &severity)
{
    if (!m_clock.isValid())
        m_clock.start();   // lazy start if reset() was never called explicitly

    TraceEvent e;
    e.t        = now();
    e.kind     = kind;
    e.text     = text;
    e.severity = severity;
    m_events.append(e);
}

QVector<TraceEvent> TraceEventLog::eventsBetween(double t0, double t1) const
{
    QVector<TraceEvent> out;
    for (const TraceEvent &e : m_events) {
        if (e.t < t0) continue;
        if (e.t > t1) break;   // m_events is time-sorted (always appended with now())
        out.append(e);
    }
    return out;
}

void TraceEventLog::clear()
{
    m_events.clear();
}
