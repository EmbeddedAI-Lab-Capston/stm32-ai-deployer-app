#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Read-only target access through ST's CubeProgrammer C API, holding one
// connection open.
//
// Deliberately free of Qt. CubeProgrammer_API.dll links ST's own Qt 6.10.2
// (Core/Qml/SerialPort/Xml); the deployer is Qt 6.11, and Windows loads a
// single DLL of a given name per process, so the two cannot share one. Keeping
// this binary Qt-free lets ST's copies load beside it without a clash - which
// is the entire reason the sidecar exists.
//
// OBSERVER PRINCIPLE (CLAUDE.md): only the symbols in allowedSymbols() are ever
// resolved from the DLL. writeMemory, massErase, sendResetCommand and the rest
// are never looked up, so this process cannot modify or reset a target even if
// a later change tried to - the same structural guarantee that
// GdbRspCodec::isAllowedOutgoing() gives the gdbserver path.
class CubeProgReader
{
public:
    CubeProgReader();
    ~CubeProgReader();

    CubeProgReader(const CubeProgReader &) = delete;
    CubeProgReader &operator=(const CubeProgReader &) = delete;

    static std::vector<std::string> allowedSymbols();
    static bool isMutatingSymbol(const std::string &name);

    // `binDir` is the folder holding STM32_Programmer_CLI.exe.
    bool load(const std::string &binDir, std::string &error);
    bool isLoaded() const { return m_loaded; }

    std::vector<std::string> probeSerials(std::string &error);

    // Hot-plug connect: a running target is neither halted nor reset. An empty
    // serial takes the only probe when exactly one is present.
    bool connectProbe(const std::string &serial, std::string &error);
    bool isConnected() const { return m_connected; }
    std::string connectedBoard() const { return m_board; }
    std::string connectedSerial() const { return m_serial; }

    bool readMemory(std::uint32_t address, std::uint32_t size, std::string &out, std::string &error);

    void disconnectProbe();

private:
    struct Impl;
    Impl       *m_d = nullptr;
    bool        m_loaded = false;
    bool        m_connected = false;
    std::string m_serial;
    std::string m_board;
};
