#include "ModelSweepRunner.h"
#include "bridge/Backend.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QTimer>

ModelSweepRunner::ModelSweepRunner(Backend *backend, QObject *parent)
    : QObject(parent), m_backend(backend)
{
    m_sampleTimer = new QTimer(this);
    m_sampleTimer->setSingleShot(true);
    connect(m_sampleTimer, &QTimer::timeout, this, &ModelSweepRunner::onSampleWindowElapsed);

    m_elfWaitTimer = new QTimer(this);
    m_elfWaitTimer->setSingleShot(true);
    connect(m_elfWaitTimer, &QTimer::timeout, this, &ModelSweepRunner::onElfWaitTimeout);
}

QString ModelSweepRunner::modelNameFor(const QString &path)
{
    return QFileInfo(path).completeBaseName();
}

QString ModelSweepRunner::outputDirFor(int index) const
{
    return m_outputRoot + QStringLiteral("/") + modelNameFor(m_modelPaths.at(index));
}

QString ModelSweepRunner::elfPathFor(int index) const
{
    // MUST mirror PipelineRunner::stepBuild()'s own "expectedElfPath" —
    // <outputDir>/build/<modelName>_<targetBoard>.elf — since Backend
    // exposes no accessor for the ELF path it actually built.
    return QDir(outputDirFor(index)).filePath(
        QStringLiteral("build/%1_%2.elf").arg(modelNameFor(m_modelPaths.at(index)), m_board));
}

void ModelSweepRunner::start(const QStringList &modelPaths, const QString &board,
                              const QString &sensorType, int secondsPerModel)
{
    if (m_running || modelPaths.isEmpty()) return;

    m_modelPaths      = modelPaths;
    m_board           = board;
    m_sensorType      = sensorType;
    m_secondsPerModel = qMax(5, secondsPerModel);
    m_currentIndex    = -1;
    m_results.clear();
    m_cancelled = false;
    m_running   = true;
    m_outputRoot = QCoreApplication::applicationDirPath() + QStringLiteral("/../out/model_sweep_")
                   + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmss"));

    connect(m_backend, &Backend::pipelineChanged, this, &ModelSweepRunner::onPipelineChanged);
    connect(m_backend, &Backend::watchLinkChanged, this, &ModelSweepRunner::onWatchLinkChanged);
    connect(m_backend, &Backend::watchSymbolsLoaded, this, &ModelSweepRunner::onSymbolsLoaded);

    emit changed();
    advanceOrFinish();
}

void ModelSweepRunner::cancel()
{
    if (!m_running) return;
    m_cancelled = true;

    if (m_stage == Stage::Compiling) {
        m_backend->cancelPipeline();
        // onPipelineChanged() fires once the cancel settles (pipelineBusy
        // -> false); it checks m_cancelled and stops the sweep there —
        // never fabricates a fake "ok" result for the interrupted model.
        return;
    }

    // Connecting/Watching/Saving: no in-flight flash to worry about, so
    // unwind immediately rather than waiting on a signal that may not
    // reliably arrive from this exact stage.
    m_elfWaitTimer->stop();
    m_sampleTimer->stop();
    if (m_backend->watchRunning()) m_backend->stopWatch();
    if (m_backend->watchLinkOpen()) m_backend->closeWatchLink();
    m_running = false;
    m_stage   = Stage::Done;
    disconnectBackend();
    emit changed();
}

void ModelSweepRunner::disconnectBackend()
{
    disconnect(m_backend, &Backend::pipelineChanged, this, &ModelSweepRunner::onPipelineChanged);
    disconnect(m_backend, &Backend::watchLinkChanged, this, &ModelSweepRunner::onWatchLinkChanged);
    disconnect(m_backend, &Backend::watchSymbolsLoaded, this, &ModelSweepRunner::onSymbolsLoaded);
}

void ModelSweepRunner::startModel(int index)
{
    m_currentIndex = index;
    m_stage = Stage::Compiling;
    emit changed();

    const QString path = m_modelPaths.at(index);
    QVariantMap cfg;
    cfg[QStringLiteral("modelPath")]    = path;
    cfg[QStringLiteral("modelName")]    = modelNameFor(path);
    cfg[QStringLiteral("architecture")] = QStringLiteral("Auto");
    cfg[QStringLiteral("quantization")] = QStringLiteral("INT8");
    cfg[QStringLiteral("sensorType")]   = m_sensorType;
    cfg[QStringLiteral("protocol")]     = QStringLiteral("I2C");
    cfg[QStringLiteral("i2cInstance")]  = QStringLiteral("I2C1");
    cfg[QStringLiteral("sdaPort")]      = QStringLiteral("GPIOB");
    cfg[QStringLiteral("sdaPin")]       = QStringLiteral("GPIO_PIN_9");
    cfg[QStringLiteral("sclPort")]      = QStringLiteral("GPIOB");
    cfg[QStringLiteral("sclPin")]       = QStringLiteral("GPIO_PIN_8");
    cfg[QStringLiteral("i2cAddress")]   = QStringLiteral("0x76");
    cfg[QStringLiteral("targetBoard")]  = m_board;
    cfg[QStringLiteral("outputDir")]    = outputDirFor(index);

    m_backend->runPipeline(cfg);
}

