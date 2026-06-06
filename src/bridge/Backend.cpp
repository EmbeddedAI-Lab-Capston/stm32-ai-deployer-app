#include "Backend.h"

#include <algorithm>
#include <limits>

#include <QSet>

#include <QSerialPortInfo>
#include <QFileInfo>
#include <QDir>
#include <QLocale>
#include <QRandomGenerator>
#include <QDate>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QHash>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QProcess>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTextStream>
#include <QUrl>
#include <QtMath>

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
constexpr int kN6DefaultBaud = 209700;

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

QString jsonArrayToShapeString(const QJsonArray &shape)
{
    QStringList parts;
    for (const QJsonValue &value : shape)
        parts << QString::number(value.toInt());
    return parts.join('x');
}

int flattenedInputElementCount(const QJsonArray &shape)
{
    int count = 1;
    for (int i = 0; i < shape.size(); ++i) {
        const int dim = shape.at(i).toInt(1);
        if (i == 0 && dim == 1)
            continue;
        count *= qMax(1, dim);
        if (count > 64)
            return count;
    }
    return qMax(1, count);
}

QPair<double, double> defaultRangeForInput(const QJsonObject &input)
{
    const QJsonObject format = input.value(QStringLiteral("data_format")).toObject();
    const QString type = format.value(QStringLiteral("type")).toString().toUpper();
    const int size = format.value(QStringLiteral("size")).toInt(32);
    const QJsonArray scales = format.value(QStringLiteral("scale")).toArray();
    const QJsonArray zeros = format.value(QStringLiteral("zero")).toArray();

    if ((type.contains(QStringLiteral("SIGNED")) || type.contains(QStringLiteral("UNSIGNED")))
        && size > 0 && size <= 16 && !scales.isEmpty()) {
        const double scale = scales.first().toDouble(1.0);
        const double zero = zeros.isEmpty() ? 0.0 : zeros.first().toDouble(0.0);
        const double qMin = type.contains(QStringLiteral("UNSIGNED")) ? 0.0 : -qPow(2.0, size - 1);
        const double qMax = type.contains(QStringLiteral("UNSIGNED")) ? qPow(2.0, size) - 1.0
                                                                      : qPow(2.0, size - 1) - 1.0;
        return { (qMin - zero) * scale, (qMax - zero) * scale };
    }

    if (type.contains(QStringLiteral("FLOAT")))
        return {0.0, 1.0};

    return {0.0, 1.0};
}

void appendInputSpecsFromArray(const QJsonArray &inputs, QVariantList *specs)
{
    for (const QJsonValue &value : inputs) {
        const QJsonObject input = value.toObject();
        const QString tensorName = input.value(QStringLiteral("name")).toString(QStringLiteral("input"));
        const QJsonArray shape = input.value(QStringLiteral("shape")).toArray();
        const QString shapeText = jsonArrayToShapeString(shape);
        const auto range = defaultRangeForInput(input);
        const int elementCount = flattenedInputElementCount(shape);
        const bool expandElements = elementCount > 1 && elementCount <= 64;

        const int rows = expandElements ? elementCount : 1;
        for (int i = 0; i < rows; ++i) {
            QVariantMap spec;
            spec[QStringLiteral("name")] = expandElements
                ? QStringLiteral("%1[%2]").arg(tensorName).arg(i)
                : tensorName;
            spec[QStringLiteral("tensorName")] = tensorName;
            spec[QStringLiteral("shape")] = shapeText.isEmpty() ? QStringLiteral("-") : shapeText;
            spec[QStringLiteral("elements")] = elementCount;
            spec[QStringLiteral("elementIndex")] = expandElements ? i : -1;
            spec[QStringLiteral("min")] = range.first;
            spec[QStringLiteral("max")] = range.second;
            specs->append(spec);
        }
    }
}

QVariantList parseInputSpecsFromReport(const QString &reportPath)
{
    QFile file(reportPath);
    if (!file.open(QIODevice::ReadOnly))
        return {};

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject())
        return {};

    QVariantList specs;
    const QJsonObject root = doc.object();
    appendInputSpecsFromArray(root.value(QStringLiteral("original_inputs")).toArray(), &specs);

    const QJsonArray graphs = root.value(QStringLiteral("graphs")).toArray();
    for (const QJsonValue &graphValue : graphs)
        appendInputSpecsFromArray(graphValue.toObject().value(QStringLiteral("original_inputs")).toArray(), &specs);

    return specs;
}

QString semanticInputLabel(const QString &sensorType, int elementIndex)
{
    if (elementIndex < 0)
        return {};

    const QString sensor = sensorType.toUpper();
    if (sensor == QStringLiteral("BME280")) {
        const QStringList labels = {
            QStringLiteral("Sicaklik (C)"),
            QStringLiteral("Nem (%)"),
            QStringLiteral("Basinc (hPa)"),
        };
        return labels.at(elementIndex % labels.size());
    }

    if (sensor == QStringLiteral("MPU6050")) {
        const QStringList labels = {
            QStringLiteral("Ivme X"),
            QStringLiteral("Ivme Y"),
            QStringLiteral("Ivme Z"),
            QStringLiteral("Gyro X"),
            QStringLiteral("Gyro Y"),
            QStringLiteral("Gyro Z"),
        };
        return labels.at(elementIndex % labels.size());
    }

    if (sensor == QStringLiteral("PDM_MIC") || sensor == QStringLiteral("PDM")) {
        return QStringLiteral("MFCC %1").arg(elementIndex);
    }

    return {};
}

QVariantList withSemanticInputLabels(QVariantList specs, const QString &sensorType)
{
    for (QVariant &value : specs) {
        QVariantMap spec = value.toMap();
        bool ok = false;
        const int parsedIndex = spec.value(QStringLiteral("elementIndex")).toInt(&ok);
        const int elementIndex = ok ? parsedIndex : -1;
        const QString label = semanticInputLabel(sensorType, elementIndex);
        if (!label.isEmpty())
            spec[QStringLiteral("label")] = label;
        value = spec;
    }
    return specs;
}

QString inputDisplayLabel(const QVariantMap &input, int fallbackIndex)
{
    const QString label = input.value(QStringLiteral("label")).toString();
    if (!label.isEmpty())
        return label;
    const QString name = input.value(QStringLiteral("name")).toString();
    if (!name.isEmpty())
        return name;
    return QStringLiteral("input[%1]").arg(fallbackIndex);
}

double rangedRandomValue(const QVariantMap &input, double fallbackMin, double fallbackMax)
{
    bool okMin = false;
    bool okMax = false;
    double lo = input.value(QStringLiteral("min")).toDouble(&okMin);
    double hi = input.value(QStringLiteral("max")).toDouble(&okMax);
    if (!okMin)
        lo = fallbackMin;
    if (!okMax)
        hi = fallbackMax;
    if (lo > hi)
        std::swap(lo, hi);

    const double unit = QRandomGenerator::global()->generateDouble();
    return lo + unit * (hi - lo);
}

QString cleanPortName(const QString &portName)
{
    return portName.section(' ', 0, 0).trimmed();
}

bool serialPortLooksLikeN6(const QString &portName)
{
    const QString clean = cleanPortName(portName);
    if (clean.isEmpty())
        return false;
    for (const QSerialPortInfo &info : SerialManager::availablePorts()) {
        if (info.portName().compare(clean, Qt::CaseInsensitive) == 0)
            return hasProductId(info, kN657VcpProductId);
    }
    return false;
}

bool boardOrPortLooksLikeN6(const BoardInfo &board, const QString &portName)
{
    return boardLooksLikeN6(board) || serialPortLooksLikeN6(portName);
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
        return kN6DefaultBaud;
    return requestedBaud > 0 ? requestedBaud : 115200;
}

