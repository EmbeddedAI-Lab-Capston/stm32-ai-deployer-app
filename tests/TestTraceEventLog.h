#pragma once

#include <QObject>

class TestTraceEventLog : public QObject
{
    Q_OBJECT

private slots:
    void eventsAreTimeSortedByArrival();
    void eventsBetweenFiltersByRange();
    void resetClearsAndRestartsClock();
    void emptyLogReturnsEmptyRange();
};
