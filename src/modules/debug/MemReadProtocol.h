#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Wire format between the deployer and the stm32aid-memread sidecar.
//
// Why a sidecar at all: CubeProgrammer_API.dll links ST's own Qt 6.10.2
// (Core/Qml/SerialPort/Xml). This application is Qt 6.11, and Windows loads one
// DLL of a given name per process, so the API cannot be loaded in-process - it
// fails with "the specified procedure could not be found". The sidecar links no
// Qt at all, which lets ST's copies load cleanly beside it.
//
// Deliberately plain C++ with no Qt types, because this header is compiled into
// both the Qt application and the Qt-free sidecar. Pure and covered by
// TestMemReadProtocol.
namespace memread {

enum class Cmd : std::uint8_t {
    Ping       = 1,
    Connect    = 2,
    Read       = 3,
    Disconnect = 4,
};

enum class Status : std::uint8_t {
    Ok    = 0,
    Error = 1,
};

struct ReadRange {
    std::uint32_t id   = 0;
    std::uint64_t addr = 0;
    std::uint32_t len  = 0;
};

struct ReadResult {
    std::uint32_t id   = 0;
    std::uint64_t addr = 0;
    bool          ok   = false;
    std::string   data;     // raw bytes in memory order, empty when !ok
    std::string   error;
};

// A whole message is a 4-byte little-endian length followed by that many bytes.
// The length covers only the payload, so a reader always knows exactly how much
// to wait for on a stream that carries no message boundaries of its own.
constexpr std::size_t kLengthPrefixBytes = 4;

// Largest payload either side will accept, so a corrupt length cannot make the
// peer try to allocate gigabytes.
constexpr std::uint32_t kMaxPayloadBytes = 8u * 1024u * 1024u;

std::string frame(const std::string &payload);

// Returns how many bytes the payload announced at the front of `buffer` needs,
// or 0 when the prefix has not fully arrived yet. Sets `tooLarge` when the
// announced size exceeds kMaxPayloadBytes.
std::uint32_t peekPayloadSize(const std::string &buffer, bool &tooLarge);

// Pulls one complete framed message out of `buffer`, erasing what it consumed.
// False when the buffer does not hold a whole message yet.
bool takeMessage(std::string &buffer, std::string &payloadOut, bool &tooLarge);

// ── Requests ────────────────────────────────────────────────────────────────
std::string encodePing();
std::string encodeConnect(const std::string &serial);
std::string encodeRead(const std::vector<ReadRange> &ranges);
std::string encodeDisconnect();

struct Request {
    Cmd                    cmd = Cmd::Ping;
    std::string            serial;
    std::vector<ReadRange> ranges;
};

bool decodeRequest(const std::string &payload, Request &out);

// ── Responses ───────────────────────────────────────────────────────────────
std::string encodeOk(const std::string &text);
std::string encodeError(const std::string &message);
std::string encodeReadResults(const std::vector<ReadResult> &results);

struct Response {
    Status                  status = Status::Ok;
    std::string             text;      // Ok/Error message, empty for read replies
    std::vector<ReadResult> results;
    bool                    isRead = false;
};

bool decodeResponse(const std::string &payload, Response &out);

} // namespace memread