qulonglong namedUIntValue(const QString &line, const QString &name, qulonglong fallback = 0)
{
    const QRegularExpression re(QStringLiteral("\\b%1\\s*=\\s*([0-9]+)").arg(QRegularExpression::escape(name)),
                                QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = re.match(line);
    if (!match.hasMatch())
        return fallback;
    bool ok = false;
    const qulonglong value = match.captured(1).toULongLong(&ok);
    return ok ? value : fallback;
}

double namedDoubleValue(const QString &line, const QString &name, double fallback = 0.0)
{
    const QRegularExpression re(QStringLiteral("\\b%1\\s*=\\s*([0-9]+(?:\\.[0-9]+)?)")
                                    .arg(QRegularExpression::escape(name)),
                                QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = re.match(line);
    if (!match.hasMatch())
        return fallback;
    bool ok = false;
    const double value = match.captured(1).toDouble(&ok);
    return ok ? value : fallback;
}

QString namedTextValue(const QString &line, const QString &name)
{
    const QRegularExpression re(QStringLiteral("\\b%1\\s*=\\s*([^\\s,|]+)")
                                    .arg(QRegularExpression::escape(name)),
                                QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = re.match(line);
    return match.hasMatch() ? match.captured(1).trimmed() : QString();
}

quint8 confidencePercentFromLine(const QString &line)
{
    const QRegularExpression re(QStringLiteral("\\b(?:conf|pct)\\s*=\\s*([0-9]+)\\s*%?"),
                                QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = re.match(line);
    if (!match.hasMatch())
        return 0;
    bool ok = false;
    const int value = match.captured(1).toInt(&ok);
    return ok ? static_cast<quint8>(qBound(0, value, 100)) : 0;
}

QString n6PredictionLabelFromLine(const QString &line)
{
    QString label = namedTextValue(line, QStringLiteral("label"));
    if (!label.isEmpty())
        return label;

    label = namedTextValue(line, QStringLiteral("best"));
    if (!label.isEmpty())
        return label;

    const QRegularExpression parenRe(QStringLiteral("\\(([^)]+)\\)"));
    const QRegularExpressionMatch parenMatch = parenRe.match(line);
    if (parenMatch.hasMatch())
        return parenMatch.captured(1).trimmed();

    const QRegularExpression pctLabelRe(QStringLiteral("\\bpct\\s*=\\s*[0-9]+\\s+([^\\s]+)\\s+cy\\s*="),
                                        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch pctLabelMatch = pctLabelRe.match(line);
    if (pctLabelMatch.hasMatch())
        return pctLabelMatch.captured(1).trimmed();

    const QRegularExpression irlRe(QStringLiteral("^\\[IRL[^\\]]*\\]\\s*([^\\s]+)"),
                                   QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch irlMatch = irlRe.match(line);
    if (irlMatch.hasMatch())
        return irlMatch.captured(1).trimmed();

    return {};
}

quint32 cyclesToMicros(qulonglong cycles, double cpuMhz)
{
    if (cycles == 0 || cpuMhz <= 0.0)
        return 0;
    const double micros = static_cast<double>(cycles) / cpuMhz;
    const qulonglong rounded = static_cast<qulonglong>(qMax<qint64>(0, qRound64(micros)));
    return static_cast<quint32>(qMin<qulonglong>(rounded, std::numeric_limits<quint32>::max()));
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
        appendBenchmarkLine("Benchmark yaniti alinamadi. Kartin guncel template firmware ile flashlandigini ve BENCH komutunu isledigini dogrulayin.", "warn");
        m_benchmarkBusy = false;
        emit benchmarkChanged();
    });

    wireSerial();
    wireFlash();
    wireAnalysis();

    // Pre-populate analysis with sample data so the tables aren't empty on first launch.
    seedAnalysisIfEmpty();
}

// â”€â”€ Tool paths â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
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
                emit statusMessage(QString("âœ“ %1 bulundu").arg(info.name));
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

// â”€â”€ Serial â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
QStringList Backend::availablePorts() const
{
    QStringList out;
    for (const QSerialPortInfo &p : SerialManager::availablePorts()) {
        QString label = p.portName();
        if (p.vendorIdentifier() == 0x0483)
            label += " (ST-Link)";
        else if (!p.description().isEmpty())
            label += " Â- " + p.description();
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
            label += " Â- " + p.description();
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
    const int actualBaud = boardOrPortLooksLikeN6(activeBoard, cleanPort)
        ? kN6DefaultBaud
        : preferredBaudForBoard(activeBoard, baud);
    if (m_state) {
        m_state->setActivePort(cleanPort);
        m_state->setActiveBaud(actualBaud);
    }
    AppSettings().setLastBaud(actualBaud);
    m_serial->connectToPort(cleanPort, actualBaud);
    if (boardOrPortLooksLikeN6(activeBoard, cleanPort)) {
        QTimer::singleShot(900, this, [this]() {
            resetN6TargetForCapture(QStringLiteral("connect"), false);
        });
    }
}

void Backend::disconnectSerial()
{
    if (m_serial) m_serial->disconnectPort();
}

void Backend::resetN6TargetForCapture(const QString &reason, bool benchmarkLog)
{
    if (!m_state)
        return;

    const BoardInfo board = m_state->activeBoard();
    const QString port = m_state->activePort();
    if (!boardOrPortLooksLikeN6(board, port))
        return;

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (nowMs - m_n6LastResetRequestMs < 1200)
        return;
    m_n6LastResetRequestMs = nowMs;

    auto logLine = [this, benchmarkLog](const QString &text, const QString &type) {
        if (benchmarkLog)
            appendBenchmarkLine(text, type);
        else
            appendMonitorLine(text, type);
    };

    AppSettings settings;
    QString cliPath = settings.programmerCliPath();
    if (cliPath.isEmpty()) {
        cliPath = FlashManager::detectCliPath();
        if (!cliPath.isEmpty())
            settings.setProgrammerCliPath(cliPath);
    }

    if (cliPath.isEmpty()) {
        logLine("[n6-reset] STM32_Programmer_CLI bulunamadi; port acikken karta manuel NRST/reset atin.", "warn");
        return;
    }

    QSerialPortInfo portInfo = preferredSerialPortForBoard(board, port);
    QString selectedSn = board.stlinkSn;
    if (selectedSn.isEmpty() && !portInfo.isNull())
        selectedSn = portInfo.serialNumber();
    if (selectedSn.isEmpty())
        selectedSn = programmerSnForBoard(cliPath, board);

    // NOTE: N6 boots from external flash with TrustZone/RIF active. Once the
    // secured firmware runs, a normal SWD connect fails with "can't get core ID"
    // (exit=1). mode=UR (connect under reset) holds NRST low during connection so
    // the core is always reachable regardless of what firmware is running.
    QStringList args{QStringLiteral("-c"), QStringLiteral("port=SWD"), QStringLiteral("mode=UR")};
    if (!selectedSn.isEmpty())
        args << QStringLiteral("sn=%1").arg(selectedSn);
    args << QStringLiteral("-rst");

    logLine(QString("[n6-reset] %1: STM32_Programmer_CLI %2")
                .arg(reason.isEmpty() ? QStringLiteral("capture") : reason,
                     args.join(QLatin1Char(' '))), "cmd");

    QProcess *resetProcess = new QProcess(this);
    connect(resetProcess, &QProcess::errorOccurred, this,
            [resetProcess, logLine](QProcess::ProcessError) {
                logLine(QString("[n6-reset] reset baslatilamadi: %1").arg(resetProcess->errorString()), "warn");
                resetProcess->deleteLater();
            });
    connect(resetProcess, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [resetProcess, logLine](int exitCode, QProcess::ExitStatus) {
                const QString output = QString::fromLocal8Bit(resetProcess->readAllStandardOutput()
                                                              + resetProcess->readAllStandardError()).trimmed();
                if (exitCode == 0) {
                    logLine("[n6-reset] reset gonderildi; boot UART cikisi yakalaniyor.", "ok");
                } else {
                    logLine(QString("[n6-reset] reset basarisiz (exit=%1). Manuel NRST/reset deneyin.%2")
                                .arg(exitCode)
                                .arg(output.isEmpty() ? QString() : QStringLiteral(" ") + output.left(180)), "warn");
                }
                resetProcess->deleteLater();
            });
    resetProcess->start(cliPath, args);
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
        m_probeStatus = "ST-Link portu bulunamadÄ±.";
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
        m_probeStatus = "STM32_Programmer_CLI bulunamadÄ±.";
        emit probeChanged();
        emit probeFinished(false, m_probeStatus);
        return;
    }

    if (m_stlinkProbe) {
        m_stlinkProbe->kill();
        m_stlinkProbe->deleteLater();
    }

    m_probeBusy = true;
    m_probeStatus = "ST-Link taranÄ±yor...";
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
                    m_probeStatus = "ST-Link probe baÅŸarÄ±sÄ±z.";
                    emit probeChanged();
                    emit probeFinished(false, m_probeStatus);
                }
            });
    QStringList args{QStringLiteral("-c"), QStringLiteral("port=SWD")};
    if (!selectedSn.isEmpty())
        args << QString("sn=%1").arg(selectedSn);
    appendMonitorLine(QString("[probe] ST-Link %1%2")
                          .arg(selectedPort.isEmpty() ? QString("SWD") : selectedPort,
                               selectedSn.isEmpty() ? QString() : QString(" Â- SN %1").arg(selectedSn)),
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
        m_state->setActiveBaud(kN6DefaultBaud);
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

    // Standard rows (benchmark / simulation / sensor share the same 12 columns):
    // date, session, board, chip, cpu, model, type, sensor, avg, ram, weights, result
    struct SeedRow { const char *date; const char *session; const char *board; const char *chip; const char *cpu; const char *model; const char *type; const char *sensor; const char *avg; const char *ram; const char *weights; const char *result; };

    static const SeedRow benchRows[] = {
        {"2026-05-28 14:12","BENCH-001","STM32F407 Discovery","STM32F407VG","Cortex-M4","har_mlp","INT8","MPU6050","8.20 ms","3.00 KiB","12.4 KiB","TamamlandÄ±"},
        {"2026-05-28 13:40","BENCH-002","STM32F407 Discovery","STM32F407VG","Cortex-M4","vibration_mlp","INT8","MPU6050","6.75 ms","2.60 KiB","9.80 KiB","TamamlandÄ±"},
        {"2026-05-27 17:05","BENCH-003","NUCLEO-H723ZG","STM32H723ZG","Cortex-M7","anomaly_cnn","INT8","BME280","0.934 ms","6.44 KiB","6.70 KiB","TamamlandÄ±"},
        {"2026-05-27 16:22","BENCH-004","NUCLEO-H723ZG","STM32H723ZG","Cortex-M7","anomaly_cnn","Float32","BME280","2.84 ms","18.2 KiB","25.2 KiB","TamamlandÄ±"},
        {"2026-05-26 11:48","BENCH-005","NUCLEO-H723ZG","STM32H723ZG","Cortex-M7","motor_fault_cnn","INT8","MPU6050","1.42 ms","9.10 KiB","18.2 KiB","TamamlandÄ±"},
        {"2026-05-25 18:31","BENCH-006","NUCLEO-N657X0-Q","STM32N657","Cortex-M55/NPU","kws_lstm","INT8","PDM_MIC","0.61 ms","22.1 KiB","96.8 KiB","TamamlandÄ±"},
        {"2026-05-25 18:02","BENCH-007","NUCLEO-N657X0-Q","STM32N657","Cortex-M55/NPU","voice_kws","INT8","PDM_MIC","0.48 ms","19.4 KiB","78.2 KiB","TamamlandÄ±"},
        {"2026-05-24 09:54","BENCH-008","NUCLEO-N657X0-Q","STM32N657","Cortex-M55/NPU","gesture_tcn","Dynamic Q","MPU6050","0.88 ms","14.7 KiB","32.4 KiB","TamamlandÄ±"},
    };
    if (m_analysis->records("benchmark").isEmpty()) {
        for (const auto &r : benchRows)
            m_analysis->addRecord("benchmark", {r.date,r.session,r.board,r.chip,r.cpu,r.model,r.type,r.sensor,r.avg,r.ram,r.weights,r.result});
    }

    static const SeedRow simRows[] = {
        {"2026-05-28 10:15","SIM-101","STM32F407 Discovery","STM32F407VG","Cortex-M4","har_mlp","INT8","SimÃ¼lasyon","8.18 ms","3.00 KiB","--","avg 94% | label=walking | n=48"},
        {"2026-05-28 10:31","SIM-102","STM32F407 Discovery","STM32F407VG","Cortex-M4","vibration_mlp","INT8","SimÃ¼lasyon","6.71 ms","2.60 KiB","--","avg 93% | label=idle | n=36"},
        {"2026-05-27 15:02","SIM-103","NUCLEO-H723ZG","STM32H723ZG","Cortex-M7","anomaly_cnn","INT8","SimÃ¼lasyon","0.94 ms","6.44 KiB","--","avg 97% | label=normal | n=62"},
        {"2026-05-27 15:20","SIM-104","NUCLEO-H723ZG","STM32H723ZG","Cortex-M7","motor_fault_cnn","INT8","SimÃ¼lasyon","1.40 ms","9.10 KiB","--","avg 91% | label=fault | n=54"},
        {"2026-05-26 19:44","SIM-105","NUCLEO-N657X0-Q","STM32N657","Cortex-M55/NPU","kws_lstm","INT8","SimÃ¼lasyon","0.62 ms","22.1 KiB","--","avg 95% | label=yes | n=80"},
        {"2026-05-26 20:01","SIM-106","NUCLEO-N657X0-Q","STM32N657","Cortex-M55/NPU","voice_kws","INT8","SimÃ¼lasyon","0.49 ms","19.4 KiB","--","avg 89% | label=stop | n=72"},
    };
    if (m_analysis->records("simulation").isEmpty()) {
        for (const auto &r : simRows)
            m_analysis->addRecord("simulation", {r.date,r.session,r.board,r.chip,r.cpu,r.model,r.type,r.sensor,r.avg,r.ram,r.weights,r.result});
    }

    static const SeedRow sensorRows[] = {
        {"2026-05-28 09:15","REAL-201","NUCLEO-H723ZG","STM32H723ZG","Cortex-M7","anomaly_cnn","INT8","BME280","0.934 ms","6.44 KiB","--","normal  90%"},
        {"2026-05-28 09:32","REAL-202","NUCLEO-H723ZG","STM32H723ZG","Cortex-M7","anomaly_cnn","INT8","BME280","0.941 ms","6.44 KiB","--","anomaly  88%"},
        {"2026-05-27 14:08","REAL-203","STM32F407 Discovery","STM32F407VG","Cortex-M4","har_mlp","INT8","MPU6050","8.22 ms","3.00 KiB","--","walking  92%"},
        {"2026-05-26 16:50","REAL-204","NUCLEO-N657X0-Q","STM32N657","Cortex-M55/NPU","kws_lstm","INT8","PDM_MIC","0.63 ms","22.1 KiB","--","yes  96%"},
        {"2026-05-25 12:37","REAL-205","STM32F407 Discovery","STM32F407VG","Cortex-M4","vibration_mlp","INT8","MPU6050","6.80 ms","2.60 KiB","--","running  87%"},
    };
    if (m_analysis->records("sensor").isEmpty()) {
        for (const auto &r : sensorRows)
            m_analysis->addRecord("sensor", {r.date,r.session,r.board,r.chip,r.cpu,r.model,r.type,r.sensor,r.avg,r.ram,r.weights,r.result});
    }

    // Compiled models use a different 12-column layout:
    // date, model, type, board, chip, sensor, input, params, macc, weights, firmware, status
    struct CompiledSeed { const char *date; const char *model; const char *type; const char *board; const char *chip; const char *sensor; const char *input; const char *params; const char *macc; const char *weights; const char *firmware; const char *status; };
    static const CompiledSeed compiledRows[] = {
        {"2026-05-27 17:10","anomaly_cnn","INT8","NUCLEO-H723ZG","STM32H723ZG","BME280","1x128x3","12.4 K","0.9 M","6.70 KiB","182.4 KiB","ArÅŸivlendi"},
        {"2026-05-28 14:18","har_mlp","INT8","STM32F407 Discovery","STM32F407VG","MPU6050","1x100x6","8.2 K","0.4 M","12.4 KiB","96.2 KiB","ArÅŸivlendi"},
        {"2026-05-25 18:36","kws_lstm","INT8","NUCLEO-N657X0-Q","STM32N657","PDM_MIC","1x16000","84 K","12.6 M","96.8 KiB","412.6 KiB","ArÅŸivlendi"},
        {"2026-05-26 11:55","motor_fault_cnn","INT8","NUCLEO-H723ZG","STM32H723ZG","MPU6050","1x256x6","18.7 K","1.8 M","18.2 KiB","201.0 KiB","Derlendi"},
        {"2026-05-24 10:02","gesture_tcn","Dynamic Q","NUCLEO-N657X0-Q","STM32N657","MPU6050","1x128x6","26.3 K","3.1 M","32.4 KiB","236.8 KiB","ArÅŸivlendi"},
    };
    if (m_analysis->records("compiled").isEmpty()) {
        for (const auto &r : compiledRows)
            m_analysis->addRecord("compiled", {r.date,r.model,r.type,r.board,r.chip,r.sensor,r.input,r.params,r.macc,r.weights,r.firmware,r.status});
    }

    emit analysisChanged();
}

// â”€â”€ Monitor terminal â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
void Backend::clearMonitor()
{
    m_monitorLines.clear();
    emit monitorLinesChanged();
}

void Backend::startSensorAnalysis()
{
    if (m_sensorAnalysisRunning)
        return;

    m_sensorAnalysisRunning = true;
    m_sensorAnalysisStartedAt = QDateTime::currentDateTime();
    m_sensorAnalysisCount = 0;
    m_sensorAnalysisTotalInfUs = 0;
    m_sensorAnalysisTotalRamB = 0;
    m_sensorAnalysisTotalAcc = 0;
    m_sensorAnalysisLastModel.clear();
    m_sensorAnalysisLastSensor.clear();
    m_sensorAnalysisLastCard.clear();
    m_sensorAnalysisLastLabel.clear();
    m_sensorAnalysisLabelCounts.clear();

    appendMonitorLine("[real] Gercek sensor analizi baslatildi.", "cmd");
    emit sensorAnalysisChanged();
}

void Backend::stopSensorAnalysis()
{
    if (!m_sensorAnalysisRunning)
        return;

    m_sensorAnalysisRunning = false;
    emit sensorAnalysisChanged();

    if (!m_analysis || m_sensorAnalysisCount == 0) {
        appendMonitorLine("[real] Analiz bitirildi; kaydedilecek sensor ornegi yok.", "warn");
        return;
    }

    QString bestLabel = m_sensorAnalysisLastLabel;
    int bestCount = -1;
    for (auto it = m_sensorAnalysisLabelCounts.cbegin(); it != m_sensorAnalysisLabelCounts.cend(); ++it) {
        if (it.value() > bestCount) {
            bestCount = it.value();
            bestLabel = it.key();
        }
    }

    const BoardInfo board = m_state ? m_state->activeBoard() : BoardInfo{};
    const double avgMs = (double)m_sensorAnalysisTotalInfUs / (double)m_sensorAnalysisCount / 1000.0;
    const double avgRamKb = (double)m_sensorAnalysisTotalRamB / (double)m_sensorAnalysisCount / 1024.0;
    const int avgAcc = qRound((double)m_sensorAnalysisTotalAcc / (double)m_sensorAnalysisCount);
    const QString model = m_sensorAnalysisLastModel.isEmpty()
        ? (m_state ? m_state->lastModelName() : QStringLiteral("--"))
        : m_sensorAnalysisLastModel;
    const QString sensor = m_sensorAnalysisLastSensor.isEmpty()
        ? (m_state ? m_state->lastSensor() : QStringLiteral("--"))
        : m_sensorAnalysisLastSensor;
    const QString card = m_sensorAnalysisLastCard.isEmpty() ? board.name : m_sensorAnalysisLastCard;
    const QString session = QStringLiteral("REAL-AVG-%1")
        .arg(m_sensorAnalysisStartedAt.toString(QStringLiteral("hhmmss")));

    QStringList cells;
    cells << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
          << session
          << (card.isEmpty() ? QStringLiteral("--") : card)
          << (board.deviceName.isEmpty() ? board.name : board.deviceName)
          << board.deviceCpu
          << model << "INT8" << sensor
          << (QString::number(avgMs, 'f', 2) + " ms")
          << (QString::number(avgRamKb, 'f', 2) + " KiB")
          << "--"
          << (bestLabel.isEmpty()
              ? QString("avg %1%  n=%2").arg(avgAcc).arg(m_sensorAnalysisCount)
              : QString("%1  %2%  n=%3").arg(bestLabel).arg(avgAcc).arg(m_sensorAnalysisCount));

    m_analysis->addRecord("sensor", cells);
    emit analysisChanged();
    appendMonitorLine(QString("[real] Ortalama kaydedildi - n=%1  ort=%2 ms  acc=%3%")
                          .arg(m_sensorAnalysisCount)
                          .arg(avgMs, 0, 'f', 2)
                          .arg(avgAcc), "ok");
}

void Backend::clearTodaySensorAnalysis()
{
    if (!m_analysis)
        return;

    const QString today = QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"));
    const int removed = m_analysis->deleteRecordsForKindOnDate(QStringLiteral("sensor"), today);
    emit analysisChanged();
    appendMonitorLine(QString("[real] Bugunun gercek sensor kayitlari temizlendi - silinen=%1")
                          .arg(removed), removed > 0 ? "ok" : "warn");
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

// â”€â”€ Simulation â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
void Backend::startSimulation(int intervalMs, double minVal, double maxVal)
{
    if (m_simRunning) return;
    m_simMin     = minVal;
    m_simMax     = maxVal;
    m_simInputRanges.clear();
    m_simUptime  = 0;
    m_simSampleCount = 0;
    m_simTotalInfUs  = 0;
    m_simTotalRamB   = 0;
    m_simTotalAcc    = 0;
    m_simLastLabel.clear();
    m_simLastModel.clear();
    m_simLastCard.clear();
    m_simRunning = true;
    emit simRunningChanged();
    appendMonitorLine(QString("[sim] SimÃ¼lasyon baÅŸladÄ± Â- aralÄ±k %1 ms").arg(intervalMs), "cmd");
    m_simTimer->start(intervalMs);
}

void Backend::startSimulationWithInputs(int intervalMs, const QVariantList &inputs)
{
    if (m_simRunning) return;
    double minValue = 0.0;
    double maxValue = 1.0;
    if (!inputs.isEmpty()) {
        minValue = std::numeric_limits<double>::infinity();
        maxValue = -std::numeric_limits<double>::infinity();
        for (const QVariant &value : inputs) {
            const QVariantMap input = value.toMap();
            bool okMin = false;
            bool okMax = false;
            const double inputMin = input.value(QStringLiteral("min")).toDouble(&okMin);
            const double inputMax = input.value(QStringLiteral("max")).toDouble(&okMax);
            if (okMin) minValue = qMin(minValue, inputMin);
            if (okMax) maxValue = qMax(maxValue, inputMax);
        }
        if (!qIsFinite(minValue)) minValue = 0.0;
        if (!qIsFinite(maxValue)) maxValue = 1.0;
    }
    startSimulation(intervalMs, minValue, maxValue);
    m_simInputRanges = inputs;
}

void Backend::stopSimulation()
{
    if (!m_simRunning) return;
    m_simTimer->stop();
    m_simRunning = false;
    emit simRunningChanged();
    appendMonitorLine("[sim] SimÃ¼lasyon durduruldu.", "warn");

    // Persist one averaged record for the whole session (not per-tick).
    if (m_analysis && m_simSampleCount > 0) {
        const double avgInfUs = static_cast<double>(m_simTotalInfUs) / m_simSampleCount;
        const double avgRamB  = static_cast<double>(m_simTotalRamB)  / m_simSampleCount;
        const int    avgAcc   = static_cast<int>(m_simTotalAcc / m_simSampleCount);

        const QString chipName = m_state ? m_state->activeBoard().deviceName : QString("--");
        const QString cpuName  = m_state ? m_state->activeBoard().deviceCpu  : QString("--");
        QStringList cells;
        cells << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
              << QString("SIM-%1").arg(QDateTime::currentDateTime().toString("hhmmss"))
              << (m_simLastCard.isEmpty() ? QString("--") : m_simLastCard)
              << (chipName.isEmpty() ? "--" : chipName)
              << (cpuName.isEmpty()  ? "--" : cpuName)
              << (m_simLastModel.isEmpty() ? QString("--") : m_simLastModel)
              << "INT8"
              << "SimÃ¼lasyon"
              << (QString::number(avgInfUs / 1000.0, 'f', 2) + " ms")
              << (QString::number(avgRamB  / 1024.0, 'f', 2) + " KiB")
              << "--"
              << QString("avg %1% | label=%2 | n=%3")
                     .arg(avgAcc)
                     .arg(m_simLastLabel.isEmpty() ? QString("--") : m_simLastLabel)
                     .arg(m_simSampleCount);
        m_analysis->addRecord("simulation", cells);
        emit analysisChanged();
        appendMonitorLine(QString("[sim] Ortalama kayÄ±t edildi Â- n=%1  ort=%2 ms  acc=%3%")
                              .arg(m_simSampleCount)
                              .arg(avgInfUs / 1000.0, 0, 'f', 2)
                              .arg(avgAcc), "ok");
    }
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
    m_hwSimInputRanges.clear();
    m_hwSimSeed = 1234;
    m_hwSimSentCount = 0;
    m_hwSimResponseCount = 0;
    m_hwSimRunning = true;
    // The N6 board runs the deployer's own template firmware, whose main loop
    // polls UART and answers the INFER command with a §inf packet — exactly like
    // the other boards. So drive it the same way (send synthetic INFER frames).
    // The old N6 "passive capture + reset" path was written for the reference KWS
    // firmware; on the template firmware the reset only dropped the live VCP
    // stream, so the simulation never appeared to start.
    m_hwSimPassiveCapture = false;
    const bool n6Board = boardOrPortLooksLikeN6(m_state->activeBoard(), port);
    const int baud = n6Board
        ? kN6DefaultBaud
        : preferredBaudForBoard(m_state->activeBoard(), m_state->activeBaud());
    m_state->setActiveBaud(baud);
    if (!m_state->isConnected())
        m_serial->connectToPort(port, baud);
    appendMonitorLine(QString("[hw-sim] Donanim simulasyonu basladi - aralik %1 ms").arg(intervalMs), "warn");
    appendMonitorLine("[hw-sim] Sentetik sensor verisi karta INFER ile gonderilecek. Yanit bekleniyor...", "info");
    m_hwSimTimer->start(qMax(50, intervalMs));
    emit simRunningChanged();
}

void Backend::startHardwareSimulationWithInputs(int intervalMs, const QVariantList &inputs)
{
    if (m_hwSimRunning) return;
    double minValue = 0.0;
    double maxValue = 1.0;
    if (!inputs.isEmpty()) {
        minValue = std::numeric_limits<double>::infinity();
        maxValue = -std::numeric_limits<double>::infinity();
        for (const QVariant &value : inputs) {
            const QVariantMap input = value.toMap();
            bool okMin = false;
            bool okMax = false;
            const double inputMin = input.value(QStringLiteral("min")).toDouble(&okMin);
            const double inputMax = input.value(QStringLiteral("max")).toDouble(&okMax);
            if (okMin) minValue = qMin(minValue, inputMin);
            if (okMax) maxValue = qMax(maxValue, inputMax);
        }
        if (!qIsFinite(minValue)) minValue = 0.0;
        if (!qIsFinite(maxValue)) maxValue = 1.0;
    }
    startHardwareSimulation(intervalMs, minValue, maxValue);
    m_hwSimInputRanges = inputs;
}

void Backend::stopHardwareSimulation()
{
    if (!m_hwSimRunning) return;
    m_hwSimTimer->stop();
    m_hwSimRunning = false;
    if (m_hwSimPassiveCapture) {
        appendMonitorLine(QString("[hw-sim] N6 pasif dinleme durduruldu - yakalanan=%1")
                              .arg(m_hwSimResponseCount), "warn");
    } else {
        appendMonitorLine(QString("[hw-sim] Simulasyon durduruldu - gonderilen=%1  yanit=%2")
                              .arg(m_hwSimSentCount).arg(m_hwSimResponseCount), "warn");
    }
    m_hwSimPassiveCapture = false;
    emit simRunningChanged();
}

void Backend::tickHardwareSimulation()
{
    if (m_hwSimPassiveCapture)
        return;

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
        // temperature (Â°C), pressure (hPa normalized 0-1), humidity (%)
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
    const QVariantList activeInputs = m_hwSimInputRanges.isEmpty() ? QVariantList() : m_hwSimInputRanges;
    if (!activeInputs.isEmpty()) {
        for (int i = 0; i < activeInputs.size(); ++i) {
            const QVariantMap input = activeInputs.at(i).toMap();
            const double value = rangedRandomValue(input, lo, hi);
            values << QString::number(qRound(value * 1000.0));
            displayParts << QString("%1=%2").arg(inputDisplayLabel(input, i),
                                                 QString::number(value, 'f', 3));
        }
    } else {
        for (int i = 0; i < profile.labels.size(); ++i) {
            m_hwSimSeed = (m_hwSimSeed * 1664525u) + 1013904223u;
            const double unit  = static_cast<double>(m_hwSimSeed & 0x00FFFFFFu) / 16777215.0;
            const double value = lo + unit * (hi - lo);
            values      << QString::number(qRound(value * 1000.0));
            displayParts << QString("%1=%2").arg(profile.labels[i],
                                                 QString::number(value, 'f', 3));
        }
    }

    const QByteArray command = QString("INFER %1").arg(values.join(' ')).toUtf8();
    ++m_hwSimSentCount;

    // For PDM_MIC condense the display (16 values is too long)
    const QString displayLine = (displayParts.size() > 6)
        ? QString("sim sensor  [%1 deger]  %2=%3 ... %4=%5")
              .arg(displayParts.size())
              .arg(displayParts.first().section('=', 0, 0), displayParts.first().section('=', 1))
              .arg(displayParts.last().section('=', 0, 0),  displayParts.last().section('=', 1))
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

    // Build a fake Â§{JSON}\r\n inference packet and feed it to PacketParser.
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

    const QVariantList activeInputs = m_simInputRanges;
    if (!activeInputs.isEmpty()) {
        QJsonArray sensorValues;
        QStringList displayParts;
        for (int i = 0; i < activeInputs.size(); ++i) {
            const QVariantMap input = activeInputs.at(i).toMap();
            const double value = rangedRandomValue(input, m_simMin, m_simMax);
            sensorValues.append(value);
            displayParts << QString("%1=%2").arg(inputDisplayLabel(input, i),
                                                 QString::number(value, 'f', 3));
        }

        QJsonObject sensorObj;
        sensorObj["t"] = "sensor";
        sensorObj["model"] = model;
        sensorObj["sensor"] = m_state ? m_state->lastSensor() : deployedModelInfo().value(QStringLiteral("sensorType")).toString();
        sensorObj["card"] = card;
        sensorObj["seq"] = static_cast<int>(m_simUptime);
        sensorObj["samples"] = sensorValues.size();
        sensorObj["inf_us"] = infUs;
        sensorObj["ram_b"] = ramB;
        sensorObj["acc_pct"] = accPct;
        sensorObj["label"] = label;
        sensorObj["values"] = sensorValues;
        const QByteArray sensorPacket =
            "\xC2\xA7" + QJsonDocument(sensorObj).toJson(QJsonDocument::Compact) + "\r\n";
        m_simParser->feed(sensorPacket);

        const QString sensorLine = (displayParts.size() > 6)
            ? QString("[sim] sensor  [%1 deger]  %2 ... %3")
                  .arg(displayParts.size())
                  .arg(displayParts.first(), displayParts.last())
            : QString("[sim] sensor  %1").arg(displayParts.join("  "));
        appendMonitorLine(sensorLine, "info");
    }

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
        QString("Â§ inf  %1  %2 ms  %3 KB  acc=%4%  label=%5")
            .arg(model)
            .arg(infUs / 1000.0, 0, 'f', 1)
            .arg(ramB  / 1024.0, 0, 'f', 1)
            .arg(accPct)
            .arg(label),
        "ok");

    // Accumulate the sample; a single averaged record is persisted on stop.
    ++m_simSampleCount;
    m_simTotalInfUs += static_cast<quint64>(infUs);
    m_simTotalRamB  += static_cast<quint64>(ramB);
    m_simTotalAcc   += static_cast<quint64>(accPct);
    m_simLastLabel   = label;
    m_simLastModel   = model;
    m_simLastCard    = card;
}

// â”€â”€ Flash â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
QString Backend::fileInfo(const QString &path) const
{
    QFileInfo fi(path);
    if (!fi.exists()) return QString();
    return fi.fileName() + "  Â-  " + QLocale().formattedDataSize(fi.size());
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
    appendFlashLine(QString("$ Flash baÅŸlatÄ±lÄ±yor: %1").arg(modelName), "cmd");
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
                appendFlashLine(ok ? "âœ“ Flash tamamlandÄ±." : "âœ— Flash baÅŸarÄ±sÄ±z.",
                                ok ? "ok" : "err");
                if (ok && m_state)
                    m_state->setLastModel(cfg.modelName,
                                          m_state->lastInferenceMs(),
                                          m_state->lastAccuracy());
            });
}

