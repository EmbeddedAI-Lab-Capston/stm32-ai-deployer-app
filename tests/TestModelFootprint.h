#pragma once

#include <QObject>

class TestModelFootprint : public QObject
{
    Q_OBJECT

private slots:
    void parsesRealH7AnalyzeOutput();
    void parsesRealF4AnalyzeOutputAndIgnoresEqualsFormDecoy();
    void garbledTextReportsUnknownNotZero();
};
