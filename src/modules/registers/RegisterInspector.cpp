#include "RegisterInspector.h"
#include "CliRegisterReader.h"
#include "GdbServerReader.h"
#include "modules/debug/DebugLink.h"
#include "core/AppSettings.h"

#include <QDateTime>
#include <algorithm>

RegisterInspector::RegisterInspector(QObject *parent)
    : QObject(parent)
    , m_cliReader(new CliRegisterReader(this))
{
    m_reader = m_cliReader;   // IRegisterReader view of the same object

    connect(&m_catalog, &SvdCatalog::deviceReady, this, &RegisterInspector::onDeviceReady);
    connect(&m_catalog, &SvdCatalog::parseError, this, &RegisterInspector::onCatalogError);
    connect(m_reader, &IRegisterReader::readFinished, this, &RegisterInspector::onReadFinished);
    connect(m_reader, &IRegisterReader::readFailed, this, &RegisterInspector::onReadFailed);
}

void RegisterInspector::setCliPath(const QString &path) { m_cliReader->setCliPath(path); }
void RegisterInspector::setSvdDirectory(const QString &dir) { m_catalog.setSvdDirectory(dir); }

void RegisterInspector::setReaderBackend(const QString &backend)
{
    m_readerBackendPref = backend;
}

void RegisterInspector::setDebugLink(DebugLink *link)
{
    m_debugLink = link;
    if (link && !m_gdbReader) {
        m_gdbReader = new GdbServerReader(link, this);
        connect(m_gdbReader, &IRegisterReader::readFinished, this, &RegisterInspector::onReadFinished);
        connect(m_gdbReader, &IRegisterReader::readFailed, this, &RegisterInspector::onReadFailed);
    }
}

bool RegisterInspector::loadCatalog()
{
    const bool ok = m_catalog.loadBoardsJson();
    if (!ok)
        m_lastError = m_catalog.errorString();
    return ok;
}

bool RegisterInspector::loadRules()
{
    const QString path = m_catalog.svdDirectory() + QStringLiteral("/rules.json");
    const bool ok = m_rules.loadRules(path);
    if (!ok)
        m_lastError = m_rules.errorString();
    return ok;
}

void RegisterInspector::prepareBoard(const BoardInfo &board)
{
    const SvdBoardMapping m = m_catalog.mappingForBoard(board);
    if (!m.isValid()) {
        emit errorOccurred(QStringLiteral("No SVD mapping for board '%1'").arg(board.name));
        return;
    }
    if (m_catalog.isCached(m.svdFile)) {
        emit catalogReady(board.name);
        return;
    }
    m_prepareBoardBySvd.insert(m.svdFile, board.name);
    m_catalog.requestDevice(board);
}

bool RegisterInspector::hasDeviceFor(const BoardInfo &board) const
{
    const SvdBoardMapping m = m_catalog.mappingForBoard(board);
    return m.isValid() && m_catalog.isCached(m.svdFile);
}

QStringList RegisterInspector::allPeripheralNames(const BoardInfo &board) const
{
    const SvdBoardMapping m = m_catalog.mappingForBoard(board);
    if (!m.isValid())
        return {};
    const SvdDevice *dev = m_catalog.cachedDevice(m.svdFile);
    if (!dev)
        return {};
    QStringList names;
    names.reserve(dev->peripherals.size());
    for (const SvdPeripheral &p : dev->peripherals)
        names << p.name;
    names.sort(Qt::CaseInsensitive);
    return names;
}

