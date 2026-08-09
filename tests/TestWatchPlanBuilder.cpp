#include "TestWatchPlanBuilder.h"
#include "modules/watcher/WatchPlanBuilder.h"

#include <QTest>

namespace {
WatchItem makeItem(quint64 addr, WatchValueType t = WatchValueType::U32, bool enabled = true)
{
    WatchItem it;
    it.address = addr;
    it.type = t;
    it.enabled = enabled;
    it.kind = WatchItemKind::Scalar;
    return it;
}
}

void TestWatchPlanBuilder::fourContiguousU32MergeIntoOneRequest()
{
    QList<WatchItem> items{
        makeItem(0x1000), makeItem(0x1004), makeItem(0x1008), makeItem(0x100C)
    };
    const WatchPlan plan = WatchPlanBuilder::build(items, 4096);
    QCOMPARE(plan.requests.size(), 1);
    QCOMPARE(plan.requests.first().addr, quint64(0x1000));
    QCOMPARE(plan.requests.first().len, quint32(16));
}

void TestWatchPlanBuilder::gapOver256BytesSplitsIntoTwoRequests()
{
    QList<WatchItem> items{ makeItem(0x1000), makeItem(0x1000 + 4 + 300) };   // gap = 300 > 256
    const WatchPlan plan = WatchPlanBuilder::build(items, 4096);
    QCOMPARE(plan.requests.size(), 2);
}

void TestWatchPlanBuilder::gapUnder256BytesMergesIntoOneRequest()
{
    QList<WatchItem> items{ makeItem(0x1000), makeItem(0x1000 + 4 + 100) };   // gap = 100 <= 256
    const WatchPlan plan = WatchPlanBuilder::build(items, 4096);
    QCOMPARE(plan.requests.size(), 1);
    QCOMPARE(plan.requests.first().len, quint32(108));   // 0x1000..(0x1000+4+100+4)
}

void TestWatchPlanBuilder::eightKbSpreadNeverExceedsMaxReadBytes()
{
    QList<WatchItem> items;
    for (int i = 0; i < 21; ++i)
        items.append(makeItem(0x20000000ull + quint64(i) * 500));   // spans ~10000 bytes

    const WatchPlan plan = WatchPlanBuilder::build(items, 4096);
    QVERIFY(plan.requests.size() >= 2);
    for (const MemoryRequest &r : plan.requests)
        QVERIFY(r.len <= 4096);
}

void TestWatchPlanBuilder::itemSlotsMapToCorrectRequestAndOffset()
{
    QList<WatchItem> items{
        makeItem(0x1000),                    // item 0 -> merges with item 1
        makeItem(0x1004),                    // item 1
        makeItem(0x1000 + 4 + 1000),         // item 2 -> its own request (gap > 256)
    };
    const WatchPlan plan = WatchPlanBuilder::build(items, 4096);
    QCOMPARE(plan.requests.size(), 2);
    QCOMPARE(plan.itemSlots.size(), 3);

    QCOMPARE(plan.itemSlots.at(0).first, 0);
    QCOMPARE(plan.itemSlots.at(0).second, 0);
    QCOMPARE(plan.itemSlots.at(1).first, 0);
    QCOMPARE(plan.itemSlots.at(1).second, 4);
    QCOMPARE(plan.itemSlots.at(2).first, 1);
    QCOMPARE(plan.itemSlots.at(2).second, 0);
}

void TestWatchPlanBuilder::disabledItemsAreExcluded()
{
    QList<WatchItem> items{ makeItem(0x1000), makeItem(0x1004, WatchValueType::U32, false) };
    const WatchPlan plan = WatchPlanBuilder::build(items, 4096);

    QCOMPARE(plan.requests.size(), 1);
    QCOMPARE(plan.requests.first().len, quint32(4));   // only the enabled item
    QCOMPARE(plan.itemSlots.at(1).first, -1);
    QCOMPARE(plan.itemSlots.at(1).second, -1);
}

// maxReadBytes is the gdbserver PacketSize budget: exceeding it would make the
// server truncate or reject the read, so the merge loop must split instead.
void TestWatchPlanBuilder::maxReadBytesLimitSplitsIntoSeparateRequests()
{
    QList<WatchItem> items;
    // Four u32s spaced 200 B apart: gaps (196 B) are under the 256 B merge
    // threshold, so ONLY the byte budget can force a split.
    for (int i = 0; i < 4; ++i) {
        WatchItem it;
        it.address = 0x24000000 + quint64(i) * 200;
        it.type    = WatchValueType::U32;
        it.enabled = true;
        items.append(it);
    }

    const WatchPlan generous = WatchPlanBuilder::build(items, 4096);
    QCOMPARE(generous.requests.size(), 1);
    QCOMPARE(generous.requests.at(0).len, quint32(604));

    // 256 B budget cannot span more than two items (204 B ok, 404 B not).
    const WatchPlan tight = WatchPlanBuilder::build(items, 256);
    QCOMPARE(tight.requests.size(), 2);
    for (const MemoryRequest &r : tight.requests)
        QVERIFY2(r.len <= 256, "a request exceeded maxReadBytes");

    // Every item must still be reachable, at a correct offset.
    for (int i = 0; i < items.size(); ++i) {
        const int req = tight.itemSlots.at(i).first;
        const int off = tight.itemSlots.at(i).second;
        QVERIFY(req >= 0 && req < tight.requests.size());
        QVERIFY(off >= 0);
        QCOMPARE(tight.requests.at(req).addr + quint64(off), items.at(i).address);
    }
}
