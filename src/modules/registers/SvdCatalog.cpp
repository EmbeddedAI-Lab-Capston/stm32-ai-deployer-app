#include "SvdCatalog.h"
#include "SvdParser.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtConcurrent/QtConcurrent>

SvdCatalog::SvdCatalog(QObject *parent)
    : QObject(parent)
    , m_svdDir(QCoreApplication::applicationDirPath() + QStringLiteral("/svd"))
{
}

void SvdCatalog::setSvdDirectory(const QString &dir)
{
    m_svdDir = dir;
}

QString SvdCatalog::svdDirectory() const
{
    return m_svdDir;
}

bool SvdCatalog::loadBoardsJson()
{
    m_error.clear();
    m_mappings.clear();

    const QString path = m_svdDir + QStringLiteral("/boards.json");
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_error = QStringLiteral("Cannot open boards.json: %1").arg(path);
        return false;
    }

    QJsonParseError perr;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &perr);
    if (perr.error != QJsonParseError::NoError) {
        m_error = QStringLiteral("boards.json parse error: %1").arg(perr.errorString());
        return false;
    }

    const QJsonArray boards = doc.object().value(QStringLiteral("boards")).toArray();
    for (const QJsonValue &v : boards) {
        const QJsonObject o = v.toObject();
        SvdBoardMapping m;
        const QJsonObject match = o.value(QStringLiteral("match")).toObject();
        for (const QJsonValue &n : match.value(QStringLiteral("names")).toArray())
            m.names << n.toString();
        for (const QJsonValue &d : match.value(QStringLiteral("deviceIds")).toArray())
            m.deviceIds << d.toString();
        m.svdFile             = o.value(QStringLiteral("svd")).toString();
        m.device              = o.value(QStringLiteral("device")).toString();
        m.access              = o.value(QStringLiteral("access")).toString();
        m.connectMode         = o.value(QStringLiteral("connectMode")).toString();
        m.fallbackConnectMode = o.value(QStringLiteral("fallbackConnectMode")).toString();
        m.notes               = o.value(QStringLiteral("notes")).toString();
        for (const QJsonValue &p : o.value(QStringLiteral("defaultPeripherals")).toArray())
            m.defaultPeripherals << p.toString();
        const QJsonObject rcc = o.value(QStringLiteral("rccOverrides")).toObject();
        for (auto it = rcc.begin(); it != rcc.end(); ++it)
            m.rccOverrides.insert(it.key(), it.value().toString());

        if (m.isValid())
            m_mappings.append(m);
    }

    if (m_mappings.isEmpty()) {
        m_error = QStringLiteral("boards.json contains no valid board mappings");
        return false;
    }
    return true;
}

SvdBoardMapping SvdCatalog::mappingForBoard(const BoardInfo &board) const
{
    const QString name  = board.name.trimmed();
    const QString devId = board.deviceId.trimmed();
    const QString probe = board.probeBoardName.trimmed();

    // 1) exact board-name match, 2) device-id match, 3) probe-name contains.
    for (const SvdBoardMapping &m : m_mappings)
        for (const QString &n : m.names)
            if (!name.isEmpty() && n.compare(name, Qt::CaseInsensitive) == 0)
                return m;

    if (!devId.isEmpty())
        for (const SvdBoardMapping &m : m_mappings)
            for (const QString &d : m.deviceIds)
                if (d.compare(devId, Qt::CaseInsensitive) == 0)
                    return m;

    if (!probe.isEmpty())
        for (const SvdBoardMapping &m : m_mappings)
            for (const QString &n : m.names)
                if (probe.contains(n, Qt::CaseInsensitive))
                    return m;

    return SvdBoardMapping{};
}

const SvdDevice *SvdCatalog::cachedDevice(const QString &svdFile) const
{
    auto it = m_cache.constFind(svdFile);
    return it == m_cache.constEnd() ? nullptr : &it.value();
}

void SvdCatalog::requestDevice(const BoardInfo &board)
{
    const SvdBoardMapping m = mappingForBoard(board);
    if (!m.isValid()) {
        emit parseError(QString(), QStringLiteral("No SVD mapping for board '%1'").arg(board.name));
        return;
    }
    const QString svdFile = m.svdFile;
    if (m_cache.contains(svdFile)) {
        emit deviceReady(svdFile);
        return;
    }
    if (m_inFlight.contains(svdFile))
        return;   // parse already running; caller will get deviceReady when done

    const QString path = m_svdDir + QLatin1Char('/') + svdFile;
    if (!QFileInfo::exists(path)) {
        emit parseError(svdFile, QStringLiteral("SVD file not found: %1").arg(path));
        return;
    }

    m_inFlight.insert(svdFile);

    // Parse off the main thread (A4: QtConcurrent, not QThread+Worker). The
    // QFutureWatcher delivers the result back on the main thread, so the cache
    // is only ever touched here.
    auto *watcher = new QFutureWatcher<SvdDevice>(this);
    connect(watcher, &QFutureWatcher<SvdDevice>::finished, this,
            [this, watcher, svdFile]() {
                const SvdDevice device = watcher->result();
                watcher->deleteLater();
                m_inFlight.remove(svdFile);
                if (!device.isValid()) {
                    emit parseError(svdFile, QStringLiteral("Failed to parse %1").arg(svdFile));
                    return;
                }
                m_cache.insert(svdFile, device);
                emit deviceReady(svdFile);
            });
    watcher->setFuture(QtConcurrent::run([path]() {
        SvdParser parser;
        return parser.parseFile(path);
    }));
}
