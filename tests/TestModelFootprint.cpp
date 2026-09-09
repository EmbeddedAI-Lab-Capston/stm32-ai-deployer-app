#include "TestModelFootprint.h"
#include "modules/flash/XCubeAIRunner.h"

#include <QTest>

// Real `stedgeai analyze --target stm32 --quiet` output snippets, captured
// live during Faz 10.1/10.5 verification (2026-09-09): H7 running
// anomaly_cnn_int8.tflite, F4 running anomaly_mlp_int8.tflite. Not
// hand-written — copied so the parser is tested against the ACTUAL ST Edge
// AI Core v4.0.1 report shape, not a guessed one.

void TestModelFootprint::parsesRealH7AnalyzeOutput()
{
    const QString output = QStringLiteral(
        " input 1/1          :   'serving_default_ke..tensor_540', int8(1x3x1), 3 Bytes, QLinear(0.037805241,45,int8), activations\n"
        " output 1/1         :   'nl_11', int8(1x2), 2 Bytes, QLinear(0.003906250,-128,int8), activations\n"
        " macc               :   15,136\n"
        " weights (ro)       :   6,856 B (6.70 KiB) (1 segment) / -19,008(-73.5%) vs float model\n"
        " activations (rw)   :   6,592 B (6.44 KiB) (1 segment) *\n"
        " ram (total)        :   6,592 B (6.44 KiB) = 6,592 + 0 + 0\n"
    );

    const ModelFootprint fp = XCubeAIRunner::parseAnalyzeOutput(output);
    QCOMPARE(fp.maccCount, qint64(15136));
    QCOMPARE(fp.weightsBytes, qint64(6856));
    QCOMPARE(fp.activationsBytes, qint64(6592));
}

// The "Complexity report" section repeats macc/weights using an EQUALS form
// ("model: macc=2,528 weights=2,696 activations=-- io=--") with a DIFFERENT
// macc number than the real summary (2,528 vs the real 2,432) — the parser
// must anchor on the colon form and not be fooled by the decoy.
void TestModelFootprint::parsesRealF4AnalyzeOutputAndIgnoresEqualsFormDecoy()
{
    const QString output = QStringLiteral(
        " model: macc=2,528 weights=2,696 activations=-- io=--\n"
        "\n"
        " input 1/1          :   'serving_default_ke..tensor_480', int8(1x3), 3 Bytes, QLinear(0.037805241,45,int8), activations\n"
        " output 1/1         :   'nl_3', int8(1x2), 2 Bytes, QLinear(0.003906250,-128,int8), activations\n"
        " macc               :   2,432\n"
        " weights (ro)       :   2,696 B (2.63 KiB) (1 segment) / -6,912(-71.9%) vs float model\n"
        " activations (rw)   :   716 B (716 B) (1 segment) *\n"
        " ram (total)        :   716 B (716 B) = 716 + 0 + 0\n"
    );

    const ModelFootprint fp = XCubeAIRunner::parseAnalyzeOutput(output);
    QCOMPARE(fp.maccCount, qint64(2432));
    QCOMPARE(fp.weightsBytes, qint64(2696));
    QCOMPARE(fp.activationsBytes, qint64(716));
}

// Nothing recognisable in the text (e.g. stedgeai crashed before printing
// the summary) must report -1 ("unknown"), never 0 — 0 would silently read
// as "this model needs no RAM at all" to a naive fit check.
void TestModelFootprint::garbledTextReportsUnknownNotZero()
{
    const QString output = QStringLiteral("Error: model file not found\nTraceback...\n");
    const ModelFootprint fp = XCubeAIRunner::parseAnalyzeOutput(output);
    QCOMPARE(fp.maccCount, qint64(-1));
    QCOMPARE(fp.weightsBytes, qint64(-1));
    QCOMPARE(fp.activationsBytes, qint64(-1));
}
