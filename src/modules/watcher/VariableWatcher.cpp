#include "VariableWatcher.h"
#include "ElfSymbolSource.h"
#include "ValueCodec.h"
#include "WatchSampler.h"
#include "modules/debug/DebugLink.h"
#include "core/AppSettings.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

#include <limits>

namespace {
constexpr quint32 kElfMatchBatchId = 0xE1F0u;

WatchValueType guessTypeFromSize(quint64 size, bool hasSize)
{
    if (!hasSize) return WatchValueType::U32;
    switch (size) {
    case 1: return WatchValueType::U8;
    case 2: return WatchValueType::U16;
    case 8: return WatchValueType::U64;
    default: return WatchValueType::U32;
    }
}

// Distinct accent hues from qml/Theme.qml, cycled by insertion order so each
// new watch item gets a visually distinguishable plot colour by default
// (plan Bolum 9.3's "line colour assigned from Theme").
const QStringList &plotPalette()
{
    static const QStringList kPalette = {
        QStringLiteral("#4F8BFF"), QStringLiteral("#3FD0C9"), QStringLiteral("#3FB950"),
        QStringLiteral("#D8A23A"), QStringLiteral("#F0616D"), QStringLiteral("#A371F7"),
    };
    return kPalette;
}
}

VariableWatcher::VariableWatcher(DebugLink *link, QObject *parent)
    : QObject(parent)
    , m_link(link)
{
    m_elfSource = new ElfSymbolSource(this);
    connect(m_elfSource, &ElfSymbolSource::loaded, this, &VariableWatcher::onElfSymbolsLoaded);
    connect(m_elfSource, &ElfSymbolSource::failed, this, &VariableWatcher::onElfSymbolLoadFailed);

    connect(m_link, &DebugLink::opened,         this, &VariableWatcher::onLinkOpened);
    connect(m_link, &DebugLink::failed,         this, &VariableWatcher::onLinkFailed);
    connect(m_link, &DebugLink::rangesRead,     this, &VariableWatcher::onRangesRead);
    connect(m_link, &DebugLink::rawSamplesReady, this, &VariableWatcher::onRawSamplesReady);
    connect(m_link, &DebugLink::samplingStats,  this, &VariableWatcher::onSamplingStats);
    connect(m_link, &DebugLink::closed,         this, &VariableWatcher::onLinkClosed);
    connect(m_link, &DebugLink::coreHalted,     this, &VariableWatcher::onCoreHalted);
    connect(m_link, &DebugLink::coreReset,      this, &VariableWatcher::onCoreReset);

    m_buffer.configure(0);
}

void VariableWatcher::setNmPath(const QString &path) { m_elfSource->setNmPath(path); }

void VariableWatcher::loadElf(const QString &path)
{
    m_elfSource->load(path);
}

// ── Items ─────────────────────────────────────────────────────────────────

QString VariableWatcher::addSymbol(const QString &symbolName)
{
    if (m_running) {
        emit errorOccurred(tr("Ornekleme calisirken degisken listesi degistirilemez - once durdurun"));
        return QString();
    }

    const Symbol *found = nullptr;
    for (const Symbol &s : m_symbols)
        if (s.name == symbolName) { found = &s; break; }

    if (!found) {
        emit errorOccurred(tr("Sembol bulunamadi: %1").arg(symbolName));
        return QString();
    }
    if (found->addressIsValue) {
        emit errorOccurred(tr("'%1' bir DEGER (A tipi) - izleme listesine adres olarak eklenemez").arg(symbolName));
        return QString();
    }

    WatchItem item;
    item.id      = QUuid::createUuid().toString(QUuid::Id128);
    item.label   = symbolName;
    item.address = found->address;
    item.kind    = WatchItemKind::Scalar;
    item.type    = guessTypeFromSize(found->size, found->hasSize);
    item.source  = QStringLiteral("elf:%1").arg(symbolName);
    item.color   = plotPalette().at(m_items.size() % plotPalette().size());

    m_items.append(item);
    emit itemsChanged();
    return item.id;
}

