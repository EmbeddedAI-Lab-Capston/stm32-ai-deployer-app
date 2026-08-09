#include "TracePlot.h"

#include <QPainter>
#include <QPainterPath>
#include <QVariantMap>

#include <cmath>

TracePlot::TracePlot(QQuickItem *parent) : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
}

void TracePlot::setFrame(const QVariantList &f)
{
    m_frame = f;
    emit frameChanged();
    update();
}

void TracePlot::setEvents(const QVariantList &e)
{
    m_events = e;
    emit eventsChanged();
    update();
}

void TracePlot::setWindowStart(double v)
{
    if (m_windowStart == v) return;
    m_windowStart = v;
    emit rangeChanged();
    update();
}

void TracePlot::setWindowEnd(double v)
{
    if (m_windowEnd == v) return;
    m_windowEnd = v;
    emit rangeChanged();
    update();
}

void TracePlot::setLaneCount(int n)
{
    n = qMax(0, n);
    if (m_laneCount == n) return;
    m_laneCount = n;
    emit laneCountChanged();
    update();
}

void TracePlot::setCursorTime(double t)
{
    if (m_cursorTime == t) return;
    m_cursorTime = t;
    emit cursorChanged();
    update();
}

double TracePlot::laneHeight() const
{
    return m_laneCount > 0 ? height() / double(m_laneCount) : 0.0;
}

double TracePlot::timeAtX(double x) const
{
    if (width() <= 0 || m_windowEnd <= m_windowStart) return m_windowStart;
    return m_windowStart + (m_windowEnd - m_windowStart) * (x / width());
}

int TracePlot::laneAtY(double y) const
{
    const double lh = laneHeight();
    if (lh <= 0) return -1;
    const int lane = int(y / lh);
    return qBound(0, lane, qMax(0, m_laneCount - 1));
}

void TracePlot::paint(QPainter *painter)
{
    // Must never crash on an empty/unconfigured plot (plan Bolum 9.6 —
    // "TracePlot::paint bos frame ile cokmez; laneCount=0 ile cokmez").
    if (width() <= 0 || height() <= 0 || m_laneCount <= 0 || m_windowEnd <= m_windowStart)
        return;

    painter->setRenderHint(QPainter::Antialiasing, true);

    const double lh = laneHeight();

    painter->setPen(QPen(m_gridColor, 1));
    for (int i = 1; i < m_laneCount; ++i) {
        const double y = i * lh;
        painter->drawLine(QPointF(0, y), QPointF(width(), y));
    }

    for (int i = 0; i < m_laneCount; ++i)
        paintLane(painter, i, i * lh, lh);

    paintEvents(painter);
    paintCursor(painter);
}

void TracePlot::paintLane(QPainter *painter, int laneIndex, double top, double h)
{
    if (h <= 0)
        return;

    const double xScale = width() / (m_windowEnd - m_windowStart);

    for (const QVariant &sv : m_frame) {
        const QVariantMap s = sv.toMap();
        if (s.value(QStringLiteral("laneIndex"), -1).toInt() != laneIndex)
            continue;

        const QVariantList points = s.value(QStringLiteral("points")).toList();
        const double yMin = s.value(QStringLiteral("yMin")).toDouble();
        const double yMax = s.value(QStringLiteral("yMax")).toDouble();
        const double range = (yMax > yMin) ? (yMax - yMin) : 1.0;

        QColor color(s.value(QStringLiteral("color")).toString());
        if (!color.isValid())
            color = m_axisColor;
        const QColor fillColor(color.red(), color.green(), color.blue(), 70);

        auto yFor = [&](double v) { return top + h - ((v - yMin) / range) * h; };
        auto xFor = [&](double t) { return (t - m_windowStart) * xScale; };

        auto flushSegment = [&](const QVector<QPointF> &topPts, const QVector<QPointF> &botPts) {
            if (topPts.size() >= 2) {
                QPainterPath seg;
                seg.moveTo(topPts.first());
                for (int k = 1; k < topPts.size(); ++k) seg.lineTo(topPts.at(k));
                for (int k = botPts.size() - 1; k >= 0; --k) seg.lineTo(botPts.at(k));
                seg.closeSubpath();
                painter->fillPath(seg, fillColor);
                painter->setPen(QPen(color, 1.2));
                painter->drawPolyline(topPts.constData(), topPts.size());
            } else if (topPts.size() == 1) {
                painter->setPen(QPen(color, 2));
                painter->drawPoint(topPts.first());
            }
        };

        QVector<QPointF> topPts, botPts;
        const int n = points.size() / 3;
        for (int c = 0; c < n; ++c) {
            const double t    = points.at(c * 3 + 0).toDouble();
            const double vmin = points.at(c * 3 + 1).toDouble();
            const double vmax = points.at(c * 3 + 2).toDouble();
            if (std::isnan(vmin) || std::isnan(vmax)) {
                flushSegment(topPts, botPts);   // gap — draw what we have, start fresh after
                topPts.clear();
                botPts.clear();
                continue;
            }
            const double x = xFor(t);
            topPts.append(QPointF(x, yFor(vmax)));
            botPts.append(QPointF(x, yFor(vmin)));
        }
        flushSegment(topPts, botPts);
    }
}

void TracePlot::paintEvents(QPainter *painter)
{
    for (const QVariant &ev : m_events) {
        const QVariantMap e = ev.toMap();
        const double t = e.value(QStringLiteral("t")).toDouble();
        if (t < m_windowStart || t > m_windowEnd)
            continue;

        const QString kind     = e.value(QStringLiteral("kind")).toString();
        const QString severity = e.value(QStringLiteral("severity")).toString();
        const bool isReset = (kind == QStringLiteral("targetReset"));

        QColor c = m_axisColor;
        if (isReset)                                  c = QColor(0xD8, 0x8A, 0x2A);   // thick orange
        else if (severity == QStringLiteral("error"))   c = QColor(0xF0, 0x61, 0x6D);
        else if (severity == QStringLiteral("warning"))  c = QColor(0xD8, 0xA2, 0x3A);

        const double x = (t - m_windowStart) / (m_windowEnd - m_windowStart) * width();

        // Honesty rule (plan Bolum 9.5): UART-timestamped events are
        // approximate, drawn dashed. targetReset is the one exception —
        // drawn thick+solid because it is very likely the CAUSE of a jump
        // in the series, regardless of the same arrival-time slop.
        QPen pen(c, isReset ? 2.5 : 1.0);
        if (!isReset)
            pen.setStyle(Qt::DashLine);
        painter->setPen(pen);
        painter->drawLine(QPointF(x, 0), QPointF(x, height()));
    }
}

void TracePlot::paintCursor(QPainter *painter)
{
    if (m_cursorTime < 0.0 || m_cursorTime < m_windowStart || m_cursorTime > m_windowEnd)
        return;
    const double x = (m_cursorTime - m_windowStart) / (m_windowEnd - m_windowStart) * width();
    painter->setPen(QPen(QColor(255, 255, 255, 180), 1));
    painter->drawLine(QPointF(x, 0), QPointF(x, height()));
}