QVariantList RegisterInspector::registersOfPeripheral(const BoardInfo &board, const QString &peripheralName) const
{
    QVariantList out;
    const SvdBoardMapping m = m_catalog.mappingForBoard(board);
    if (!m.isValid())
        return out;
    const SvdDevice *dev = m_catalog.cachedDevice(m.svdFile);
    if (!dev)
        return out;
    const SvdPeripheral *periph = dev->findPeripheral(peripheralName);
    if (!periph)
        return out;

    for (const SvdRegister &r : periph->registers) {
        QVariantMap e;
        e[QStringLiteral("name")]             = r.name;
        e[QStringLiteral("addr")]             = QStringLiteral("0x%1").arg(periph->addressOf(r), 0, 16);
        e[QStringLiteral("addressValue")]     = QVariant::fromValue(periph->addressOf(r));
        e[QStringLiteral("size")]             = r.sizeBits;
        e[QStringLiteral("access")]           = r.access;
        e[QStringLiteral("readAction")]       = r.readAction;
        e[QStringLiteral("hasReadSideEffect")] = r.hasReadSideEffect();
        e[QStringLiteral("description")]      = r.description;
        e[QStringLiteral("clockKnown")]       = m_hasClockInfo;
        e[QStringLiteral("clockEnabled")]     = m_hasClockInfo && m_clockEnabled.contains(periph->name);
        out.append(e);
    }
    return out;
}

QStringList RegisterInspector::defaultPeripherals(const BoardInfo &board) const
{
    const SvdBoardMapping m = m_catalog.mappingForBoard(board);
    if (!m.isValid())
        return {};
    const SvdDevice *dev = m_catalog.cachedDevice(m.svdFile);
    if (!dev)
        return m.defaultPeripherals;   // not parsed yet: return as-is
    QStringList out;
    for (const QString &name : m.defaultPeripherals)
        if (dev->findPeripheral(name))
            out << name;
    return out;
}

QString RegisterInspector::supportLevel(const BoardInfo &board) const
{
    const SvdBoardMapping m = m_catalog.mappingForBoard(board);
    if (!m.isValid())
        return QStringLiteral("unsupported");
    return m.access.isEmpty() ? QStringLiteral("stable") : m.access;
}

void RegisterInspector::takeSnapshot(int slot, const BoardInfo &board,
                                     const QStringList &peripherals)
{
    if (m_busy) {
        emit errorOccurred(QStringLiteral("Register Inspector is busy"));
        return;
    }
    if (slot < 0 || slot > 1) {
        emit errorOccurred(QStringLiteral("Invalid snapshot slot"));
        return;
    }
    const SvdBoardMapping m = m_catalog.mappingForBoard(board);
    if (!m.isValid()) {
        emit errorOccurred(QStringLiteral("No SVD mapping for board '%1'").arg(board.name));
        return;
    }

    m_pendingSlot     = slot;
    m_pendingBoard    = board;
    m_pendingSelected = peripherals;
    m_pendingMapping  = m;
    m_pendingSvdFile  = m.svdFile;
    m_phase           = Phase::Idle;
    setBusy(true);

    if (m_catalog.isCached(m.svdFile)) {
        beginSnapshot();
    } else {
        setStage(QStringLiteral("load-svd"));
        m_catalog.requestDevice(board);
    }
}

void RegisterInspector::onDeviceReady(const QString &svdFile)
{
    if (m_prepareBoardBySvd.contains(svdFile))
        emit catalogReady(m_prepareBoardBySvd.take(svdFile));

    if (m_busy && m_phase == Phase::Idle && svdFile == m_pendingSvdFile)
        beginSnapshot();
}

void RegisterInspector::onCatalogError(const QString &svdFile, const QString &message)
{
    if (m_prepareBoardBySvd.contains(svdFile)) {
        m_prepareBoardBySvd.remove(svdFile);
        emit errorOccurred(message);
    }
    if (m_busy && m_phase == Phase::Idle && svdFile == m_pendingSvdFile)
        finishWithError(message);
}

void RegisterInspector::beginSnapshot()
{
    const SvdDevice *dev = m_catalog.cachedDevice(m_pendingSvdFile);
    if (!dev) {
        finishWithError(QStringLiteral("SVD not available after load"));
        return;
    }

    resolveActiveReader();

    m_reader->setStlinkSn(m_pendingBoard.stlinkSn);
    m_reader->setConnectMode(m_pendingMapping.connectMode.isEmpty()
                                 ? QStringLiteral("HOTPLUG")
                                 : m_pendingMapping.connectMode);

    m_phase = Phase::ReadRcc;
    setStage(QStringLiteral("read-rcc"));
    m_reader->read(m_builder.buildRccPlan(*dev));
}

