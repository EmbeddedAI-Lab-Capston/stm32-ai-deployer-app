#pragma once
#include "RegisterSnapshot.h"
#include "ReadPlanBuilder.h"
#include "RegisterDecoder.h"
#include "RegisterDiff.h"
#include "SnapshotDiffer.h"
#include "RegisterRuleModel.h"
#include "RuleEngine.h"
#include "SvdCatalog.h"
#include "modules/board/BoardPresets.h"

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>

class IRegisterReader;
class CliRegisterReader;
class GdbServerReader;
class DebugLink;

// ── RegisterInspector ──────────────────────────────────────────────────────
// Orchestrator/manager for the Register feature (plan Bolum 5.1). Owns the
// SvdCatalog + IRegisterReader and drives the snapshot state machine:
//   idle -> load-svd -> read-rcc -> read-registers -> decode -> ready
// Holds two snapshot slots (A/B). Created in main.cpp and handed to Backend;
// QML only ever talks to Backend (facade rule).
class RegisterInspector : public QObject
{
    Q_OBJECT
public:
    explicit RegisterInspector(QObject *parent = nullptr);

    void setCliPath(const QString &path);
    void setSvdDirectory(const QString &dir);   // optional; default exe/svd

    // GDB read backend (Faz 2, docs/variable_watcher_plan.md Bolum 5.1).
    // "cli" | "gdb" is a PREFERENCE, not a guarantee — resolveActiveReader()
    // re-checks it every snapshot and falls back to "cli" if the gdbserver
    // path is unknown or the link fails to open. Default stays "cli".
    void setReaderBackend(const QString &backend);
    void setDebugLink(DebugLink *link);   // creates the GdbServerReader lazily
    bool loadCatalog();                          // read boards.json
    bool loadRules();                            // read rules.json (same dir as SVDs)
    QString lastError() const { return m_lastError; }

    // Kick off (async) parse of a board's SVD. Emits catalogReady(board) when the
    // peripheral list is available. No-op if already cached (emits immediately).
    void prepareBoard(const BoardInfo &board);

    bool hasDeviceFor(const BoardInfo &board) const;
    QStringList allPeripheralNames(const BoardInfo &board) const;
    QStringList defaultPeripherals(const BoardInfo &board) const;
    QString supportLevel(const BoardInfo &board) const;   // stable/experimental/unsupported

    // Take a snapshot into slot 0(A) or 1(B). Loads the SVD first if needed.
    void takeSnapshot(int slot, const BoardInfo &board, const QStringList &peripherals);

    const RegisterSnapshot *snapshot(int slot) const;
    void clearSnapshots();

    // A/B field-level diff (plan Bolum 1a). Both slots must be filled and
    // from the same board/SVD; otherwise SnapshotDiff.comparable is false.
    bool         diffAvailable() const;
    SnapshotDiff computeDiff() const;

    // Deterministic (non-LLM) consistency checks against a decoded slot
    // (plan Bolum 1b). Empty if the slot is empty or no rules matched.
    QList<RuleViolation> ruleViolations(int slot) const;

    bool    isBusy() const { return m_busy; }
    QString stage() const { return m_stage; }

signals:
    void busyChanged();
    void stageChanged();
    void snapshotReady(int slot);
    void errorOccurred(const QString &message);
    void catalogReady(const QString &boardName);

private:
    enum class Phase { Idle, ReadRcc, ReadBlocks };

    void onDeviceReady(const QString &svdFile);
    void onCatalogError(const QString &svdFile, const QString &message);
    void onReadFinished(const RegisterReadResult &result);
    void onReadFailed(const QString &message);

    void beginSnapshot();                 // device is ready: start RCC read
    void resolveActiveReader();           // picks m_reader per plan Bolum 5.1 chain (a-d)
    void warnGdbFallbackOnce(const QString &message);
    void finishWithError(const QString &message);
    void setBusy(bool busy);
    void setStage(const QString &stage);

    SvdCatalog       m_catalog;
    IRegisterReader  *m_reader = nullptr;      // used for all orchestration
    CliRegisterReader *m_cliReader = nullptr;  // same object; kept for setCliPath() only
    GdbServerReader   *m_gdbReader = nullptr;  // lazily created in setDebugLink()
    DebugLink         *m_debugLink = nullptr;
    QString            m_readerBackendPref = QStringLiteral("cli");   // "cli" | "gdb", user preference
    bool               m_usingGdb = false;      // resolved backend for the IN-PROGRESS/last snapshot
    bool               m_gdbFallbackWarned = false;   // once per app run, not per snapshot
    ReadPlanBuilder  m_builder;
    RegisterDecoder  m_decoder;
    SnapshotDiffer   m_differ;
    RuleEngine       m_rules;

    RegisterSnapshot m_slots[2];
    bool             m_slotFilled[2] = { false, false };

    // Active snapshot request context.
    Phase       m_phase = Phase::Idle;
    bool        m_busy = false;
    QString     m_stage = QStringLiteral("idle");
    int         m_pendingSlot = 0;
    BoardInfo   m_pendingBoard;
    QStringList m_pendingSelected;
    SvdBoardMapping m_pendingMapping;
    QString     m_pendingSvdFile;

    QHash<quint64, quint32> m_rccValues;
    QSet<QString>           m_clockEnabled;
    QSet<QString>           m_gateable;

    // svdFile -> boardName for parses requested only to populate the peripheral
    // list (so catalogReady can name the board when the async parse completes).
    QHash<QString, QString> m_prepareBoardBySvd;
    QString                 m_lastError;
};
