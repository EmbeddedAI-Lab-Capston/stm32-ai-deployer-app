#pragma once
#include "SvdModel.h"
#include "modules/board/BoardPresets.h"   // BoardInfo

#include <QObject>
#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>

// A single board -> SVD mapping record from svd/boards.json.
struct SvdBoardMapping
{
    QStringList names;                  // ["STM32H7", "NUCLEO-H723ZG"]
    QStringList deviceIds;              // ["0x483"]
    QString     svdFile;                // "STM32H723.svd"
    QString     device;                 // "STM32H723ZGTx"
    QString     access;                 // "stable" | "experimental"
    QString     connectMode;            // "HOTPLUG"
    QString     fallbackConnectMode;    // "UR" (N6)
    QString     notes;
    QStringList defaultPeripherals;
    QHash<QString, QString> rccOverrides;

    bool isValid() const { return !svdFile.isEmpty(); }
    bool isExperimental() const { return access == QStringLiteral("experimental"); }
};

// ── SvdCatalog ─────────────────────────────────────────────────────────────
// Owns svd/boards.json + the parsed SvdDevice cache. Maps a BoardInfo to the
// right SVD, parses it off the main thread (QtConcurrent), caches the result.
// Board-specific knowledge lives in boards.json + the SVD, never here
// (docs/register_inspector_plan.md Bolum 3.3). Finds files via
// applicationDirPath()/svd, same as PipelineRunner's templates/ pattern.
class SvdCatalog : public QObject
{
    Q_OBJECT
public:
    explicit SvdCatalog(QObject *parent = nullptr);

    // Defaults to applicationDirPath()/svd; override for tests or AppSettings.
    void    setSvdDirectory(const QString &dir);
    QString svdDirectory() const;

    // Load boards.json. Returns false + errorString() on failure.
    bool    loadBoardsJson();
    QString errorString() const { return m_error; }

    const QList<SvdBoardMapping> &boardMappings() const { return m_mappings; }
    SvdBoardMapping mappingForBoard(const BoardInfo &board) const;

    // Kick off async parse for a board's SVD (no-op if cached/in-flight).
    // Emits deviceReady(svdFile) or parseError(svdFile, msg) on the main thread.
    void requestDevice(const BoardInfo &board);

    bool isCached(const QString &svdFile) const { return m_cache.contains(svdFile); }
    const SvdDevice *cachedDevice(const QString &svdFile) const;

signals:
    void deviceReady(const QString &svdFile);
    void parseError(const QString &svdFile, const QString &message);

private:
    QString                 m_svdDir;
    QList<SvdBoardMapping>  m_mappings;
    QHash<QString, SvdDevice> m_cache;
    QSet<QString>           m_inFlight;
    QString                 m_error;
};
