#pragma once

#include <QObject>

class TestRateCheck : public QObject
{
    Q_OBJECT

private slots:
    void noCountSamplesReportsNotOk();
    void zeroCountDeltaReportsNotOk();
    void missingInfUsReportsNotOk();
    void consistentCaseComputesExpectedHz();
    void observedRateExceedingTheoreticalMaxIsInconsistent();
};