void RegisterInspector::resolveActiveReader()
{
    // Preference resolution chain (plan Bolum 5.1) — re-evaluated on every
    // snapshot, never cached, so "gdb" is always opt-in-but-verified rather
    // than a blind trust of a stale setting.
    //
    // (a) preference isn't "gdb" at all -> CLI, no questions asked.
    if (m_readerBackendPref != QStringLiteral("gdb")) {
        m_reader   = m_cliReader;
        m_usingGdb = false;
        return;
    }

    // (b) gdbserver path unknown, or no DebugLink wired up -> CLI + one-time warning.
    // Reads AppSettings directly (rather than threading a bool through
    // Backend) so a path picked in Settings mid-session takes effect on the
    // very next snapshot, matching "her snapshot'ta" in the plan.
    if (!m_debugLink || !m_gdbReader || AppSettings().gdbServerPath().isEmpty()) {
        m_reader   = m_cliReader;
        m_usingGdb = false;
        warnGdbFallbackOnce(QStringLiteral("gdbserver yolu bulunamadi, CLI arka ucuna dusuldu"));
        return;
    }

    // (c) is handled reactively in onReadFailed(): if the very first (RCC)
    // read of a fresh attempt fails while m_usingGdb is true, nothing has
    // been read yet, so it silently retries the whole snapshot via CLI.
    // (d) otherwise.
    m_reader   = m_gdbReader;
    m_usingGdb = true;
}

void RegisterInspector::warnGdbFallbackOnce(const QString &message)
{
    if (m_gdbFallbackWarned) return;
    m_gdbFallbackWarned = true;
    emit errorOccurred(message);   // Backend maps this straight to statusMessage — not a fatal error
}

void RegisterInspector::onReadFinished(const RegisterReadResult &result)
{
    const SvdDevice *dev = m_catalog.cachedDevice(m_pendingSvdFile);
    if (!dev) {
        finishWithError(QStringLiteral("SVD went missing mid-snapshot"));
        return;
    }

    if (m_phase == Phase::ReadRcc) {
        if (!result.processOk) {
            finishWithError(QStringLiteral("RCC read failed (exit %1)").arg(result.exitCode));
            return;
        }
        m_rccValues    = result.values;
        m_clockEnabled = m_builder.clockEnabledPeripherals(*dev, m_rccValues,
                                                           m_pendingMapping.rccOverrides);
        m_gateable     = m_builder.rccGateablePeripherals(*dev, m_pendingMapping.rccOverrides);
        m_hasClockInfo = true;

        // Block read excludes RCC — its values already came from the gating read.
        QStringList blockSel;
        for (const QString &n : m_pendingSelected)
            if (n.compare(QStringLiteral("RCC"), Qt::CaseInsensitive) != 0)
                blockSel << n;

        m_phase = Phase::ReadBlocks;
        setStage(QStringLiteral("read-registers"));
        m_reader->read(m_builder.build(*dev, blockSel, &m_clockEnabled));
        return;
    }

    if (m_phase == Phase::ReadBlocks) {
        QHash<quint64, quint32> combined = m_rccValues;
        for (auto it = result.values.constBegin(); it != result.values.constEnd(); ++it)
            combined.insert(it.key(), it.value());

        setStage(QStringLiteral("decode"));
        RegisterSnapshot snap;
        snap.takenAt      = QDateTime::currentDateTime();
        snap.boardName    = m_pendingBoard.name;
        snap.deviceName   = dev->name;
        snap.svdFile      = m_pendingSvdFile;
        // "GDB-ATTACH" is a distinct value, never silently reported as
        // HOTPLUG/UR — a gdb-attach session doesn't have a connect mode and
        // the UI must not claim otherwise (plan Bolum 5.1).
        snap.connectMode  = m_usingGdb
                                ? QStringLiteral("GDB-ATTACH")
                                : (m_pendingMapping.connectMode.isEmpty()
                                       ? QStringLiteral("HOTPLUG") : m_pendingMapping.connectMode);
        snap.supportLevel = m_pendingMapping.access.isEmpty()
                                ? QStringLiteral("stable") : m_pendingMapping.access;
        snap.peripherals  = m_decoder.decode(*dev, m_pendingSelected, combined,
                                             m_clockEnabled, m_gateable);
        snap.errors       = result.errors;
        snap.valid        = true;

        m_slots[m_pendingSlot]       = snap;
        m_slotFilled[m_pendingSlot]  = true;

        m_phase = Phase::Idle;
        setBusy(false);
        setStage(QStringLiteral("ready"));
        emit snapshotReady(m_pendingSlot);
    }
}

