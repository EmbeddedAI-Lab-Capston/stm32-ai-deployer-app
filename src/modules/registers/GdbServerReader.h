#pragma once
#include "IRegisterReader.h"
#include "modules/debug/DebugLinkTypes.h"

#include <QString>
#include <QVector>

class DebugLink;

// ── GdbServerReader ──────────────────────────────────────────────────────
// Second IRegisterReader implementation: executes a ReadPlan over the shared
// DebugLink (GDB Remote Serial Protocol) instead of spawning
// STM32_Programmer_CLI per read (docs/variable_watcher_plan.md Bolum 5.1).
// CliRegisterReader is NOT replaced — RegisterInspector holds both and picks
// one per snapshot (see RegisterInspector::resolveActiveReader()).
//
// Session lifetime (binding, plan Bolum 5.1 + DebugLink's retain/release
// contract in Bolum 4.6):
//   1. read() calls DebugLink::retain() first.
//   2. If the link isn't open yet, the plan just waits — read() never blocks,
//      never spins a wait loop. Execution resumes from opened()/failed().
//   3. release() is called AFTER readFinished/readFailed is emitted, not
//      before — this lets a same-tick follow-up read() (RegisterInspector
//      issues the RCC read, then the peripheral-block read, back to back)
//      observe the link as still open (refCount 1->2->1) instead of paying
//      for a full close+reopen between the two reads of one snapshot.
//   4. A failed retain() does NOT consume the refcount, so release() is not
//      called in that case either — matches DebugLink's contract exactly.
class GdbServerReader : public IRegisterReader
{
    Q_OBJECT
public:
    explicit GdbServerReader(DebugLink *link, QObject *parent = nullptr);

    void setStlinkSn(const QString &sn) override;
    void setConnectMode(const QString &mode) override;   // NOTE: ignored, gdb attach is always live
    bool isBusy() const override { return m_busy; }
    void read(const ReadPlan &plan) override;

private slots:
    void onLinkOpened();
    void onLinkFailed(const QString &message);
    void onRangesRead(quint32 batchId, const QVector<MemoryReply> &replies);

private:
    void startReadingPendingPlan();

    DebugLink *m_link = nullptr;

    bool     m_busy        = false;
    bool     m_awaitingOpen = false;
    ReadPlan m_pendingPlan;

    quint32           m_batchId = 0;
    QVector<MemoryRequest> m_requests;
    QVector<QString>       m_chunkOwner;   // index-aligned with m_requests: which peripheral each chunk belongs to
};
