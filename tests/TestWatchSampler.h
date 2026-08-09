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
};
