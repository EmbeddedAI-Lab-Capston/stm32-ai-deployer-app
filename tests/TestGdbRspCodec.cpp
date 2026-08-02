#include "TestGdbRspCodec.h"
#include "modules/debug/GdbRspCodec.h"

#include <QTest>

void TestGdbRspCodec::checksumMatchesGdbReference()
{
    // Known-good GDB reference value for "qSupported" is 0x37.
    QCOMPARE(GdbRspCodec::checksum("qSupported"), quint8(0x37));
}

void TestGdbRspCodec::frameProducesDollarHashHex()
{
    QCOMPARE(GdbRspCodec::frame("qSupported"), QByteArray("$qSupported#37"));
}

void TestGdbRspCodec::extractHandlesPartialStream()
{
    const QByteArray full = GdbRspCodec::frame("qSupported");
    QVERIFY(full.size() >= 3);

    // Split into three arbitrary chunks.
    const int c1 = full.size() / 3;
    const int c2 = full.size() / 3;
    const QByteArray part1 = full.left(c1);
    const QByteArray part2 = full.mid(c1, c2);
    const QByteArray part3 = full.mid(c1 + c2);

    QByteArray buffer;
    QByteArray payload;

    buffer += part1;
    QCOMPARE(GdbRspCodec::extract(buffer, payload), GdbRspCodec::Extract::NeedMore);

    buffer += part2;
    QCOMPARE(GdbRspCodec::extract(buffer, payload), GdbRspCodec::Extract::NeedMore);

    buffer += part3;
    QCOMPARE(GdbRspCodec::extract(buffer, payload), GdbRspCodec::Extract::Packet);
    QCOMPARE(payload, QByteArray("qSupported"));
    QVERIFY(buffer.isEmpty());
}

void TestGdbRspCodec::extractReportsNotificationSeparately()
{
    const QByteArray body = "Stop:T05thread:1;";
    QByteArray buffer = "%" + body + "#"
        + QByteArray::number(GdbRspCodec::checksum(body), 16).rightJustified(2, '0');

    QByteArray payload;
    QCOMPARE(GdbRspCodec::extract(buffer, payload), GdbRspCodec::Extract::Notification);
    QCOMPARE(payload, body);
}

void TestGdbRspCodec::expandRunLengthSimpleRun()
{
    bool ok = false;
    QCOMPARE(GdbRspCodec::expandRunLength("0* ", &ok), QByteArray("0000"));
    QVERIFY(ok);
}

void TestGdbRspCodec::expandRunLengthMidStringRun()
{
    bool ok = false;
    const QByteArray result = GdbRspCodec::expandRunLength("ab*\"cd", &ok);
    QVERIFY(ok);
    QCOMPARE(result, QByteArray("abbbbbbcd"));
    QCOMPARE(result.count('b'), 6);
}

void TestGdbRspCodec::expandRunLengthMalformedNoPrecedingChar()
{
    bool ok = true;
    GdbRspCodec::expandRunLength("*x", &ok);
    QVERIFY(!ok);
}

void TestGdbRspCodec::hexDecodeRoundTrip()
{
    bool ok = false;
    const QByteArray decoded = GdbRspCodec::hexDecode("78563412", &ok);
    QVERIFY(ok);
    QCOMPARE(decoded, QByteArray::fromHex("78563412"));
}

void TestGdbRspCodec::memoryReadPacketFormat()
{
    const QByteArray packet = GdbRspCodec::memoryReadPacket(0xE000EDF0u, 4u);
    QCOMPARE(packet.toLower(), QByteArray("me000edf0,4"));
}

void TestGdbRspCodec::isErrorReplyBothForms()
{
    QString code;
    QVERIFY(GdbRspCodec::isErrorReply("E01", &code));
    QCOMPARE(code, QStringLiteral("01"));

    code.clear();
    QVERIFY(GdbRspCodec::isErrorReply("E 01", &code));
    QCOMPARE(code, QStringLiteral("01"));
}

void TestGdbRspCodec::isAllowedOutgoingWhitelist()
{
    QVERIFY(GdbRspCodec::isAllowedOutgoing("m"));
    QVERIFY(GdbRspCodec::isAllowedOutgoing("qSupported"));
    QVERIFY(GdbRspCodec::isAllowedOutgoing("QNonStop:1"));
    QVERIFY(GdbRspCodec::isAllowedOutgoing("vCont;c"));
    QVERIFY(GdbRspCodec::isAllowedOutgoing("D"));
}

void TestGdbRspCodec::isAllowedOutgoingBlacklist()
{
    QVERIFY(!GdbRspCodec::isAllowedOutgoing("?"));
    QVERIFY(!GdbRspCodec::isAllowedOutgoing("Z0,0,4"));
    QVERIFY(!GdbRspCodec::isAllowedOutgoing("M20000000,4:12345678"));
    QVERIFY(!GdbRspCodec::isAllowedOutgoing("X20000000,4:...."));
    QVERIFY(!GdbRspCodec::isAllowedOutgoing("vCont;t"));
    QVERIFY(!GdbRspCodec::isAllowedOutgoing("k"));
    QVERIFY(!GdbRspCodec::isAllowedOutgoing("\x03"));
}
