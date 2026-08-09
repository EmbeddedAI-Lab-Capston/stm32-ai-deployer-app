#include "WatchProfile.h"

QList<QStringList> WatchProfile::buildRows(const WatchProfileInput &session,
                                            const QList<WatchItem> &items,
                                            const QList<WatchStats> &statsPerItem,
                                            double actualHz, double durationS)
{
    QList<QStringList> rows;
    const int n = qMin(items.size(), statsPerItem.size());
    rows.reserve(n);

    for (int i = 0; i < n; ++i) {
        const WatchItem &it = items.at(i);
        const WatchStats &st = statsPerItem.at(i);

        // WatchStats accumulates RAW decoded values (WatchSampler never
        // applies scale/offset — that's a display-time transform, see
        // ValueCodec::format). Apply it here so the stored number actually
        // matches the unit stored alongside it in c11 (e.g. "ms" really
        // means the value is in ms, not raw microseconds).
        const auto scaled = [&](double raw) { return raw * it.scale + it.offset; };

        QStringList cells;
        cells << session.model
              << session.board
              << it.label
              << QString::number(st.count)
              << QString::number(actualHz, 'f', 2)
              << QString::number(scaled(st.min), 'g', 10)
              << QString::number(scaled(st.max), 'g', 10)
              << QString::number(scaled(st.mean), 'g', 10)
              << QString::number(st.stddev() * qAbs(it.scale), 'g', 10)
              << QString::number(scaled(st.last), 'g', 10)
              << QString::number(durationS, 'f', 3)
              << it.unit
              << it.role
              << session.rawSeriesPath
              << session.note;
        rows.append(cells);
    }
    return rows;
}