QString VariableWatcher::addAddress(quint64 addr, WatchValueType t, const QString &label)
{
    if (m_running) {
        emit errorOccurred(tr("Ornekleme calisirken degisken listesi degistirilemez - once durdurun"));
        return QString();
    }

    // Soft validation against the loaded ELF's symbol address range — a
    // WARNING, never a block (plan Bolum 7.4: "hicbir durumda C++'a sabit
    // RAM tablosu yazilmaz"). qXfer:memory-map:read is not wired up yet
    // (deferred — DebugLinkWorker's whitelist allows it, nothing calls it),
    // so this is the ELF-range fallback only, per the plan's own point 2.
    if (!m_symbols.isEmpty()) {
        quint64 lo = std::numeric_limits<quint64>::max();
        quint64 hi = 0;
        for (const Symbol &s : m_symbols) {
            if (s.addressIsValue) continue;
            lo = qMin(lo, s.address);
            hi = qMax(hi, s.address + qMax<quint64>(1, s.size));
        }
        if (lo <= hi && (addr < lo || addr > hi)) {
            emit errorOccurred(tr("Uyari: 0x%1 yuklu ELF sembollerinin adres araligi disinda (yine de eklendi)")
                                    .arg(addr, 0, 16));
        }
    }

    WatchItem item;
    item.id      = QUuid::createUuid().toString(QUuid::Id128);
    item.label   = label.isEmpty() ? QStringLiteral("0x%1").arg(addr, 0, 16) : label;
    item.address = addr;
    item.kind    = WatchItemKind::Scalar;
    item.type    = t;
    item.source  = QStringLiteral("manual");
    item.color   = plotPalette().at(m_items.size() % plotPalette().size());

    m_items.append(item);
    emit itemsChanged();
    return item.id;
}

void VariableWatcher::updateItem(const QString &id, const QVariantMap &props)
{
    if (m_running) {
        emit errorOccurred(tr("Ornekleme calisirken degisken listesi degistirilemez - once durdurun"));
        return;
    }

    for (WatchItem &item : m_items) {
        if (item.id != id) continue;

        if (props.contains(QStringLiteral("label")))   item.label   = props.value(QStringLiteral("label")).toString();
        // NOTE: "role" MUST be settable here — TimeSeriesRuleEngine matches
        // rules by WatchItem::role (appliesToRole) and WatchProfile stores it
        // for compareWatchProfiles()'s findByRole(). Dropping it silently made
        // every role-based rule and the whole profile comparison dead code.
        if (props.contains(QStringLiteral("role")))     item.role    = props.value(QStringLiteral("role")).toString();
        if (props.contains(QStringLiteral("address")))  item.address = props.value(QStringLiteral("address")).toULongLong();
        if (props.contains(QStringLiteral("regionBytes")))
            item.regionBytes = quint32(props.value(QStringLiteral("regionBytes")).toUInt());
        if (props.contains(QStringLiteral("type")))     item.type    = watchValueTypeFromString(props.value(QStringLiteral("type")).toString());
        if (props.contains(QStringLiteral("format")))   item.format  = displayFormatFromString(props.value(QStringLiteral("format")).toString());
        if (props.contains(QStringLiteral("scale")))    item.scale   = props.value(QStringLiteral("scale")).toDouble();
        if (props.contains(QStringLiteral("offset")))   item.offset  = props.value(QStringLiteral("offset")).toDouble();
        if (props.contains(QStringLiteral("unit")))     item.unit    = props.value(QStringLiteral("unit")).toString();
        if (props.contains(QStringLiteral("enabled")))  item.enabled = props.value(QStringLiteral("enabled")).toBool();
        if (props.contains(QStringLiteral("color")))    item.color   = props.value(QStringLiteral("color")).toString();
        if (props.contains(QStringLiteral("laneIndex"))) item.laneIndex = props.value(QStringLiteral("laneIndex")).toInt();

        emit itemsChanged();
        return;
    }
}

void VariableWatcher::removeItem(const QString &id)
{
    if (m_running) {
        emit errorOccurred(tr("Ornekleme calisirken degisken listesi degistirilemez - once durdurun"));
        return;
    }
    const int before = m_items.size();
    m_items.removeIf([&id](const WatchItem &it) { return it.id == id; });
    if (m_items.size() != before)
        emit itemsChanged();
}

