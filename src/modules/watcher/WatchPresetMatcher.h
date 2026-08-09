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
    QString        symbol;          // scalar items
    WatchValueType type = WatchValueType::U32;
    DisplayFormat  format = DisplayFormat::Dec;
    double         scale = 1.0;
    QString        unit;

    bool           isRegionScan = false;
    QStringList    regionFromAlternatives;   // tried in order; first that resolves wins
    QString        regionTo;
    QString        pattern;                  // watermark fill pattern, e.g. "0xA5A5A5A5"
    int            rateHz = 0;
};

struct WatchPreset
{
    QString id, label;
    bool    always = false;
    QStringList requiresAnySymbol;
    QList<WatchPresetItem> items;
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
    // Items whose symbol can't be found are skipped, not errors.
    static QList<WatchItem> resolveSuggestions(const QList<WatchPreset> &presets,
                                                const QList<Symbol> &symbols);

    // "<sym>", "<sym>-<sym2>", or "<sym>+<sym2>" — resolves each operand via
    // the symbol table (using its VALUE if addressIsValue, else its
    // address), applies +/-, and returns false if any operand is missing.
    // Intentionally narrow — not a general expression evaluator (plan 11.5).
    static bool resolveAddressExpr(const QString &expr, const QList<Symbol> &symbols, quint64 &out);
};
