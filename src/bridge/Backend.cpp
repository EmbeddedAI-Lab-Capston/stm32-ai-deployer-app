#include "Backend.h"

#include <algorithm>

#include <QSerialPortInfo>
#include <QFileInfo>
#include <QDir>
#include <QLocale>
#include <QRandomGenerator>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QProcess>
#include <QRegularExpression>
#include <QTextStream>
#include <QUrl>

#include "core/AppState.h"
#include "core/AppSettings.h"
#include "core/ToolDetector.h"
#include "modules/serial/SerialManager.h"
#include "modules/serial/PacketParser.h"
#include "modules/flash/FlashManager.h"
#include "modules/flash/PipelineRunner.h"
#include "modules/analysis/AnalysisManager.h"
#include "modules/board/BoardPresets.h"

namespace
{
constexpr quint16 kStVendorId = 0x0483;
constexpr quint16 kN657VcpProductId = 0x3754;
constexpr quint16 kH7VcpProductId = 0x374E;

QString boardSearchText(const BoardInfo &board)
{
    return QStringList{board.name,
                       board.probeBoardName,
                       board.deviceName,
                       board.deviceCpu}
        .join(' ')
        .toUpper();
}

bool boardLooksLikeN6(const BoardInfo &board)
{
    const QString text = boardSearchText(board);
    return text.contains("N657")
        || text.contains("N655")
        || text.contains("STM32N6")
        || text.contains("NUCLEO-N6")
        || text.contains("CORTEX-M55")
        || text.contains("NPU");
}

bool boardLooksLikeH7(const BoardInfo &board)
{
    const QString text = boardSearchText(board);
    return text.contains("H723")
        || text.contains("STM32H7")
        || text.contains("NUCLEO-H7")
        || text.contains("CORTEX-M7");
}

bool isStVcp(const QSerialPortInfo &info)
{
    return info.hasVendorIdentifier() && info.vendorIdentifier() == kStVendorId;
}

bool hasProductId(const QSerialPortInfo &info, quint16 productId)
{
    return isStVcp(info) && info.hasProductIdentifier() && info.productIdentifier() == productId;
}

QString cleanPortName(const QString &portName)
{
    return portName.section(' ', 0, 0).trimmed();
}

QSerialPortInfo preferredSerialPortForBoard(const BoardInfo &board, const QString &requestedPort = {})
{
    const auto ports = SerialManager::availablePorts();
    const QString requested = cleanPortName(requestedPort);

    for (const QSerialPortInfo &info : ports) {
        if (!requested.isEmpty() && info.portName().compare(requested, Qt::CaseInsensitive) == 0)
            return info;
    }

    if (!board.portName.isEmpty()) {
        const QString boardPort = cleanPortName(board.portName);
        for (const QSerialPortInfo &info : ports) {
            if (info.portName().compare(boardPort, Qt::CaseInsensitive) == 0)
                return info;
        }
    }

    if (!board.stlinkSn.isEmpty()) {
        for (const QSerialPortInfo &info : ports) {
            if (info.serialNumber().compare(board.stlinkSn, Qt::CaseInsensitive) == 0)
                return info;
        }
    }

    if (boardLooksLikeN6(board)) {
        for (const QSerialPortInfo &info : ports) {
            if (hasProductId(info, kN657VcpProductId))
                return info;
        }
    }

    if (boardLooksLikeH7(board)) {
        for (const QSerialPortInfo &info : ports) {
            if (hasProductId(info, kH7VcpProductId))
                return info;
        }
    }

    for (const QSerialPortInfo &info : ports) {
        if (isStVcp(info))
            return info;
    }

    return {};
}

int preferredBaudForBoard(const BoardInfo &board, int requestedBaud)
{
    if (boardLooksLikeN6(board))
        return 115200;
    return requestedBaud > 0 ? requestedBaud : 115200;
}

QString programmerSnForBoard(const QString &cliPath, const BoardInfo &board)
{
    if (cliPath.isEmpty() || (!boardLooksLikeN6(board) && !boardLooksLikeH7(board)))
        return {};

    QProcess process;
    process.start(cliPath, {QStringLiteral("-l")});
    if (!process.waitForFinished(4000))
        return {};

    const QString output = QString::fromLocal8Bit(process.readAllStandardOutput()
                                                  + process.readAllStandardError());
    QString currentSn;
    bool currentMatches = false;
    auto flushBlock = [&]() -> QString {
        if (currentMatches && !currentSn.isEmpty())
            return currentSn;
        return {};
    };

    for (const QString &line : output.split(QRegularExpression("[\\r\\n]+"), Qt::SkipEmptyParts)) {
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith("ST-LINK SN", Qt::CaseInsensitive)) {
            const QString matched = flushBlock();
            if (!matched.isEmpty())
                return matched;
            currentSn = trimmed.section(':', 1).trimmed();
            currentMatches = false;
            continue;
        }

        const QString upper = trimmed.toUpper();
        if (boardLooksLikeN6(board)
            && (upper.contains("N657") || upper.contains("NUCLEO-N657") || upper.contains("STM32N6"))) {
            currentMatches = true;
        } else if (boardLooksLikeH7(board)
                   && (upper.contains("H723") || upper.contains("NUCLEO-H7") || upper.contains("STM32H7"))) {
            currentMatches = true;
        }
    }

    return flushBlock();
}
}

Backend::Backend(AppState        *state,
                 SerialManager   *serial,
                 FlashManager    *flash,
                 AnalysisManager *analysis,
                 QObject         *parent)
    : QObject(parent)
    , m_state(state)
    , m_serial(serial)
    , m_flash(flash)
    , m_analysis(analysis)
{
    m_simTimer  = new QTimer(this);
    m_simParser = new PacketParser(this);
    m_hwSimTimer = new QTimer(this);
    m_benchmarkTimeout = new QTimer(this);
    m_pipelinePulseTimer = new QTimer(this);
    m_benchmarkTimeout->setSingleShot(true);

    connect(m_simTimer, &QTimer::timeout, this, &Backend::tickSimulation);
    connect(m_hwSimTimer, &QTimer::timeout, this, &Backend::tickHardwareSimulation);
    connect(m_pipelinePulseTimer, &QTimer::timeout, this, [this]() {
        if (!m_pipelineBusy) return;
        const int cap = m_pipelineStage.contains("5/", Qt::CaseInsensitive) ? 95
                      : m_pipelineStage.contains("4/", Qt::CaseInsensitive) ? 82
                      : m_pipelineStage.contains("3/", Qt::CaseInsensitive) ? 55
                      : m_pipelineStage.contains("2/", Qt::CaseInsensitive) ? 35
                      : 15;
        if (m_pipelineProgress < cap) {
            m_pipelineProgress = qMin(cap, m_pipelineProgress + 1);
            emit pipelineChanged();
        }
    });
    m_pipelinePulseTimer->setInterval(1200);
    connect(m_benchmarkTimeout, &QTimer::timeout, this, [this]() {
        if (!m_benchmarkBusy) return;
        appendBenchmarkLine("Benchmark yanıtı alınamadı. Firmware BENCH komutunu destekliyor mu?", "warn");
        m_benchmarkBusy = false;
        emit benchmarkChanged();
    });

    wireSerial();
    wireFlash();
    wireAnalysis();

    // Pre-populate analysis with sample data so the tables aren't empty on first launch.
    seedAnalysisIfEmpty();
}

// ── Tool paths ──────────────────────────────────────────────────────────────
QVariantList Backend::toolPaths() const
{
    AppSettings s;
    auto entry = [](const QString &name, const QString &key, const QString &path) {
        QVariantMap m;
        m["name"]  = name;
        m["key"]   = key;
        m["path"]  = path;
        m["found"] = !path.isEmpty();
        return QVariant(m);
    };
    QVariantList list;
    list << entry("STM32_Programmer_CLI", "programmer/cli_path",    s.programmerCliPath());
    list << entry("stedgeai (X-CUBE-AI)", "tools/xcubeai_cli_path", s.xcubeAICliPath());
    list << entry("arm-none-eabi-gcc",    "tools/gcc_path",         s.gccPath());
    list << entry("make",                 "tools/make_path",        s.makePath());
    return list;
}

