#include "NmSymbolParser.h"

#include <QRegularExpression>
#include <QStringList>

namespace {

SymbolKind kindForType(char t)
{
    switch (QChar(QLatin1Char(t)).toUpper().toLatin1()) {
    case 'B': return SymbolKind::Bss;
    case 'D': return SymbolKind::Data;
    case 'R': return SymbolKind::ReadOnly;
    case 'T': return SymbolKind::Code;
    case 'W':
    case 'V': return SymbolKind::Weak;
    case 'A': return SymbolKind::Absolute;
    case 'C': return SymbolKind::Common;
    default:  return SymbolKind::Other;
    }
}

}

QList<Symbol> NmSymbolParser::parse(const QString &nmOutput)
{
    static const QRegularExpression ws(QStringLiteral("\\s+"));

    QList<Symbol> out;
    const QStringList lines = nmOutput.split(QLatin1Char('\n'), Qt::SkipEmptyParts);

    for (const QString &rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty())
            continue;

        const QStringList fields = line.split(ws, Qt::SkipEmptyParts);
        if (fields.size() < 3)
            continue;   // header/warning/anything not shaped like a symbol line

        bool addrOk = false;
        const quint64 addr = fields.at(0).toULongLong(&addrOk, 16);
        if (!addrOk)
            continue;

        Symbol sym;
        sym.address = addr;

        if (fields.size() >= 4 && fields.at(2).size() == 1) {
            // "<addr> <size> <type> <name>"
            bool sizeOk = false;
            const quint64 size = fields.at(1).toULongLong(&sizeOk, 16);
            sym.size    = sizeOk ? size : 0;
            sym.hasSize = sizeOk;
            sym.nmType  = fields.at(2).at(0).toLatin1();
            sym.name    = fields.mid(3).join(QLatin1Char(' '));
        } else if (fields.at(1).size() == 1) {
            // "<addr> <type> <name>" — nm gave no size
            sym.hasSize = false;
            sym.nmType  = fields.at(1).at(0).toLatin1();
            sym.name    = fields.mid(2).join(QLatin1Char(' '));
        } else {
            continue;   // neither shape matched — not a symbol line
        }

        if (sym.name.isEmpty())
            continue;

        sym.kind           = kindForType(sym.nmType);
        sym.addressIsValue = (sym.kind == SymbolKind::Absolute);
        out.append(sym);
    }

    return out;
}