void VariableWatcher::clearItems()
{
    if (m_running) {
        emit errorOccurred(tr("Ornekleme calisirken degisken listesi degistirilemez - once durdurun"));
        return;
    }
    if (m_items.isEmpty()) return;
    m_items.clear();
    emit itemsChanged();
}

// ── Sampling ──────────────────────────────────────────────────────────────

void VariableWatcher::start(int targetRateHz)
{
    if (m_running) return;
    if (!m_link->isOpen()) {
        emit errorOccurred(tr("Izleyici baglantisi acik degil"));
        return;
    }
    if (m_items.isEmpty()) {
        emit errorOccurred(tr("Izlenecek degisken yok"));
        return;
    }
    if (m_elfMatchResult == ElfMatchResult::Mismatch && !m_mismatchAcknowledged) {
        emit errorOccurred(tr("ELF hedefle eslesmiyor - once onaylayin (Yine de devam et)"));
        return;
    }

    m_targetRateHz = targetRateHz;
    rebuildPlan();
    m_buffer.configure(m_items.size());
    m_pendingTimes.clear();
    m_pendingSeries.assign(m_items.size(), QVector<double>());
    m_flushTimer.start();
    m_droppedTotal = 0;
    m_readErrors   = 0;
    m_lastGoodValues = QVector<double>(m_items.size(), 0.0);
    m_actualRateHz = 0.0;
    m_rttMsAvg     = 0.0;

    setRunning(true);
    m_link->startSampling(m_plan.requests, targetRateHz);
}

void VariableWatcher::stop()
{
    if (m_isPlayback) { endPlayback(); return; }
    if (!m_running) return;
    m_link->stopSampling();
    flushPending();
    if (m_recorder.isRecording())
        m_recorder.stop();
    setRunning(false);
}

void VariableWatcher::setRunning(bool running)
{
    if (m_running == running) return;
    m_running = running;
    emit runningChanged();
}

void VariableWatcher::rebuildPlan()
{
    quint32 maxBytes = m_link->maxReadBytes();
    if (maxBytes == 0) maxBytes = 4096;
    m_plan = WatchPlanBuilder::build(m_items, maxBytes);
}

void VariableWatcher::clearBuffer()
{
    m_buffer.clear();
    m_pendingTimes.clear();
    for (auto &s : m_pendingSeries) s.clear();
}

QVariantMap VariableWatcher::rateInfo() const
{
    QVariantMap m;
    m[QStringLiteral("targetHz")]    = m_targetRateHz;
    m[QStringLiteral("actualHz")]    = m_actualRateHz;
    m[QStringLiteral("rttMs")]       = m_rttMsAvg;
    m[QStringLiteral("blocks")]      = m_plan.roundTripsPerSample();
    m[QStringLiteral("skewUs")]      = m_lastSkewUs;
    m[QStringLiteral("missed")]      = m_droppedTotal;
    m[QStringLiteral("readErrors")]  = qulonglong(m_readErrors);
    m[QStringLiteral("coreRunning")] = m_coreRunning;
    return m;
}

void VariableWatcher::onRawSamplesReady(const QVector<MemoryReply> &replies, double t, double skewUs)
{
    if (!m_running) return;

    QVector<bool> ok;
    const QVector<double> values = WatchSampler::decodeSample(m_items, m_plan, replies, &ok);

    m_pendingTimes.append(t);
    if (m_pendingSeries.size() != m_items.size())
        m_pendingSeries.resize(m_items.size());
    if (m_lastGoodValues.size() != m_items.size())
        m_lastGoodValues = QVector<double>(m_items.size(), 0.0);

    for (int i = 0; i < m_items.size(); ++i) {
        // A failed read must NOT enter the trace as a literal 0.0: it would be
        // indistinguishable from a genuine zero, and rules like
        // "stack headroom < 512 B" would fire on it as a hard false alarm.
        // Hold the last good value instead and count the failure so the UI can
        // show it (rateInfo()["readErrors"]).
        const bool good = (i < ok.size()) && ok.at(i) && (i < values.size());
        if (good)
            m_lastGoodValues[i] = values.at(i);
        else
            ++m_readErrors;
        m_pendingSeries[i].append(m_lastGoodValues.at(i));
    }
    m_lastSkewUs = skewUs;

    if (!m_flushTimer.isValid())
        m_flushTimer.start();

    // <=30 Hz or 512 samples, whichever comes first (plan Bolum 2.2).
    if (m_flushTimer.elapsed() >= 33 || m_pendingTimes.size() >= 512)
        flushPending();
}

