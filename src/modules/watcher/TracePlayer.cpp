#include "TracePlayer.h"

#include <QFile>
#include <QMap>
#include <QTextStream>

namespace {

// Pulls "key=value" pairs out of a line where values may contain spaces
// (e.g. Windows paths) — splits on the position of each KNOWN key marker
// rather than blindly on whitespace, so "elf=C:/a b/x.elf model=y" parses
// correctly even though the elf path has a space in it.
QMap<QString, QString> parseSpacedKv(const QString &line, const QStringList &keysInOrder)
{
    QMap<QString, QString> out;
    QVector<int> pos(keysInOrder.size(), -1);
    for (int i = 0; i < keysInOrder.size(); ++i)
        pos[i] = line.indexOf(keysInOrder.at(i) + QStringLiteral("="));

    for (int i = 0; i < keysInOrder.size(); ++i) {
        if (pos[i] < 0) continue;
        const int valueStart = pos[i] + keysInOrder.at(i).size() + 1;
        int valueEnd = line.size();
        for (int j = 0; j < keysInOrder.size(); ++j) {
            if (j != i && pos[j] > pos[i] && pos[j] < valueEnd)
                valueEnd = pos[j];
        }
        out[keysInOrder.at(i)] = line.mid(valueStart, valueEnd - valueStart).trimmed();
    }
    return out;
}

// "key=value,key2=value2,..." — used for the trailing "# summary" line.
QMap<QString, QString> parseCommaKv(const QString &line)
{
    QMap<QString, QString> out;
    const QStringList parts = line.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (const QString &p : parts) {
        const int eq = p.indexOf(QLatin1Char('='));
        if (eq > 0)
            out[p.left(eq).trimmed()] = p.mid(eq + 1).trimmed();
    }
    return out;
}

// Splits a "# event,<t>,<kind>,"<text>",<severity>" line, respecting the
// one quoted field (which may contain escaped "" and, unlike the other
// comma-split helpers here, may itself contain commas).
QStringList splitEventLine(const QString &rest)
{
    QStringList out;
    QString cur;
    bool inQuotes = false;
    for (int i = 0; i < rest.size(); ++i) {
        const QChar c = rest.at(i);
        if (inQuotes) {
            if (c == QLatin1Char('"')) {
                if (i + 1 < rest.size() && rest.at(i + 1) == QLatin1Char('"')) {
                    cur += QLatin1Char('"');
                    ++i;
                } else {
                    inQuotes = false;
                }
            } else {
                cur += c;
            }
        } else if (c == QLatin1Char('"')) {
            inQuotes = true;
        } else if (c == QLatin1Char(',')) {
            out << cur;
            cur.clear();
        } else {
            cur += c;
        }
    }
    out << cur;
    return out;
}

}