void ModelSweepRunner::onPipelineChanged()
{
    if (!m_running || m_stage != Stage::Compiling) return;
    if (m_backend->pipelineBusy()) return;   // still running - wait for the next signal

    if (m_cancelled) { advanceOrFinish(); return; }

    const bool ok = m_backend->pipelineProgress() == 100;
    if (!ok) {
        finishModel(QStringLiteral("failed: derleme/flash hatasi"));
        return;
    }

    m_stage = Stage::Connecting;
    emit changed();
    if (m_backend->watchLinkOpen())
        onWatchLinkChanged();   // already open - proceed without waiting for a signal that won't fire
    else
        m_backend->openWatchLink();
}

void ModelSweepRunner::onWatchLinkChanged()
{
    if (!m_running || m_stage != Stage::Connecting) return;

    if (m_backend->watchLinkState() == QStringLiteral("failed")) {
        finishModel(QStringLiteral("failed: ST-Link baglantisi basarisiz"));
        return;
    }
    if (!m_backend->watchLinkOpen()) return;   // still connecting

    if (m_cancelled) { finishModel(QStringLiteral("failed: iptal edildi")); return; }

    // Each model gets its OWN preset resolution — without this, a later
    // model's profile silently accumulates the previous model's items too
    // (applyWatchPresets() only ADDS, it never replaces), caught live: a
    // second model's saved profile had 24 rows instead of 12.
    m_backend->clearWatchItems();

    // loadWatchElf() parses symbols via an async arm-none-eabi-nm subprocess
    // (ElfSymbolSource) — applyWatchPresets() MUST wait for onSymbolsLoaded()
    // rather than running right after this call. Calling it immediately was
    // a real bug caught live in this Faz's own verification: it resolved
    // presets against the PREVIOUS model's (still-cached) symbol table
    // instead of the new one, and against an EMPTY table for the very first
    // model in a sweep (0 items added -> saveWatchProfile() correctly
    // refused to save an empty profile).
    m_backend->loadWatchElf(elfPathFor(m_currentIndex));
    m_elfWaitTimer->start(15000);
}

void ModelSweepRunner::onSymbolsLoaded(int count)
{
    if (!m_running || m_stage != Stage::Connecting) return;
    m_elfWaitTimer->stop();

    if (m_cancelled) { finishModel(QStringLiteral("failed: iptal edildi")); return; }
    if (count <= 0) {
        finishModel(QStringLiteral("failed: ELF sembolleri okunamadi"));
        return;
    }

    m_backend->applyWatchPresets();
    m_backend->startWatch(200);
    m_stage = Stage::Watching;
    emit changed();
    m_sampleTimer->start(m_secondsPerModel * 1000);
}

void ModelSweepRunner::onElfWaitTimeout()
{
    if (!m_running || m_stage != Stage::Connecting) return;
    finishModel(QStringLiteral("failed: ELF sembolleri zaman asimina ugradi"));
}

void ModelSweepRunner::onSampleWindowElapsed()
{
    if (!m_running) return;

    m_backend->stopWatch();
    m_stage = Stage::Saving;
    emit changed();

    const bool saved = m_backend->saveWatchProfile(modelNameFor(m_modelPaths.at(m_currentIndex)));
    m_backend->closeWatchLink();
    finishModel(saved ? QStringLiteral("ok") : QStringLiteral("failed: profil kaydedilemedi"));
}

void ModelSweepRunner::finishModel(const QString &status)
{
    QVariantMap m;
    m[QStringLiteral("model")]  = modelNameFor(m_modelPaths.at(m_currentIndex));
    m[QStringLiteral("status")] = status;
    m_results.append(m);
    emit changed();
    advanceOrFinish();
}

void ModelSweepRunner::advanceOrFinish()
{
    if (m_cancelled || m_currentIndex + 1 >= m_modelPaths.size()) {
        m_running = false;
        m_stage   = Stage::Done;
        disconnectBackend();
        emit changed();
        return;
    }
    startModel(m_currentIndex + 1);
}

QVariantMap ModelSweepRunner::status() const
{
    QVariantMap m;
    m[QStringLiteral("running")]      = m_running;
    m[QStringLiteral("currentIndex")] = m_currentIndex;
    m[QStringLiteral("totalModels")]  = int(m_modelPaths.size());
    m[QStringLiteral("currentModel")] = (m_currentIndex >= 0 && m_currentIndex < m_modelPaths.size())
                                             ? modelNameFor(m_modelPaths.at(m_currentIndex)) : QString();
    switch (m_stage) {
    case Stage::Idle:       m[QStringLiteral("stage")] = QStringLiteral("idle"); break;
    case Stage::Compiling:  m[QStringLiteral("stage")] = QStringLiteral("compiling"); break;
    case Stage::Connecting: m[QStringLiteral("stage")] = QStringLiteral("connecting"); break;
    case Stage::Watching:   m[QStringLiteral("stage")] = QStringLiteral("watching"); break;
    case Stage::Saving:     m[QStringLiteral("stage")] = QStringLiteral("saving"); break;
    case Stage::Done:       m[QStringLiteral("stage")] = QStringLiteral("done"); break;
    }
    m[QStringLiteral("results")] = m_results;
    return m;
}
