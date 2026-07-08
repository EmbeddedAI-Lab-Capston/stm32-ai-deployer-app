#include "RegisterInspector.h"
#include "CliRegisterReader.h"

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
    m_reader->setStlinkSn(m_pendingBoard.stlinkSn);
    m_reader->setConnectMode(m_pendingMapping.connectMode.isEmpty()
                                 ? QStringLiteral("HOTPLUG")
                                 : m_pendingMapping.connectMode);

    m_phase = Phase::ReadRcc;
    setStage(QStringLiteral("read-rcc"));
    m_reader->read(m_builder.buildRccPlan(*dev));
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
        snap.connectMode  = m_pendingMapping.connectMode.isEmpty()
                                ? QStringLiteral("HOTPLUG") : m_pendingMapping.connectMode;
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
    if (m_busy)
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