void Backend::setToolPath(const QString &key, const QString &path)
{
    AppSettings s;
    if (key == "programmer/cli_path")         s.setProgrammerCliPath(path);
    else if (key == "tools/xcubeai_cli_path") s.setXCubeAICliPath(path);
    else if (key == "tools/gcc_path")         s.setGccPath(path);
    else if (key == "tools/make_path")        s.setMakePath(path);
    if (m_flash && key == "programmer/cli_path")
        m_flash->setCliPath(path);
    emit toolPathsChanged();
}

void Backend::scanTools()
{
    if (m_scanning) return;
    m_scanning = true;
    emit scanningChanged();

    if (!m_detector)
        m_detector = new ToolDetector(this);

    connect(m_detector, &ToolDetector::toolFound, this,
            [this](const ToolInfo &info) {
                if (!info.found) return;
                AppSettings s;
                if      (info.key == "tools/gcc_path")            s.setGccPath(info.path);
                else if (info.key == "tools/make_path")           s.setMakePath(info.path);
                else if (info.key == "programmer/cli_path")       s.setProgrammerCliPath(info.path);
                else if (info.key == "tools/xcubeai_cli_path")    s.setXCubeAICliPath(info.path);
                emit statusMessage(QString("✓ %1 bulundu").arg(info.name));
            }, Qt::UniqueConnection);

    connect(m_detector, &ToolDetector::detectionFinished, this,
            [this](const QList<ToolInfo> &) {
                AppSettings s; s.setToolsAutoDetected(true);
                if (m_flash) m_flash->setCliPath(AppSettings().programmerCliPath());
                m_scanning = false;
                emit scanningChanged();
                emit toolPathsChanged();
            }, Qt::UniqueConnection);

    m_detector->detectAll();
}

// ── Serial ──────────────────────────────────────────────────────────────────
QStringList Backend::availablePorts() const
{
    QStringList out;
    for (const QSerialPortInfo &p : SerialManager::availablePorts()) {
        QString label = p.portName();
        if (p.vendorIdentifier() == 0x0483)
            label += " (ST-Link)";
        else if (!p.description().isEmpty())
            label += " · " + p.description();
        out << label;
    }
    return out;
}

QVariantList Backend::availablePortEntries() const
{
    QVariantList out;
    for (const QSerialPortInfo &p : SerialManager::availablePorts()) {
        QVariantMap m;
        const bool isStlink = isStVcp(p);
        QString label = p.portName();
        if (isStlink)
            label += " (ST-Link)";
        else if (!p.description().isEmpty())
            label += " · " + p.description();
        QString role;
        if (hasProductId(p, kN657VcpProductId)) {
            role = "NUCLEO-N657 VCP";
            label = p.portName() + " (NUCLEO-N657 VCP)";
        } else if (hasProductId(p, kH7VcpProductId)) {
            role = "ST-Link VCP";
            label = p.portName() + " (ST-Link VCP)";
        } else if (isStlink) {
            role = "ST-Link";
        }
        m["label"] = label;
        m["portName"] = p.portName();
        m["isStlink"] = isStlink;
        m["description"] = p.description();
        m["serialNumber"] = p.serialNumber();
        m["vendorId"] = p.hasVendorIdentifier() ? p.vendorIdentifier() : 0;
        m["productId"] = p.hasProductIdentifier() ? p.productIdentifier() : 0;
        m["role"] = role;
        out.append(m);
    }
    return out;
}

QString Backend::detectedStLinkPort() const
{
    const QSerialPortInfo info = preferredSerialPortForBoard(m_state ? m_state->activeBoard() : BoardInfo{});
    return info.isNull() ? QString() : info.portName();
}

void Backend::connectSerial(const QString &portName, int baud)
{
    if (!m_serial) return;
    QString cleanPort = cleanPortName(portName);
    if (cleanPort.isEmpty()) {
        const QSerialPortInfo preferred = preferredSerialPortForBoard(m_state ? m_state->activeBoard() : BoardInfo{});
        cleanPort = preferred.portName();
    }
    if (cleanPort.isEmpty()) return;
    const BoardInfo activeBoard = m_state ? m_state->activeBoard() : BoardInfo{};
    const int actualBaud = preferredBaudForBoard(activeBoard, baud);
    if (m_state) {
        m_state->setActivePort(cleanPort);
        m_state->setActiveBaud(actualBaud);
    }
    AppSettings().setLastBaud(actualBaud);
    m_serial->connectToPort(cleanPort, actualBaud);
}

void Backend::disconnectSerial()
{
    if (m_serial) m_serial->disconnectPort();
}

void Backend::probeStLinkBoard()
{
    probeStLinkBoardForPort(m_state ? m_state->activePort() : QString());
}

void Backend::probeStLinkBoardForPort(const QString &portName)
{
    if (m_probeBusy) return;

    const BoardInfo activeBoard = m_state ? m_state->activeBoard() : BoardInfo{};
    QSerialPortInfo stlink = preferredSerialPortForBoard(activeBoard, portName);
    if (stlink.isNull()) {
        m_probeStatus = "ST-Link portu bulunamadı.";
        emit probeChanged();
        emit probeFinished(false, m_probeStatus);
        return;
    }

    AppSettings settings;
    QString cliPath = settings.programmerCliPath();
    if (cliPath.isEmpty()) {
        cliPath = FlashManager::detectCliPath();
        if (!cliPath.isEmpty())
            settings.setProgrammerCliPath(cliPath);
    }
    if (cliPath.isEmpty()) {
        m_probeStatus = "STM32_Programmer_CLI bulunamadı.";
        emit probeChanged();
        emit probeFinished(false, m_probeStatus);
        return;
    }

    if (m_stlinkProbe) {
        m_stlinkProbe->kill();
        m_stlinkProbe->deleteLater();
    }

    m_probeBusy = true;
    m_probeStatus = "ST-Link taranıyor...";
    emit probeChanged();

    const QString selectedPort = stlink.portName();
    QString selectedSn = stlink.serialNumber();
    if (selectedSn.isEmpty())
        selectedSn = programmerSnForBoard(cliPath, activeBoard);

    m_stlinkProbe = new QProcess(this);
    connect(m_stlinkProbe, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus) {
                const QString output = QString::fromLocal8Bit(
                    m_stlinkProbe->readAllStandardOutput() + m_stlinkProbe->readAllStandardError());
                m_stlinkProbe->deleteLater();
                m_stlinkProbe = nullptr;
                m_probeBusy = false;
                if (exitCode == 0 && !output.trimmed().isEmpty()) {
                    applyDetectedStLinkBoard(output);
                    m_probeStatus = "ST-Link kart bilgisi okundu.";
                    emit probeChanged();
                    emit probeFinished(true, m_probeStatus);
                } else {
                    m_probeStatus = "ST-Link probe başarısız.";
                    emit probeChanged();
                    emit probeFinished(false, m_probeStatus);
                }
            });
    QStringList args{QStringLiteral("-c"), QStringLiteral("port=SWD")};
    if (!selectedSn.isEmpty())
        args << QString("sn=%1").arg(selectedSn);
    appendMonitorLine(QString("[probe] ST-Link %1%2")
                          .arg(selectedPort.isEmpty() ? QString("SWD") : selectedPort,
                               selectedSn.isEmpty() ? QString() : QString(" · SN %1").arg(selectedSn)),
                      "cmd");
    m_stlinkProbe->start(cliPath, args);
}

