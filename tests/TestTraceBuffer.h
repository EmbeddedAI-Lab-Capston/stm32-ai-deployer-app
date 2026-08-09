#pragma once

#include <QObject>

class TestTraceBuffer : public QObject
{
    Q_OBJECT

private slots:
    void appendAndDecimateProduceRequestedColumnCount();
    void overflowCapsRingButStatsKeepGrowing();
    void decimateOutsideDataRangeHasNoDataNoCrash();
    void decimateOnEmptyBufferDoesNotCrash();
};
