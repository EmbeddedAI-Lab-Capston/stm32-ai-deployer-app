#include "CubeProgReader.h"

#include <windows.h>

#include <algorithm>
#include <cctype>

namespace {

// ── ST ABI, transcribed from CubeProgrammer_API.h (STM32CubeProgrammer 2.23.0) ──
//
// ST's header is proprietary and lives only inside the user's installation, so
// including it would make the build depend on CubeProgrammer being present.
// Only the types the read path needs are mirrored. A layout mismatch would show
// up as garbage in the fields getStLinkList() fills, which is what
// looksLikeText() checks before anything here trusts them.

enum DbgPort { DBG_JTAG = 0, DBG_SWD = 1 };
enum DbgConnect { NORMAL_MODE = 0, HOTPLUG_MODE = 1, UNDER_RESET_MODE = 2,
                  POWER_DOWN_MODE = 3, PRE_RESET_MODE = 4, HW_RST_PULSE_MODE = 5 };

struct Frequencies {
    unsigned int jtagFreq[12];
    unsigned int jtagFreqNumber;
    unsigned int swdFreq[12];
    unsigned int swdFreqNumber;
};

struct DebugConnectParameters {
    DbgPort     dbgPort;
    int         index;
    char        serialNumber[33];
    char        firmwareVersion[20];
    char        targetVoltage[5];
    int         accessPortNumber;
    int         accessPort;
    DbgConnect  connectionMode;
    int         resetMode;
    int         isOldFirmware;
    Frequencies freq;
    int         frequency;
    int         isBridge;
    int         shared;
    char        board[100];
    int         dbgSleep;
    int         speed;
};

struct DisplayCallbacks {
    void (*initProgressBar)();
    void (*logMessage)(int, const wchar_t *);
    void (*loadBar)(int, int);
};

void noProgressBar() {}
void noLogMessage(int, const wchar_t *) {}
void noLoadBar(int, int) {}

bool looksLikeText(const char *field, std::size_t capacity)
{
    for (std::size_t i = 0; i < capacity; ++i) {
        const unsigned char c = static_cast<unsigned char>(field[i]);
        if (c == '\0')
            return i > 0;
        if (c < 0x20 || c > 0x7E)
            return false;
    }
    return false;   // never terminated
}

std::string toLower(const std::string &s)
{
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

std::wstring widen(const std::string &s)
{
    if (s.empty())
        return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring out(static_cast<std::size_t>(n ? n - 1 : 0), L'\0');
    if (n > 1)
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &out[0], n);
    return out;
}

} // namespace

struct CubeProgReader::Impl
{
    HMODULE dll = nullptr;

    void (*setDisplayCallbacks)(DisplayCallbacks)                    = nullptr;
    void (*setVerbosityLevel)(int)                                   = nullptr;
    void (*setLoadersPath)(const char *)                             = nullptr;
    int  (*getStLinkList)(DebugConnectParameters **, int)            = nullptr;
    int  (*connectStLink)(DebugConnectParameters)                    = nullptr;
    int  (*readMemory)(unsigned int, unsigned char **, unsigned int) = nullptr;
    void (*freeLibraryMemory)(void *)                                = nullptr;
    void (*disconnect)()                                             = nullptr;

    DebugConnectParameters *probes = nullptr;
    int                     probeCount = 0;
};

CubeProgReader::CubeProgReader() : m_d(new Impl) {}

CubeProgReader::~CubeProgReader()
{
    disconnectProbe();
    delete m_d;
}

std::vector<std::string> CubeProgReader::allowedSymbols()
{
    return {
        "setDisplayCallbacks",
        "setVerbosityLevel",
        "setLoadersPath",
        "getStLinkList",
        "connectStLink",
        "readMemory",
        "freeLibraryMemory",
        "disconnect",
    };
}

bool CubeProgReader::isMutatingSymbol(const std::string &name)
{
    static const char *fragments[] = {
        "write", "erase", "reset", "download", "program", "option",
        "fuse", "provision", "execute", "run", "halt", "blank",
    };
    const std::string lowered = toLower(name);
    for (const char *fragment : fragments) {
        if (lowered.find(fragment) != std::string::npos)
            return true;
    }
    return false;
}

