#include "MemReadClient.h"

#include <QElapsedTimer>
#include <QFileInfo>
#include <QProcess>

#include "MemReadProtocol.h"

namespace {

constexpr int kStartTimeoutMs   = 5000;
// Opening an ST-Link takes about a second; a read answers in ~2 ms but must
// still tolerate the odd USB hiccup without tearing the session down.
constexpr int kConnectTimeoutMs = 15000;
constexpr int kReadTimeoutMs    = 3000;

std::string toStd(const QByteArray &bytes)
{
    return std::string(bytes.constData(), static_cast<std::size_t>(bytes.size()));
}

QByteArray fromStd(const std::string &bytes)
{
    return QByteArray(bytes.data(), static_cast<int>(bytes.size()));
}

} // namespace

MemReadClient::MemReadClient(QObject *parent) : QObject(parent) {}

MemReadClient::~MemReadClient()
{
    stop();
}

void MemReadClient::setPaths(const QString &sidecarPath, const QString &cubeProgrammerBinDir)
{
    m_sidecarPath = sidecarPath;
    m_binDir = cubeProgrammerBinDir;
}

void MemReadClient::setStlinkSerial(const QString &serial)
{
    m_serial = serial;
}

bool MemReadClient::isRunning() const
{
    return m_process && m_process->state() == QProcess::Running;
}

void MemReadClient::setError(const QString &message, QString *error)
{
    m_lastError = message;
    if (error)
        *error = message;
}

bool MemReadClient::start(QString *error)
{
    if (isRunning())
        return true;

    if (m_sidecarPath.isEmpty() || !QFileInfo::exists(m_sidecarPath)) {
        setError(QStringLiteral("stm32aid-memread bulunamadi: ") + m_sidecarPath, error);
        return false;
    }
    if (m_binDir.isEmpty()) {
        setError(QStringLiteral("STM32CubeProgrammer dizini ayarlanmamis."), error);
        return false;
    }

    delete m_process;
    m_process = new QProcess(this);
    m_buffer.clear();
    m_connected = false;

    // stderr is diagnostics only; keeping it separate means a stray message can
    // never be mistaken for protocol bytes on stdout.
    m_process->setProcessChannelMode(QProcess::SeparateChannels);
    m_process->start(m_sidecarPath, {m_binDir});
    if (!m_process->waitForStarted(kStartTimeoutMs)) {
        setError(QStringLiteral("stm32aid-memread baslatilamadi: ") + m_process->errorString(), error);
        delete m_process;
        m_process = nullptr;
        return false;
    }

    QByteArray response;
    if (!exchange(fromStd(memread::encodePing()), response, kStartTimeoutMs, error)) {
        stop();
        return false;
    }

    memread::Response parsed;
    if (!memread::decodeResponse(toStd(response), parsed) || parsed.status != memread::Status::Ok) {
        setError(QStringLiteral("stm32aid-memread beklenmeyen yanit verdi."), error);
        stop();
        return false;
    }

    emit logLine(QStringLiteral("[memread] %1 hazir.").arg(QString::fromStdString(parsed.text)));
    return true;
}

bool MemReadClient::connectTarget(QString *error)
{
    if (!isRunning() && !start(error))
        return false;

    QByteArray response;
    if (!exchange(fromStd(memread::encodeConnect(m_serial.toStdString())),
                  response, kConnectTimeoutMs, error))
        return false;

    memread::Response parsed;
    if (!memread::decodeResponse(toStd(response), parsed)) {
        setError(QStringLiteral("baglanti yaniti cozulemedi."), error);
        return false;
    }
    if (parsed.status != memread::Status::Ok) {
        setError(QString::fromStdString(parsed.text), error);
        return false;
    }

    m_connected = true;
    m_board = QString::fromStdString(parsed.text);
    emit logLine(QStringLiteral("[memread] baglandi: %1")
                     .arg(m_board.isEmpty() ? m_serial : m_board));
    return true;
}

