#pragma once

#include <QObject>

class TestWatchModel : public QObject
{
    Q_OBJECT

private slots:
    void statsMatchTwoPassReferenceWithinTolerance();
    void statsResetClearsEverything();
    void statsTrackMinMaxLast();
};