void Backend::applyDetectedStLinkBoard(const QString &probeOutput)
{
    QString boardName;
    QString deviceName;
    QString deviceId;
    QString revisionId;
    QString nvmSize;
    QString cpuName;
    QString stlinkSn;
    QString stlinkFw;
    QString voltage;

    for (const QString &line : probeOutput.split(QRegularExpression("[\\r\\n]+"), Qt::SkipEmptyParts)) {
        const QString trimmed = line.trimmed();
        const int sep = trimmed.indexOf(':');
        if (sep < 0) continue;

        const QString key = trimmed.left(sep).trimmed();
        const QString value = trimmed.mid(sep + 1).trimmed();
        if (key.compare("Board", Qt::CaseInsensitive) == 0) boardName = value;
        else if (key.compare("ST-LINK SN", Qt::CaseInsensitive) == 0) stlinkSn = value;
        else if (key.compare("ST-LINK FW", Qt::CaseInsensitive) == 0) stlinkFw = value;
        else if (key.compare("Voltage", Qt::CaseInsensitive) == 0) voltage = value;
        else if (key.compare("Device name", Qt::CaseInsensitive) == 0) deviceName = value;
        else if (key.compare("Device ID", Qt::CaseInsensitive) == 0) deviceId = value;
        else if (key.compare("Revision ID", Qt::CaseInsensitive) == 0) revisionId = value;
        else if (key.compare("NVM size", Qt::CaseInsensitive) == 0) nvmSize = value;
        else if (key.compare("Device CPU", Qt::CaseInsensitive) == 0) cpuName = value;
    }

    const QString lookup = QStringList{boardName, deviceName, deviceId, cpuName}.join(' ');
    BoardInfo board = BoardPresets::find(lookup);
    if (board.isNull() && !boardName.isEmpty()) board = BoardPresets::find(boardName);
    if (board.isNull() && !deviceName.isEmpty()) board = BoardPresets::find(deviceName);
    if (board.isNull()) {
        board.name = boardName.isEmpty() ? deviceName : boardName;
        board.isPreset = false;
    }

    board.probeBoardName = boardName;
    board.deviceId = deviceId;
    board.revisionId = revisionId;
    board.deviceName = deviceName;
    board.nvmSize = nvmSize;
    board.deviceCpu = cpuName;
    board.stlinkSn = stlinkSn;
    board.stlinkFw = stlinkFw;
    board.voltage = voltage;
    const QSerialPortInfo preferredPort = preferredSerialPortForBoard(board);
    board.portName = preferredPort.isNull() ? detectedStLinkPort() : preferredPort.portName();

    if (m_state && !board.name.isEmpty()) {
        m_state->setActiveBoard(board);
        if (!board.portName.isEmpty())
            m_state->setActivePort(board.portName);
        m_state->setActiveBaud(preferredBaudForBoard(board, m_state->activeBaud()));
    }
}

void Backend::selectBoard(const QString &boardName)
{
    if (!m_state) return;
    BoardInfo board = BoardPresets::find(boardName);
    if (board.isNull()) {
        for (const BoardInfo &custom : AppSettings().customBoards()) {
            if (custom.name.compare(boardName, Qt::CaseInsensitive) == 0) {
                board = custom;
                break;
            }
        }
    }
    if (board.isNull()) {
        board.name = boardName;
        board.isPreset = false;
    }
    m_state->setActiveBoard(board);
    if (boardLooksLikeN6(board))
        m_state->setActiveBaud(115200);
}

void Backend::addCustomBoard(const QString &name, int flashKb, int ramKb, int clockMhz)
{
    const QString cleanName = name.trimmed();
    if (cleanName.isEmpty()) return;

    BoardInfo board;
    board.name = cleanName;
    board.flashKb = qMax(0, flashKb);
    board.ramKb = qMax(0, ramKb);
    board.clockMhz = qMax(0, clockMhz);
    board.isPreset = false;

    AppSettings().addCustomBoard(board);
    if (m_state)
        m_state->setActiveBoard(board);
}

QVariantList Backend::customBoards() const
{
    AppSettings s;
    QVariantList out;
    for (const BoardInfo &b : s.customBoards()) {
        QVariantMap m;
        m["name"]     = b.name;
        m["flashKb"]  = b.flashKb;
        m["ramKb"]    = b.ramKb;
        m["clockMhz"] = b.clockMhz;
        out.append(m);
    }
    return out;
}

void Backend::seedAnalysisIfEmpty()
{
    if (!m_analysis || !m_analysis->isOpen()) return;

    // Benchmark seed rows (matches AnalysisTab's populateBenchmarkData)
    struct SeedRow { const char *date; const char *session; const char *board; const char *chip; const char *cpu; const char *model; const char *type; const char *sensor; const char *avg; const char *ram; const char *weights; const char *result; };
    static const SeedRow benchRows[] = {
        {"2026-05-24 12:41","BENCH-001","NUCLEO-H723ZG","STM32H723ZG","Cortex-M7","anomaly_cnn","INT8","BME280","0.934 ms","6.44 KiB","6.70 KiB","Tamamlandı"},
        {"2026-05-24 11:58","BENCH-002","NUCLEO-H723ZG","STM32H723ZG","Cortex-M7","anomaly_cnn","Float32","BME280","2.84 ms","18.2 KiB","25.2 KiB","Tamamlandı"},
        {"2026-05-22 18:12","BENCH-003","STM32F407 Discovery","STM32F407VG","Cortex-M4","har_mlp","INT8","MPU6050","8.20 ms","3.00 KiB","12.4 KiB","Tamamlandı"},
        {"2026-05-20 16:40","BENCH-004","NUCLEO-N657X0-Q","STM32N657","Cortex-M55/NPU","kws_lstm","INT8","PDM_MIC","0.61 ms","22.1 KiB","96.8 KiB","Tamamlandı"},
    };
    if (m_analysis->records("benchmark").isEmpty()) {
        for (const auto &r : benchRows)
            m_analysis->addRecord("benchmark", {r.date,r.session,r.board,r.chip,r.cpu,r.model,r.type,r.sensor,r.avg,r.ram,r.weights,r.result});
    }

    static const SeedRow simRows[] = {
        {"2026-05-23 10:00","SIM-001","NUCLEO-H723ZG","STM32H723ZG","Cortex-M7","anomaly_cnn","INT8","Simülasyon","0.940 ms","6.44 KiB","--","normal  96%"},
        {"2026-05-23 10:01","SIM-002","NUCLEO-H723ZG","STM32H723ZG","Cortex-M7","anomaly_cnn","INT8","Simülasyon","0.951 ms","6.44 KiB","--","anomaly  94%"},
        {"2026-05-23 10:02","SIM-003","NUCLEO-H723ZG","STM32H723ZG","Cortex-M7","anomaly_cnn","INT8","Simülasyon","0.921 ms","6.44 KiB","--","normal  98%"},
    };
    if (m_analysis->records("simulation").isEmpty()) {
        for (const auto &r : simRows)
            m_analysis->addRecord("simulation", {r.date,r.session,r.board,r.chip,r.cpu,r.model,r.type,r.sensor,r.avg,r.ram,r.weights,r.result});
    }

    static const SeedRow sensorRows[] = {
        {"2026-05-22 09:15","REAL-LIVE-1","NUCLEO-H723ZG","STM32H723ZG","Cortex-M7","anomaly_cnn","INT8","BME280","0.934 ms","6.44 KiB","--","normal  90%"},
        {"2026-05-22 09:32","REAL-LIVE-2","NUCLEO-H723ZG","STM32H723ZG","Cortex-M7","anomaly_cnn","INT8","BME280","0.941 ms","6.44 KiB","--","anomaly  88%"},
    };
    if (m_analysis->records("sensor").isEmpty()) {
        for (const auto &r : sensorRows)
            m_analysis->addRecord("sensor", {r.date,r.session,r.board,r.chip,r.cpu,r.model,r.type,r.sensor,r.avg,r.ram,r.weights,r.result});
    }

    emit analysisChanged();
}

// ── Monitor terminal ────────────────────────────────────────────────────────
void Backend::clearMonitor()
{
    m_monitorLines.clear();
    emit monitorLinesChanged();
}

