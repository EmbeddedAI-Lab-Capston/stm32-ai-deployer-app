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

        QStringList cells;
        cells << session.model
              << session.board
              << it.label
              << QString::number(st.count)
              << QString::number(actualHz, 'f', 2)
              << QString::number(st.min, 'g', 10)
              << QString::number(st.max, 'g', 10)
              << QString::number(st.mean, 'g', 10)
              << QString::number(st.stddev(), 'g', 10)
              << QString::number(st.last, 'g', 10)
              << QString::number(durationS, 'f', 3)
              << it.unit
              << it.role
              << session.rawSeriesPath
              << session.note;
        rows.append(cells);
    }
    return rows;
}
