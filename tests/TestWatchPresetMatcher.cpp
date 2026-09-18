#include "TestWatchPresetMatcher.h"
#include "modules/watcher/WatchPresetMatcher.h"

#include <QFile>
#include <QTest>

namespace {
Symbol makeSymbol(const QString &name, quint64 address, bool addressIsValue = false)
{
    Symbol s;
    s.name = name;
    s.address = address;
    s.hasSize = true;
    s.size = 4;
    s.kind = addressIsValue ? SymbolKind::Absolute : SymbolKind::Bss;
    s.addressIsValue = addressIsValue;
    return s;
}
}

void TestWatchPresetMatcher::alwaysPresetAppliesWithoutAnyRequiredSymbol()
{
    WatchPreset preset;
    preset.id = "hal_timebase";
    preset.always = true;
    WatchPresetItem item;
    item.role = "hwTick";
    item.symbol = "uwTick";
    item.unit = "ms";
    preset.items << item;

    const QList<Symbol> symbols = { makeSymbol("uwTick", 0x20000010) };
    const QList<WatchItem> out = WatchPresetMatcher::resolveSuggestions({preset}, symbols);

    QCOMPARE(out.size(), 1);
    QCOMPARE(out.first().label, QStringLiteral("uwTick"));
    QCOMPARE(out.first().role, QStringLiteral("hwTick"));
    QCOMPARE(out.first().address, quint64(0x20000010));
}

void TestWatchPresetMatcher::requiresAnySymbolGatesPresetApplicability()
{
    WatchPreset preset;
    preset.id = "xcubeai_runtime";
    preset.requiresAnySymbol = { "s_network", "net_exec_ctx" };
    WatchPresetItem item;
    item.role = "inferenceUs";
    item.symbol = "g_ai_last_inference_us";
    preset.items << item;

    const QList<Symbol> withoutAny = { makeSymbol("unrelated_symbol", 0x1000) };
    QVERIFY(WatchPresetMatcher::applicablePresets({preset}, withoutAny).isEmpty());
    QVERIFY(WatchPresetMatcher::resolveSuggestions({preset}, withoutAny).isEmpty());

    const QList<Symbol> withOne = {
        makeSymbol("net_exec_ctx", 0x2000),
        makeSymbol("g_ai_last_inference_us", 0x24000127),
    };
    const QList<WatchItem> out = WatchPresetMatcher::resolveSuggestions({preset}, withOne);
    QCOMPARE(out.size(), 1);
    QCOMPARE(out.first().address, quint64(0x24000127));
}

void TestWatchPresetMatcher::missingSymbolIsSkippedNotAnError()
{
    WatchPreset preset;
    preset.id = "core_memory";
    preset.always = true;
    WatchPresetItem heapEnd; heapEnd.role = "heapEnd"; heapEnd.symbol = "__sbrk_heap_end";
    WatchPresetItem bssEnd;  bssEnd.role  = "bssEnd";  bssEnd.symbol  = "_end";
    preset.items << heapEnd << bssEnd;

    // Only _end is present in this firmware's symbol table.
    const QList<Symbol> symbols = { makeSymbol("_end", 0x20009000) };
    const QList<WatchItem> out = WatchPresetMatcher::resolveSuggestions({preset}, symbols);

    QCOMPARE(out.size(), 1);
    QCOMPARE(out.first().label, QStringLiteral("_end"));
}

void TestWatchPresetMatcher::regionScanFallsBackToSecondAlternativeWhenFirstMissing()
{
    WatchPreset preset;
    preset.id = "core_memory";
    preset.always = true;
    WatchPresetItem region;
    region.role = "stackWatermark";
    region.isRegionScan = true;
    region.regionFromAlternatives = { "_sstack", "_estack-_Min_Stack_Size" };
    region.regionTo = "_estack";
    preset.items << region;

    // _sstack absent (F4/H7-style linker script); _estack + _Min_Stack_Size present.
    const QList<Symbol> symbols = {
        makeSymbol("_estack", 0x20020000),
        makeSymbol("_Min_Stack_Size", 2048, /*addressIsValue=*/true),
    };
    const QList<WatchItem> out = WatchPresetMatcher::resolveSuggestions({preset}, symbols);

    QCOMPARE(out.size(), 1);
    QCOMPARE(out.first().kind, WatchItemKind::RegionScan);
    QCOMPARE(out.first().address, quint64(0x20020000 - 2048));
    QCOMPARE(out.first().regionBytes, quint32(2048));
}