bool MemReadClient::readRanges(const QVector<MemoryRequest> &requests,
                               QVector<MemoryReply> &replies,
                               QString *error)
{
    replies.clear();
    if (!m_connected) {
        setError(QStringLiteral("memread baglantisi yok."), error);
        return false;
    }
    if (requests.isEmpty())
        return true;

    std::vector<memread::ReadRange> ranges;
    ranges.reserve(static_cast<std::size_t>(requests.size()));
    for (const MemoryRequest &request : requests) {
        if (request.len > kMaxReadBytes) {
            setError(QStringLiteral("tek okuma %1 bayti asamaz (istenen %2).")
                         .arg(kMaxReadBytes).arg(request.len), error);
            return false;
        }
        ranges.push_back({request.id, request.addr, request.len});
    }

    QByteArray response;
    if (!exchange(fromStd(memread::encodeRead(ranges)), response, kReadTimeoutMs, error))
        return false;

    memread::Response parsed;
    if (!memread::decodeResponse(toStd(response), parsed)) {
        setError(QStringLiteral("okuma yaniti cozulemedi."), error);
        return false;
    }
    if (parsed.status != memread::Status::Ok) {
        setError(QString::fromStdString(parsed.text), error);
        return false;
    }
    // One reply per request is what every caller assumes when it pairs results
    // back to watch items by position.
    if (!parsed.isRead || parsed.results.size() != static_cast<std::size_t>(requests.size())) {
        setError(QStringLiteral("okuma yaniti eksik: %1 istek, %2 sonuc.")
                     .arg(requests.size()).arg(parsed.results.size()), error);
        return false;
    }

    replies.reserve(requests.size());
    for (const memread::ReadResult &result : parsed.results) {
        MemoryReply reply;
        reply.id = result.id;
        reply.addr = result.addr;
        reply.ok = result.ok;
        reply.data = fromStd(result.data);
        reply.error = QString::fromStdString(result.error);
        replies.append(reply);
    }
    return true;
}

bool MemReadClient::exchange(const QByteArray &payload, QByteArray &response,
                             int timeoutMs, QString *error)
{
    if (!isRunning()) {
        setError(QStringLiteral("stm32aid-memread calismiyor."), error);
        return false;
    }

    const QByteArray framed = fromStd(memread::frame(toStd(payload)));
    if (m_process->write(framed) != framed.size() || !m_process->waitForBytesWritten(timeoutMs)) {
        setError(QStringLiteral("stm32aid-memread'e yazilamadi: ") + m_process->errorString(), error);
        return false;
    }

    QElapsedTimer elapsed;
    elapsed.start();
    for (;;) {
        std::string buffer = toStd(m_buffer);
        std::string message;
        bool tooLarge = false;
        if (memread::takeMessage(buffer, message, tooLarge)) {
            m_buffer = fromStd(buffer);
            response = fromStd(message);
            return true;
        }
        if (tooLarge) {
            // The stream is out of sync; carrying on would read garbage as
            // replies, so the session is finished.
            setError(QStringLiteral("memread cercevesi bozuk - oturum kapatiliyor."), error);
            stop();
            return false;
        }

        const int remaining = timeoutMs - static_cast<int>(elapsed.elapsed());
        if (remaining <= 0) {
            setError(QStringLiteral("stm32aid-memread yanit vermedi (%1 ms).").arg(timeoutMs), error);
            return false;
        }
        if (!m_process->waitForReadyRead(remaining)) {
            if (m_process->state() != QProcess::Running) {
                setError(QStringLiteral("stm32aid-memread beklenmedik sekilde kapandi."), error);
                m_connected = false;
                return false;
            }
            continue;   // spurious wake-up; the deadline above is what ends this
        }
        m_buffer.append(m_process->readAllStandardOutput());
    }
}

void MemReadClient::stop()
{
    if (!m_process)
        return;

    m_connected = false;
    m_board.clear();

    if (m_process->state() == QProcess::Running) {
        // Closing stdin is the whole shutdown protocol: the sidecar's read()
        // returns 0 and it exits. Killing is only the fallback for a wedged
        // process, never the normal path.
        m_process->closeWriteChannel();
        if (!m_process->waitForFinished(3000)) {
            m_process->kill();
            m_process->waitForFinished(1000);
        }
    }

    delete m_process;
    m_process = nullptr;
    m_buffer.clear();
}
