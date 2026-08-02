#include "TestValueCodec.h"
#include "modules/watcher/ValueCodec.h"

#include <QTest>

void TestValueCodec::decodeU32LittleEndian()
{
    const QByteArray buf = QByteArray::fromHex("78563412");
    bool ok = false;
    const double v = ValueCodec::decode(buf, 0, WatchValueType::U32, &ok);
    QVERIFY(ok);
    QCOMPARE(v, double(0x12345678u));
}

void TestValueCodec::decodeI16Negative()
{
    const QByteArray buf = QByteArray::fromHex("FFFF");
    bool ok = false;
    const double v = ValueCodec::decode(buf, 0, WatchValueType::I16, &ok);
    QVERIFY(ok);
    QCOMPARE(v, -1.0);
}

void TestValueCodec::decodeF32()
{
    const QByteArray buf = QByteArray::fromHex("0000803F");   // IEEE754 1.0f, little-endian
    bool ok = false;
    const double v = ValueCodec::decode(buf, 0, WatchValueType::F32, &ok);
    QVERIFY(ok);
    QCOMPARE(v, 1.0);
}

void TestValueCodec::decodeOutOfRangeFails()
{
    const QByteArray buf = QByteArray::fromHex("1234");   // only 2 bytes
    bool ok = true;
    ValueCodec::decode(buf, 0, WatchValueType::U32, &ok);   // needs 4
    QVERIFY(!ok);
}

void TestValueCodec::formatAppliesScaleAndUnit()
{
    WatchItem item;
    item.type   = WatchValueType::U32;
    item.scale  = 0.001;
    item.unit   = QStringLiteral("ms");
    item.format = DisplayFormat::Dec;
    QCOMPARE(ValueCodec::format(8200.0, item), QStringLiteral("8.200 ms"));
}

void TestValueCodec::formatPlainIntegerHasNoDecimals()
{
    WatchItem item;
    item.type   = WatchValueType::U32;
    item.format = DisplayFormat::Dec;   // scale=1.0, offset=0.0 (defaults) — a raw counter
    QCOMPARE(ValueCodec::format(42.0, item), QStringLiteral("42"));
}

void TestValueCodec::formatHexIgnoresScale()
{
    WatchItem item;
    item.type   = WatchValueType::U32;
    item.scale  = 0.001;   // must NOT affect hex display
    item.format = DisplayFormat::Hex;
    QCOMPARE(ValueCodec::format(double(0x1234), item), QStringLiteral("0x00001234"));
}
