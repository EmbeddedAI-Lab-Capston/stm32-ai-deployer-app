#include "TestN6RamImage.h"

#include <QTest>

#include "modules/flash/N6RamImage.h"

using namespace N6RamImage;

void TestN6RamImage::parsesSingleWordFromCliOutput()
{
    const QString output = QStringLiteral(
        "  Address:      : 0x46008100\n"
        "0x46008100 : 00000002 \n");
    quint32 value = 0;
    QVERIFY(parseCliWord32(output, value));
    QCOMPARE(value, 0x00000002U);
}

void TestN6RamImage::parsesVectorPairFromOneLine()
{
    // STM32_Programmer_CLI prints up to four words per line.
    const QString output = QStringLiteral("0x34000400 : 34200000 340095E9 340012F1 340012F5\n");
    quint32 sp = 0;
    quint32 pc = 0;
    QVERIFY(parseCliVector(output, sp, pc));
    QCOMPARE(sp, 0x34200000U);
    QCOMPARE(pc, 0x340095E9U);
}

void TestN6RamImage::parsesVectorPairAcrossSeparateReads()
{
    const QString output = QStringLiteral(
        "0x34000400 : 34200000\n"
        "0x34000404 : 340095E9\n");
    quint32 sp = 0;
    quint32 pc = 0;
    QVERIFY(parseCliVector(output, sp, pc));
    QCOMPARE(sp, 0x34200000U);
    QCOMPARE(pc, 0x340095E9U);
}

void TestN6RamImage::rejectsOutputWithoutWords()
{
    quint32 sp = 0;
    quint32 pc = 0;
    QVERIFY(!parseCliVector(QStringLiteral("Error: failed to read the requested memory content"),
                            sp, pc));
    quint32 value = 0;
    QVERIFY(!parseCliWord32(QStringLiteral("Error: Unable to get core ID"), value));
}

void TestN6RamImage::acceptsRealImageVector()
{
    // Measured on NUCLEO-N657X0-Q, anomaly_mlp_int8 build.
    QVERIFY(vectorLooksValid(0x34200000U, 0x340095E9U));
}

void TestN6RamImage::rejectsClearedRam()
{
    // After a power cycle the image is gone; restarting into this would fault
    // instead of reporting that there is nothing to restart.
    QVERIFY(!vectorLooksValid(0x00000000U, 0x00000000U));
    QVERIFY(!vectorLooksValid(0xFFFFFFFFU, 0xFFFFFFFFU));
}

void TestN6RamImage::rejectsNonThumbResetHandler()
{
    QVERIFY(!vectorLooksValid(0x34200000U, 0x340095E8U));
}

void TestN6RamImage::rejectsStackPointerOutsideRam()
{
    QVERIFY(!vectorLooksValid(0x20010000U, 0x340095E9U));
}

void TestN6RamImage::connectArgsReplaceExistingMode()
{
    const QStringList base{QStringLiteral("port=SWD"),
                           QStringLiteral("mode=NORMAL"),
                           QStringLiteral("sn=001A002734")};
    const QStringList args = connectArgsWithMode(base, QStringLiteral("HOTPLUG"));
    QVERIFY(!args.contains(QStringLiteral("mode=NORMAL")));
    QVERIFY(args.contains(QStringLiteral("mode=HOTPLUG")));
    QVERIFY(args.contains(QStringLiteral("sn=001A002734")));
    QCOMPARE(args.count(QStringLiteral("mode=HOTPLUG")), 1);
}

void TestN6RamImage::resetUsesRunNotRst()
{
    const QStringList args = resetArgs({QStringLiteral("port=SWD")});
    QVERIFY(args.contains(QStringLiteral("mode=UR")));
    QVERIFY(args.contains(QStringLiteral("-run")));
    // "-rst" leaves the N6 in a state HOTPLUG can no longer attach to, so the
    // following read/arm step would fail.
    QVERIFY(!args.contains(QStringLiteral("-rst")));
}

void TestN6RamImage::armArgsCarryVtorCpacrAndCoreRegs()
{
    const QStringList args = armArgs({QStringLiteral("port=SWD")},
                                     0x34200000U, 0x340095E9U,
                                     QStringLiteral("C:/tmp/app.bin"));
    QVERIFY(args.contains(QStringLiteral("mode=HOTPLUG")));
    QVERIFY(args.contains(QStringLiteral("-halt")));
    QVERIFY(args.contains(QStringLiteral("C:/tmp/app.bin")));
    QVERIFY(args.contains(hex32(kVtorAddress)));
    QVERIFY(args.contains(hex32(kCpacrAddress)));
    QVERIFY(args.contains(hex32(kCpacrFullAccess)));
    QVERIFY(args.contains(QStringLiteral("MSP=0x34200000")));
    // The Thumb bit is dropped when the value goes into PC.
    QVERIFY(args.contains(QStringLiteral("PC=0x340095e8")));
    QVERIFY(args.endsWith(QStringLiteral("-run")));
}

void TestN6RamImage::armArgsOmitWriteWhenNoBinaryGiven()
{
    // Restart path: the image is already in AXISRAM, only the core is re-pointed.
    const QStringList args = armArgs({QStringLiteral("port=SWD")}, 0x34200000U, 0x340095E9U);
    QVERIFY(!args.contains(QStringLiteral("-w")));
    QVERIFY(args.contains(QStringLiteral("-w32")));
    QVERIFY(args.contains(QStringLiteral("MSP=0x34200000")));
}