void VariableWatcher::flushPending()
{
    if (m_pendingTimes.isEmpty())
        return;

    WatchSampleBatch batch;
    batch.times       = m_pendingTimes;
    batch.series       = m_pendingSeries;
    batch.coreRunning = m_coreRunning;
    batch.skewUs       = m_lastSkewUs;
    m_buffer.append(batch);
    if (m_recorder.isRecording())
        m_recorder.appendBatch(batch);

    m_pendingTimes.clear();
    for (auto &s : m_pendingSeries) s.clear();
    m_flushTimer.restart();

    emit samplesAppended();
}

void VariableWatcher::onSamplingStats(double actualRateHz, double rttMsAvg, quint32 dropped)
{
    m_actualRateHz  = actualRateHz;
    m_rttMsAvg      = rttMsAvg;
    m_droppedTotal += dropped;
    emit statsChanged();
}

void VariableWatcher::onLinkClosed()
{
    // Any in-flight ELF match check is dead with the link; clear the guard so
    // a later reconnect can re-run it instead of being stuck "in flight".
    m_elfMatchStep = 0;
    if (m_elfMatchTimeoutTimer) m_elfMatchTimeoutTimer->stop();
    setElfMatch(ElfMatchResult::Unknown, ElfMatchReport{});

    if (m_isPlayback || !m_running)
        return;   // playback needs no link

    if (m_recorder.isRecording())
        m_recorder.stop();   // flush the summary/event trailer we do have
    setRunning(false);
    emit errorOccurred(tr("Baglanti kesildi - ornekleme durduruldu"));
}

void VariableWatcher::onCoreHalted()
{
    m_coreRunning = false;
    if (m_running) {
        stop();
        emit errorOccurred(tr("Hedef durdu (S_HALT) - ornekleme durduruldu"));
    }
}

void VariableWatcher::onCoreReset()
{
    // Sampling continues on purpose (plan Bolum 4.5) — values just become
    // discontinuous around this point. TraceEventLog (Faz 6) will mark this
    // visibly on the graph; for now it is at least surfaced as a message.
    emit errorOccurred(tr("Hedef resetlendi - bu noktadan sonraki degerler sureksiz olabilir"));
}

// ── ELF symbols + target match ───────────────────────────────────────────

void VariableWatcher::onElfSymbolsLoaded(const QList<Symbol> &symbols, const QString &elfPath)
{
    m_symbols = symbols;
    m_elfPath = elfPath;
    m_mismatchAcknowledged = false;
    emit symbolsLoaded(symbols.size());
    maybeCheckElfMatch();
}

void VariableWatcher::onElfSymbolLoadFailed(const QString &message)
{
    emit errorOccurred(message);
}

void VariableWatcher::onLinkOpened()
{
    maybeCheckElfMatch();
}

void VariableWatcher::onLinkFailed(const QString &message)
{
    emit errorOccurred(message);
}

void VariableWatcher::maybeCheckElfMatch()
{
    if (m_elfMatchStep != 0)
        return;   // a check is already in flight
    if (!m_link->isOpen() || m_symbols.isEmpty()) {
        setElfMatch(ElfMatchResult::Unknown, ElfMatchReport{});
        return;
    }

    m_elfMatchStep = 1;
    QVector<MemoryRequest> reqs{ MemoryRequest{ 1, 0xE000ED08ull, 4u } };   // VTOR
    m_link->readRanges(kElfMatchBatchId, reqs);
    armElfMatchTimeout();
}

void VariableWatcher::armElfMatchTimeout()
{
    if (!m_elfMatchTimeoutTimer) {
        m_elfMatchTimeoutTimer = new QTimer(this);
        m_elfMatchTimeoutTimer->setSingleShot(true);
        connect(m_elfMatchTimeoutTimer, &QTimer::timeout, this, &VariableWatcher::onElfMatchTimeout);
    }
    // A reply that never arrives (RSP request the server never answers, as
    // opposed to the link outright closing — onLinkClosed() already handles
    // that case) used to leave m_elfMatchStep stuck at a nonzero value
    // forever, since maybeCheckElfMatch() refuses to start a second check
    // while one looks "in flight". 5 s is generous for a single-block
    // memory read; a real reply normally arrives within one RTT (<1 ms measured).
    m_elfMatchTimeoutTimer->start(5000);
}