// â”€â”€ Pipeline â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
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
    cfg.sdaPin = value("sdaPin", "GPIO_PIN_9");
    cfg.sclPort = value("sclPort", "GPIOB");
    cfg.sclPin = value("sclPin", "GPIO_PIN_8");
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
        appendPipelineLine("HATA: Model dosyasÄ± seÃ§ilmedi veya bulunamadÄ±.", "err");
        return;
    }
    if (m_lastPipelineConfig.outputDir.isEmpty()) {
        appendPipelineLine("HATA: Ã‡Ä±ktÄ± dizini seÃ§ilmedi.", "err");
        return;
    }

    if (m_pipelineRunner)
        m_pipelineRunner->deleteLater();
    m_pipelineRunner = new PipelineRunner(this);
    m_pipelineLines.clear();
    m_pipelineProgress = 0;
    m_pipelineBusy = true;
    m_pipelineStage = "Pipeline baÅŸlatÄ±lÄ±yor...";
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
        m_pipelineStage = success ? "Pipeline tamamlandÄ±" : "Pipeline baÅŸarÄ±sÄ±z";
        if (success) {
            addCompiledRecord(m_lastPipelineConfig);
            AppSettings settings;
            settings.setDeployedModelName(m_lastPipelineConfig.modelName);
            settings.setDeployedModelPath(m_lastPipelineConfig.modelPath);
            settings.setDeployedModelOutputDir(m_lastPipelineConfig.outputDir);
            settings.setDeployedSensorType(m_lastPipelineConfig.sensorType);
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
        m_pipelineStage = "HazÄ±r";
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
          << (firmwarePath.isEmpty() ? "Derlendi" : "ArÅŸivlendi")
          << firmwarePath
          << config.modelPath
          << config.outputDir;
    m_analysis->addRecord("compiled", cells);
    emit analysisChanged();
}

