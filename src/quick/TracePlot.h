#pragma once

#include <QColor>
#include <QQuickPaintedItem>
#include <QVariantList>
#include <qqmlintegration.h>

// ── TracePlot ─────────────────────────────────────────────────────────────
// Per-pixel-column min/max envelope plot (plan docs/variable_watcher_plan.md
// Bolum 9.2/9.3). Deliberately NOT Qt Charts: Charts' QGraphicsScene-based
// LineSeries becomes frame-time-unacceptable at a few thousand points and
// needs QPointF-list copies on every update. This paints directly from a
// pre-decimated "frame" (Backend::watchPlotFrame(), <=800 columns) built on
// the host side, so cost is bounded by pixel width, never by sample rate.
//
// Layout: one horizontal "lane" per distinct laneIndex present in frame[],
// full width, shared X (time) axis, independent Y auto-scale per lane
// (plan 9.3: mixing units like us/bytes/class-index on one Y axis is
// misleading). Two series sharing a laneIndex are superimposed.
class TracePlot : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT
    // frame: [{ id, label, color, unit, points: [t,min,max, t,min,max, ...],
    //           yMin, yMax, laneIndex }] — points may contain NaN
    //           min/max for a column with no samples (gap, not interpolated).
    Q_PROPERTY(QVariantList frame READ frame WRITE setFrame NOTIFY frameChanged)
    // events: [{ t, kind, text, severity }]
    Q_PROPERTY(QVariantList events READ events WRITE setEvents NOTIFY eventsChanged)
    Q_PROPERTY(double windowStart READ windowStart WRITE setWindowStart NOTIFY rangeChanged)
    Q_PROPERTY(double windowEnd   READ windowEnd   WRITE setWindowEnd   NOTIFY rangeChanged)
    Q_PROPERTY(int    laneCount   READ laneCount   WRITE setLaneCount   NOTIFY laneCountChanged)
    Q_PROPERTY(double cursorTime  READ cursorTime  WRITE setCursorTime  NOTIFY cursorChanged)
    Q_PROPERTY(QColor gridColor MEMBER m_gridColor NOTIFY styleChanged)
    Q_PROPERTY(QColor axisColor MEMBER m_axisColor NOTIFY styleChanged)

public:
    explicit TracePlot(QQuickItem *parent = nullptr);

    QVariantList frame() const { return m_frame; }
    void setFrame(const QVariantList &f);

    QVariantList events() const { return m_events; }
    void setEvents(const QVariantList &e);

    double windowStart() const { return m_windowStart; }
    void setWindowStart(double v);
    double windowEnd() const { return m_windowEnd; }
    void setWindowEnd(double v);

    int laneCount() const { return m_laneCount; }
    void setLaneCount(int n);

    double cursorTime() const { return m_cursorTime; }
    void setCursorTime(double t);

    void paint(QPainter *painter) override;

    Q_INVOKABLE double timeAtX(double x) const;
    Q_INVOKABLE int    laneAtY(double y) const;

signals:
    void frameChanged();
    void eventsChanged();
    void rangeChanged();
    void laneCountChanged();
    void cursorChanged();
    void styleChanged();

private:
    double laneHeight() const;
    void paintLane(QPainter *painter, int laneIndex, double top, double h);
    void paintEvents(QPainter *painter);
    void paintCursor(QPainter *painter);

    QVariantList m_frame;
    QVariantList m_events;
    double m_windowStart = 0.0;
    double m_windowEnd   = 1.0;
    int    m_laneCount   = 1;
    double m_cursorTime  = -1.0;   // < 0 = no cursor shown
    QColor m_gridColor { 0x2A, 0x31, 0x3C };
    QColor m_axisColor { 0x5B, 0x65, 0x73 };
};
