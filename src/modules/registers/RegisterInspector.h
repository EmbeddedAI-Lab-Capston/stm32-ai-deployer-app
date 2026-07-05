#pragma once
#include "RegisterSnapshot.h"
#include "ReadPlanBuilder.h"
#include "RegisterDecoder.h"
#include "SvdCatalog.h"
#include "modules/board/BoardPresets.h"

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>

class RegisterReader;

// ── RegisterInspector ──────────────────────────────────────────────────────
// Orchestrator/manager for the Register feature (plan Bolum 5.1). Owns the
// SvdCatalog + RegisterReader and drives the snapshot state machine:
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
    bool loadCatalog();                          // read boards.json
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
    void finishWithError(const QString &message);
    void setBusy(bool busy);
    void setStage(const QString &stage);

    SvdCatalog       m_catalog;
    RegisterReader  *m_reader = nullptr;
    ReadPlanBuilder  m_builder;
    RegisterDecoder  m_decoder;

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