void TestWatchPresetMatcher::regionScanUsesAbsoluteSymbolAsValueNotAddress()
{
    const QList<Symbol> symbols = {
        makeSymbol("_estack", 0x20020000),
        makeSymbol("_Min_Stack_Size", 2048, /*addressIsValue=*/true),
    };
    quint64 result = 0;
    QVERIFY(WatchPresetMatcher::resolveAddressExpr("_estack-_Min_Stack_Size", symbols, result));
    // If _Min_Stack_Size's raw nm address (not its value) were used by mistake,
    // this would be wildly wrong (nm addresses for Absolute symbols are
    // arbitrary) — asserting the exact expected numeric result pins down
    // that the VALUE (2048) was used, per plan 11.5's stated trap.
    QCOMPARE(result, quint64(0x20020000 - 2048));
}

void TestWatchPresetMatcher::regionScanSkippedWhenNoAlternativeResolves()
{
    WatchPreset preset;
    preset.id = "core_memory";
    preset.always = true;
    WatchPresetItem region;
    region.role = "stackWatermark";
    region.isRegionScan = true;
    region.regionFromAlternatives = { "_sstack", "_estack-_Min_Stack_Size" };
    region.regionTo = "_estack";
    preset.items << region;

    // Neither _sstack nor _Min_Stack_Size present -> both alternatives fail.
    const QList<Symbol> symbols = { makeSymbol("_estack", 0x20020000) };
    const QList<WatchItem> out = WatchPresetMatcher::resolveSuggestions({preset}, symbols);
    QVERIFY(out.isEmpty());
}

// offset_bytes lets a preset item point into a field of a struct symbol
// (e.g. g_telemetry.sensor[0]) rather than only at the symbol's own start.
void TestWatchPresetMatcher::offsetBytesIsAddedToSymbolAddress()
{
    WatchPreset preset;
    preset.id = "sensor_memory";
    preset.always = true;
    WatchPresetItem item;
    item.role = "sensor0";
    item.symbol = "g_telemetry";
    item.offsetBytes = 4;
    preset.items << item;

    const QList<Symbol> symbols = { makeSymbol("g_telemetry", 0x24000100) };
    const QList<WatchItem> out = WatchPresetMatcher::resolveSuggestions({preset}, symbols);

    QCOMPARE(out.size(), 1);
    QCOMPARE(out.first().address, quint64(0x24000104));
}

// guardBegin/guardEnd resolve against the symbol table the same way the main
// symbol does, landing in WatchItem::guardBeginAddr/guardEndAddr so
// WatchPlanBuilder can fold them into the item's seqlock-guarded read.
void TestWatchPresetMatcher::guardSymbolsResolveIntoWatchItemGuardAddresses()
{
    WatchPreset preset;
    preset.id = "sensor_memory";
    preset.always = true;
    WatchPresetItem item;
    item.role = "sensor0";
    item.symbol = "g_telemetry";
    item.offsetBytes = 4;
    item.hasGuardBegin = true;
    item.guardBeginSymbol = "g_telemetry";
    item.guardBeginOffsetBytes = 0;
    item.hasGuardEnd = true;
    item.guardEndSymbol = "g_telemetry";
    item.guardEndOffsetBytes = 88;
    preset.items << item;

    const QList<Symbol> symbols = { makeSymbol("g_telemetry", 0x24000100) };
    const QList<WatchItem> out = WatchPresetMatcher::resolveSuggestions({preset}, symbols);

    QCOMPARE(out.size(), 1);
    QCOMPARE(out.first().guardBeginAddr, quint64(0x24000100));
    QCOMPARE(out.first().guardEndAddr, quint64(0x24000100 + 88));
}

// Several items reading fields of one struct symbol used to all be labelled
// with the bare symbol name, so the item table showed three identical
// "g_telemetry" rows. An explicit JSON label wins; otherwise the offset is
// appended so the rows can still be told apart.
void TestWatchPresetMatcher::itemsIntoOneStructAreLabelledApart()
{
    const QByteArray json = R"({"presets":[{"id":"sensor_memory","always":true,"items":[
        {"role":"sensor0","label":"g_telemetry.sensor[0]","symbol":"g_telemetry","offset_bytes":4,"type":"f32"},
        {"role":"sensor1","symbol":"g_telemetry","offset_bytes":8,"type":"f32"},
        {"role":"base","symbol":"g_telemetry","type":"u32"}]}]})";
    QString error;
    const QList<WatchPreset> presets = WatchPresetMatcher::loadPresetsFromJson(json, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));

    const QList<Symbol> symbols = { makeSymbol("g_telemetry", 0x24000100) };
    const QList<WatchItem> out = WatchPresetMatcher::resolveSuggestions(presets, symbols);

    QCOMPARE(out.size(), 3);
    QCOMPARE(out.at(0).label, QStringLiteral("g_telemetry.sensor[0]"));
    QCOMPARE(out.at(1).label, QStringLiteral("g_telemetry+8"));
    QCOMPARE(out.at(2).label, QStringLiteral("g_telemetry"));
}