void Backend::appendMonitorLine(const QString &text, const QString &type)
{
    QVariantMap m;
    m["text"] = text;
    m["type"] = type;
    m_monitorLines.append(m);
    if (m_monitorLines.size() > kMaxMonitorLines)
        m_monitorLines.removeFirst();
    emit monitorLinesChanged();
}

// ── Simulation ──────────────────────────────────────────────────────────────
void Backend::startSimulation(int intervalMs, double minVal, double maxVal)
{
    if (m_simRunning) return;
    m_simMin     = minVal;
    m_simMax     = maxVal;
    m_simUptime  = 0;
    m_simRunning = true;
    emit simRunningChanged();
    appendMonitorLine(QString("[sim] Simülasyon başladı · aralık %1 ms").arg(intervalMs), "cmd");
    m_simTimer->start(intervalMs);
}

void Backend::stopSimulation()
{
    if (!m_simRunning) return;
    m_simTimer->stop();
    m_simRunning = false;
    emit simRunningChanged();
    appendMonitorLine("[sim] Simülasyon durduruldu.", "warn");
}

void Backend::startHardwareSimulation(int intervalMs, double minVal, double maxVal)
{
    if (!m_serial) return;
    if (m_hwSimRunning) return;
    if (!m_state) return;
    QString port = m_state->activePort();
    if (port.isEmpty()) {
        const QSerialPortInfo preferred = preferredSerialPortForBoard(m_state->activeBoard());
        port = preferred.portName();
        if (!port.isEmpty()) {
            m_state->setActivePort(port);
            m_state->setActiveBaud(preferredBaudForBoard(m_state->activeBoard(), m_state->activeBaud()));
            appendMonitorLine("[hw-sim] Otomatik VCP secildi: " + port, "cmd");
        }
    }
    if (port.isEmpty()) {
        appendMonitorLine("[hw-sim] Aktif COM port yok.", "err");
        return;
    }
    m_hwSimMin = minVal;
    m_hwSimMax = maxVal;
    m_hwSimSeed = 1234;
    m_hwSimSentCount = 0;
    m_hwSimResponseCount = 0;
    m_hwSimRunning = true;
    appendMonitorLine(QString("[hw-sim] Donanim simulasyonu basladi · aralik %1 ms").arg(intervalMs), "warn");
    appendMonitorLine("[hw-sim] Sentetik sensor verisi karta gonderilecek. Yanit bekleniyor...", "info");
    const int baud = preferredBaudForBoard(m_state->activeBoard(), m_state->activeBaud());
    m_state->setActiveBaud(baud);
    if (!m_state->isConnected())
        m_serial->connectToPort(port, baud);
    m_hwSimTimer->start(qMax(50, intervalMs));
    emit simRunningChanged();
}

void Backend::stopHardwareSimulation()
{
    if (!m_hwSimRunning) return;
    m_hwSimTimer->stop();
    m_hwSimRunning = false;
    appendMonitorLine(QString("[hw-sim] Simulasyon durduruldu · gonderilen=%1  yanit=%2")
                          .arg(m_hwSimSentCount).arg(m_hwSimResponseCount), "warn");
    emit simRunningChanged();
}

void Backend::tickHardwareSimulation()
{
    if (!m_serial || !m_state || !m_state->isConnected()) {
        appendMonitorLine("[hw-sim] UART baglantisi yok; durduruldu.", "err");
        stopHardwareSimulation();
        return;
    }

    // Sensor profile: label list + default value ranges
    struct SensorProfile {
        QStringList labels;
        double defaultLo;
        double defaultHi;
    };

    const QString sensor = m_state ? m_state->lastSensor() : QString();
    SensorProfile profile;
    if (sensor == "BME280") {
        // temperature (°C), pressure (hPa normalized 0-1), humidity (%)
        profile.labels    = {"temp", "press", "hum"};
        profile.defaultLo = 0.0;
        profile.defaultHi = 1.0;
    } else if (sensor == "PDM_MIC") {
        // 16 raw audio samples
        profile.labels.clear();
        for (int i = 0; i < 16; ++i)
            profile.labels << QString("s%1").arg(i);
        profile.defaultLo = -1.0;
        profile.defaultHi =  1.0;
    } else {
        // MPU6050 default: 3-axis accel + 3-axis gyro
        profile.labels    = {"ax", "ay", "az", "gx", "gy", "gz"};
        profile.defaultLo = -2.0;
        profile.defaultHi =  2.0;
    }

    const double lo = (m_hwSimMin != 0.0 || m_hwSimMax != 1.0)
                      ? qMin(m_hwSimMin, m_hwSimMax)
                      : profile.defaultLo;
    const double hi = (m_hwSimMin != 0.0 || m_hwSimMax != 1.0)
                      ? qMax(m_hwSimMin, m_hwSimMax)
                      : profile.defaultHi;

    QStringList values;
    QStringList displayParts;
    for (int i = 0; i < profile.labels.size(); ++i) {
        m_hwSimSeed = (m_hwSimSeed * 1664525u) + 1013904223u;
        const double unit  = static_cast<double>(m_hwSimSeed & 0x00FFFFFFu) / 16777215.0;
        const double value = lo + unit * (hi - lo);
        values      << QString::number(qRound(value * 1000.0));
        displayParts << QString("%1=%2").arg(profile.labels[i],
                                             QString::number(value, 'f', 3));
    }

    const QByteArray command = QString("INFER %1").arg(values.join(' ')).toUtf8();
    ++m_hwSimSentCount;

    // For PDM_MIC condense the display (16 values is too long)
    const QString displayLine = (profile.labels.size() > 6)
        ? QString("sim sensor  [%1 deger]  %2=%3 ... %4=%5")
              .arg(profile.labels.size())
              .arg(profile.labels.first(), displayParts.first().section('=', 1))
              .arg(profile.labels.last(),  displayParts.last().section('=', 1))
        : QString("sim sensor  %1").arg(displayParts.join("  "));

    appendMonitorLine(displayLine, "warn");
    m_serial->writeLine(command);

    // Warn after 6 frames with no board response
    if (m_hwSimSentCount >= 6 && m_hwSimResponseCount == 0) {
        appendMonitorLine("Karttan INFER yaniti gelmedi. Pipeline Wizard ile firmware'i yeniden uretip flash ettiginizden emin olun.", "err");
        m_hwSimSentCount = 0;
    }
}

