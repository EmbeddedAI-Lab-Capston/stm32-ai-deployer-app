#include "HexDumpParser.h"

#include <QRegularExpression>

namespace HexDumpParser {

QHash<quint64, quint32> parse(const QStringList &lines)
{
    QHash<quint64, quint32> out;

    // "0x58024400 : 0307C025 40000710 00000150 20000089"  (1-4 words per line)
    static const QRegularExpression lineRe(
        QStringLiteral("^0x([0-9A-Fa-f]{8})\\s*:\\s*(.+)$"));
    static const QRegularExpression wordRe(QStringLiteral("^[0-9A-Fa-f]{8}$"));
    static const QRegularExpression wsRe(QStringLiteral("\\s+"));

    for (const QString &raw : lines) {
        const QString line = raw.trimmed();
        const QRegularExpressionMatch m = lineRe.match(line);
        if (!m.hasMatch())
            continue;   // banner / "  Address: : 0x..." header / blank

        bool ok = false;
        quint64 addr = m.captured(1).toULongLong(&ok, 16);
        if (!ok)
            continue;

        const QStringList words =
            m.captured(2).split(wsRe, Qt::SkipEmptyParts);
        for (const QString &w : words) {
            if (!wordRe.match(w).hasMatch())
                break;   // defensive: stop at first token that isn't a 32-bit word
            out.insert(addr, static_cast<quint32>(w.toULongLong(nullptr, 16)));
            addr += 4;
        }
    }
    return out;
}

QHash<quint64, quint32> parse(const QString &text)
{
    return parse(text.split(QLatin1Char('\n')));
}

QStringList errorMarkers(const QStringList &lines)
{
    static const QRegularExpression errRe(
        QStringLiteral("error|cannot|not readable|fail|can't get core"),
        QRegularExpression::CaseInsensitiveOption);
    QStringList out;
    for (const QString &raw : lines) {
        const QString line = raw.trimmed();
        if (line.isEmpty())
            continue;
        if (errRe.match(line).hasMatch())
            out << line;
    }
    return out;
}

} // namespace HexDumpParser
