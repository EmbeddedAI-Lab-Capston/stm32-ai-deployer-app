#include "TestMemReadProtocol.h"

#include <QTest>

#include "modules/debug/MemReadProtocol.h"

using namespace memread;

void TestMemReadProtocol::roundTripsPing()
{
    Request r;
    QVERIFY(decodeRequest(encodePing(), r));
    QVERIFY(r.cmd == Cmd::Ping);
}

void TestMemReadProtocol::roundTripsConnect()
{
    Request r;
    QVERIFY(decodeRequest(encodeConnect("001A00273434511734313937"), r));
    QVERIFY(r.cmd == Cmd::Connect);
    QCOMPARE(QString::fromStdString(r.serial), QStringLiteral("001A00273434511734313937"));
}

void TestMemReadProtocol::roundTripsReadRanges()
{
    // The real N6 watch plan shape: a few scattered ranges of different sizes.
    const std::vector<ReadRange> ranges{
        {1, 0x34010910ull, 8},
        {2, 0x34010bf0ull, 0x5c},
        {3, 0xE000EDF0ull, 4},
    };
    Request r;
    QVERIFY(decodeRequest(encodeRead(ranges), r));
    QVERIFY(r.cmd == Cmd::Read);
    QCOMPARE(r.ranges.size(), std::size_t(3));
    QCOMPARE(r.ranges[1].id, 2u);
    QCOMPARE(r.ranges[1].addr, 0x34010bf0ull);
    QCOMPARE(r.ranges[1].len, 0x5cu);
    QCOMPARE(r.ranges[2].addr, 0xE000EDF0ull);
}

void TestMemReadProtocol::roundTripsDisconnect()
{
    Request r;
    QVERIFY(decodeRequest(encodeDisconnect(), r));
    QVERIFY(r.cmd == Cmd::Disconnect);
}

void TestMemReadProtocol::roundTripsOkResponse()
{
    Response resp;
    QVERIFY(decodeResponse(encodeOk("NUCLEO-N657X0-Q"), resp));
    QVERIFY(resp.status == Status::Ok);
    QVERIFY(!resp.isRead);
    QCOMPARE(QString::fromStdString(resp.text), QStringLiteral("NUCLEO-N657X0-Q"));
}

void TestMemReadProtocol::roundTripsErrorResponse()
{
    Response resp;
    QVERIFY(decodeResponse(encodeError("ST-Link baglantisi basarisiz (kod -545)."), resp));
    QVERIFY(resp.status == Status::Error);
    QVERIFY(resp.text.find("-545") != std::string::npos);
}

void TestMemReadProtocol::roundTripsReadResults()
{
    std::vector<ReadResult> results;
    ReadResult a; a.id = 7; a.addr = 0x34010de8ull; a.ok = true;
    a.data = std::string("\x01\x02\x03\x04", 4);
    ReadResult b; b.id = 8; b.addr = 0xDEADBEEFull; b.ok = false; b.error = "okunamadi";
    results.push_back(a);
    results.push_back(b);

    Response resp;
    QVERIFY(decodeResponse(encodeReadResults(results), resp));
    QVERIFY(resp.isRead);
    QCOMPARE(resp.results.size(), std::size_t(2));
    QVERIFY(resp.results[0].ok);
    QCOMPARE(resp.results[0].id, 7u);
    QCOMPARE(resp.results[0].data.size(), std::size_t(4));
    QCOMPARE(int(static_cast<unsigned char>(resp.results[0].data[3])), 4);
    QVERIFY(!resp.results[1].ok);
    QCOMPARE(QString::fromStdString(resp.results[1].error), QStringLiteral("okunamadi"));
}

void TestMemReadProtocol::carriesBinaryDataWithNulBytes()
{
    // Target memory is full of zero bytes; length-prefixing rather than any
    // terminator is what keeps them intact.
    std::vector<ReadResult> results;
    ReadResult r; r.id = 1; r.addr = 0x20000000ull; r.ok = true;
    r.data = std::string("\x00\x00\xFF\x00\x41", 5);
    results.push_back(r);

    Response resp;
    QVERIFY(decodeResponse(encodeReadResults(results), resp));
    QCOMPARE(resp.results[0].data.size(), std::size_t(5));
    QCOMPARE(int(static_cast<unsigned char>(resp.results[0].data[2])), 255);
    QCOMPARE(int(static_cast<unsigned char>(resp.results[0].data[4])), 0x41);
}

