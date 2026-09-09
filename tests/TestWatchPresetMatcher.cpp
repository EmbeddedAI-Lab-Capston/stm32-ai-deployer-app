#include "TestWatchPresetMatcher.h"
#include "modules/watcher/WatchPresetMatcher.h"

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
