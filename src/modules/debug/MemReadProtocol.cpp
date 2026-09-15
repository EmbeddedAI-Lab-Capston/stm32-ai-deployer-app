#include "MemReadProtocol.h"

namespace memread {
namespace {

void putU8(std::string &out, std::uint8_t v)
{
    out.push_back(static_cast<char>(v));
}

void putU32(std::string &out, std::uint32_t v)
{
    for (int i = 0; i < 4; ++i)
        out.push_back(static_cast<char>((v >> (8 * i)) & 0xFF));
}

void putU64(std::string &out, std::uint64_t v)
{
    for (int i = 0; i < 8; ++i)
        out.push_back(static_cast<char>((v >> (8 * i)) & 0xFF));
}

void putBytes(std::string &out, const std::string &bytes)
{
    putU32(out, static_cast<std::uint32_t>(bytes.size()));
    out.append(bytes);
}

// Bounds-checked cursor. Every getter refuses rather than reading past the end,
// so a truncated or hostile payload can only make decoding fail.
struct Cursor {
    const std::string &buf;
    std::size_t pos = 0;
    bool bad = false;

    explicit Cursor(const std::string &b) : buf(b) {}

    bool has(std::size_t n) const { return !bad && pos + n <= buf.size(); }

    std::uint8_t u8()
    {
        if (!has(1)) { bad = true; return 0; }
        return static_cast<std::uint8_t>(buf[pos++]);
    }

    std::uint32_t u32()
    {
        if (!has(4)) { bad = true; return 0; }
        std::uint32_t v = 0;
        for (int i = 0; i < 4; ++i)
            v |= static_cast<std::uint32_t>(static_cast<std::uint8_t>(buf[pos + i])) << (8 * i);
        pos += 4;
        return v;
    }

    std::uint64_t u64()
    {
        if (!has(8)) { bad = true; return 0; }
        std::uint64_t v = 0;
        for (int i = 0; i < 8; ++i)
            v |= static_cast<std::uint64_t>(static_cast<std::uint8_t>(buf[pos + i])) << (8 * i);
        pos += 8;
        return v;
    }

