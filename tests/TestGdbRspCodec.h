#pragma once

#include <QObject>

class TestGdbRspCodec : public QObject
{
    Q_OBJECT

private slots:
    void checksumMatchesGdbReference();
    void frameProducesDollarHashHex();
    void extractHandlesPartialStream();
    void extractReportsNotificationSeparately();
    void expandRunLengthSimpleRun();
    void expandRunLengthMidStringRun();
    void expandRunLengthMalformedNoPrecedingChar();
    void hexDecodeRoundTrip();
    void memoryReadPacketFormat();
    void isErrorReplyBothForms();
    void isAllowedOutgoingWhitelist();
    void isAllowedOutgoingBlacklist();
};
