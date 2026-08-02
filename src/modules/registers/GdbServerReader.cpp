#include "GdbServerReader.h"
#include "modules/debug/DebugLink.h"

namespace {
quint32 bytesToU32Le(const QByteArray &d, int off)
{
    return quint32(static_cast<unsigned char>(d[off]))
         | (quint32(static_cast<unsigned char>(d[off + 1])) << 8)
         | (quint32(static_cast<unsigned char>(d[off + 2])) << 16)
         | (quint32(static_cast<unsigned char>(d[off + 3])) << 24);
}
}

GdbServerReader::GdbServerReader(DebugLink *link, QObject *parent)
    : IRegisterReader(parent)
    , m_link(link)
{
    qRegisterMetaType<RegisterReadResult>();

    connect(m_link, &DebugLink::opened,     this, &GdbServerReader::onLinkOpened);
    connect(m_link, &DebugLink::failed,     this, &GdbServerReader::onLinkFailed);
    connect(m_link, &DebugLink::rangesRead, this, &GdbServerReader::onRangesRead);
}

void GdbServerReader::setStlinkSn(const QString &sn)
{
    // DebugLink is a shared, already-possibly-running resource: this only
    // takes effect the next time its gdbserver process is (re)started from
    // scratch (refCount 0->1). Fine in practice — only one ST-Link is ever
    // connected at a time (see CLAUDE.md hardware notes).
    if (m_link) m_link->setStlinkSerial(sn);
}

void GdbServerReader::setConnectMode(const QString &mode)
{
    Q_UNUSED(mode);
    // Intentionally ignored: a gdb-attach session is always "live", HOTPLUG
    // vs UR has no meaning here. RegisterInspector tags the resulting
    // snapshot's connectMode as "GDB-ATTACH" so the UI doesn't lie about it.
}

void GdbServerReader::read(const ReadPlan &plan)
{
    if (m_busy) {
        emit readFailed(QStringLiteral("GdbServerReader is busy"));
        return;
    }
    if (!m_link) {
        emit readFailed(QStringLiteral("Debug link not set"));
        return;
    }

    m_pendingPlan  = plan;
    m_busy         = true;
    m_awaitingOpen = true;
    m_link->retain();
}

void GdbServerReader::onLinkOpened()
{
    if (!m_awaitingOpen) return;   // not our retain() — some other owner's link opened
    m_awaitingOpen = false;
    startReadingPendingPlan();
}

void GdbServerReader::onLinkFailed(const QString &message)
{
    if (!m_awaitingOpen) return;   // not our retain() attempt
    m_awaitingOpen = false;
    m_busy = false;
    // retain() failure never consumes DebugLink's refcount (plan Bolum 4.6
    // point 4) — release() is deliberately NOT called here.
    emit readFailed(message);
}

void GdbServerReader::startReadingPendingPlan()
{
    m_requests.clear();
    m_chunkOwner.clear();

    quint32 maxBytes = m_link->maxReadBytes();
    if (maxBytes < 4) maxBytes = 4096;          // defensive fallback, shouldn't happen once Open
    maxBytes = (maxBytes / 4) * 4;              // keep chunks word-aligned (ReadPlanItem is always a multiple of 4)

    quint32 nextId = 0;
    for (const ReadPlanItem &item : m_pendingPlan.items) {
        quint32 offset = 0;
        while (offset < item.byteCount) {
            const quint32 chunk = qMin(maxBytes, item.byteCount - offset);
            MemoryRequest req;
            req.id   = nextId++;
            req.addr = item.startAddr + offset;
            req.len  = chunk;
            m_requests.append(req);
            m_chunkOwner.append(item.peripheralName);
            offset += chunk;
        }
    }

    if (m_requests.isEmpty()) {
        // Nothing to read (mirrors CliRegisterReader's empty-plan handling).
        m_busy = false;
        RegisterReadResult empty;
        empty.processOk = true;
        empty.exitCode  = 0;
        emit readFinished(empty);
        m_link->release();
        return;
    }

    ++m_batchId;
    m_link->readRanges(m_batchId, m_requests);
}

void GdbServerReader::onRangesRead(quint32 batchId, const QVector<MemoryReply> &replies)
{
    if (!m_busy || batchId != m_batchId) return;   // not this read (e.g. a stray/earlier batch)

    RegisterReadResult result;
    result.processOk = true;
    result.exitCode  = 0;

    QHash<QString, QString> firstErrorByOwner;
    for (int i = 0; i < replies.size(); ++i) {
        const MemoryReply &reply = replies.at(i);
        if (!reply.ok) {
            if (i < m_chunkOwner.size() && !firstErrorByOwner.contains(m_chunkOwner.at(i)))
                firstErrorByOwner.insert(m_chunkOwner.at(i), reply.error);
            continue;
        }
        for (int off = 0; off + 4 <= reply.data.size(); off += 4)
            result.values.insert(reply.addr + off, bytesToU32Le(reply.data, off));
    }

    // Block-error attribution: same semantic as CliRegisterReader — a whole
    // ReadPlanItem range with NO addresses present in `values` failed, the
    // rest of the snapshot still stands.
    for (const ReadPlanItem &item : m_pendingPlan.items) {
        bool anyPresent = false;
        for (quint32 off = 0; off < item.byteCount; off += 4) {
            if (result.values.contains(item.startAddr + off)) { anyPresent = true; break; }
        }
        if (!anyPresent) {
            const QString msg = firstErrorByOwner.value(item.peripheralName,
                                                          QStringLiteral("no data returned for range"));
            result.errors.append({ item.peripheralName, item.startAddr, msg });
        }
    }

    m_busy = false;
    emit readFinished(result);
    m_link->release();
}
