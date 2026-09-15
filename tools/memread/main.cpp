// stm32aid-memread - read-only memory server for the STM32 AI Deployer.
//
// Speaks the MemReadProtocol framing over stdin/stdout and keeps one
// CubeProgrammer connection open, so the deployer can sample a running target
// at hundreds of hertz instead of paying ~312 ms of process startup per read
// through STM32_Programmer_CLI.
//
// It exists as a separate process because CubeProgrammer_API.dll links ST's own
// Qt 6.10.2 while the deployer is Qt 6.11, and Windows allows one DLL of a
// given name per process. Nothing here links Qt.
//
// Lifetime is tied to the parent by the pipe itself: when the deployer closes
// stdin, read() returns 0 and this exits. There is no port to allocate and no
// server left listening, which is exactly the failure mode that made
// ST-LINK_gdbserver lock up ST-Link probes on Windows.
//
// Usage: stm32aid-memread <cubeprogrammer-bin-dir>

#include <fcntl.h>
#include <io.h>
#include <stdio.h>

#include <string>
#include <vector>

#include "CubeProgReader.h"
#include "modules/debug/MemReadProtocol.h"

namespace {

bool writeAll(const std::string &bytes)
{
    std::size_t sent = 0;
    while (sent < bytes.size()) {
        const int n = _write(_fileno(stdout), bytes.data() + sent,
                             static_cast<unsigned>(bytes.size() - sent));
        if (n <= 0)
            return false;
        sent += static_cast<std::size_t>(n);
    }
    return fflush(stdout) == 0;
}

void respond(const std::string &payload)
{
    writeAll(memread::frame(payload));
}

void handle(CubeProgReader &reader, const std::string &binDir, const memread::Request &request)
{
    std::string error;

    switch (request.cmd) {
    case memread::Cmd::Ping:
        respond(memread::encodeOk("stm32aid-memread/1"));
        return;

    case memread::Cmd::Connect: {
        if (!reader.isLoaded() && !reader.load(binDir, error)) {
            respond(memread::encodeError(error));
            return;
        }
        if (!reader.connectProbe(request.serial, error)) {
            respond(memread::encodeError(error));
            return;
        }
        respond(memread::encodeOk(reader.connectedBoard()));
        return;
    }

    case memread::Cmd::Read: {
        std::vector<memread::ReadResult> results;
        results.reserve(request.ranges.size());
        for (const memread::ReadRange &range : request.ranges) {
            memread::ReadResult result;
            result.id = range.id;
            result.addr = range.addr;
            // The API addresses memory with 32 bits; anything above that is a
            // bad plan rather than something to truncate silently.
            if (range.addr > 0xFFFFFFFFull) {
                result.ok = false;
                result.error = "adres 32 bit disinda";
            } else {
                std::string data;
                result.ok = reader.readMemory(static_cast<std::uint32_t>(range.addr),
                                              range.len, data, result.error);
                if (result.ok)
                    result.data = std::move(data);
            }
            results.push_back(std::move(result));
        }
        respond(memread::encodeReadResults(results));
        return;
    }

    case memread::Cmd::Disconnect:
        reader.disconnectProbe();
        respond(memread::encodeOk("disconnected"));
        return;
    }

    respond(memread::encodeError("bilinmeyen komut"));
}

} // namespace

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "kullanim: stm32aid-memread <cubeprogrammer-bin-dir>\n");
        return 2;
    }
    const std::string binDir = argv[1];

    // Binary mode is mandatory: in text mode Windows would rewrite 0x0A bytes
    // inside memory contents, silently corrupting every read that happens to
    // contain one.
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);

    CubeProgReader reader;
    std::string buffer;
    char chunk[16384];

    for (;;) {
        const int n = _read(_fileno(stdin), chunk, sizeof(chunk));
        if (n <= 0)
            break;          // parent closed the pipe - shut down with it
        buffer.append(chunk, static_cast<std::size_t>(n));

        for (;;) {
            std::string payload;
            bool tooLarge = false;
            if (!memread::takeMessage(buffer, payload, tooLarge)) {
                if (tooLarge) {
                    // A length this wrong means the stream is out of sync;
                    // continuing would read garbage as commands.
                    fprintf(stderr, "cerceve bozuk, cikiliyor\n");
                    return 3;
                }
                break;
            }
            memread::Request request;
            if (!memread::decodeRequest(payload, request)) {
                respond(memread::encodeError("istek cozulemedi"));
                continue;
            }
            handle(reader, binDir, request);
        }
    }

    reader.disconnectProbe();
    return 0;
}