    std::string bytes()
    {
        const std::uint32_t n = u32();
        if (bad || !has(n)) { bad = true; return {}; }
        std::string s = buf.substr(pos, n);
        pos += n;
        return s;
    }
};

} // namespace

std::string frame(const std::string &payload)
{
    std::string out;
    out.reserve(kLengthPrefixBytes + payload.size());
    putU32(out, static_cast<std::uint32_t>(payload.size()));
    out.append(payload);
    return out;
}

std::uint32_t peekPayloadSize(const std::string &buffer, bool &tooLarge)
{
    tooLarge = false;
    if (buffer.size() < kLengthPrefixBytes)
        return 0;
    std::uint32_t size = 0;
    for (int i = 0; i < 4; ++i)
        size |= static_cast<std::uint32_t>(static_cast<std::uint8_t>(buffer[i])) << (8 * i);
    if (size > kMaxPayloadBytes) {
        tooLarge = true;
        return 0;
    }
    return size;
}

bool takeMessage(std::string &buffer, std::string &payloadOut, bool &tooLarge)
{
    const std::uint32_t size = peekPayloadSize(buffer, tooLarge);
    if (tooLarge)
        return false;
    if (buffer.size() < kLengthPrefixBytes)
        return false;
    if (buffer.size() < kLengthPrefixBytes + size)
        return false;
    payloadOut = buffer.substr(kLengthPrefixBytes, size);
    buffer.erase(0, kLengthPrefixBytes + size);
    return true;
}

std::string encodePing()
{
    std::string out;
    putU8(out, static_cast<std::uint8_t>(Cmd::Ping));
    return out;
}

std::string encodeConnect(const std::string &serial)
{
    std::string out;
    putU8(out, static_cast<std::uint8_t>(Cmd::Connect));
    putBytes(out, serial);
    return out;
}

std::string encodeRead(const std::vector<ReadRange> &ranges)
{
    std::string out;
    putU8(out, static_cast<std::uint8_t>(Cmd::Read));
    putU32(out, static_cast<std::uint32_t>(ranges.size()));
    for (const ReadRange &r : ranges) {
        putU32(out, r.id);
        putU64(out, r.addr);
        putU32(out, r.len);
    }
    return out;
}

std::string encodeDisconnect()
{
    std::string out;
    putU8(out, static_cast<std::uint8_t>(Cmd::Disconnect));
    return out;
}

bool decodeRequest(const std::string &payload, Request &out)
{
    out = Request{};
    Cursor c(payload);
    const std::uint8_t raw = c.u8();
    if (c.bad)
        return false;

    switch (raw) {
    case static_cast<std::uint8_t>(Cmd::Ping):       out.cmd = Cmd::Ping;       break;
    case static_cast<std::uint8_t>(Cmd::Connect):    out.cmd = Cmd::Connect;    break;
    case static_cast<std::uint8_t>(Cmd::Read):       out.cmd = Cmd::Read;       break;
    case static_cast<std::uint8_t>(Cmd::Disconnect): out.cmd = Cmd::Disconnect; break;
    default: return false;
    }

    if (out.cmd == Cmd::Connect) {
        out.serial = c.bytes();
    } else if (out.cmd == Cmd::Read) {
        const std::uint32_t count = c.u32();
        if (c.bad)
            return false;
        // A count is only believable if the remaining bytes could hold it; this
        // stops a bogus header from reserving a huge vector.
        if (static_cast<std::size_t>(count) * 16u > payload.size())
            return false;
        out.ranges.reserve(count);
        for (std::uint32_t i = 0; i < count; ++i) {
            ReadRange r;
            r.id   = c.u32();
            r.addr = c.u64();
            r.len  = c.u32();
            if (c.bad)
                return false;
            out.ranges.push_back(r);
        }
    }
    return !c.bad;
}

std::string encodeOk(const std::string &text)
{
    std::string out;
    putU8(out, static_cast<std::uint8_t>(Status::Ok));
    putU8(out, 0);          // not a read reply
    putBytes(out, text);
    return out;
}

std::string encodeError(const std::string &message)
{
    std::string out;
    putU8(out, static_cast<std::uint8_t>(Status::Error));
    putU8(out, 0);
    putBytes(out, message);
    return out;
}

std::string encodeReadResults(const std::vector<ReadResult> &results)
{
    std::string out;
    putU8(out, static_cast<std::uint8_t>(Status::Ok));
    putU8(out, 1);          // read reply
    putU32(out, static_cast<std::uint32_t>(results.size()));
    for (const ReadResult &r : results) {
        putU32(out, r.id);
        putU64(out, r.addr);
        putU8(out, r.ok ? 1 : 0);
        putBytes(out, r.data);
        putBytes(out, r.error);
    }
    return out;
}

bool decodeResponse(const std::string &payload, Response &out)
{
    out = Response{};
    Cursor c(payload);
    const std::uint8_t status = c.u8();
    const std::uint8_t isRead = c.u8();
    if (c.bad)
        return false;
    if (status != static_cast<std::uint8_t>(Status::Ok)
        && status != static_cast<std::uint8_t>(Status::Error))
        return false;

    out.status = static_cast<Status>(status);
    out.isRead = isRead != 0;

    if (!out.isRead) {
        out.text = c.bytes();
        return !c.bad;
    }

    const std::uint32_t count = c.u32();
    if (c.bad)
        return false;
    if (static_cast<std::size_t>(count) * 21u > payload.size())
        return false;
    out.results.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        ReadResult r;
        r.id    = c.u32();
        r.addr  = c.u64();
        r.ok    = c.u8() != 0;
        r.data  = c.bytes();
        r.error = c.bytes();
        if (c.bad)
            return false;
        out.results.push_back(r);
    }
    return !c.bad;
}

} // namespace memread
