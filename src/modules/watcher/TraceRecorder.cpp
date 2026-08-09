#include "TraceRecorder.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>

namespace {
QString csvEscape(const QString &s)
{
    QString out = s;
    out.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return out;
}
}

bool TraceRecorder::start(const QString &path, const QString &board, const QString &elfPath,
                           const QString &model, int targetRateHz, const QList<WatchItem> &items)
{
    if (m_file.isOpen())
        stop();

    QDir().mkpath(QFileInfo(path).absolutePath());

    m_file.setFileName(path);
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        m_lastError = m_file.errorString();
        return false;
    }
    m_stream.setDevice(&m_file);

    m_sampleCount = 0;
    m_haveFirst   = false;
    m_events.clear();
    m_lastError.clear();

    m_stream << "# stm32-ai-deployer watch trace v1\n";
    m_stream << "# board=" << board << " elf=" << elfPath << " model=" << model
              << " started=" << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
    m_stream << "# targetHz=" << targetRateHz << " items=" << items.size() << "\n";

    for (int i = 0; i < items.size(); ++i) {
        const WatchItem &it = items.at(i);
        m_stream << "# item," << i << ',' << it.label << ",0x"
                  << QString::number(it.address, 16) << ','
                  << watchValueTypeToString(it.type) << ','
                  << displayFormatToString(it.format) << ','
                  << it.scale << ',' << it.offset << ','
                  << it.unit << ',' << it.role << "\n";
    }

    m_stream << "t";
    for (int i = 0; i < items.size(); ++i)
        m_stream << ',' << i;
    m_stream << "\n";

    return true;
}

void TraceRecorder::appendBatch(const WatchSampleBatch &batch)
{
    if (!m_file.isOpen())
        return;

    const int n = batch.times.size();
    for (int k = 0; k < n; ++k) {
        const double t = batch.times.at(k);
        m_stream << QString::number(t, 'f', 6);
        for (int it = 0; it < batch.series.size(); ++it) {
            const double v = (k < batch.series.at(it).size()) ? batch.series.at(it).at(k) : 0.0;
            m_stream << ',' << QString::number(v, 'g', 17);   // full double round-trip precision
        }
        m_stream << "\n";

        if (!m_haveFirst) { m_firstT = t; m_haveFirst = true; }
        m_lastT = t;
        ++m_sampleCount;
    }
}

void TraceRecorder::addEvent(double t, const QString &kind, const QString &text, const QString &severity)
{
    if (!m_file.isOpen())
        return;
    m_events.append({t, kind, text, severity});
}

void TraceRecorder::stop()
{
    if (!m_file.isOpen())
        return;

    const double duration = m_haveFirst ? (m_lastT - m_firstT) : 0.0;
    const double actualHz = (duration > 0.0) ? (double(m_sampleCount) / duration) : 0.0;

    m_stream << "# summary,actualHz=" << QString::number(actualHz, 'f', 2)
              << ",samples=" << m_sampleCount
              << ",durationS=" << QString::number(duration, 'f', 3) << "\n";

    for (const PendingEvent &e : m_events) {
        m_stream << "# event," << QString::number(e.t, 'f', 6) << ',' << e.kind
                  << ",\"" << csvEscape(e.text) << "\"," << e.severity << "\n";
    }

    m_stream.flush();
    m_file.close();
}