void VariableWatcher::onElfMatchTimeout()
{
    if (m_elfMatchStep == 0)
        return;   // completed (or reset by onLinkClosed()) before the timer fired
    m_elfMatchStep = 0;
    setElfMatch(ElfMatchResult::Unknown, ElfMatchReport{});
    emit errorOccurred(tr("ELF eslesme kontrolu zaman asimina ugradi - hedef yanit vermiyor olabilir"));
}

void VariableWatcher::onRangesRead(quint32 batchId, const QVector<MemoryReply> &replies)
{
    if (batchId != kElfMatchBatchId)
        return;   // not ours (e.g. a Register Inspector snapshot) — ignore
    handleElfMatchReply(replies);
}

void VariableWatcher::handleElfMatchReply(const QVector<MemoryReply> &replies)
{
    if (m_elfMatchStep == 1) {
        if (replies.isEmpty() || !replies.first().ok || replies.first().data.size() < 4) {
            m_elfMatchStep = 0;
            if (m_elfMatchTimeoutTimer) m_elfMatchTimeoutTimer->stop();
            setElfMatch(ElfMatchResult::Unknown, ElfMatchReport{});
            return;
        }
        const QByteArray &d = replies.first().data;
        m_pendingVtor = quint32(uchar(d[0])) | (quint32(uchar(d[1])) << 8)
                       | (quint32(uchar(d[2])) << 16) | (quint32(uchar(d[3])) << 24);

        m_elfMatchStep = 2;
        QVector<MemoryRequest> reqs{ MemoryRequest{ 2, quint64(m_pendingVtor), 8u } };
        m_link->readRanges(kElfMatchBatchId, reqs);
        armElfMatchTimeout();   // second round trip — reset the watchdog for it too
        return;
    }

    if (m_elfMatchStep == 2) {
        m_elfMatchStep = 0;
        if (m_elfMatchTimeoutTimer) m_elfMatchTimeoutTimer->stop();
        if (replies.isEmpty() || !replies.first().ok || replies.first().data.size() < 8) {
            setElfMatch(ElfMatchResult::Unknown, ElfMatchReport{});
            return;
        }
        const ElfMatchReport report = ElfTargetMatcher::evaluate(m_pendingVtor, replies.first().data, m_symbols);
        setElfMatch(report.result, report);
    }
}

void VariableWatcher::setElfMatch(ElfMatchResult result, const ElfMatchReport &report)
{
    const bool resultChanged = (result != m_elfMatchResult);
    m_elfMatchResult = result;
    m_elfMatchReport = report;
    if (resultChanged && result == ElfMatchResult::Mismatch)
        m_mismatchAcknowledged = false;
    emit elfMatchChanged();
}

// ── Persistence ───────────────────────────────────────────────────────────

