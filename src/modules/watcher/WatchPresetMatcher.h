#pragma once
#include "SymbolModel.h"
#include "WatchModel.h"

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

// ── WatchPresetMatcher ────────────────────────────────────────────────────
// Pure: matches watch/watch_presets.json against a loaded ELF's symbol table
// to produce ready-to-add WatchItem suggestions (plan
// docs/variable_watcher_plan.md Bolum 11.5). A missing symbol silently
// drops just that one item — the preset still applies partially, never an
// error. No board name appears in the JSON; adding a new board is purely a
// .svd + boards.json + linker template change, this file is untouched.
struct WatchPresetItem
{
    QString        role;
    QString        label;           // optional; empty = symbol name (+offset when non-zero)
    QString        symbol;          // scalar items
    WatchValueType type = WatchValueType::U32;
    DisplayFormat  format = DisplayFormat::Dec;
    double         scale = 1.0;
    QString        unit;

    // Byte offset added to `symbol`'s address — lets one preset item point
    // into a field of a struct symbol (e.g. g_telemetry.sensor[0]) instead
    // of only at a symbol's own start.
    qint64         offsetBytes = 0;

    // Seqlock guard words: "guardBegin"/"guardEnd" JSON objects, each
    // {"symbol","offset_bytes"}. Both must resolve for the produced
    // WatchItem to carry guardBeginAddr/guardEndAddr (see WatchModel.h).
    bool           hasGuardBegin = false;
    QString        guardBeginSymbol;
    qint64         guardBeginOffsetBytes = 0;
    bool           hasGuardEnd = false;
    QString        guardEndSymbol;
    qint64         guardEndOffsetBytes = 0;

    bool           isRegionScan = false;
    QStringList    regionFromAlternatives;   // tried in order; first that resolves wins
    QString        regionTo;
    QString        pattern;                  // watermark fill pattern, e.g. "0xA5A5A5A5"
    int            rateHz = 0;
};

// Renames items another preset produced, by role. Lets a sensor-specific
// preset (gated on e.g. the BME280 driver's symbol) say what the generic
// g_telemetry.sensor[N] slots mean, without the generic preset — or any C++ —
// knowing which sensor is fitted.
struct WatchPresetRelabel
{
    QString role;
    QString label;
    bool    hasUnit = false;
    QString unit;
};

struct WatchPreset
{
    QString id, label;
    bool    always = false;
    QStringList requiresAnySymbol;
    QList<WatchPresetItem> items;
    QList<WatchPresetRelabel> relabels;
};

class WatchPresetMatcher
{
public:
    static QList<WatchPreset> loadPresetsFromJson(const QByteArray &json, QString *errorOut = nullptr);

    // Which presets would apply given the loaded symbol table (always==true,
    // or at least one of requiresAnySymbol present) — a preview before
    // actually resolving/adding anything (plan's watchPresetSuggestions()).
    static QList<WatchPreset> applicablePresets(const QList<WatchPreset> &presets,
                                                 const QList<Symbol> &symbols);

    // Resolves every item of every applicable preset against symbols into
    // concrete WatchItem suggestions (address filled in; id left empty —
    // the caller assigns a fresh one only for items it actually adds).
    // Items whose symbol can't be found are skipped, not errors. Relabels of
    // every applicable preset are applied last, so their order in the JSON
    // relative to the presets that produce the items does not matter.
    static QList<WatchItem> resolveSuggestions(const QList<WatchPreset> &presets,
                                                const QList<Symbol> &symbols);

    // Drops suggestions already on the watch list (same address and kind), so
    // applying presets twice does not double every row - and with it every
    // rule violation and the sampling cost of a region scan.
    static QList<WatchItem> withoutAlreadyWatched(const QList<WatchItem> &suggestions,
                                                  const QList<WatchItem> &existing);

    // "<sym>", "<sym>-<sym2>", or "<sym>+<sym2>" — resolves each operand via
    // the symbol table (using its VALUE if addressIsValue, else its
    // address), applies +/-, and returns false if any operand is missing.
    // Intentionally narrow — not a general expression evaluator (plan 11.5).
    static bool resolveAddressExpr(const QString &expr, const QList<Symbol> &symbols, quint64 &out);
};