// A sensor preset names the generic sensor slots only when its driver is
// linked into the firmware; without that symbol the slots keep their
// neutral names, and a relabel preset listed BEFORE the item-producing one
// still applies.
void TestWatchPresetMatcher::relabelAppliesOnlyWhenItsSymbolIsPresent()
{
    const QByteArray json = R"({"presets":[
        {"id":"bme280_labels","requiresAnySymbol":["BME280_ReadAll"],"relabels":[
            {"role":"sensor0","label":"BME280 sicaklik","unit":"C"},
            {"role":"sensor1","label":"BME280 nem"}]},
        {"id":"sensor_memory","requiresAnySymbol":["g_telemetry"],"items":[
            {"role":"sensor0","label":"g_telemetry.sensor[0]","symbol":"g_telemetry","offset_bytes":4,"type":"f32"},
            {"role":"sensor1","label":"g_telemetry.sensor[1]","symbol":"g_telemetry","offset_bytes":8,"type":"f32","unit":"raw"}]}]})";
    QString error;
    const QList<WatchPreset> presets = WatchPresetMatcher::loadPresetsFromJson(json, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));

    const QList<Symbol> withoutDriver = { makeSymbol("g_telemetry", 0x24000100) };
    QList<WatchItem> out = WatchPresetMatcher::resolveSuggestions(presets, withoutDriver);
    QCOMPARE(out.size(), 2);
    QCOMPARE(out.at(0).label, QStringLiteral("g_telemetry.sensor[0]"));
    QCOMPARE(out.at(0).unit, QString());

    const QList<Symbol> withDriver = { makeSymbol("g_telemetry", 0x24000100),
                                       makeSymbol("BME280_ReadAll", 0x08001000) };
    out = WatchPresetMatcher::resolveSuggestions(presets, withDriver);
    QCOMPARE(out.size(), 2);
    QCOMPARE(out.at(0).label, QStringLiteral("BME280 sicaklik"));
    QCOMPARE(out.at(0).unit, QStringLiteral("C"));
    QCOMPARE(out.at(0).role, QStringLiteral("sensor0"));   // role untouched: profile comparison keys on it
    QCOMPARE(out.at(1).label, QStringLiteral("BME280 nem"));
    QCOMPARE(out.at(1).unit, QStringLiteral("raw"));       // a relabel without "unit" keeps the item's unit
}

// The shipped watch_presets.json itself: a firmware that never painted its
// stack (it has _sstack/_estack but no StackPaint_Init) must not get a
// stackWatermark row, because its scan would read ~0 B and raise a false
// "stack exhausted" alarm. A pipeline build that paints still gets it.
void TestWatchPresetMatcher::stackWatermarkNeedsPaintedStack()
{
    const QString path = QFINDTESTDATA("../watch/watch_presets.json");
    QVERIFY2(!path.isEmpty(), "watch/watch_presets.json not found");
    QFile file(path);
    QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(file.fileName()));
    QString error;
    const QList<WatchPreset> presets = WatchPresetMatcher::loadPresetsFromJson(file.readAll(), &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));

    const auto hasWatermark = [](const QList<WatchItem> &items) {
        for (const WatchItem &i : items)
            if (i.role == QStringLiteral("stackWatermark")) return true;
        return false;
    };

    QList<Symbol> unpainted = { makeSymbol("_sstack", 0x341ff800, true),
                                makeSymbol("_estack", 0x34200000) };
    QVERIFY(!hasWatermark(WatchPresetMatcher::resolveSuggestions(presets, unpainted)));

    QList<Symbol> painted = unpainted;
    painted << makeSymbol("StackPaint_Init", 0x08001e54);
    QVERIFY(hasWatermark(WatchPresetMatcher::resolveSuggestions(presets, painted)));
}

void TestWatchPresetMatcher::applyingTwiceDoesNotDuplicateItems()
{
    WatchItem existing;
    existing.address = 0x3401e494;
    existing.kind = WatchItemKind::Scalar;

    WatchItem same = existing;          // already watched
    WatchItem sameAddrRegion = existing;
    sameAddrRegion.kind = WatchItemKind::RegionScan;   // different kind: a separate row
    WatchItem other;
    other.address = 0x3401e498;
    other.kind = WatchItemKind::Scalar;

    const QList<WatchItem> out = WatchPresetMatcher::withoutAlreadyWatched(
        { same, sameAddrRegion, other }, { existing });

    QCOMPARE(out.size(), 2);
    QCOMPARE(out.at(0).kind, WatchItemKind::RegionScan);
    QCOMPARE(out.at(1).address, quint64(0x3401e498));
}