void VariableWatcher::saveItems(const QString &boardName)
{
    QJsonArray arr;
    for (const WatchItem &it : m_items) {
        QJsonObject o;
        o[QStringLiteral("id")]          = it.id;
        o[QStringLiteral("label")]       = it.label;
        o[QStringLiteral("role")]        = it.role;
        o[QStringLiteral("address")]     = QString::number(it.address);
        o[QStringLiteral("kind")]        = (it.kind == WatchItemKind::RegionScan)
                                                ? QStringLiteral("region") : QStringLiteral("scalar");
        o[QStringLiteral("type")]        = watchValueTypeToString(it.type);
        o[QStringLiteral("format")]      = displayFormatToString(it.format);
        o[QStringLiteral("regionBytes")] = int(it.regionBytes);
        o[QStringLiteral("scale")]       = it.scale;
        o[QStringLiteral("offset")]      = it.offset;
        o[QStringLiteral("unit")]        = it.unit;
        o[QStringLiteral("enabled")]     = it.enabled;
        o[QStringLiteral("source")]      = it.source;
        o[QStringLiteral("color")]       = it.color;
        o[QStringLiteral("laneIndex")]   = it.laneIndex;
        arr.append(o);
    }
    AppSettings().setWatchItemsJson(boardName, QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

void VariableWatcher::loadItems(const QString &boardName)
{
    const QJsonArray arr = QJsonDocument::fromJson(AppSettings().watchItemsJson(boardName)).array();
    m_items.clear();
    for (const QJsonValue &v : arr) {
        const QJsonObject o = v.toObject();
        WatchItem it;
        it.id          = o.value(QStringLiteral("id")).toString();
        it.label       = o.value(QStringLiteral("label")).toString();
        it.role        = o.value(QStringLiteral("role")).toString();
        it.address     = o.value(QStringLiteral("address")).toString().toULongLong();
        it.kind        = (o.value(QStringLiteral("kind")).toString() == QStringLiteral("region"))
                              ? WatchItemKind::RegionScan : WatchItemKind::Scalar;
        it.type        = watchValueTypeFromString(o.value(QStringLiteral("type")).toString());
        it.format      = displayFormatFromString(o.value(QStringLiteral("format")).toString());
        it.regionBytes = quint32(o.value(QStringLiteral("regionBytes")).toInt());
        it.scale       = o.value(QStringLiteral("scale")).toDouble(1.0);
        it.offset      = o.value(QStringLiteral("offset")).toDouble(0.0);
        it.unit        = o.value(QStringLiteral("unit")).toString();
        it.enabled     = o.value(QStringLiteral("enabled")).toBool(true);
        it.source      = o.value(QStringLiteral("source")).toString();
        it.color       = o.value(QStringLiteral("color")).toString();
        it.laneIndex   = o.value(QStringLiteral("laneIndex")).toInt(-1);
        m_items.append(it);
    }
    emit itemsChanged();
}

// ── Faz 7: recording ─────────────────────────────────────────────────────

QString VariableWatcher::startRecording(const QString &path, const QString &board, const QString &model)
{
    if (m_isPlayback) {
        const QString msg = tr("Kayittan oynatma sirasinda kayit baslatilamaz");
        emit errorOccurred(msg);
        return msg;
    }
    if (!m_running) {
        const QString msg = tr("Kayit icin once izlemeyi baslatin");
        emit errorOccurred(msg);
        return msg;
    }
    if (!m_recorder.start(path, board, m_elfPath, model, m_targetRateHz, m_items)) {
        const QString msg = m_recorder.lastError();
        emit errorOccurred(msg);
        return msg;
    }
    return QString();
}

void VariableWatcher::stopRecording()
{
    m_recorder.stop();
}

void VariableWatcher::addRecordingEvent(double t, const QString &kind, const QString &text, const QString &severity)
{
    if (m_recorder.isRecording())
        m_recorder.addEvent(t, kind, text, severity);
}

// ── Faz 7: playback ───────────────────────────────────────────────────────

QString VariableWatcher::startPlayback(const QString &path, double speed)
{
    if (m_running) {
        const QString msg = tr("Canli ornekleme calisirken oynatma baslatilamaz - once durdurun");
        emit errorOccurred(msg);
        return msg;
    }
    if (!m_player.load(path)) {
        const QString msg = m_player.lastError();
        emit errorOccurred(msg);
        return msg;
    }

    if (!m_player.loadWarning().isEmpty())
        emit errorOccurred(m_player.loadWarning());   // loaded, but not intact

    m_preservedLiveItems = m_items;
    m_hadPreservedItems  = true;

    m_items.clear();
    for (const LoadedTraceItem &li : m_player.items()) {
        WatchItem it;
        it.id      = QUuid::createUuid().toString(QUuid::Id128);
        it.label   = li.label;
        it.address = li.address;
        it.kind    = WatchItemKind::Scalar;
        it.type    = li.type;
        it.format  = li.format;
        it.scale   = li.scale;
        it.offset  = li.offset;
        it.unit    = li.unit;
        it.role    = li.role;
        it.source  = QStringLiteral("playback");
        it.color   = plotPalette().at(m_items.size() % plotPalette().size());
        m_items.append(it);
    }
    emit itemsChanged();

    const int capacity = qMax(1, m_player.times().size());
    m_buffer.configure(m_items.size(), capacity);

    m_isPlayback        = true;
    m_playbackSpeed      = qMax(0.0, speed);
    m_playbackNextIndex = 0;
    m_playbackBaseT     = m_player.times().isEmpty() ? 0.0 : m_player.times().first();
    m_playbackVirtualT  = m_playbackBaseT;
    m_playbackClock.start();

    if (!m_playbackTimer) {
        m_playbackTimer = new QTimer(this);
        connect(m_playbackTimer, &QTimer::timeout, this, &VariableWatcher::onPlaybackTick);
    }
    m_playbackTimer->start(33);   // same ~30 Hz batching cadence as live (plan Bolum 2.2)

    setRunning(true);
    return QString();
}

void VariableWatcher::stopPlayback()
{
    if (!m_isPlayback) return;
    endPlayback();
}

void VariableWatcher::setPlaybackSpeed(double speed)
{
    m_playbackSpeed = qMax(0.0, speed);
    if (m_playbackClock.isValid())
        m_playbackClock.restart();   // drop any elapsed time accrued under the old speed
}

void VariableWatcher::stepPlayback()
{
    if (!m_isPlayback) return;
    if (m_playbackNextIndex >= m_player.times().size()) return;

    WatchSampleBatch batch;
    appendPlaybackSample(m_playbackNextIndex, batch);
    m_playbackVirtualT = m_player.times().at(m_playbackNextIndex);
    ++m_playbackNextIndex;

    batch.coreRunning = true;
    m_buffer.append(batch);
    emit samplesAppended();

    if (m_playbackNextIndex >= m_player.times().size()) {
        endPlayback();
        emit playbackFinished();
    }
}

QVariantMap VariableWatcher::playbackInfo() const
{
    QVariantMap m;
    m[QStringLiteral("active")]    = m_isPlayback;
    m[QStringLiteral("board")]     = m_player.board();
    m[QStringLiteral("model")]     = m_player.model();
    m[QStringLiteral("elfPath")]   = m_player.elfPath();
    m[QStringLiteral("started")]   = m_player.started();
    m[QStringLiteral("targetHz")]  = m_player.targetRateHz();
    m[QStringLiteral("actualHz")]  = m_player.actualHz();
    m[QStringLiteral("speed")]     = m_playbackSpeed;
    const double total = m_player.times().isEmpty()
                              ? 0.0 : (m_player.times().last() - m_player.times().first());
    m[QStringLiteral("totalS")]    = total;
    m[QStringLiteral("positionS")] = qBound(0.0, m_playbackVirtualT - m_playbackBaseT, total);
    return m;
}

void VariableWatcher::appendPlaybackSample(int idx, WatchSampleBatch &batch)
{
    const QVector<double> &times = m_player.times();
    batch.times.append(times.at(idx) - m_playbackBaseT);
    if (batch.series.size() != m_items.size())
        batch.series.resize(m_items.size());
    for (int i = 0; i < m_items.size(); ++i) {
        const QVector<double> &s = m_player.series().at(i);
        batch.series[i].append(idx < s.size() ? s.at(idx) : 0.0);
    }
}

void VariableWatcher::onPlaybackTick()
{
    if (!m_isPlayback) return;

    const qint64 ms = m_playbackClock.restart();
    if (m_playbackSpeed <= 0.0) return;   // paused — only stepPlayback() advances

    m_playbackVirtualT += (double(ms) / 1000.0) * m_playbackSpeed;

    const QVector<double> &times = m_player.times();
    WatchSampleBatch batch;
    while (m_playbackNextIndex < times.size() && times.at(m_playbackNextIndex) <= m_playbackVirtualT) {
        appendPlaybackSample(m_playbackNextIndex, batch);
        ++m_playbackNextIndex;
    }

    if (!batch.times.isEmpty()) {
        batch.coreRunning = true;
        m_buffer.append(batch);
        emit samplesAppended();
    }

    if (m_playbackNextIndex >= times.size()) {
        endPlayback();
        emit playbackFinished();
    }
}

void VariableWatcher::endPlayback()
{
    if (m_playbackTimer)
        m_playbackTimer->stop();
    m_isPlayback = false;

    if (m_hadPreservedItems) {
        m_items = m_preservedLiveItems;
        m_preservedLiveItems.clear();
        m_hadPreservedItems = false;
        m_buffer.configure(m_items.size());
        emit itemsChanged();
    }
    setRunning(false);
}
