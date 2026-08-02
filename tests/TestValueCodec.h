#pragma once

#include <QObject>

class TestValueCodec : public QObject
{
    Q_OBJECT

private slots:
    void decodeU32LittleEndian();
    void decodeI16Negative();
    void decodeF32();
    void decodeOutOfRangeFails();
    void formatAppliesScaleAndUnit();
    void formatPlainIntegerHasNoDecimals();
    void formatHexIgnoresScale();
};
