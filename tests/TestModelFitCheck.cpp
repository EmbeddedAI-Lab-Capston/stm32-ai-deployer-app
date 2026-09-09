#include "TestModelFitCheck.h"
#include "modules/flash/ModelFitCheck.h"

#include <QTest>

// Real anomaly_mlp_int8.tflite footprint on F4, from the same stedgeai
// analyze run captured for TestModelFootprint's F4 fixture.
void TestModelFitCheck::realF4ModelFitsRealF4BoardWithNoWarnings()
{
    ModelFootprint fp;
    fp.weightsBytes     = 2696;
    fp.activationsBytes = 716;
    fp.maccCount        = 2432;

    const BoardInfo f4 = BoardPresets::find(QStringLiteral("STM32F4"));
    QVERIFY(!f4.isNull());

    const QStringList warnings = checkModelFitsBoard(fp, f4);
    QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join("; ")));
}

// Real footprint of kws_dnn_direct_int8.tflite (a genuinely large KWS model,
// measured live via `stedgeai analyze`), checked against a synthetic
// tiny-capacity test fixture (not a real product) specifically to exercise
// both thresholds — no available real board in BoardPresets is actually
// this small, which is exactly why a real model never triggers this path
// live in this project's hardware set (see TODO.md Faz 10.5 notes).
void TestModelFitCheck::oversizedModelOnTinyBoardWarnsForBothRamAndFlash()
{
    ModelFootprint fp;
    fp.weightsBytes     = 261424;
    fp.activationsBytes = 12824;
    fp.maccCount        = 261280;

    BoardInfo tinyBoard;
    tinyBoard.name    = QStringLiteral("test-fixture-tiny");
    tinyBoard.flashKb = 16;
    tinyBoard.ramKb   = 8;

    const QStringList warnings = checkModelFitsBoard(fp, tinyBoard);
    QCOMPARE(warnings.size(), 2);
    QVERIFY(warnings.at(0).contains(QStringLiteral("RAM")));
    QVERIFY(warnings.at(1).contains(QStringLiteral("Flash")));
}

// RAM and Flash are independent checks — a board with generous RAM but a
// tiny Flash must warn about Flash ONLY, not both.
void TestModelFitCheck::tinyFlashOnlyWarnsAboutFlashNotRam()
{
    ModelFootprint fp;
    fp.weightsBytes     = 261424;   // way over a 16 KB flash
    fp.activationsBytes = 716;      // comfortably under a 1024 KB RAM

    BoardInfo board;
    board.name    = QStringLiteral("test-fixture-flash-only");
    board.flashKb = 16;
    board.ramKb   = 1024;

    const QStringList warnings = checkModelFitsBoard(fp, board);
    QCOMPARE(warnings.size(), 1);
    QVERIFY(warnings.first().contains(QStringLiteral("Flash")));
}
