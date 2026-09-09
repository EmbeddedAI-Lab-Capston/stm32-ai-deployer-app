#include "WatchPresetMatcher.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace {
bool findSymbolValue(const QString &name, const QList<Symbol> &symbols, quint64 &value)
{
    for (const Symbol &s : symbols) {
        if (s.name == name) {
            value = s.address;   // holds the VALUE when addressIsValue, else the address
            return true;
        }
    }
    return false;
}
}

QList<WatchPreset> WatchPresetMatcher::loadPresetsFromJson(const QByteArray &json, QString *errorOut)
{
    QList<WatchPreset> out;
    QJsonParseError perr;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &perr);
    if (perr.error != QJsonParseError::NoError) {
        if (errorOut) *errorOut = perr.errorString();
        return out;
    }

    const QJsonArray arr = doc.object().value(QStringLiteral("presets")).toArray();
    for (const QJsonValue &v : arr) {
        const QJsonObject o = v.toObject();
        WatchPreset preset;
        preset.id = o.value(QStringLiteral("id")).toString();
        if (preset.id.isEmpty())
            continue;   // malformed entry — skip silently

        preset.label  = o.value(QStringLiteral("label")).toString();
        preset.always = o.value(QStringLiteral("always")).toBool(false);
        for (const QJsonValue &rv : o.value(QStringLiteral("requiresAnySymbol")).toArray())
            preset.requiresAnySymbol << rv.toString();

        for (const QJsonValue &iv : o.value(QStringLiteral("items")).toArray()) {
            const QJsonObject io = iv.toObject();
            WatchPresetItem item;
            item.role   = io.value(QStringLiteral("role")).toString();
            item.unit   = io.value(QStringLiteral("unit")).toString();
            item.scale  = io.value(QStringLiteral("scale")).toDouble(1.0);
            item.type   = watchValueTypeFromString(io.value(QStringLiteral("type")).toString());
            item.format = displayFormatFromString(io.value(QStringLiteral("format")).toString());

            if (io.value(QStringLiteral("kind")).toString() == QStringLiteral("regionScan")) {
                item.isRegionScan = true;
                item.regionTo = io.value(QStringLiteral("regionTo")).toString();
                item.pattern  = io.value(QStringLiteral("pattern")).toString();
                item.rateHz   = io.value(QStringLiteral("rateHz")).toInt();
                const QJsonValue rf = io.value(QStringLiteral("regionFrom"));
                if (rf.isArray()) {
                    for (const QJsonValue &a : rf.toArray()) item.regionFromAlternatives << a.toString();
                } else if (rf.isString()) {
                    item.regionFromAlternatives << rf.toString();
                }
            } else {
                item.symbol = io.value(QStringLiteral("symbol")).toString();
            }
            preset.items.append(item);
        }
        out.append(preset);
    }
    return out;
}

QList<WatchPreset> WatchPresetMatcher::applicablePresets(const QList<WatchPreset> &presets,
                                                          const QList<Symbol> &symbols)
{
    QList<WatchPreset> out;
    for (const WatchPreset &p : presets) {
        if (p.always) {
            out.append(p);
            continue;
        }
        bool anyFound = false;
        for (const QString &req : p.requiresAnySymbol) {
            for (const Symbol &s : symbols) {
                if (s.name == req) { anyFound = true; break; }
            }
            if (anyFound) break;
        }
        if (anyFound)
            out.append(p);
    }
    return out;
}

bool WatchPresetMatcher::resolveAddressExpr(const QString &exprIn, const QList<Symbol> &symbols, quint64 &out)
{
    const QString expr = exprIn.trimmed();
    if (expr.isEmpty())
        return false;

    int opPos = -1;
    QChar opChar;
    for (int i = 1; i < expr.size(); ++i) {   // start at 1: a leading sign isn't a real operator here
        if (expr.at(i) == QLatin1Char('+') || expr.at(i) == QLatin1Char('-')) {
            opPos = i;
            opChar = expr.at(i);
            break;
        }
    }

    if (opPos < 0)
        return findSymbolValue(expr, symbols, out);

    quint64 lv = 0, rv = 0;
    if (!findSymbolValue(expr.left(opPos).trimmed(), symbols, lv))
        return false;
    if (!findSymbolValue(expr.mid(opPos + 1).trimmed(), symbols, rv))
        return false;
    out = (opChar == QLatin1Char('+')) ? (lv + rv) : (lv - rv);
    return true;
}

QList<WatchItem> WatchPresetMatcher::resolveSuggestions(const QList<WatchPreset> &presets,
                                                         const QList<Symbol> &symbols)
{
    QList<WatchItem> out;
    const QList<WatchPreset> applicable = applicablePresets(presets, symbols);

    for (const WatchPreset &preset : applicable) {
        for (const WatchPresetItem &pi : preset.items) {
            if (pi.isRegionScan) {
                quint64 fromAddr = 0;
                bool fromOk = false;
                for (const QString &alt : pi.regionFromAlternatives) {
                    if (resolveAddressExpr(alt, symbols, fromAddr)) { fromOk = true; break; }
                }
                quint64 toAddr = 0;
                const bool toOk = resolveAddressExpr(pi.regionTo, symbols, toAddr);
                if (!fromOk || !toOk || toAddr <= fromAddr)
                    continue;   // silently skipped — preset applies partially

                WatchItem item;
                item.label       = pi.role;
                item.role         = pi.role;
                item.address     = fromAddr;
                item.kind         = WatchItemKind::RegionScan;
                item.regionBytes = quint32(toAddr - fromAddr);
                item.unit         = pi.unit;
                item.source       = QStringLiteral("preset:%1").arg(preset.id);
                bool patternOk = false;
                const quint32 parsedPattern = pi.pattern.trimmed()
                    .mid(pi.pattern.trimmed().startsWith(QStringLiteral("0x"), Qt::CaseInsensitive) ? 2 : 0)
                    .toUInt(&patternOk, 16);
                if (patternOk) item.regionPattern = parsedPattern;
                out.append(item);
                continue;
            }

            const Symbol *sym = nullptr;
            for (const Symbol &s : symbols) {
                if (s.name == pi.symbol && !s.addressIsValue) { sym = &s; break; }
            }
            if (!sym)
                continue;   // symbol not present in this firmware — skip, not an error

            WatchItem item;
            item.label   = pi.symbol;
            item.role     = pi.role;
            item.address = sym->address;
            item.kind     = WatchItemKind::Scalar;
            item.type     = pi.type;
            item.format   = pi.format;
            item.scale   = pi.scale;
            item.unit     = pi.unit;
            item.source   = QStringLiteral("preset:%1").arg(preset.id);
            out.append(item);
        }
    }
    return out;
}