// â”€â”€ Benchmark â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
void Backend::appendBenchmarkLine(const QString &text, const QString &type)
{
    QVariantMap m;
    m["text"] = text;
    m["type"] = type;
    m_benchmarkLines.append(m);
    emit benchmarkChanged();
}

QVariantMap Backend::deployedModelInfo() const
{
    AppSettings settings;
    QVariantMap info;
    const QString modelPath = settings.deployedModelPath();
    QString outputDir = settings.deployedModelOutputDir();
    if (outputDir.isEmpty())
        outputDir = settings.lastOutputDir();
    QString modelName = settings.deployedModelName();
    if (modelName.isEmpty() && m_state)
        modelName = m_state->lastModelName();

    info[QStringLiteral("name")] = modelName;
    info[QStringLiteral("modelPath")] = modelPath;
    info[QStringLiteral("outputDir")] = outputDir;
    info[QStringLiteral("sensorType")] = settings.deployedSensorType();
    info[QStringLiteral("reportPath")] = outputDir.isEmpty()
        ? QString()
        : QDir(outputDir).filePath(QStringLiteral("xcubeai_output/network_c_info.json"));
    info[QStringLiteral("hasModelPath")] = !modelPath.isEmpty() && QFileInfo::exists(modelPath);
    info[QStringLiteral("hasReport")] = !info.value(QStringLiteral("reportPath")).toString().isEmpty()
        && QFileInfo::exists(info.value(QStringLiteral("reportPath")).toString());
    return info;
}

