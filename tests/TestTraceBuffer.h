#pragma once

#include <QObject>

class TestTraceBuffer : public QObject
{
    Q_OBJECT

private slots:
    void wraparoundKeepsNewestSamplesAndTimes();
    void appendAndDecimateProduceRequestedColumnCount();
    void overflowCapsRingButStatsKeepGrowing();
    void decimateOutsideDataRangeHasNoDataNoCrash();
    void decimateOnEmptyBufferDoesNotCrash();
    void valueAtFindsClosestSample();
    void valueAtOnEmptyBufferReturnsNan();
    void decimatePerfUnder20MsFor1eSamples800Columns();
};
