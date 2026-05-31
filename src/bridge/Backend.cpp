#include "Backend.h"

#include <QSerialPortInfo>
#include <QFileInfo>
#include <QLocale>

#include "core/AppState.h"
#include "core/AppSettings.h"
#include "core/ToolDetector.h"
#include "modules/serial/SerialManager.h"
#include "modules/flash/FlashManager.h"
#include "modules/analysis/AnalysisManager.h"
#include "modules/board/BoardPresets.h"

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
    wireSerial();
    wireFlash();
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
    if (key == "programmer/cli_path")    s.setProgrammerCliPath(path);
    else if (key == "tools/xcubeai_cli_path") s.setXCubeAICliPath(path);
    else if (key == "tools/gcc_path")    s.setGccPath(path);
    else if (key == "tools/make_path")   s.setMakePath(path);
    if (m_flash && key == "programmer/cli_path")
        m_flash->setCliPath(path);
    emit toolPathsChanged();
}

void Backend::scanTools()
{
    if (m_scanning)
        return;
    m_scanning = true;
    emit scanningChanged();

    if (!m_detector)
        m_detector = new ToolDetector(this);

    connect(m_detector, &ToolDetector::toolFound, this,
            [this](const ToolInfo &info) {
                if (!info.found)
                    return;
                AppSettings s;
                if (info.key == "tools/gcc_path")            s.setGccPath(info.path);
                else if (info.key == "tools/make_path")      s.setMakePath(info.path);
                else if (info.key == "programmer/cli_path")  s.setProgrammerCliPath(info.path);
                else if (info.key == "tools/xcubeai_cli_path") s.setXCubeAICliPath(info.path);
                emit statusMessage(QString("✓ %1 bulundu").arg(info.name));
            }, Qt::UniqueConnection);

    connect(m_detector, &ToolDetector::detectionFinished, this,
            [this](const QList<ToolInfo> &) {
                AppSettings s;
                s.setToolsAutoDetected(true);
                if (m_flash)
                    m_flash->setCliPath(AppSettings().programmerCliPath());
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
    const auto ports = SerialManager::availablePorts();
    for (const QSerialPortInfo &p : ports) {
        QString label = p.portName();
        if (p.vendorIdentifier() == 0x0483)      // ST-Microelectronics
            label += " (ST-Link)";
        else if (!p.description().isEmpty())
            label += " · " + p.description();
        out << label;
    }
    return out;
}

QString Backend::detectedStLinkPort() const
{
    const QSerialPortInfo info = SerialManager::findStLink();
    return info.isNull() ? QString() : info.portName();
}

void Backend::connectSerial(const QString &portName, int baud)
{
    if (!m_serial)
        return;
    // Strip any " (ST-Link)" / " · desc" suffix from a display string.
    QString port = portName.section(' ', 0, 0);
    m_serial->connectToPort(port, baud);
}

void Backend::disconnectSerial()
{
    if (m_serial)
        m_serial->disconnectPort();
}

// ── Monitor terminal ────────────────────────────────────────────────────────
void Backend::clearMonitor()
{
    m_monitorLines.clear();
    emit monitorLinesChanged();
}

void Backend::appendLine(const QString &text, const QString &type)
{
    QVariantMap m;
    m["text"] = text;
    m["type"] = type;
    m_monitorLines.append(m);
    if (m_monitorLines.size() > kMaxMonitorLines)
        m_monitorLines.removeFirst();
    emit monitorLinesChanged();
}

// ── Flash ───────────────────────────────────────────────────────────────────
QString Backend::fileInfo(const QString &path) const
{
    QFileInfo fi(path);
    if (!fi.exists())
        return QString();
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
    if (!m_flash || m_flashBusy)
        return;

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

    m_flashProgress = 0;
    emit flashProgressChanged();
    m_flashBusy = true;
    emit flashBusyChanged();
    appendFlashLine(QString("$ Flash başlatılıyor: %1").arg(modelName), "cmd");
    m_flash->flash(cfg);
}

void Backend::cancelFlash()
{
    if (m_flash && m_flashBusy)
        m_flash->cancel();
}

void Backend::wireFlash()
{
    if (!m_flash)
        return;

    connect(m_flash, &FlashManager::outputLine, this,
            [this](const QString &l) { appendFlashLine(l, "info"); });
    connect(m_flash, &FlashManager::errorLine, this,
            [this](const QString &l) { appendFlashLine(l, "err"); });
    connect(m_flash, &FlashManager::progressChanged, this,
            [this](int p) { m_flashProgress = p; emit flashProgressChanged(); });
    connect(m_flash, &FlashManager::flashFinished, this,
            [this](bool ok, const FlashConfig &cfg) {
                m_flashProgress = ok ? 100 : m_flashProgress;
                emit flashProgressChanged();
                m_flashBusy = false;
                emit flashBusyChanged();
                appendFlashLine(ok ? "✓ Flash tamamlandı." : "✗ Flash başarısız.",
                                ok ? "ok" : "err");
                if (ok && m_state)
                    m_state->setLastModel(cfg.modelName,
                                          m_state->lastInferenceMs(),
                                          m_state->lastAccuracy());
            });
}

// ── Serial → AppState + monitor wiring (replicates MainWindow logic) ─────────
void Backend::wireSerial()
{
    if (!m_serial)
        return;

    connect(m_serial, &SerialManager::connectionChanged, this,
            [this](bool connected, const QString &info) {
                if (m_state)
                    m_state->setConnected(connected, info);
                appendLine(connected ? ("[bağlandı] " + info) : "[bağlantı kesildi]",
                           connected ? "ok" : "warn");
                if (connected)
                    m_serial->requestBoardInfo();
            });

    connect(m_serial, &SerialManager::rawLineReceived, this,
            [this](const QString &line) { appendLine(line, "info"); });

    connect(m_serial, &SerialManager::bootReceived, this,
            [this](const BootData &boot) {
                appendLine(QString("[boot] %1 · %2 · %3")
                               .arg(boot.card, boot.sdk).arg(boot.baud), "cmd");
                if (m_state) {
                    if (!boot.model.isEmpty())
                        m_state->setLastModel(boot.model, 0.0, 0);
                    if (boot.baud > 0)
                        m_state->setActiveBaud(static_cast<qint32>(boot.baud));
                }
            });

    connect(m_serial, &SerialManager::inferenceReceived, this,
            [this](const InferenceData &d) {
                appendLine(QString("[inf] %1 us · acc %2%% · %3")
                               .arg(d.inf_us).arg(d.acc_pct).arg(d.label), "ok");
                if (m_state)
                    m_state->setLastModel(
                        d.model.isEmpty() ? m_state->lastModelName() : d.model,
                        d.inf_us / 1000.0, static_cast<quint8>(d.acc_pct));
            });

    connect(m_serial, &SerialManager::sysReceived, this,
            [this](const SysData &s) {
                appendLine(QString("[sys] uptime %1s · %2°C · %3")
                               .arg(s.uptime_s).arg(s.temp_c).arg(s.state), "info");
            });

    connect(m_serial, &SerialManager::errorReceived, this,
            [this](const ErrorData &e) {
                appendLine(QString("[err %1] %2").arg(e.code).arg(e.msg), "err");
            });
}
