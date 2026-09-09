#pragma once

#include <QObject>

class TestRamBudget : public QObject
{
    Q_OBJECT

private slots:
    void missingRamTotalReportsNotOk();
    void missingEstackReportsNotOk();
    void missingStackWatermarkReportsNotOk();
    void normalCaseComputesExpectedBreakdown();
    void heapNotWatchedTreatsHeapAsUnused();
    void stackPastStaticEndReportsCollisionButStillOk();
};
