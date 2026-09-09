#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

class Backend;
class QTimer;

// ── ModelSweepRunner ──────────────────────────────────────────────────────
// Faz 10.6: runs the compile→flash→watch→save cycle across several models
// in sequence, entirely through Backend's OWN public invokables/properties
// — it never touches PipelineRunner or VariableWatcher directly. That is
// deliberate: Backend is the single arbiter for the shared ST-Link
// (CLAUDE.md "Tek ST-Link, tek sahip"), and driving it through the same
// facade a human uses means this class automatically obeys that arbiter
// instead of needing its own copy of the rule. Concretely: the watch link
// is closed before each model's compile+flash step (flash needs the link
// free) and reopened after, exactly like using the UI by hand would.
//
// One model failing (compile error, flash refusal, ST-Link connect
// failure, profile save failure) does NOT stop the sweep — it is recorded
// in results() and the runner moves on to the next model.
class ModelSweepRunner : public QObject
{
    Q_OBJECT
public:
    explicit ModelSweepRunner(Backend *backend, QObject *parent = nullptr);

    void start(const QStringList &modelPaths, const QString &board,
               const QString &sensorType, int secondsPerModel);
    // Stops after the CURRENT model's in-flight step settles (pipeline
    // cancel, or an immediate stop+close if sampling) — never leaves a
    // half-flashed board or the ST-Link held by "watch" behind.
    void cancel();

    QVariantMap status() const;

signals:
    void changed();   // Backend forwards this 1:1 as sweepChanged()

private slots:
    void onPipelineChanged();
    void onWatchLinkChanged();
    void onSymbolsLoaded(int count);
    void onElfWaitTimeout();
    void onSampleWindowElapsed();

private:
    enum class Stage { Idle, Compiling, Connecting, Watching, Saving, Done };

    void startModel(int index);
    void finishModel(const QString &status);
    void advanceOrFinish();
    void disconnectBackend();
    static QString modelNameFor(const QString &path);
    QString elfPathFor(int index) const;
    QString outputDirFor(int index) const;

    Backend *m_backend = nullptr;
    QTimer  *m_sampleTimer = nullptr;
    QTimer  *m_elfWaitTimer = nullptr;   // bounds the wait for ELF symbols to load (nm subprocess)

    bool        m_running   = false;
    bool        m_cancelled = false;
    QStringList m_modelPaths;
    QString     m_board;
    QString     m_sensorType;
    int         m_secondsPerModel = 20;
    int         m_currentIndex = -1;
    Stage       m_stage = Stage::Idle;
    QString     m_outputRoot;
    QVariantList m_results;   // [{model, status}], status: "ok" | "failed: <reason>"
};
