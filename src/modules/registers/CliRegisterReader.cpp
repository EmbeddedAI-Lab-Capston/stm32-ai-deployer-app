#include "CliRegisterReader.h"
#include "HexDumpParser.h"
#include "modules/flash/CliRunner.h"

CliRegisterReader::CliRegisterReader(QObject *parent)
    : IRegisterReader(parent)
    , m_cli(new CliRunner(this))
{
    qRegisterMetaType<RegisterReadResult>();

    connect(m_cli, &CliRunner::outputLine, this,
            [this](const QString &line) { m_output << line; });
    connect(m_cli, &CliRunner::errorLine, this,
            [this](const QString &line) { m_output << line; });
    connect(m_cli, &CliRunner::finished, this, &CliRegisterReader::onCliFinished);
}

void CliRegisterReader::setCliPath(const QString &path) { m_cli->setCliPath(path); }
void CliRegisterReader::setStlinkSn(const QString &sn)  { m_stlinkSn = sn; }
void CliRegisterReader::setConnectMode(const QString &mode)
{
    if (!mode.isEmpty())
        m_connectMode = mode;
}

void CliRegisterReader::read(const ReadPlan &plan)
{
    if (m_busy) {
        emit readFailed(QStringLiteral("CliRegisterReader is busy"));
        return;
    }
    if (m_cli->cliPath().isEmpty()) {
        emit readFailed(QStringLiteral("STM32_Programmer_CLI path is not set"));
        return;
    }
    if (plan.items.isEmpty()) {
        // Nothing to read (e.g. every selected peripheral was clock-gated).
        RegisterReadResult empty;
        empty.processOk = true;
        empty.exitCode = 0;
        emit readFinished(empty);
        return;
    }

    m_currentPlan = plan;
    m_output.clear();
    m_busy = true;

    // -c port=SWD mode=<mode> [sn=<sn>] -r32 <addr> <bytes> -r32 ...
    QStringList args{ QStringLiteral("-c"), QStringLiteral("port=SWD"),
                      QStringLiteral("mode=%1").arg(m_connectMode) };
    if (!m_stlinkSn.isEmpty())
        args << QStringLiteral("sn=%1").arg(m_stlinkSn);
    for (const ReadPlanItem &item : plan.items) {
        args << QStringLiteral("-r32")
             << QStringLiteral("0x%1").arg(item.startAddr, 8, 16, QLatin1Char('0'))
             << QStringLiteral("0x%1").arg(item.byteCount, 0, 16);
    }

    m_cli->run(args);
}

void CliRegisterReader::onCliFinished(bool success, int exitCode)
{
    m_busy = false;

    RegisterReadResult result;
    result.processOk = success;
    result.exitCode = exitCode;
    result.values = HexDumpParser::parse(m_output);

    // Block-error attribution: a range whose addresses are entirely absent from
    // the parsed output failed to read (clock-off ranges still return zeros, so
    // absence — not zero — signals failure). The snapshot survives regardless.
    const QStringList markers = HexDumpParser::errorMarkers(m_output);
    for (const ReadPlanItem &item : m_currentPlan.items) {
        bool anyPresent = false;
        for (quint32 off = 0; off < item.byteCount; off += 4) {
            if (result.values.contains(item.startAddr + off)) {
                anyPresent = true;
                break;
            }
        }
        if (!anyPresent) {
            const QString msg = markers.isEmpty()
                ? QStringLiteral("no data returned for range")
                : markers.join(QStringLiteral("; "));
            result.errors.append({ item.peripheralName, item.startAddr, msg });
        }
    }

    emit readFinished(result);
}
