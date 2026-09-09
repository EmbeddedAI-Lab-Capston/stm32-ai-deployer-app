#pragma once

#include <QObject>

class TestWatchPresetMatcher : public QObject
{
    Q_OBJECT

private slots:
    void alwaysPresetAppliesWithoutAnyRequiredSymbol();
    void requiresAnySymbolGatesPresetApplicability();
    void missingSymbolIsSkippedNotAnError();
    void regionScanFallsBackToSecondAlternativeWhenFirstMissing();
    void regionScanUsesAbsoluteSymbolAsValueNotAddress();
    void regionScanSkippedWhenNoAlternativeResolves();
    void offsetBytesIsAddedToSymbolAddress();
    void guardSymbolsResolveIntoWatchItemGuardAddresses();
};