void RegisterInspector::onReadFailed(const QString &message)
{
    if (!m_busy)
        return;

    // Plan Bolum 5.1 condition (c): the GDB backend failed on the very FIRST
    // read of a fresh attempt (RCC phase) — nothing has reached the user yet,
    // so falling back to CLI and restarting the snapshot is always safe.
    // Broadened slightly beyond "only a retain() failure" to any failure this
    // early: the plan's own framing is "hiz kazanci konfordur, dogruluk
    // degil" — degrading silently is strictly better than surfacing an error
    // for what is ultimately a speed optimisation. A LATER-phase failure
    // (ReadBlocks) is a genuine error and is not retried.
    if (m_usingGdb && m_phase == Phase::ReadRcc) {
        const SvdDevice *dev = m_catalog.cachedDevice(m_pendingSvdFile);
        if (dev) {
            warnGdbFallbackOnce(QStringLiteral("GDB arka ucu kullanilamadi (%1), CLI'ye dusuldu").arg(message));
            m_reader   = m_cliReader;
            m_usingGdb = false;
            m_reader->setStlinkSn(m_pendingBoard.stlinkSn);
            m_reader->setConnectMode(m_pendingMapping.connectMode.isEmpty()
                                         ? QStringLiteral("HOTPLUG") : m_pendingMapping.connectMode);
            setStage(QStringLiteral("read-rcc"));
            m_reader->read(m_builder.buildRccPlan(*dev));
            return;
        }
    }

    finishWithError(message);
}

const RegisterSnapshot *RegisterInspector::snapshot(int slot) const
{
    if (slot < 0 || slot > 1 || !m_slotFilled[slot])
        return nullptr;
    return &m_slots[slot];
}

bool RegisterInspector::diffAvailable() const
{
    return m_slotFilled[0] && m_slotFilled[1];
}

SnapshotDiff RegisterInspector::computeDiff() const
{
    if (!diffAvailable()) {
        SnapshotDiff empty;
        empty.comparable = false;
        empty.incomparableReason = QStringLiteral("İki snapshot da (A ve B) alınmalı");
        return empty;
    }
    return m_differ.diff(m_slots[0], m_slots[1]);
}

QList<RuleViolation> RegisterInspector::ruleViolations(int slot) const
{
    const RegisterSnapshot *s = snapshot(slot);
    if (!s)
        return {};
    return m_rules.evaluate(s->peripherals);
}

void RegisterInspector::clearSnapshots()
{
    m_slotFilled[0] = m_slotFilled[1] = false;
    m_slots[0] = RegisterSnapshot{};
    m_slots[1] = RegisterSnapshot{};
}

void RegisterInspector::finishWithError(const QString &message)
{
    // A failed attempt must not leave the target slot showing whatever it
    // held before (possibly a different board's snapshot entirely — this
    // was reproduced live: NUCLEO-N657X0-Q's failed read left slot A still
    // "valid" and on-screen with STM32F407's peripherals/values from an
    // earlier attempt). registerModel/registerSnapshotInfo must reflect "no
    // snapshot" rather than silently keep stale, possibly wrong-board data
    // — the same "never show data that might be wrong without a visible
    // warning" principle CLAUDE.md already applies to the Watcher's ELF
    // mismatch check.
    if (m_pendingSlot == 0 || m_pendingSlot == 1) {
        m_slotFilled[m_pendingSlot] = false;
        m_slots[m_pendingSlot] = RegisterSnapshot{};
    }
    m_phase = Phase::Idle;
    m_lastError = message;
    setBusy(false);
    setStage(QStringLiteral("error"));
    emit errorOccurred(message);
}

void RegisterInspector::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit busyChanged();
}

void RegisterInspector::setStage(const QString &stage)
{
    if (m_stage == stage)
        return;
    m_stage = stage;
    emit stageChanged();
}
