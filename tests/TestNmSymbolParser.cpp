#include "TestNmSymbolParser.h"
#include "modules/watcher/NmSymbolParser.h"

#include <QFile>
#include <QTest>
#include <QTextStream>

namespace {
QString loadFixture()
{
    const QString path = QFINDTESTDATA("fixtures/nm_h7.txt");
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();
    return QString::fromUtf8(f.readAll());
}
}

void TestNmSymbolParser::fourFieldLineParsesAddressSizeTypeName()
{
    const QList<Symbol> symbols = NmSymbolParser::parse(QStringLiteral(
        "24000fe8 00000004 B uwTick\n"));
    QCOMPARE(symbols.size(), 1);
    QCOMPARE(symbols.first().address, quint64(0x24000fe8));
    QCOMPARE(symbols.first().size, quint64(0x4));
    QCOMPARE(symbols.first().hasSize, true);
    QCOMPARE(symbols.first().nmType, 'B');
    QCOMPARE(symbols.first().name, QStringLiteral("uwTick"));
    QCOMPARE(symbols.first().kind, SymbolKind::Bss);
}

void TestNmSymbolParser::threeFieldLineHasNoSize()
{
    const QList<Symbol> symbols = NmSymbolParser::parse(QStringLiteral(
        "080007d8 T __aeabi_uldivmod\n"));
    QCOMPARE(symbols.size(), 1);
    QCOMPARE(symbols.first().hasSize, false);
    QCOMPARE(symbols.first().name, QStringLiteral("__aeabi_uldivmod"));
    QCOMPARE(symbols.first().kind, SymbolKind::Code);
}

void TestNmSymbolParser::absoluteSymbolIsValueNotAddress()
{
    const QString nmOutput = loadFixture();
    QVERIFY(!nmOutput.isEmpty());
    const QList<Symbol> symbols = NmSymbolParser::parse(nmOutput);

    bool found = false;
    for (const Symbol &s : symbols) {
        if (s.name == QStringLiteral("_Min_Stack_Size")) {
            found = true;
            QCOMPARE(s.kind, SymbolKind::Absolute);
            QCOMPARE(s.addressIsValue, true);
            QCOMPARE(s.address, quint64(0x800));   // this is a VALUE (2048), not an address
        }
    }
    QVERIFY(found);
}

void TestNmSymbolParser::estackIsInRamRange()
{
    const QString nmOutput = loadFixture();
    QVERIFY(!nmOutput.isEmpty());
    const QList<Symbol> symbols = NmSymbolParser::parse(nmOutput);

    bool found = false;
    for (const Symbol &s : symbols) {
        if (s.name == QStringLiteral("_estack")) {
            found = true;
            QVERIFY(s.address > 0x20000000ull);
            QVERIFY(s.kind != SymbolKind::Absolute);   // a real address, not a value
        }
    }
    QVERIFY(found);
}

void TestNmSymbolParser::weakSymbolIsClassifiedWeak()
{
    const QString nmOutput = loadFixture();
    QVERIFY(!nmOutput.isEmpty());
    const QList<Symbol> symbols = NmSymbolParser::parse(nmOutput);

    bool found = false;
    for (const Symbol &s : symbols) {
        if (s.name == QStringLiteral("Reset_Handler")) {
            found = true;
            QCOMPARE(s.nmType, 'W');
            QCOMPARE(s.kind, SymbolKind::Weak);
        }
    }
    QVERIFY(found);
}

void TestNmSymbolParser::malformedLineIsSkippedNotCrashed()
{
    const QList<Symbol> symbols = NmSymbolParser::parse(QStringLiteral(
        "\n"
        "this is not a symbol line at all\n"
        "nm: warning: something went sideways\n"
        "24000fe8 00000004 B uwTick\n"
        "\n"));
    // The one real symbol line survives; the garbage lines are silently dropped.
    QCOMPARE(symbols.size(), 1);
    QCOMPARE(symbols.first().name, QStringLiteral("uwTick"));
}