bool TracePlayer::load(const QString &path)
{
    m_lastError.clear();
    m_loadWarning.clear();
    m_skippedRows = 0;
    m_board.clear(); m_elfPath.clear(); m_model.clear(); m_started.clear();
    m_targetRateHz = 0;
    m_actualHz = 0.0;
    m_durationS = 0.0;
    m_items.clear();
    m_times.clear();
    m_series.clear();
    m_events.clear();

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_lastError = file.errorString();
        return false;
    }

    QTextStream in(&file);
    int lineNo = 0;
    int malformedRows = 0;

    while (!in.atEnd()) {
        const QString line = in.readLine();
        ++lineNo;
        if (line.isEmpty())
            continue;

        if (line.startsWith(QStringLiteral("# board="))) {
            const auto kv = parseSpacedKv(line, {"board", "elf", "model", "started"});
            m_board   = kv.value(QStringLiteral("board"));
            m_elfPath = kv.value(QStringLiteral("elf"));
            m_model   = kv.value(QStringLiteral("model"));
            m_started = kv.value(QStringLiteral("started"));
            continue;
        }
        if (line.startsWith(QStringLiteral("# targetHz="))) {
            const auto kv = parseSpacedKv(line, {"targetHz", "items"});
            m_targetRateHz = kv.value(QStringLiteral("targetHz")).toInt();
            continue;
        }
        if (line.startsWith(QStringLiteral("# item,"))) {
            const QStringList f = line.mid(2).split(QLatin1Char(','));   // drop "# "
            if (f.size() < 9) { ++malformedRows; continue; }
            LoadedTraceItem it;
            it.label   = f.at(2);
            bool addrOk = false;
            it.address = f.at(3).trimmed().toULongLong(&addrOk, 16 /* "0x..." handled by Qt with base 0 too, but be explicit */);
            if (!addrOk) {
                // toULongLong with base 16 chokes on the "0x" prefix on some Qt
                // versions unless base 0 is used — retry that way.
                it.address = f.at(3).trimmed().toULongLong(&addrOk, 0);
            }
            it.type    = watchValueTypeFromString(f.at(4));
            it.format  = displayFormatFromString(f.at(5));
            // An unparseable/blank scale column must fall back to 1.0, not to
            // toDouble()'s 0.0 — a zero scale silently flattens the whole
            // replayed series to the offset. (Same default as
            // VariableWatcher::loadItems().)
            bool scaleOk = false;
            const double parsedScale = f.at(6).trimmed().toDouble(&scaleOk);
            it.scale   = scaleOk ? parsedScale : 1.0;
            it.offset  = f.at(7).trimmed().toDouble();
            it.unit    = f.at(8);
            it.role    = f.size() > 9 ? f.at(9) : QString();
            m_items.append(it);
            continue;
        }
        if (line.startsWith(QStringLiteral("# summary,"))) {
            const auto kv = parseCommaKv(line.mid(2));
            m_actualHz  = kv.value(QStringLiteral("actualHz")).toDouble();
            m_durationS = kv.value(QStringLiteral("durationS")).toDouble();
            continue;
        }
        if (line.startsWith(QStringLiteral("# event,"))) {
            const QStringList f = splitEventLine(line.mid(QStringLiteral("# event,").size()));
            if (f.size() < 4) { ++malformedRows; continue; }
            LoadedTraceEvent e;
            e.t        = f.at(0).toDouble();
            e.kind     = f.at(1);
            e.text     = f.at(2);
            e.severity = f.at(3);
            m_events.append(e);
            continue;
        }
        if (line.startsWith(QLatin1Char('#')))
            continue;   // unknown/forward-compatible comment — skip
        if (line.startsWith(QStringLiteral("t,")) || line == QStringLiteral("t"))
            continue;   // column header row, purely informational

        // Data row: t,v0,v1,...
        const QStringList f = line.split(QLatin1Char(','));
        if (f.size() < 1) { ++malformedRows; continue; }
        bool tOk = false;
        const double t = f.at(0).toDouble(&tOk);
        if (!tOk) { ++malformedRows; continue; }

        if (m_series.size() != m_items.size())
            m_series.resize(m_items.size());
        m_times.append(t);
        for (int i = 0; i < m_items.size(); ++i) {
            const double v = (i + 1 < f.size()) ? f.at(i + 1).toDouble() : 0.0;
            m_series[i].append(v);
        }
    }

    if (m_items.isEmpty() && m_times.isEmpty()) {
        m_lastError = QStringLiteral("Gecerli bir izleme kaydi bulunamadi (bos veya bozuk dosya)");
        return false;
    }
    // NOT m_lastError: the load SUCCEEDED. Callers check lastError() only on
    // failure, so putting it there meant the user was never told that rows had
    // been dropped. Surfaced separately instead.
    m_skippedRows = malformedRows;
    if (malformedRows > 0)
        m_loadWarning = QStringLiteral("%1 satir bozuk oldugu icin atlandi").arg(malformedRows);

    return true;
}