bool Backend::saveMonitorLog(const QString &path) const
{
    QString outPath = path;
    if (outPath.startsWith("file:///"))
        outPath = QUrl(outPath).toLocalFile();
    if (outPath.isEmpty()) return false;
    QFile file(outPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    for (const QVariant &line : m_monitorLines)
        stream << line.toMap().value("text").toString() << '\n';
    return true;
}

QStringList Backend::simLabelsForSensor() const
{
    const QString sensor = m_state ? m_state->lastSensor() : QString();
    if (sensor == "BME280")
        return {"normal", "anomaly"};
    if (sensor == "PDM_MIC")
        return {"yes", "no", "up", "down", "left", "right", "on", "off", "stop", "go"};
    // MPU6050 (default / HAR)
    return {"walking", "running", "idle", "standing", "stairs"};
}

void Backend::tickSimulation()
{
    ++m_simUptime;
    auto *rng = QRandomGenerator::global();

    // Build a fake §{JSON}\r\n inference packet and feed it to PacketParser.
    const QStringList labels = simLabelsForSensor();
    const QString label = labels.at(rng->bounded(labels.size()));
    const int infUs     = 800 + rng->bounded(500);
    const int accPct    = 88 + rng->bounded(12);
    const int ramB      = 6000 + rng->bounded(1000);

    const QString model = m_state ? m_state->lastModelName() : "sim_model";
    const QString card  = m_state ? m_state->activeBoard().name : "STM32F4";

    QJsonObject obj;
    obj["t"]       = "inf";
    obj["model"]   = model;
    obj["inf_us"]  = infUs;
    obj["ram_b"]   = ramB;
    obj["acc_pct"] = accPct;
    obj["label"]   = label;
    obj["card"]    = card;

    const QByteArray packet =
        "\xC2\xA7" + QJsonDocument(obj).toJson(QJsonDocument::Compact) + "\r\n";
    m_simParser->feed(packet);

    // Every 5 ticks also generate a sys packet so the Sistem card updates.
    if (m_simUptime % 5 == 0) {
        QJsonObject sysObj;
        sysObj["t"]          = "sys";
        sysObj["uptime_s"]   = static_cast<int>(m_simUptime);
        sysObj["temp_c"]     = 35 + static_cast<int>(rng->bounded(10u));
        sysObj["free_ram_b"] = 180000 + static_cast<int>(rng->bounded(10000u));
        sysObj["state"]      = "running";
        const QByteArray sysPacket =
            "\xC2\xA7" + QJsonDocument(sysObj).toJson(QJsonDocument::Compact) + "\r\n";
        m_simParser->feed(sysPacket);
    }

    // Log to monitor (same format as real board inference)
    appendMonitorLine(
        QString("§ inf  %1  %2 ms  %3 KB  acc=%4%  label=%5")
            .arg(model)
            .arg(infUs / 1000.0, 0, 'f', 1)
            .arg(ramB  / 1024.0, 0, 'f', 1)
            .arg(accPct)
            .arg(label),
        "ok");

    // Persist to analysis (simulation kind)
    if (m_analysis) {
        const QString chipName = m_state ? m_state->activeBoard().deviceName : QString("--");
        const QString cpuName  = m_state ? m_state->activeBoard().deviceCpu  : QString("--");
        QStringList cells;
        cells << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
              << QString("SIM-LIVE-%1").arg(m_simUptime)
              << card
              << (chipName.isEmpty() ? "--" : chipName)
              << (cpuName.isEmpty()  ? "--" : cpuName)
              << model
              << "INT8"
              << "Simülasyon"
              << (QString::number(infUs / 1000.0, 'f', 2) + " ms")
              << (QString::number(ramB  / 1024.0, 'f', 2) + " KiB")
              << "--"
              << (label + "  " + QString::number(accPct) + "%");
        m_analysis->addRecord("simulation", cells);
        emit analysisChanged();
    }
}

// ── Flash ───────────────────────────────────────────────────────────────────
QString Backend::fileInfo(const QString &path) const
{
    QFileInfo fi(path);
    if (!fi.exists()) return QString();
    return fi.fileName() + "  ·  " + QLocale().formattedDataSize(fi.size());
}

void Backend::clearFlashLog()
{
    m_flashLines.clear();
    emit flashLinesChanged();
}

void Backend::appendFlashLine(const QString &text, const QString &type)
{
    QVariantMap m;
    m["text"] = text;
    m["type"] = type;
    m_flashLines.append(m);
    emit flashLinesChanged();
}

void Backend::flashFirmware(const QString &path, const QString &modelName,
                            const QString &architecture, const QString &quantization,
                            bool simulationMode)
{
    if (!m_flash || m_flashBusy) return;

    FlashConfig cfg;
    cfg.hexPath        = path;
    cfg.modelName      = modelName;
    cfg.architecture   = architecture;
    cfg.quantization   = quantization;
    cfg.targetBoard    = m_state ? m_state->activeBoard().name : QString();
    cfg.simulationMode = simulationMode;

    QString err;
    if (!m_flash->validateConfig(cfg, err)) {
        appendFlashLine("HATA: " + err, "err");
        return;
    }

    m_flashProgress = 0; emit flashProgressChanged();
    m_flashBusy     = true; emit flashBusyChanged();
    appendFlashLine(QString("$ Flash başlatılıyor: %1").arg(modelName), "cmd");
    m_flash->flash(cfg);
}

void Backend::cancelFlash()
{
    if (m_flash && m_flashBusy) m_flash->cancel();
}

void Backend::wireFlash()
{
    if (!m_flash) return;

    connect(m_flash, &FlashManager::outputLine,   this,
            [this](const QString &l) { appendFlashLine(l, "info"); });
    connect(m_flash, &FlashManager::errorLine,    this,
            [this](const QString &l) { appendFlashLine(l, "err"); });
    connect(m_flash, &FlashManager::progressChanged, this,
            [this](int p) { m_flashProgress = p; emit flashProgressChanged(); });
    connect(m_flash, &FlashManager::flashFinished, this,
            [this](bool ok, const FlashConfig &cfg) {
                m_flashProgress = ok ? 100 : m_flashProgress;
                emit flashProgressChanged();
                m_flashBusy = false; emit flashBusyChanged();
                appendFlashLine(ok ? "✓ Flash tamamlandı." : "✗ Flash başarısız.",
                                ok ? "ok" : "err");
                if (ok && m_state)
                    m_state->setLastModel(cfg.modelName,
                                          m_state->lastInferenceMs(),
                                          m_state->lastAccuracy());
            });
}

// ── Pipeline ───────────────────────────────────────────────────────────────
void Backend::appendPipelineLine(const QString &text, const QString &type)
{
    QVariantMap m;
    m["text"] = text;
    m["type"] = type;
    m_pipelineLines.append(m);
    emit pipelineChanged();
    appendFlashLine(text, type);
}

PipelineConfig Backend::pipelineConfigFromMap(const QVariantMap &config) const
{
    auto value = [&config](const QString &key, const QString &fallback = QString()) {
        return config.value(key, fallback).toString();
    };

    AppSettings settings;
    PipelineConfig cfg;
    cfg.modelPath = value("modelPath");
    cfg.modelName = value("modelName", QFileInfo(cfg.modelPath).baseName());
    cfg.architecture = value("architecture", "Bilinmiyor");
    cfg.quantization = value("quantization", "INT8");
    cfg.sensorType = value("sensorType", "BME280");
    cfg.protocol = cfg.sensorType == "PDM_MIC" || cfg.sensorType == "PDM" ? "SAI" : "I2C";
    cfg.i2cInstance = value("i2cInstance", "I2C1");
    cfg.sdaPort = value("sdaPort", "GPIOB");
    cfg.sdaPin = value("sdaPin", "GPIO_PIN_7");
    cfg.sclPort = value("sclPort", "GPIOB");
    cfg.sclPin = value("sclPin", "GPIO_PIN_6");
    cfg.i2cAddress = value("i2cAddress", cfg.sensorType == "MPU6050" ? "0xD0" : "0x76");
    cfg.saiInstance = value("saiInstance", "SAI1");
    cfg.clkPort = value("clkPort", "GPIOB");
    cfg.clkPin = value("clkPin", "GPIO_PIN_5");
    cfg.dataPort = value("dataPort", "GPIOB");
    cfg.dataPin = value("dataPin", "GPIO_PIN_3");
    cfg.targetBoard = value("targetBoard", m_state ? m_state->activeBoard().name : "STM32F4");
    cfg.gccPath = settings.gccPath();
    cfg.makePath = settings.makePath();
    cfg.xcubeCliPath = settings.xcubeAICliPath();
    cfg.programmerCliPath = settings.programmerCliPath();
    cfg.outputDir = value("outputDir", settings.lastOutputDir());
    cfg.cubeSdkPath = settings.cubeSdkPath();
    return cfg;
}

void Backend::runPipeline(const QVariantMap &config)
{
    if (m_pipelineBusy) return;
    m_lastPipelineConfig = pipelineConfigFromMap(config);
    if (m_lastPipelineConfig.modelPath.isEmpty() || !QFileInfo::exists(m_lastPipelineConfig.modelPath)) {
        appendPipelineLine("HATA: Model dosyası seçilmedi veya bulunamadı.", "err");
        return;
    }
    if (m_lastPipelineConfig.outputDir.isEmpty()) {
        appendPipelineLine("HATA: Çıktı dizini seçilmedi.", "err");
        return;
    }

    if (m_pipelineRunner)
        m_pipelineRunner->deleteLater();
    m_pipelineRunner = new PipelineRunner(this);
    m_pipelineLines.clear();
    m_pipelineProgress = 0;
    m_pipelineBusy = true;
    m_pipelineStage = "Pipeline başlatılıyor...";
    emit pipelineChanged();
    if (m_pipelinePulseTimer) m_pipelinePulseTimer->start();

    connect(m_pipelineRunner, &PipelineRunner::stageChanged, this, [this](const QString &stage) {
        m_pipelineStage = stage;
        emit pipelineChanged();
    });
    connect(m_pipelineRunner, &PipelineRunner::progressChanged, this, [this](int p) {
        m_pipelineProgress = p;
        emit pipelineChanged();
    });
    connect(m_pipelineRunner, &PipelineRunner::outputLine, this,
            [this](const QString &line) { appendPipelineLine(line, "info"); });
    connect(m_pipelineRunner, &PipelineRunner::errorLine, this,
            [this](const QString &line) { appendPipelineLine(line, "err"); });
    connect(m_pipelineRunner, &PipelineRunner::finished, this, [this](bool success) {
        m_pipelineBusy = false;
        if (m_pipelinePulseTimer) m_pipelinePulseTimer->stop();
        m_pipelineProgress = success ? 100 : 0;
        m_pipelineStage = success ? "Pipeline tamamlandı" : "Pipeline başarısız";
        if (success) {
            addCompiledRecord(m_lastPipelineConfig);
            if (m_state) {
                m_state->setLastModel(m_lastPipelineConfig.modelName,
                                      m_state->lastInferenceMs(), m_state->lastAccuracy());
                if (!m_lastPipelineConfig.sensorType.isEmpty())
                    m_state->setLastSensor(m_lastPipelineConfig.sensorType);
            }
        }
        QVariantMap artifact;
        artifact["modelName"] = m_lastPipelineConfig.modelName;
        artifact["outputDir"] = m_lastPipelineConfig.outputDir;
        artifact["targetBoard"] = m_lastPipelineConfig.targetBoard;
        emit pipelineChanged();
        emit pipelineFinished(success, artifact);
    });

    m_pipelineRunner->run(m_lastPipelineConfig);
}

void Backend::cancelPipeline()
{
    if (m_pipelineRunner && m_pipelineBusy)
        m_pipelineRunner->cancel();
    if (m_pipelinePulseTimer) m_pipelinePulseTimer->stop();
    m_pipelineBusy = false;
    m_pipelineProgress = 0;
    m_pipelineStage = "Pipeline iptal edildi";
    appendPipelineLine("Pipeline iptal edildi.", "warn");
    emit pipelineChanged();
}

void Backend::clearPipelineLog()
{
    m_pipelineLines.clear();
    if (!m_pipelineBusy) {
        m_pipelineProgress = 0;
        m_pipelineStage = "Hazır";
    }
    emit pipelineChanged();
}

void Backend::addCompiledRecord(const PipelineConfig &config)
{
    if (!m_analysis) return;

    const QString expectedElf = QDir(config.outputDir).filePath(
        QString("build/%1_%2.elf").arg(config.modelName, config.targetBoard));
    const QString firmwarePath = QFileInfo::exists(expectedElf) ? expectedElf : QString();
    const QFileInfo fwInfo(firmwarePath);
    const QString firmwareSize = fwInfo.exists()
        ? QString("%1 KiB").arg(fwInfo.size() / 1024.0, 0, 'f', 2)
        : QString("--");

    QStringList cells;
    cells << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
          << config.modelName
          << config.quantization
          << config.targetBoard
          << (m_state ? m_state->activeBoard().deviceName : QString("--"))
          << config.sensorType
          << "--"
          << "--"
          << "--"
          << "--"
          << firmwareSize
          << (firmwarePath.isEmpty() ? "Derlendi" : "Arşivlendi")
          << firmwarePath
          << config.modelPath
          << config.outputDir;
    m_analysis->addRecord("compiled", cells);
    emit analysisChanged();
}

// ── Benchmark ──────────────────────────────────────────────────────────────
void Backend::appendBenchmarkLine(const QString &text, const QString &type)
{
    QVariantMap m;
    m["text"] = text;
    m["type"] = type;
    m_benchmarkLines.append(m);
    emit benchmarkChanged();
}

void Backend::startBenchmark(int samples, double minValue, double maxValue, int seed)
{
    if (!m_serial || !m_state || m_benchmarkBusy) return;
    QString port = m_state->activePort();
    if (port.isEmpty()) {
        const QSerialPortInfo preferred = preferredSerialPortForBoard(m_state->activeBoard());
        port = preferred.portName();
        if (!port.isEmpty()) {
            m_state->setActivePort(port);
            m_state->setActiveBaud(preferredBaudForBoard(m_state->activeBoard(), m_state->activeBaud()));
        }
    }
    if (port.isEmpty()) {
        appendBenchmarkLine("Aktif COM port yok. Kartlar ekranından port seçin.", "err");
        return;
    }

    m_benchmarkLines.clear();
    m_benchmarkMetrics.clear();
    m_benchmarkBusy = true;
    emit benchmarkChanged();

    auto sendCommand = [this, samples, minValue, maxValue, seed]() {
        if (!m_state || !m_state->isConnected()) {
            appendBenchmarkLine("UART bağlantısı kurulamadı.", "err");
            m_benchmarkBusy = false;
            emit benchmarkChanged();
            return;
        }
        const int minMilli = qRound(minValue * 1000.0);
        const int maxMilli = qRound(maxValue * 1000.0);
        const QByteArray command = QString("BENCH %1 %2 %3 %4")
            .arg(qMax(1, samples))
            .arg(minMilli)
            .arg(maxMilli)
            .arg(qMax(0, seed))
            .toUtf8();
        appendBenchmarkLine("> " + QString::fromUtf8(command), "cmd");
        m_serial->writeLine(command);
        m_benchmarkTimeout->start(10000);
    };

    const int baud = preferredBaudForBoard(m_state->activeBoard(), m_state->activeBaud());
    m_state->setActiveBaud(baud);

    if (m_state->isConnected()) {
        sendCommand();
    } else {
        appendBenchmarkLine(QString("UART bağlantısı açılıyor: %1 @ %2")
                                .arg(port).arg(m_state->activeBaud()), "cmd");
        m_serial->connectToPort(port, m_state->activeBaud() > 0 ? m_state->activeBaud() : 115200);
        QTimer::singleShot(1200, this, sendCommand);
    }
}

void Backend::cancelBenchmark()
{
    if (!m_benchmarkBusy) return;
    if (m_benchmarkTimeout) m_benchmarkTimeout->stop();
    m_benchmarkBusy = false;
    appendBenchmarkLine("Benchmark iptal edildi.", "warn");
    emit benchmarkChanged();
}

void Backend::clearBenchmarkLog()
{
    m_benchmarkLines.clear();
    emit benchmarkChanged();
}

// ── Analysis ────────────────────────────────────────────────────────────────
QVariantList Backend::recordsForKind(const QString &kind) const
{
    if (!m_analysis) return {};
    QVariantList out;
    for (const AnalysisRecord &r : m_analysis->records(kind)) {
        QVariantMap m;
        m["id"]    = r.id;
        m["kind"]  = r.kind;
        m["cells"] = QVariant::fromValue(r.cells);
        out.append(m);
    }
    return out;
}

QVariantList Backend::benchmarkRecords()  const { return recordsForKind("benchmark"); }
QVariantList Backend::simulationRecords() const { return recordsForKind("simulation"); }
QVariantList Backend::sensorRecords()     const { return recordsForKind("sensor"); }
QVariantList Backend::compiledRecords()   const { return recordsForKind("compiled"); }
QVariantList Backend::recordsForKindQml(const QString &kind) const { return recordsForKind(kind); }

QVariantList Backend::recentAnalysisRecords(int limit) const
{
    QVariantList all;
    const QStringList kinds = {"benchmark", "simulation", "sensor", "compiled"};
    for (const QString &kind : kinds) {
        for (const AnalysisRecord &r : m_analysis ? m_analysis->records(kind) : QVector<AnalysisRecord>{}) {
            QVariantMap m;
            m["id"] = r.id;
            m["kind"] = kind;
            m["cells"] = QVariant::fromValue(r.cells);
            all.append(m);
        }
    }
    std::sort(all.begin(), all.end(), [](const QVariant &a, const QVariant &b) {
        const QString ad = a.toMap().value("cells").toStringList().value(0);
        const QString bd = b.toMap().value("cells").toStringList().value(0);
        return ad > bd;
    });
    while (limit > 0 && all.size() > limit)
        all.removeLast();
    return all;
}

void Backend::deleteAnalysisRecord(int id)
{
    if (m_analysis) { m_analysis->deleteRecord(id); emit analysisChanged(); }
}

bool Backend::flashCompiledModel(int recordId)
{
    if (!m_analysis || !m_flash) return false;
    for (const AnalysisRecord &record : m_analysis->records("compiled")) {
        if (record.id != recordId) continue;
        const QString firmwarePath = record.cells.value(12);
        const QString modelName = record.cells.value(1);
        if (firmwarePath.isEmpty() || !QFileInfo::exists(firmwarePath)) {
            appendFlashLine("HATA: Arşivlenmiş firmware bulunamadı.", "err");
            return false;
        }
        flashFirmware(firmwarePath, modelName, record.cells.value(2), "INT8", false);
        return true;
    }
    return false;
}

static QString normalizedLocalPath(QString path, const QString &suffix)
{
    if (path.startsWith("file:///"))
        path = QUrl(path).toLocalFile();
    if (!path.endsWith(suffix, Qt::CaseInsensitive))
        path += suffix;
    return path;
}

static QString csvEscape(QString value)
{
    value.replace("\"", "\"\"");
    if (value.contains(',') || value.contains('"') || value.contains('\n') || value.contains('\r'))
        return '"' + value + '"';
    return value;
}

static QString columnTitle(const QVariant &column)
{
    const QVariantMap m = column.toMap();
    return m.value("title").toString();
}

bool Backend::exportAnalysisCsv(const QString &path,
                                const QVariantList &columns,
                                const QVariantList &rows)
{
    const QString outPath = normalizedLocalPath(path, ".csv");
    QFile file(outPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit statusMessage("CSV yazılamadı: " + file.errorString());
        return false;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);

    QStringList headers;
    for (const QVariant &column : columns)
        headers << csvEscape(columnTitle(column));
    stream << headers.join(',') << '\n';

    for (const QVariant &rowVar : rows) {
        QStringList cells;
        const QVariantList row = rowVar.toList();
        for (int i = 0; i < columns.size(); ++i)
            cells << csvEscape(i < row.size() ? row.at(i).toString() : QString());
        stream << cells.join(',') << '\n';
    }

    emit statusMessage("CSV oluşturuldu: " + outPath);
    return true;
}

bool Backend::exportAnalysisPdf(const QString &path,
                                const QString &title,
                                const QVariantList &columns,
                                const QVariantList &rows)
{
    const QString outPath = normalizedLocalPath(path, ".pdf");
    QPdfWriter writer(outPath);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setResolution(144);
    writer.setPageMargins(QMarginsF(12, 12, 12, 12), QPageLayout::Millimeter);

    QPainter painter(&writer);
    if (!painter.isActive()) {
        emit statusMessage("PDF oluşturulamadı.");
        return false;
    }

    const QRect page = writer.pageLayout().paintRectPixels(writer.resolution());
    const int left = page.left();
    const int right = page.right();
    const int width = page.width();
    int y = page.top();

    QFont titleFont("Arial", 17, QFont::Bold);
    QFont metaFont("Arial", 8);
    QFont headerFont("Arial", 7, QFont::Bold);
    QFont bodyFont("Arial", 7);

    painter.setPen(QColor("#111827"));
    painter.setFont(titleFont);
    painter.drawText(QRect(left, y, width, 42), Qt::AlignLeft | Qt::AlignVCenter,
                     title.isEmpty() ? "STM32 AI Analiz Raporu" : title);
    y += 42;

    painter.setFont(metaFont);
    painter.setPen(QColor("#64748b"));
    painter.drawText(QRect(left, y, width, 24), Qt::AlignLeft | Qt::AlignVCenter,
                     QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm"));
    y += 34;

    const int rowHeight = 34;
    const int cols = qMax(1, columns.size());
    const int colWidth = width / cols;

    auto drawHeader = [&]() {
        painter.fillRect(QRect(left, y, width, rowHeight), QColor("#eef2ff"));
        painter.setPen(QColor("#4f46e5"));
        painter.setFont(headerFont);
        for (int c = 0; c < cols; ++c) {
            const QRect cell(left + c * colWidth + 4, y, colWidth - 8, rowHeight);
            painter.drawText(cell, Qt::AlignVCenter | Qt::AlignLeft,
                             painter.fontMetrics().elidedText(columnTitle(columns.at(c)), Qt::ElideRight, cell.width()));
        }
        y += rowHeight;
    };

    drawHeader();
    painter.setFont(bodyFont);

    for (int r = 0; r < rows.size(); ++r) {
        if (y + rowHeight > page.bottom()) {
            writer.newPage();
            y = page.top();
            drawHeader();
            painter.setFont(bodyFont);
        }

        painter.fillRect(QRect(left, y, width, rowHeight),
                         (r % 2 == 0) ? QColor("#ffffff") : QColor("#f8fafc"));
        painter.setPen(QColor("#334155"));
        const QVariantList row = rows.at(r).toList();
        for (int c = 0; c < cols; ++c) {
            const QRect cell(left + c * colWidth + 4, y, colWidth - 8, rowHeight);
            const QString value = c < row.size() ? row.at(c).toString() : QString();
            painter.drawText(cell, Qt::AlignVCenter | Qt::AlignLeft,
                             painter.fontMetrics().elidedText(value, Qt::ElideRight, cell.width()));
        }
        painter.setPen(QColor("#e2e8f0"));
        painter.drawLine(left, y + rowHeight - 1, right, y + rowHeight - 1);
        y += rowHeight;
    }

    painter.end();
    emit statusMessage("PDF oluşturuldu: " + outPath);
    return true;
}

void Backend::wireAnalysis()
{
    if (!m_serial) return;

    // Benchmark results from real hardware
    connect(m_serial, &SerialManager::benchReceived, this,
            [this](const BenchData &d) {
                if (m_benchmarkTimeout) m_benchmarkTimeout->stop();
                m_benchmarkBusy = false;
                m_benchmarkMetrics["model"] = d.model.isEmpty() ? (m_state ? m_state->lastModelName() : "--") : d.model;
                m_benchmarkMetrics["avg"] = QString("%1 us").arg(d.avg_us);
                m_benchmarkMetrics["min"] = QString("%1 us").arg(d.min_us);
                m_benchmarkMetrics["max"] = QString("%1 us").arg(d.max_us);
                m_benchmarkMetrics["ram"] = QString("%1 B").arg(d.ram_b);
                m_benchmarkMetrics["freeRam"] = QString("%1 B").arg(d.free_ram_b);
                m_benchmarkMetrics["samples"] = d.samples;
                appendBenchmarkLine(QString("BENCH result: samples=%1 avg=%2us min=%3us max=%4us ram=%5B")
                                        .arg(d.samples).arg(d.avg_us).arg(d.min_us).arg(d.max_us).arg(d.ram_b), "ok");
                if (!m_analysis) return;
                const BoardInfo board = m_state ? m_state->activeBoard() : BoardInfo{};
                QStringList cells;
                cells << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
                      << QString("BENCH-LIVE-%1").arg(d.samples)
                      << (d.card.isEmpty() ? board.name : d.card)
                      << (board.deviceName.isEmpty() ? board.name : board.deviceName)
                      << board.deviceCpu
                      << (d.model.isEmpty() ? (m_state ? m_state->lastModelName() : "--") : d.model)
                      << "INT8" << "--"
                      << (QString::number(d.avg_us / 1000.0, 'f', 2) + " ms")
                      << (QString::number(d.ram_b  / 1024.0, 'f', 2) + " KiB")
                      << "--" << "Tamamlandı";
                m_analysis->addRecord("benchmark", cells);
                emit analysisChanged();
                emit benchmarkChanged();
            });

    // Real sensor results
    connect(m_serial, &SerialManager::sensorReceived, this,
            [this](const SensorData &d) {
                if (!m_analysis) return;
                const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
                if (nowMs - m_lastSensorAnalysisMs < 1000)
                    return;
                m_lastSensorAnalysisMs = nowMs;
                const BoardInfo board = m_state ? m_state->activeBoard() : BoardInfo{};
                QStringList cells;
                cells << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
                      << QString("REAL-LIVE-%1").arg(d.seq > 0 ? (int)d.seq : 1)
                      << (d.card.isEmpty() ? board.name : d.card)
                      << (board.deviceName.isEmpty() ? board.name : board.deviceName)
                      << board.deviceCpu
                      << d.model << "INT8" << d.sensor
                      << (d.inf_us > 0 ? QString::number(d.inf_us / 1000.0, 'f', 2) + " ms" : "--")
                      << (d.ram_b  > 0 ? QString::number(d.ram_b  / 1024.0, 'f', 2) + " KiB" : "--")
                      << "--"
                      << (d.label + "  " + QString::number(d.acc_pct) + "%");
                m_analysis->addRecord("sensor", cells);
                emit analysisChanged();
            });

    // Simulation packets feed analysis via simParser
    connect(m_simParser, &PacketParser::inferenceReceived, this,
            [this](const InferenceData &d) {
                if (m_state)
                    m_state->setLiveMetrics(
                        d.model.isEmpty() ? m_state->lastModelName() : d.model,
                        d.inf_us / 1000.0, static_cast<quint8>(d.acc_pct),
                        d.ram_b / 1024.0, d.label);
            });

    connect(m_simParser, &PacketParser::sysReceived, this,
            [this](const SysData &s) {
                if (m_state)
                    m_state->setSystemMetrics(static_cast<int>(s.uptime_s),
                                              s.temp_c,
                                              s.free_ram_b / 1024.0);
            });
}

// ── Serial → AppState + monitor wiring ──────────────────────────────────────
void Backend::wireSerial()
{
    if (!m_serial) return;

    connect(m_serial, &SerialManager::connectionChanged, this,
            [this](bool connected, const QString &info) {
                if (m_state) m_state->setConnected(connected, info);
                appendMonitorLine(connected ? ("[bağlandı] " + info) : "[bağlantı kesildi]",
                                  connected ? "ok" : "warn");
                if (connected) requestBoardInfoBurst();
            });

    connect(m_serial, &SerialManager::rawLineReceived, this,
            [this](const QString &line) { appendMonitorLine(line, "info"); });

    connect(m_serial, &SerialManager::bootReceived, this,
            [this](const BootData &boot) {
                appendMonitorLine(QString("§ boot  card=%1  sdk=%2  model=%3  baud=%4")
                                      .arg(boot.card.isEmpty()  ? "--" : boot.card)
                                      .arg(boot.sdk.isEmpty()   ? "--" : boot.sdk)
                                      .arg(boot.model.isEmpty() ? "--" : boot.model)
                                      .arg(boot.baud), "ok");
                if (m_state) {
                    if (!boot.model.isEmpty())  m_state->setLastModel(boot.model, 0.0, 0);
                    if (!boot.sensor.isEmpty()) m_state->setLastSensor(boot.sensor);
                    if (boot.baud > 0) m_state->setActiveBaud(static_cast<qint32>(boot.baud));
                    if (!boot.card.isEmpty()) {
                        BoardInfo board = BoardPresets::find(boot.card);
                        if (board.isNull()) {
                            board.name = boot.card;
                            board.isPreset = false;
                        }
                        if (boot.flash_kb > 0) board.flashKb = static_cast<int>(boot.flash_kb);
                        if (boot.ram_kb > 0) board.ramKb = static_cast<int>(boot.ram_kb);
                        if (boot.clock_mhz > 0) board.clockMhz = static_cast<int>(boot.clock_mhz);
                        board.portName = m_state->activePort();
                        m_state->setActiveBoard(board);
                    }
                }
            });

    connect(m_serial, &SerialManager::inferenceReceived, this,
            [this](const InferenceData &d) {
                if (m_hwSimRunning) ++m_hwSimResponseCount;
                const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
                const bool logLine = nowMs - m_lastInferenceLogMs >= 250;
                if (logLine)
                    m_lastInferenceLogMs = nowMs;
                if (logLine)
                appendMonitorLine(QString("§ inf  %1  %2 ms  %3 KB  acc=%4%  label=%5")
                                      .arg(d.model.isEmpty() ? "--" : d.model)
                                      .arg(d.inf_us / 1000.0, 0, 'f', 1)
                                      .arg(d.ram_b  / 1024.0, 0, 'f', 1)
                                      .arg(d.acc_pct)
                                      .arg(d.label.isEmpty() ? "--" : d.label), "cmd");
                if (m_state)
                    m_state->setLiveMetrics(
                        d.model.isEmpty() ? m_state->lastModelName() : d.model,
                        d.inf_us / 1000.0, static_cast<quint8>(d.acc_pct),
                        d.ram_b / 1024.0, d.label);
            });

    connect(m_serial, &SerialManager::sensorReceived, this,
            [this](const SensorData &d) {
                const QString valStr = d.values.isEmpty() ? "--" : d.values.join(", ");
                const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
                const bool logLine = nowMs - m_lastSensorLogMs >= 250;
                if (logLine)
                    m_lastSensorLogMs = nowMs;
                if (logLine)
                appendMonitorLine(QString("§ sensor  %1  model=%2  values=[%3]  %4%  label=%5")
                                      .arg(d.sensor.isEmpty() ? "--" : d.sensor)
                                      .arg(d.model.isEmpty()  ? "--" : d.model)
                                      .arg(valStr)
                                      .arg(d.acc_pct)
                                      .arg(d.label.isEmpty()  ? "--" : d.label), "info");
                if (m_state && (d.inf_us > 0 || d.ram_b > 0 || !d.label.isEmpty())) {
                    m_state->setLiveMetrics(
                        d.model.isEmpty() ? m_state->lastModelName() : d.model,
                        d.inf_us / 1000.0, static_cast<quint8>(d.acc_pct),
                        d.ram_b / 1024.0, d.label);
                }
            });

    connect(m_serial, &SerialManager::sysReceived, this,
            [this](const SysData &s) {
                const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
                const bool logLine = nowMs - m_lastSysLogMs >= 1000;
                if (logLine)
                    m_lastSysLogMs = nowMs;
                if (logLine)
                appendMonitorLine(QString("§ sys  uptime=%1s  temp=%2C  free=%3 KB  state=%4")
                                      .arg(s.uptime_s)
                                      .arg(s.temp_c)
                                      .arg(s.free_ram_b / 1024)
                                      .arg(s.state.isEmpty() ? "--" : s.state), "info");
                if (m_state)
                    m_state->setSystemMetrics(static_cast<int>(s.uptime_s), s.temp_c, s.free_ram_b / 1024.0);
            });

    connect(m_serial, &SerialManager::errorReceived, this,
            [this](const ErrorData &e) {
                appendMonitorLine(QString("[err %1] %2").arg(e.code).arg(e.msg), "err");
            });
}

void Backend::requestBoardInfoBurst()
{
    if (!m_serial) return;

    auto request = [this]() {
        if (!m_serial || !m_serial->isConnected()) return;
        appendMonitorLine("[cmd] kart bilgisi isteniyor: INFO? / BOOT?", "cmd");
        m_serial->requestBoardInfo();
    };

    QTimer::singleShot(100, this, request);
    QTimer::singleShot(700, this, request);
    QTimer::singleShot(1500, this, request);
}
