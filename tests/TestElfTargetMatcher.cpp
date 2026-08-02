#include "TestElfTargetMatcher.h"
#include "modules/watcher/ElfTargetMatcher.h"

#include <QTest>

namespace {
QByteArray vectorBytes(quint32 sp, quint32 resetVec)
{
    QByteArray b(8, char(0));
    b[0] = char(sp & 0xFF);        b[1] = char((sp >> 8) & 0xFF);
    b[2] = char((sp >> 16) & 0xFF); b[3] = char((sp >> 24) & 0xFF);
    b[4] = char(resetVec & 0xFF);        b[5] = char((resetVec >> 8) & 0xFF);
    b[6] = char((resetVec >> 16) & 0xFF); b[7] = char((resetVec >> 24) & 0xFF);
    return b;
}

Symbol makeSymbol(const QString &name, quint64 addr)
{
    Symbol s;
    s.name = name;
    s.address = addr;
    s.kind = SymbolKind::ReadOnly;
    return s;
}
}

void TestElfTargetMatcher::matchingSpAndResetVectorYieldsMatch()
{
    const quint64 estack = 0x24050000ull;
    const quint64 resetHandler = 0x0800220cull;
    const QList<Symbol> symbols{ makeSymbol("_estack", estack), makeSymbol("Reset_Handler", resetHandler) };

    const ElfMatchReport report = ElfTargetMatcher::evaluate(
        0xE000ED08ull, vectorBytes(quint32(estack), quint32(resetHandler | 1)), symbols);

    QCOMPARE(report.result, ElfMatchResult::Match);
    QVERIFY(report.spMatches);
    QVERIFY(report.resetMatches);
}

void TestElfTargetMatcher::resetVectorWithoutThumbBitYieldsMismatch()
{
    const quint64 estack = 0x24050000ull;
    const quint64 resetHandler = 0x0800220cull;
    const QList<Symbol> symbols{ makeSymbol("_estack", estack), makeSymbol("Reset_Handler", resetHandler) };

    // Reset vector WITHOUT the Thumb bit — must not match even though the
    // numeric value is otherwise "close".
    const ElfMatchReport report = ElfTargetMatcher::evaluate(
        0xE000ED08ull, vectorBytes(quint32(estack), quint32(resetHandler)), symbols);

    QCOMPARE(report.result, ElfMatchResult::Mismatch);
    QVERIFY(report.spMatches);
    QVERIFY(!report.resetMatches);
}

void TestElfTargetMatcher::spMatchesButResetVectorDoesNotYieldsMismatch()
{
    const quint64 estack = 0x24050000ull;
    const quint64 resetHandler = 0x0800220cull;
    const QList<Symbol> symbols{ makeSymbol("_estack", estack), makeSymbol("Reset_Handler", resetHandler) };

    const ElfMatchReport report = ElfTargetMatcher::evaluate(
        0xE000ED08ull, vectorBytes(quint32(estack), 0x08009999u), symbols);

    QCOMPARE(report.result, ElfMatchResult::Mismatch);
    QVERIFY(report.spMatches);
    QVERIFY(!report.resetMatches);
}

void TestElfTargetMatcher::missingEstackSymbolYieldsUnknown()
{
    const QList<Symbol> symbols{ makeSymbol("Reset_Handler", 0x0800220cull) };   // no _estack

    const ElfMatchReport report = ElfTargetMatcher::evaluate(
        0xE000ED08ull, vectorBytes(0x24050000u, 0x0800220du), symbols);

    QCOMPARE(report.result, ElfMatchResult::Unknown);
    QVERIFY(!report.detail.isEmpty());
}
