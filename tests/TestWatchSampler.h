#pragma once

#include <QObject>

class TestWatchSampler : public QObject
{
    Q_OBJECT

private slots:
    void decodesScalarsAtTheirPlanOffsets();
    void failedReplyReportsNotOkAndDoesNotFakeAZero();
    void unmappedItemReportsNotOk();
    void shortReplyReportsNotOk();
    void regionScanFullyPaintedReportsWholeRegion();
    void regionScanMismatchReportsOffsetOfFirstBadWord();
    void regionScanMismatchAcrossChunkBoundary();
    void regionScanFailedChunkReportsNotOk();
    void guardsMatchingDecodesNormally();
    void guardsMismatchedReportsNotOk();
    void guardChunkFailedReportsNotOk();
};
