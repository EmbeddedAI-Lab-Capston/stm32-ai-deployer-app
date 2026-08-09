#pragma once

#include <QObject>

class TestWatchPlanBuilder : public QObject
{
    Q_OBJECT

private slots:
    void maxReadBytesLimitSplitsIntoSeparateRequests();
    void fourContiguousU32MergeIntoOneRequest();
    void gapOver256BytesSplitsIntoTwoRequests();
    void gapUnder256BytesMergesIntoOneRequest();
    void eightKbSpreadNeverExceedsMaxReadBytes();
    void itemSlotsMapToCorrectRequestAndOffset();
    void disabledItemsAreExcluded();
};
