#pragma once

#include <QObject>

class TestTimeSeriesRuleEngine : public QObject
{
    Q_OBJECT

private slots:
    void ruleDoesNotMatchAnItemWithADifferentOrEmptyRole();
    void thresholdBoundaryIsExclusiveForStrictLessThan();
    void gateRejectsAnEventOutsideTheWindowOnEitherSide();
    void thresholdSingleSampleUnderSustainMsIsNotAViolation();
    void thresholdSustainedOverWindowIsAViolationWithCorrectTime();
    void zscoreSpikeIsDetectedWithExpectedZ();
    void zscoreBelowMinSamplesIsNotAViolation();
    void driftRisingSeriesIsDetectedWithSlopeAndR2();
    void driftNoisyFlatSeriesIsRejectedByR2Gate();
    void gateSuppressesViolationWithoutMatchingEvent();
    void gateAllowsViolationWithMatchingEventWithinWindow();
    void loadRulesFromJsonParsesKnownFields();
    void loadRulesFromJsonSkipsEntriesMissingId();
};
