#pragma once

#include <QObject>

class TestTraceRecorderPlayer : public QObject
{
    Q_OBJECT

private slots:
    void roundTripPreservesTimesAndValues();
    void headerRestoresItemMetadata();
    void eventsRoundTrip();
    void corruptFileReturnsErrorNoCrash();
    void missingFileReturnsErrorNoCrash();
};