bool CubeProgReader::load(const std::string &binDir, std::string &error)
{
    if (m_loaded)
        return true;
    if (binDir.empty()) {
        error = "CubeProgrammer bin dizini bos.";
        return false;
    }

    std::string dir = binDir;
    while (!dir.empty() && (dir.back() == '/' || dir.back() == '\\'))
        dir.pop_back();
    const std::string dllPath = dir + "\\CubeProgrammer_API.dll";

    // The API pulls in a dozen siblings from its own folder (STLinkUSBDriver,
    // ST's Qt copies, OpenSSL). Loading by absolute path is not enough - those
    // are resolved through the search path, so without this the load fails with
    // a bare "module could not be found" naming the DLL we did find.
    const std::wstring wideDir = widen(dir);
    SetDllDirectoryW(wideDir.c_str());
    m_d->dll = LoadLibraryW(widen(dllPath).c_str());
    const DWORD loadError = GetLastError();
    SetDllDirectoryW(nullptr);

    if (!m_d->dll) {
        error = "CubeProgrammer_API.dll yuklenemedi (" + std::to_string(loadError) + "): " + dllPath;
        return false;
    }

    // Resolution goes strictly through the whitelist, so a mutating entry point
    // stays unreachable even if someone later adds a call for one.
    const std::vector<std::string> allowed = allowedSymbols();
    auto resolve = [&](const char *name) -> FARPROC {
        const std::string key(name);
        const bool permitted =
            std::find(allowed.begin(), allowed.end(), key) != allowed.end();
        if (!permitted || isMutatingSymbol(key))
            return nullptr;
        return GetProcAddress(m_d->dll, name);
    };

    m_d->setDisplayCallbacks = reinterpret_cast<void (*)(DisplayCallbacks)>(resolve("setDisplayCallbacks"));
    m_d->setVerbosityLevel   = reinterpret_cast<void (*)(int)>(resolve("setVerbosityLevel"));
    m_d->setLoadersPath      = reinterpret_cast<void (*)(const char *)>(resolve("setLoadersPath"));
    m_d->getStLinkList       = reinterpret_cast<int (*)(DebugConnectParameters **, int)>(resolve("getStLinkList"));
    m_d->connectStLink       = reinterpret_cast<int (*)(DebugConnectParameters)>(resolve("connectStLink"));
    m_d->readMemory          = reinterpret_cast<int (*)(unsigned int, unsigned char **, unsigned int)>(resolve("readMemory"));
    m_d->freeLibraryMemory   = reinterpret_cast<void (*)(void *)>(resolve("freeLibraryMemory"));
    m_d->disconnect          = reinterpret_cast<void (*)()>(resolve("disconnect"));

    if (!m_d->setDisplayCallbacks || !m_d->getStLinkList || !m_d->connectStLink
        || !m_d->readMemory || !m_d->freeLibraryMemory || !m_d->disconnect) {
        FreeLibrary(m_d->dll);
        m_d->dll = nullptr;
        error = "CubeProgrammer_API.dll beklenen fonksiyonlari sunmuyor.";
        return false;
    }

    // Without callbacks the API writes to stdout - which is our protocol
    // channel - and can call through null pointers.
    DisplayCallbacks callbacks{noProgressBar, noLogMessage, noLoadBar};
    m_d->setDisplayCallbacks(callbacks);
    if (m_d->setVerbosityLevel)
        m_d->setVerbosityLevel(0);
    // connectStLink fails with -545 unless the API can find its device
    // database, which it resolves relative to this path.
    if (m_d->setLoadersPath)
        m_d->setLoadersPath(dir.c_str());

    m_loaded = true;
    return true;
}

std::vector<std::string> CubeProgReader::probeSerials(std::string &error)
{
    if (!m_loaded) {
        error = "CubeProgrammer API yuklenmedi.";
        return {};
    }

    m_d->probes = nullptr;
    m_d->probeCount = m_d->getStLinkList(&m_d->probes, 0);
    if (m_d->probeCount <= 0 || !m_d->probes) {
        error = "ST-Link bulunamadi.";
        return {};
    }

    std::vector<std::string> serials;
    for (int i = 0; i < m_d->probeCount; ++i) {
        const DebugConnectParameters &p = m_d->probes[i];
        if (!looksLikeText(p.serialNumber, sizeof(p.serialNumber))) {
            error = "ST-Link listesi okunamadi - CubeProgrammer surumu beklenenden farkli olabilir.";
            return {};
        }
        serials.emplace_back(p.serialNumber);
    }
    return serials;
}

bool CubeProgReader::connectProbe(const std::string &serial, std::string &error)
{
    if (!m_loaded) {
        error = "CubeProgrammer API yuklenmedi.";
        return false;
    }
    if (m_connected)
        disconnectProbe();

    const std::vector<std::string> serials = probeSerials(error);
    if (serials.empty())
        return false;

    int index = -1;
    const std::string wanted = toLower(serial);
    for (std::size_t i = 0; i < serials.size(); ++i) {
        if (toLower(serials[i]) == wanted) { index = static_cast<int>(i); break; }
    }
    if (index < 0 && serial.empty() && serials.size() == 1)
        index = 0;
    if (index < 0) {
        error = "ST-Link bulunamadi: " + serial;
        return false;
    }

    DebugConnectParameters params = m_d->probes[index];
    params.connectionMode = HOTPLUG_MODE;   // leaves a running target running
    params.shared = 0;

    const int rc = m_d->connectStLink(params);
    if (rc != 0) {
        error = "ST-Link baglantisi basarisiz (kod " + std::to_string(rc) + ").";
        return false;
    }

    m_connected = true;
    m_serial = serials[static_cast<std::size_t>(index)];
    m_board = looksLikeText(params.board, sizeof(params.board)) ? std::string(params.board) : std::string();
    return true;
}

bool CubeProgReader::readMemory(std::uint32_t address, std::uint32_t size,
                                std::string &out, std::string &error)
{
    out.clear();
    if (!m_connected) {
        error = "Baglanti yok.";
        return false;
    }
    if (size == 0)
        return true;

    unsigned char *buffer = nullptr;
    const int rc = m_d->readMemory(address, &buffer, size);
    if (rc != 0 || !buffer) {
        char addrText[32];
        snprintf(addrText, sizeof(addrText), "0x%08X", address);
        error = std::string(addrText) + " adresinden " + std::to_string(size)
                + " bayt okunamadi (kod " + std::to_string(rc) + ").";
        return false;
    }

    out.assign(reinterpret_cast<const char *>(buffer), size);
    // ST allocates a fresh buffer per call; without this a sampler running for
    // minutes would leak steadily.
    m_d->freeLibraryMemory(buffer);
    return true;
}

void CubeProgReader::disconnectProbe()
{
    if (m_connected && m_d && m_d->disconnect)
        m_d->disconnect();
    m_connected = false;
    m_serial.clear();
    m_board.clear();
}
