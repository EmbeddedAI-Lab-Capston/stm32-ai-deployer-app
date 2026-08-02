#pragma once

#include <QObject>

class TestNmSymbolParser : public QObject
{
    Q_OBJECT

private slots:
    void fourFieldLineParsesAddressSizeTypeName();
    void threeFieldLineHasNoSize();
    void absoluteSymbolIsValueNotAddress();
    void estackIsInRamRange();
    void weakSymbolIsClassifiedWeak();
    void malformedLineIsSkippedNotCrashed();
};