void TestMemReadProtocol::framingSplitsTwoMessages()
{
    std::string stream = frame(encodePing()) + frame(encodeDisconnect());
    std::string payload;
    bool tooLarge = false;

    QVERIFY(takeMessage(stream, payload, tooLarge));
    Request first;
    QVERIFY(decodeRequest(payload, first));
    QVERIFY(first.cmd == Cmd::Ping);

    QVERIFY(takeMessage(stream, payload, tooLarge));
    Request second;
    QVERIFY(decodeRequest(payload, second));
    QVERIFY(second.cmd == Cmd::Disconnect);

    QVERIFY(!takeMessage(stream, payload, tooLarge));
    QVERIFY(stream.empty());
}

void TestMemReadProtocol::framingWaitsForIncompletePrefix()
{
    std::string stream = frame(encodePing()).substr(0, 2);
    std::string payload;
    bool tooLarge = false;
    QVERIFY(!takeMessage(stream, payload, tooLarge));
    QVERIFY(!tooLarge);
    QCOMPARE(stream.size(), std::size_t(2));   // nothing consumed
}

void TestMemReadProtocol::framingWaitsForIncompleteBody()
{
    // A pipe hands over arbitrary chunks, so a half-arrived body must not be
    // consumed or misread.
    const std::string whole = frame(encodeConnect("001A00273434511734313937"));
    std::string stream = whole.substr(0, whole.size() - 3);
    std::string payload;
    bool tooLarge = false;
    QVERIFY(!takeMessage(stream, payload, tooLarge));

    stream += whole.substr(whole.size() - 3);
    QVERIFY(takeMessage(stream, payload, tooLarge));
    Request r;
    QVERIFY(decodeRequest(payload, r));
    QCOMPARE(QString::fromStdString(r.serial), QStringLiteral("001A00273434511734313937"));
}

void TestMemReadProtocol::framingRejectsAbsurdLength()
{
    std::string stream(4, '\xFF');   // announces ~4 GB
    std::string payload;
    bool tooLarge = false;
    QVERIFY(!takeMessage(stream, payload, tooLarge));
    QVERIFY(tooLarge);
}

void TestMemReadProtocol::rejectsUnknownCommand()
{
    Request r;
    QVERIFY(!decodeRequest(std::string(1, char(0x7F)), r));
    QVERIFY(!decodeRequest(std::string(), r));
}

void TestMemReadProtocol::rejectsTruncatedRequest()
{
    const std::string good = encodeConnect("ABC");
    for (std::size_t cut = 1; cut < good.size(); ++cut) {
        Request r;
        QVERIFY2(!decodeRequest(good.substr(0, cut), r),
                 qPrintable(QStringLiteral("cut=%1 kabul edildi").arg(cut)));
    }
}

void TestMemReadProtocol::rejectsTruncatedResponse()
{
    std::vector<ReadResult> results;
    ReadResult r; r.id = 1; r.addr = 0x100ull; r.ok = true; r.data = "abcd";
    results.push_back(r);
    const std::string good = encodeReadResults(results);
    for (std::size_t cut = 1; cut < good.size(); ++cut) {
        Response resp;
        QVERIFY2(!decodeResponse(good.substr(0, cut), resp),
                 qPrintable(QStringLiteral("cut=%1 kabul edildi").arg(cut)));
    }
}

void TestMemReadProtocol::rejectsImpossibleRangeCount()
{
    // Header claims a million ranges in a handful of bytes.
    std::string payload;
    payload.push_back(char(Cmd::Read));
    for (int i = 0; i < 4; ++i)
        payload.push_back(char((1000000u >> (8 * i)) & 0xFF));
    Request r;
    QVERIFY(!decodeRequest(payload, r));
}

void TestMemReadProtocol::rejectsImpossibleResultCount()
{
    std::string payload;
    payload.push_back(char(Status::Ok));
    payload.push_back(char(1));
    for (int i = 0; i < 4; ++i)
        payload.push_back(char((1000000u >> (8 * i)) & 0xFF));
    Response resp;
    QVERIFY(!decodeResponse(payload, resp));
}