QVariantList Backend::deployedModelInputSpecs()
{
    const QVariantMap info = deployedModelInfo();
    const QString sensorType = info.value(QStringLiteral("sensorType")).toString();
    const QString reportPath = info.value(QStringLiteral("reportPath")).toString();
    if (!reportPath.isEmpty() && QFileInfo::exists(reportPath)) {
        const QVariantList specs = parseInputSpecsFromReport(reportPath);
        if (!specs.isEmpty())
            return withSemanticInputLabels(specs, sensorType);
    }

    const QString modelPath = info.value(QStringLiteral("modelPath")).toString();
    if (!modelPath.isEmpty() && QFileInfo::exists(modelPath))
        return withSemanticInputLabels(modelInputSpecs(modelPath), sensorType);

    return {};
}

QVariantList Backend::modelInputSpecs(const QString &modelPath)
{
    const QString path = QUrl(modelPath).isLocalFile() ? QUrl(modelPath).toLocalFile() : modelPath;
    if (path.trimmed().isEmpty() || !QFileInfo::exists(path))
        return {};

    AppSettings settings;
    QString cliPath = settings.xcubeAICliPath();
    if (cliPath.isEmpty()) {
        cliPath = ToolDetector::detectXCubeAI();
        if (!cliPath.isEmpty())
            settings.setXCubeAICliPath(cliPath);
    }

    if (cliPath.isEmpty())
        return {};

    QTemporaryDir outputDir(QDir::tempPath() + "/stm32-ai-inputs-XXXXXX");
    if (!outputDir.isValid())
        return {};

    QProcess proc;
    proc.setProgram(cliPath);
    proc.setArguments({
        QStringLiteral("analyze"),
        QStringLiteral("--model"), path,
        QStringLiteral("--target"), QStringLiteral("stm32"),
        QStringLiteral("--output"), outputDir.path(),
        QStringLiteral("--no-workspace"),
    });
    proc.start();
    if (!proc.waitForFinished(30000))
        return {};

    if (proc.exitCode() != 0)
        return {};

    QVariantList specs = parseInputSpecsFromReport(
        QDir(outputDir.path()).filePath(QStringLiteral("network_c_info.json")));
    if (!specs.isEmpty())
        return specs;

    QVariantMap fallback;
    fallback[QStringLiteral("name")] = QFileInfo(path).baseName() + QStringLiteral("_input");
    fallback[QStringLiteral("tensorName")] = fallback.value(QStringLiteral("name"));
    fallback[QStringLiteral("shape")] = QStringLiteral("-");
    fallback[QStringLiteral("elements")] = 1;
    fallback[QStringLiteral("elementIndex")] = -1;
    fallback[QStringLiteral("min")] = 0.0;
    fallback[QStringLiteral("max")] = 1.0;
    QVariantList list;
    list.append(fallback);
    return list;
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
        appendBenchmarkLine("Aktif COM port yok. Kartlar ekranÄ±ndan port seÃ§in.", "err");
        return;
    }

    const bool n6Board = boardOrPortLooksLikeN6(m_state->activeBoard(), port);
    m_benchmarkLines.clear();
    m_benchmarkMetrics.clear();
    m_benchmarkBusy = true;
    emit benchmarkChanged();

    auto sendCommand = [this, samples, minValue, maxValue, seed]() {
        if (!m_state || !m_state->isConnected()) {
            appendBenchmarkLine("UART baÄŸlantÄ±sÄ± kurulamadÄ±.", "err");
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

    const int baud = n6Board ? kN6DefaultBaud
                             : preferredBaudForBoard(m_state->activeBoard(), m_state->activeBaud());
    m_state->setActiveBaud(baud);

    // NOTE: The N6 board runs the deployer's own template firmware (§JSON
    // protocol). Its main loop polls UART before the sensor read, so it processes
    // "BENCH n min max seed" even with no sensor wired — Run_Benchmark feeds
    // synthetic input and replies with a §bench packet. So drive it the same way
    // as the other boards: send the BENCH command, no reset/passive capture.
    if (m_state->isConnected()) {
        sendCommand();
    } else {
        appendBenchmarkLine(QString("UART baglantisi aciliyor: %1 @ %2")
                                .arg(port).arg(m_state->activeBaud()), "cmd");
        m_serial->connectToPort(port, m_state->activeBaud() > 0 ? m_state->activeBaud() : 115200);
        QTimer::singleShot(1200, this, sendCommand);
    }
}

void Backend::startBenchmarkWithInputs(int samples, const QVariantList &inputs, int seed)
{
    if (inputs.isEmpty()) {
        startBenchmark(samples, 0.0, 1.0, seed);
        return;
    }

    double minValue = std::numeric_limits<double>::infinity();
    double maxValue = -std::numeric_limits<double>::infinity();

    for (const QVariant &value : inputs) {
        const QVariantMap input = value.toMap();
        bool okMin = false;
        bool okMax = false;
        const double inputMin = input.value(QStringLiteral("min")).toDouble(&okMin);
        const double inputMax = input.value(QStringLiteral("max")).toDouble(&okMax);
        if (okMin)
            minValue = qMin(minValue, inputMin);
        if (okMax)
            maxValue = qMax(maxValue, inputMax);
    }

    if (!qIsFinite(minValue))
        minValue = 0.0;
    if (!qIsFinite(maxValue))
        maxValue = 1.0;
    if (minValue > maxValue)
        std::swap(minValue, maxValue);

    startBenchmark(samples, minValue, maxValue, seed);
    if (inputs.size() > 1 && m_state && !boardOrPortLooksLikeN6(m_state->activeBoard(), m_state->activePort()))
        appendBenchmarkLine("Not: Mevcut kart firmware'i tek BENCH araligi destekliyor; input araliklari birlestirilerek gonderilecek.", "warn");
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

// â”€â”€ Analysis â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
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
            appendFlashLine("HATA: ArÅŸivlenmiÅŸ firmware bulunamadÄ±.", "err");
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

// Parse the first numeric token in a string ("0.934 ms" -> 0.934, "12,6 M" -> 12.6).
static double parseLeadingNumber(const QString &text)
{
    static const QRegularExpression re(QStringLiteral("[-+]?[0-9]*[.,]?[0-9]+"));
    const QRegularExpressionMatch m = re.match(text);
    if (!m.hasMatch())
        return std::numeric_limits<double>::quiet_NaN();
    QString token = m.captured(0);
    token.replace(',', '.');
    bool ok = false;
    const double v = token.toDouble(&ok);
    return ok ? v : std::numeric_limits<double>::quiet_NaN();
}

// Parse a percentage anywhere in the string ("avg 94% | ...", "normal 90%").
static double parsePercent(const QString &text)
{
    static const QRegularExpression re(QStringLiteral("([0-9]+(?:[.,][0-9]+)?)\\s*%"));
    const QRegularExpressionMatch m = re.match(text);
    if (!m.hasMatch())
        return std::numeric_limits<double>::quiet_NaN();
    return m.captured(1).replace(',', '.').toDouble();
}

static int columnIndexByTitle(const QVariantList &columns, const QStringList &candidates)
{
    for (const QString &cand : candidates)
        for (int i = 0; i < columns.size(); ++i)
            if (columnTitle(columns.at(i)).compare(cand, Qt::CaseInsensitive) == 0)
                return i;
    return -1;
}

static QString cellAt(const QVariantList &row, int col)
{
    return (col >= 0 && col < row.size()) ? row.at(col).toString() : QString();
}

static int distinctCount(const QVariantList &rows, int col)
{
    if (col < 0) return 0;
    QSet<QString> seen;
    for (const QVariant &rv : rows) {
        const QString v = cellAt(rv.toList(), col).trimmed();
        if (!v.isEmpty() && v != QStringLiteral("--") && v != QStringLiteral("â€”"))
            seen.insert(v);
    }
    return seen.size();
}

bool Backend::exportAnalysisCsv(const QString &path,
                                const QVariantList &columns,
                                const QVariantList &rows)
{
    const QString outPath = normalizedLocalPath(path, ".csv");
    QFile file(outPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit statusMessage("CSV yazÄ±lamadÄ±: " + file.errorString());
        return false;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream.setGenerateByteOrderMark(true); // UTF-8 BOM so Excel renders Turkish chars

    // Compute summary stats (same column-title resolution as the PDF report).
    const int boardCol  = columnIndexByTitle(columns, {"Kart"});
    const int modelCol  = columnIndexByTitle(columns, {"Model"});
    const int metricCol = columnIndexByTitle(columns, {"Ort", "MACC", "Params"});
    const int resultCol = columnIndexByTitle(columns, {"SonuÃ§"});
    double accSum = 0; int accCount = 0;
    for (const QVariant &rv : rows) {
        const double p = parsePercent(cellAt(rv.toList(), resultCol));
        if (!std::isnan(p)) { accSum += p; ++accCount; }
    }

    // â”€â”€ Metadata header block (Excel-friendly: sep hint + two-column key/value) â”€â”€
    stream << "sep=,\n"; // tells Excel (incl. tr-TR locale) to split on comma
    stream << csvEscape("STM32 AI Deployer â€” Analiz Raporu") << '\n';
    stream << csvEscape("Kurum") << ',' << csvEscape("Marmara Ãœniversitesi Â- Bilgisayar MÃ¼hendisliÄŸi") << '\n';
    stream << csvEscape("Rapor Tarihi") << ',' << csvEscape(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm")) << '\n';
    stream << csvEscape("Toplam KayÄ±t") << ',' << csvEscape(QString::number(rows.size())) << '\n';
    if (boardCol >= 0)
        stream << csvEscape("Kart SayÄ±sÄ±") << ',' << csvEscape(QString::number(distinctCount(rows, boardCol))) << '\n';
    if (modelCol >= 0)
        stream << csvEscape("Model SayÄ±sÄ±") << ',' << csvEscape(QString::number(distinctCount(rows, modelCol))) << '\n';
    if (accCount > 0)
        stream << csvEscape("Ort. DoÄŸruluk") << ',' << csvEscape(QString("%%1").arg(qRound(accSum / accCount))) << '\n';
    stream << '\n';

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

    emit statusMessage("CSV oluÅŸturuldu: " + outPath);
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
    writer.setPageMargins(QMarginsF(14, 14, 14, 16), QPageLayout::Millimeter);

    QPainter painter(&writer);
    if (!painter.isActive()) {
        emit statusMessage("PDF oluÅŸturulamadÄ±.");
        return false;
    }
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    // NOTE: With QPdfWriter the painter origin is already at the top-left of the
    // printable area (inside the margins), so we draw from (0,0). Using the page
    // rect's left/top would double-apply the margin and shift content right.
    const QRect page = writer.pageLayout().paintRectPixels(writer.resolution());
    const int left   = 0;
    const int width  = page.width();
    const int right  = width;
    const int height = page.height();

    // â”€â”€ Palette â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    const QColor cInk("#0f172a");      // near-black text
    const QColor cMuted("#64748b");    // grey text
    const QColor cFaint("#94a3b8");
    const QColor cAccent("#4f46e5");   // indigo
    const QColor cAccent2("#06b6d4");  // cyan
    const QColor cBandTop("#1e293b");  // dark header band
    const QColor cHeaderBg("#eef2ff");
    const QColor cZebra("#f8fafc");
    const QColor cGrid("#e2e8f0");
    const QColor cCardBg("#f1f5f9");

    // â”€â”€ Fonts â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    const QFont titleFont("Arial", 18, QFont::Bold);
    const QFont subFont("Arial", 9);
    const QFont metaFont("Arial", 8);
    const QFont sectionFont("Arial", 11, QFont::Bold);
    const QFont cardValFont("Arial", 14, QFont::Bold);
    const QFont cardLblFont("Arial", 7);
    const QFont headerFont("Arial", 7, QFont::Bold);
    const QFont bodyFont("Arial", 7);
    const QFont footFont("Arial", 7);

    int pageNo = 1;
    const QString reportTitle = title.isEmpty() ? QStringLiteral("STM32 AI Analiz Raporu") : title;
    const QString stamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm");

    auto drawFooter = [&](int n) {
        painter.setFont(footFont);
        painter.setPen(cFaint);
        const int fy = height - 22;
        painter.setPen(cGrid);
        painter.drawLine(left, fy, right, fy);
        painter.setPen(cFaint);
        painter.drawText(QRect(left, fy + 2, width, 20), Qt::AlignLeft | Qt::AlignVCenter,
                         "Marmara Ãœniversitesi Â- Bilgisayar MÃ¼hendisliÄŸi Â- STM32 AI Deployer");
        painter.drawText(QRect(left, fy + 2, width, 20), Qt::AlignRight | Qt::AlignVCenter,
                         QString("Sayfa %1").arg(n));
    };

    // â”€â”€ Cover header band â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    int y = 0;
    const int bandH = 90;
    QLinearGradient bandGrad(left, y, right, y + bandH);
    bandGrad.setColorAt(0.0, cBandTop);
    bandGrad.setColorAt(1.0, cAccent);
    painter.setPen(Qt::NoPen);
    painter.setBrush(bandGrad);
    painter.drawRoundedRect(QRect(left, y, width, bandH), 10, 10);
    // accent underline strip
    painter.setBrush(cAccent2);
    painter.drawRoundedRect(QRect(left + 18, y + bandH - 12, 120, 5), 2, 2);

    painter.setPen(QColor("#ffffff"));
    painter.setFont(titleFont);
    painter.drawText(QRect(left + 18, y + 14, width - 36, 34), Qt::AlignLeft | Qt::AlignVCenter, reportTitle);
    painter.setFont(subFont);
    painter.setPen(QColor("#cbd5e1"));
    painter.drawText(QRect(left + 18, y + 48, width - 36, 22), Qt::AlignLeft | Qt::AlignVCenter,
                     "KarÅŸÄ±laÅŸtÄ±rmalÄ± Performans ve Kaynak Analizi");
    y += bandH + 16;

    // â”€â”€ Meta line â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    painter.setFont(metaFont);
    painter.setPen(cMuted);
    painter.drawText(QRect(left, y, width, 18), Qt::AlignLeft | Qt::AlignVCenter,
                     QString("Rapor Tarihi: %1").arg(stamp));
    painter.drawText(QRect(left, y, width, 18), Qt::AlignRight | Qt::AlignVCenter,
                     QString("Toplam KayÄ±t: %1").arg(rows.size()));
    y += 26;

    // â”€â”€ Resolve key columns for stats / chart â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    const int boardCol  = columnIndexByTitle(columns, {"Kart"});
    const int modelCol  = columnIndexByTitle(columns, {"Model"});
    const int metricCol = columnIndexByTitle(columns, {"Ort", "MACC", "Params"});
    const int resultCol = columnIndexByTitle(columns, {"SonuÃ§"});
    const QString metricTitle = metricCol >= 0 ? columnTitle(columns.at(metricCol)) : QString();
    // Unit suffix from the first parseable metric value (e.g. "ms", "M").
    QString metricUnit;
    for (const QVariant &rv : rows) {
        const QString s = cellAt(rv.toList(), metricCol);
        const QRegularExpression ru(QStringLiteral("[A-Za-zÂµ%]+\\s*$"));
        const QRegularExpressionMatch mu = ru.match(s.trimmed());
        if (!std::isnan(parseLeadingNumber(s)) && mu.hasMatch()) { metricUnit = mu.captured(0).trimmed(); break; }
    }

    // Accuracy average across rows (where a % is present).
    double accSum = 0; int accCount = 0;
    for (const QVariant &rv : rows) {
        const double p = parsePercent(cellAt(rv.toList(), resultCol));
        if (!std::isnan(p)) { accSum += p; ++accCount; }
    }
    // Metric average.
    double metSum = 0; int metCount = 0;
    for (const QVariant &rv : rows) {
        const double v = parseLeadingNumber(cellAt(rv.toList(), metricCol));
        if (!std::isnan(v)) { metSum += v; ++metCount; }
    }

    // â”€â”€ Summary stat cards â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    struct Stat { QString label; QString value; QColor accent; };
    QVector<Stat> stats;
    stats.append({"Toplam KayÄ±t", QString::number(rows.size()), cAccent});
    stats.append({"Kart SayÄ±sÄ±",  QString::number(distinctCount(rows, boardCol)), cAccent2});
    stats.append({"Model SayÄ±sÄ±", QString::number(distinctCount(rows, modelCol)), QColor("#16a34a")});
    if (accCount > 0)
        stats.append({"Ort. DoÄŸruluk", QString("%%1").arg(qRound(accSum / accCount)), QColor("#f59e0b")});
    else if (metCount > 0)
        stats.append({QString("Ort. %1").arg(metricTitle),
                      QString("%1 %2").arg(metSum / metCount, 0, 'f', 2).arg(metricUnit), QColor("#f59e0b")});
    else
        stats.append({"Kategori", reportTitle.section(' ', -2, -2), QColor("#f59e0b")});

    const int cardGap = 12;
    const int cardW = (width - cardGap * (stats.size() - 1)) / stats.size();
    const int cardH = 56;
    for (int i = 0; i < stats.size(); ++i) {
        const int cx = left + i * (cardW + cardGap);
        painter.setPen(Qt::NoPen);
        painter.setBrush(cCardBg);
        painter.drawRoundedRect(QRect(cx, y, cardW, cardH), 8, 8);
        painter.setBrush(stats[i].accent);
        painter.drawRoundedRect(QRect(cx, y + 10, 4, cardH - 20), 2, 2);
        painter.setPen(cMuted);
        painter.setFont(cardLblFont);
        painter.drawText(QRect(cx + 14, y + 8, cardW - 20, 16), Qt::AlignLeft | Qt::AlignVCenter,
                         stats[i].label.toUpper());
        painter.setPen(cInk);
        painter.setFont(cardValFont);
        painter.drawText(QRect(cx + 14, y + 24, cardW - 20, 26), Qt::AlignLeft | Qt::AlignVCenter,
                         stats[i].value);
    }
    y += cardH + 22;

    // â”€â”€ Bar chart (top records by metric) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    if (metricCol >= 0 && metCount > 0) {
        painter.setPen(cInk);
        painter.setFont(sectionFont);
        const QString chartTitle = QString("Model KarÅŸÄ±laÅŸtÄ±rmasÄ± â€” %1%2")
                                       .arg(metricTitle)
                                       .arg(metricUnit.isEmpty() ? QString() : QString(" (%1)").arg(metricUnit));
        painter.drawText(QRect(left, y, width, 22), Qt::AlignLeft | Qt::AlignVCenter, chartTitle);
        y += 26;

        // Collect up to 8 (label, value) pairs.
        struct Bar { QString label; double value; };
        QVector<Bar> bars;
        double maxV = 0;
        for (const QVariant &rv : rows) {
            if (bars.size() >= 8) break;
            const QVariantList row = rv.toList();
            const double v = parseLeadingNumber(cellAt(row, metricCol));
            if (std::isnan(v)) continue;
            QString lbl = cellAt(row, modelCol >= 0 ? modelCol : 0);
            if (lbl.isEmpty()) lbl = QString("#%1").arg(bars.size() + 1);
            bars.append({lbl, v});
            maxV = qMax(maxV, v);
        }

        const int chartH = 150;
        const int axisX = left + 4;
        const int baseY = y + chartH;
        // axis
        painter.setPen(cGrid);
        painter.drawLine(axisX, y, axisX, baseY);
        painter.drawLine(axisX, baseY, right, baseY);

        const int n = bars.size();
        if (n > 0 && maxV > 0) {
            const int slot = (right - axisX - 10) / n;
            const int barW = qMin(60, slot * 6 / 10);
            for (int i = 0; i < n; ++i) {
                const int cx = axisX + 10 + i * slot + (slot - barW) / 2;
                const int h = qMax(3, int((chartH - 8) * (bars[i].value / maxV)));
                const int by = baseY - h;
                QLinearGradient g(cx, by, cx, baseY);
                g.setColorAt(0.0, cAccent);
                g.setColorAt(1.0, cAccent2);
                painter.setPen(Qt::NoPen);
                painter.setBrush(g);
                painter.drawRoundedRect(QRect(cx, by, barW, h), 3, 3);
                // value on top
                painter.setPen(cInk);
                painter.setFont(cardLblFont);
                painter.drawText(QRect(cx - slot / 2 + barW / 2, by - 16, slot, 14),
                                 Qt::AlignHCenter | Qt::AlignBottom,
                                 QString::number(bars[i].value, 'f', bars[i].value < 10 ? 2 : 1));
                // label under axis
                painter.setPen(cMuted);
                painter.drawText(QRect(axisX + 10 + i * slot, baseY + 4, slot, 26),
                                 Qt::AlignHCenter | Qt::AlignTop,
                                 painter.fontMetrics().elidedText(bars[i].label, Qt::ElideRight, slot - 2));
            }
        }
        y = baseY + 34;
    }

    // â”€â”€ Data table â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    painter.setPen(cInk);
    painter.setFont(sectionFont);
    painter.drawText(QRect(left, y, width, 22), Qt::AlignLeft | Qt::AlignVCenter, "DetaylÄ± KayÄ±t Tablosu");
    y += 28;

    const int cols = qMax(1, columns.size());
    // Proportional column widths from declared widths (0 -> stretch).
    QVector<int> colW(cols, 0);
    int fixedTotal = 0, stretchCount = 0;
    for (int c = 0; c < cols; ++c) {
        const QVariantMap m = columns.at(c).toMap();
        int w = m.value("width", m.value("w", 0)).toInt();
        colW[c] = w;
        if (w > 0) fixedTotal += w; else ++stretchCount;
    }
    // Scale fixed widths into page; distribute remainder to stretch columns.
    const int minStretch = 70;
    int remaining = width - fixedTotal;
    if (stretchCount > 0 && remaining < stretchCount * minStretch) {
        // shrink fixed columns proportionally so stretch columns get a minimum
        const double scale = double(width - stretchCount * minStretch) / qMax(1, fixedTotal);
        fixedTotal = 0;
        for (int c = 0; c < cols; ++c) if (colW[c] > 0) { colW[c] = qMax(36, int(colW[c] * scale)); fixedTotal += colW[c]; }
        remaining = width - fixedTotal;
    }
    const int stretchW = stretchCount > 0 ? remaining / stretchCount : 0;
    for (int c = 0; c < cols; ++c) if (colW[c] == 0) colW[c] = stretchW;
    // final fit correction
    int sumW = 0; for (int c = 0; c < cols; ++c) sumW += colW[c];
    if (cols > 0) colW[cols - 1] += (width - sumW);

    const int headH = 30;
    const int rowH  = 26;

    auto colX = [&](int c) { int x = left; for (int i = 0; i < c; ++i) x += colW[i]; return x; };

    auto drawTableHeader = [&]() {
        painter.setPen(Qt::NoPen);
        painter.setBrush(cHeaderBg);
        painter.drawRoundedRect(QRect(left, y, width, headH), 5, 5);
        painter.setPen(cAccent);
        painter.setFont(headerFont);
        for (int c = 0; c < cols; ++c) {
            const QRect cell(colX(c) + 5, y, colW[c] - 8, headH);
            painter.drawText(cell, Qt::AlignVCenter | Qt::AlignLeft,
                             painter.fontMetrics().elidedText(columnTitle(columns.at(c)).toUpper(),
                                                              Qt::ElideRight, cell.width()));
        }
        y += headH;
    };

    drawTableHeader();
    painter.setFont(bodyFont);

    for (int r = 0; r < rows.size(); ++r) {
        if (y + rowH > height - 40) {
            drawFooter(pageNo++);
            writer.newPage();
            y = 0;
            painter.setFont(sectionFont);
            painter.setPen(cInk);
            painter.drawText(QRect(left, y, width, 20), Qt::AlignLeft | Qt::AlignVCenter,
                             "DetaylÄ± KayÄ±t Tablosu (devam)");
            y += 26;
            drawTableHeader();
            painter.setFont(bodyFont);
        }

        if (r % 2 == 1) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(cZebra);
            painter.drawRect(QRect(left, y, width, rowH));
        }
        painter.setPen(QColor("#334155"));
        const QVariantList row = rows.at(r).toList();
        for (int c = 0; c < cols; ++c) {
            const QRect cell(colX(c) + 5, y, colW[c] - 8, rowH);
            painter.setFont(c == 0 ? headerFont : bodyFont);
            painter.setPen(c == 0 ? cInk : QColor("#334155"));
            painter.drawText(cell, Qt::AlignVCenter | Qt::AlignLeft,
                             painter.fontMetrics().elidedText(cellAt(row, c), Qt::ElideRight, cell.width()));
        }
        painter.setPen(cGrid);
        painter.drawLine(left, y + rowH, right, y + rowH);
        y += rowH;
    }

    drawFooter(pageNo);
    painter.end();
    emit statusMessage("PDF oluÅŸturuldu: " + outPath);
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
                      << "--" << "TamamlandÄ±";
                m_analysis->addRecord("benchmark", cells);
                emit analysisChanged();
                emit benchmarkChanged();
            });

    // Real sensor results are persisted as explicit sessions only.
    connect(m_serial, &SerialManager::sensorReceived, this,
            [this](const SensorData &d) {
                if (!m_sensorAnalysisRunning)
                    return;
                ++m_sensorAnalysisCount;
                m_sensorAnalysisTotalInfUs += d.inf_us;
                m_sensorAnalysisTotalRamB += d.ram_b;
                m_sensorAnalysisTotalAcc += d.acc_pct;
                if (!d.model.isEmpty()) m_sensorAnalysisLastModel = d.model;
                if (!d.sensor.isEmpty()) m_sensorAnalysisLastSensor = d.sensor;
                if (!d.card.isEmpty()) m_sensorAnalysisLastCard = d.card;
                if (!d.label.isEmpty()) {
                    m_sensorAnalysisLastLabel = d.label;
                    ++m_sensorAnalysisLabelCounts[d.label];
                }
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

void Backend::handleN6TextLine(const QString &line)
{
    if (!m_state)
        return;

    const QString trimmed = line.trimmed();
    if (trimmed.isEmpty())
        return;

    const bool activeN6 = boardOrPortLooksLikeN6(m_state->activeBoard(), m_state->activePort());
    const bool lineLooksN6 = trimmed.contains("N6", Qt::CaseInsensitive)
                          || trimmed.contains("STAI", Qt::CaseInsensitive)
                          || trimmed.contains("NPU", Qt::CaseInsensitive)
                          || trimmed.contains("BENCH-SUMMARY", Qt::CaseInsensitive)
                          || trimmed.contains("[PRED]", Qt::CaseInsensitive)
                          || trimmed.contains("[IRL", Qt::CaseInsensitive);
    if (!activeN6 && !lineLooksN6)
        return;

    const QRegularExpression cpuRe(QStringLiteral("\\bCPU\\s*=\\s*([0-9]+(?:\\.[0-9]+)?)\\s*MHz"),
                                   QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch cpuMatch = cpuRe.match(trimmed);
    if (cpuMatch.hasMatch()) {
        bool ok = false;
        const double cpuMhz = cpuMatch.captured(1).toDouble(&ok);
        if (ok && cpuMhz > 0.0)
            m_n6LastCpuMhz = cpuMhz;
    }
    const double namedCpuMhz = namedDoubleValue(trimmed, QStringLiteral("cpu_mhz"), 0.0);
    if (namedCpuMhz > 0.0)
        m_n6LastCpuMhz = namedCpuMhz;

    QString model = namedTextValue(trimmed, QStringLiteral("model"));
    if (model.isEmpty()) {
        const QRegularExpression modelRe(QStringLiteral("^\\[[^\\]]*N6[^\\]]*\\]\\s*([^|]+)"),
                                         QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch modelMatch = modelRe.match(trimmed);
        if (modelMatch.hasMatch())
            model = modelMatch.captured(1).trimmed();
    }
    if (!model.isEmpty()) {
        m_n6LastTextModel = model;
        m_state->setLastModel(model, m_state->lastInferenceMs(), m_state->lastAccuracy());
    }

    if (trimmed.startsWith(QStringLiteral("CSV:"), Qt::CaseInsensitive)) {
        const QString csvBody = trimmed.mid(4).trimmed();
        const QStringList fields = csvBody.split(',', Qt::KeepEmptyParts);
        if (fields.size() >= 11) {
            const QString csvModel = fields.value(2).trimmed();
            const QString csvBoard = fields.value(5).trimmed();
            bool okSamples = false;
            bool okMean = false;
            bool okMin = false;
            bool okMax = false;
            const quint32 samples = fields.value(6).trimmed().toUInt(&okSamples);
            const double meanUs = fields.value(7).trimmed().toDouble(&okMean);
            const double minUs = fields.value(9).trimmed().toDouble(&okMin);
            const double maxUs = fields.value(10).trimmed().toDouble(&okMax);
            if (okMean) {
                if (m_benchmarkTimeout)
                    m_benchmarkTimeout->stop();
                m_benchmarkBusy = false;

                const QString benchModel = csvModel.isEmpty() ? m_state->lastModelName() : csvModel;
                const quint32 avgUs = static_cast<quint32>(qMax(0.0, meanUs));
                const quint32 minValUs = static_cast<quint32>(qMax(0.0, okMin ? minUs : meanUs));
                const quint32 maxValUs = static_cast<quint32>(qMax(0.0, okMax ? maxUs : meanUs));
                const quint32 sampleCount = okSamples && samples > 0 ? samples : 1;

                m_benchmarkMetrics["model"] = benchModel;
                m_benchmarkMetrics["avg"] = QString("%1 us").arg(avgUs);
                m_benchmarkMetrics["min"] = QString("%1 us").arg(minValUs);
                m_benchmarkMetrics["max"] = QString("%1 us").arg(maxValUs);
                m_benchmarkMetrics["ram"] = QStringLiteral("--");
                m_benchmarkMetrics["freeRam"] = QStringLiteral("--");
                m_benchmarkMetrics["samples"] = sampleCount;
                appendBenchmarkLine(QString("N6 CSV result: samples=%1 avg=%2us min=%3us max=%4us")
                                        .arg(sampleCount).arg(avgUs).arg(minValUs).arg(maxValUs), "ok");
                if (m_hwSimRunning && m_hwSimPassiveCapture)
                    ++m_hwSimResponseCount;

                if (m_analysis) {
                    const BoardInfo board = m_state->activeBoard();
                    QStringList cells;
                    cells << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
                          << QString("BENCH-N6-%1").arg(sampleCount)
                          << (csvBoard.isEmpty() ? (board.name.isEmpty() ? QStringLiteral("STM32N6") : board.name) : csvBoard)
                          << (board.deviceName.isEmpty() ? board.name : board.deviceName)
                          << (board.deviceCpu.isEmpty() ? QStringLiteral("Cortex-M55 + NPU") : board.deviceCpu)
                          << (benchModel.isEmpty() ? QStringLiteral("--") : benchModel)
                          << "INT8" << "--"
                          << (QString::number(avgUs / 1000.0, 'f', 2) + " ms")
                          << "--" << "--" << "Tamamlandi";
                    m_analysis->addRecord("benchmark", cells);
                    emit analysisChanged();
                }
                emit benchmarkChanged();
            }
        }
        return;
    }

    if (trimmed.contains("BENCH-SUMMARY", Qt::CaseInsensitive)) {
        const qulonglong samples = namedUIntValue(trimmed, QStringLiteral("n"),
                                  namedUIntValue(trimmed, QStringLiteral("samples"), 1));
        const qulonglong meanCy = namedUIntValue(trimmed, QStringLiteral("infer_mean_cy"),
                                namedUIntValue(trimmed, QStringLiteral("avg_cy"), 0));
        const qulonglong minCy = namedUIntValue(trimmed, QStringLiteral("min"),
                               namedUIntValue(trimmed, QStringLiteral("min_cy"), meanCy));
        const qulonglong maxCy = namedUIntValue(trimmed, QStringLiteral("max"),
                               namedUIntValue(trimmed, QStringLiteral("max_cy"), meanCy));
        if (meanCy > 0) {
            if (m_benchmarkTimeout)
                m_benchmarkTimeout->stop();
            m_benchmarkBusy = false;

            const QString benchModel = !model.isEmpty() ? model
                                    : (!m_n6LastTextModel.isEmpty() ? m_n6LastTextModel
                                                                    : m_state->lastModelName());
            const quint32 avgUs = cyclesToMicros(meanCy, m_n6LastCpuMhz);
            const quint32 minUs = cyclesToMicros(minCy, m_n6LastCpuMhz);
            const quint32 maxUs = cyclesToMicros(maxCy, m_n6LastCpuMhz);

            m_benchmarkMetrics["model"] = benchModel;
            m_benchmarkMetrics["avg"] = QString("%1 us").arg(avgUs);
            m_benchmarkMetrics["min"] = QString("%1 us").arg(minUs);
            m_benchmarkMetrics["max"] = QString("%1 us").arg(maxUs);
            m_benchmarkMetrics["ram"] = QStringLiteral("--");
            m_benchmarkMetrics["freeRam"] = QStringLiteral("--");
            m_benchmarkMetrics["samples"] = static_cast<quint32>(qMax<qulonglong>(1, samples));
            appendBenchmarkLine(QString("N6 BENCH result: samples=%1 avg=%2us min=%3us max=%4us cpu=%5MHz")
                                    .arg(samples).arg(avgUs).arg(minUs).arg(maxUs)
                                    .arg(m_n6LastCpuMhz, 0, 'f', 0), "ok");
            if (m_hwSimRunning && m_hwSimPassiveCapture)
                ++m_hwSimResponseCount;

            // N6 reference firmware usually emits both BENCH-SUMMARY and CSV for one run.
            // Keep summary for live metrics; persist only CSV to avoid duplicate analysis rows.
            emit benchmarkChanged();
        }
        return;
    }

    const bool predictionLine = trimmed.contains("[PRED]", Qt::CaseInsensitive)
                             || trimmed.contains("[IRL", Qt::CaseInsensitive);
    if (!predictionLine)
        return;

    QString label = n6PredictionLabelFromLine(trimmed);
    if (!label.isEmpty())
        m_n6LastTextLabel = label;

    const qulonglong cycles = namedUIntValue(trimmed, QStringLiteral("cy"),
                            namedUIntValue(trimmed, QStringLiteral("cycles"), 0));
    const quint32 infUs = cyclesToMicros(cycles, m_n6LastCpuMhz);
    const quint8 confidence = confidencePercentFromLine(trimmed);
    const QString liveModel = !m_n6LastTextModel.isEmpty() ? m_n6LastTextModel : m_state->lastModelName();
    const QString liveLabel = !label.isEmpty() ? label : m_n6LastTextLabel;
    if (infUs > 0 || !liveLabel.isEmpty()) {
        m_state->setLiveMetrics(liveModel, infUs / 1000.0, confidence, m_state->lastRamKb(), liveLabel);
        if (m_hwSimRunning && m_hwSimPassiveCapture)
            ++m_hwSimResponseCount;
        if (m_sensorAnalysisRunning) {
            ++m_sensorAnalysisCount;
            m_sensorAnalysisTotalInfUs += infUs;
            m_sensorAnalysisTotalAcc += confidence;
            if (!liveModel.isEmpty()) m_sensorAnalysisLastModel = liveModel;
            if (!liveLabel.isEmpty()) {
                m_sensorAnalysisLastLabel = liveLabel;
                ++m_sensorAnalysisLabelCounts[liveLabel];
            }
            m_sensorAnalysisLastCard = m_state->activeBoard().name;
            if (m_sensorAnalysisLastSensor.isEmpty())
                m_sensorAnalysisLastSensor = m_state->lastSensor();
        }
    }
}

// â”€â”€ Serial â†’ AppState + monitor wiring â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
void Backend::wireSerial()
{
    if (!m_serial) return;

    connect(m_serial, &SerialManager::connectionChanged, this,
            [this](bool connected, const QString &info) {
                if (m_state) m_state->setConnected(connected, info);
                appendMonitorLine(connected ? ("[baÄŸlandÄ±] " + info) : "[baÄŸlantÄ± kesildi]",
                                  connected ? "ok" : "warn");
                if (connected) requestBoardInfoBurst();
            });

    connect(m_serial, &SerialManager::rawLineReceived, this,
            [this](const QString &line) {
                appendMonitorLine(line, "info");
                handleN6TextLine(line);
            });

    connect(m_serial, &SerialManager::bootReceived, this,
            [this](const BootData &boot) {
                appendMonitorLine(QString("Â§ boot  card=%1  sdk=%2  model=%3  baud=%4")
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
                appendMonitorLine(QString("Â§ inf  %1  %2 ms  %3 KB  acc=%4%  label=%5")
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
                appendMonitorLine(QString("Â§ sensor  %1  model=%2  values=[%3]  %4%  label=%5")
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
                appendMonitorLine(QString("Â§ sys  uptime=%1s  temp=%2C  free=%3 KB  state=%4")
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
    if (m_state && boardOrPortLooksLikeN6(m_state->activeBoard(), m_state->activePort()))
        return;

    auto request = [this]() {
        if (!m_serial || !m_serial->isConnected()) return;
        appendMonitorLine("[cmd] kart bilgisi isteniyor: INFO? / BOOT?", "cmd");
        m_serial->requestBoardInfo();
    };

    QTimer::singleShot(100, this, request);
    QTimer::singleShot(700, this, request);
    QTimer::singleShot(1500, this, request);
}
