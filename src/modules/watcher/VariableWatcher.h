#pragma once
#include "WatchModel.h"
#include "WatchPlanBuilder.h"
#include "TraceBuffer.h"
#include "ElfTargetMatcher.h"
#include "SymbolModel.h"
#include "modules/debug/DebugLinkTypes.h"

#include <QElapsedTimer>
#include <QList>
#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QVector>

class DebugLink;
class ElfSymbolSource;

// ── VariableWatcher ───────────────────────────────────────────────────────
// Main-thread orchestrator for the Variable Watcher feature (plan
// docs/variable_watcher_plan.md Bolum 7.4). Owns the watch item list, the
// ELF symbol source, the ELF<->target match check, and the TraceBuffer.
// Does NOT own the ST-Link session lifetime — Backend::openWatchLink()/
// closeWatchLink() call DebugLink::retain()/release() directly (plan Bolum
// 7.5/7.6); this class only calls startSampling()/stopSampling() on an
// ALREADY-open link.
class VariableWatcher : public QObject
{
    Q_OBJECT
public:
    explicit VariableWatcher(DebugLink *link, QObject *parent = nullptr);

    void setNmPath(const QString &path);
    void loadElf(const QString &path);
    const QList<Symbol> &symbols() const { return m_symbols; }
    QString elfPath() const { return m_elfPath; }

    const QList<WatchItem> &items() const { return m_items; }
    // Item-list mutators refuse while sampling is running ("stop first") —
    // rebuilding the plan/buffer mid-stream is not worth the edge cases for
    // what a "pause, edit, resume" workflow already covers just as well.
    QString addSymbol(const QString &symbolName);        // "" = could not add (e.g. Absolute symbol)
    QString addAddress(quint64 addr, WatchValueType t, const QString &label);
    void    updateItem(const QString &id, const QVariantMap &props);
    void    removeItem(const QString &id);
    void    clearItems();

    void start(int targetRateHz);   // 0 = max
    void stop();
    bool isRunning() const { return m_running; }

    const TraceBuffer &buffer() const { return m_buffer; }
    QVariantMap rateInfo() const;
    void clearBuffer();   // wipes trace data + session stats, keeps the item list

    void saveItems(const QString &boardName);
    void loadItems(const QString &boardName);

    // ELF <-> target match (plan Bolum 6.2) — re-evaluated whenever the link
    // opens or a new ELF loads. startWatch() refuses to run on Mismatch until
    // acknowledgeElfMismatch() is called for THIS mismatch.
    ElfMatchResult elfMatchResult() const { return m_elfMatchResult; }
    const ElfMatchReport &elfMatchDetail() const { return m_elfMatchReport; }
    void acknowledgeElfMismatch() { m_mismatchAcknowledged = true; }

signals:
    void itemsChanged();
    void samplesAppended();      // <=30 Hz
    void statsChanged();         // <=4 Hz
    void runningChanged();
    void symbolsLoaded(int count);
    void errorOccurred(const QString &message);
    void elfMatchChanged();

private slots:
    void onLinkOpened();
    void onLinkFailed(const QString &message);
    void onElfSymbolsLoaded(const QList<Symbol> &symbols, const QString &elfPath);
    void onElfSymbolLoadFailed(const QString &message);
    void onRangesRead(quint32 batchId, const QVector<MemoryReply> &replies);
    void onRawSamplesReady(const QVector<MemoryReply> &replies, double t, double skewUs);
    void onSamplingStats(double actualRateHz, double rttMsAvg, quint32 dropped);
    void onCoreHalted();
    void onCoreReset();

private:
    void rebuildPlan();
    void setRunning(bool running);
    void flushPending();
    void maybeCheckElfMatch();
    void handleElfMatchReply(const QVector<MemoryReply> &replies);
    void setElfMatch(ElfMatchResult result, const ElfMatchReport &report);

    DebugLink        *m_link = nullptr;
    ElfSymbolSource  *m_elfSource = nullptr;

    QList<Symbol> m_symbols;
    QString       m_elfPath;

    QList<WatchItem> m_items;
    WatchPlan         m_plan;
    TraceBuffer       m_buffer;

    bool m_running = false;
    int  m_targetRateHz = 0;

    double  m_actualRateHz = 0.0;
    double  m_rttMsAvg     = 0.0;
    double  m_lastSkewUs   = 0.0;
    quint32 m_droppedTotal = 0;
    bool    m_coreRunning  = true;

    // <=30 Hz / 512-sample coalescing gate — the batching Faz 1's
    // DebugLinkWorker deferred to "whoever consumes rawSamplesReady at real
    // rates" (see DebugLinkWorker.h scope note). This is that consumer.
    QVector<double>           m_pendingTimes;
    QVector<QVector<double>>  m_pendingSeries;
    QElapsedTimer             m_flushTimer;

    ElfMatchResult m_elfMatchResult = ElfMatchResult::Unknown;
    ElfMatchReport m_elfMatchReport;
    bool           m_mismatchAcknowledged = false;
    int            m_elfMatchStep = 0;     // 0=idle, 1=awaiting VTOR, 2=awaiting vector table
    quint32        m_pendingVtor = 0;
};
