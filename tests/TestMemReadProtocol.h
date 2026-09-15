#pragma once

#include <QObject>

class TestMemReadProtocol : public QObject
{
    Q_OBJECT

private slots:
    void roundTripsPing();
    void roundTripsConnect();
    void roundTripsReadRanges();
    void roundTripsDisconnect();

    void roundTripsOkResponse();
    void roundTripsErrorResponse();
    void roundTripsReadResults();
    void carriesBinaryDataWithNulBytes();

    void framingSplitsTwoMessages();
    void framingWaitsForIncompletePrefix();
    void framingWaitsForIncompleteBody();
    void framingRejectsAbsurdLength();

    void rejectsUnknownCommand();
    void rejectsTruncatedRequest();
    void rejectsTruncatedResponse();
    void rejectsImpossibleRangeCount();
    void rejectsImpossibleResultCount();
};
