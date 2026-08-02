#pragma once

#include <QObject>

class TestElfTargetMatcher : public QObject
{
    Q_OBJECT

private slots:
    void matchingSpAndResetVectorYieldsMatch();
    void resetVectorWithoutThumbBitYieldsMismatch();
    void spMatchesButResetVectorDoesNotYieldsMismatch();
    void missingEstackSymbolYieldsUnknown();
};
