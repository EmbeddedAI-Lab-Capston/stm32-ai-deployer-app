#pragma once

#include <QHash>
#include <QObject>
#include <QString>

class QJsonObject;
class QLocalServer;
class QLocalSocket;
class QQuickWindow;

// ── DebugBridge ─────────────────────────────────────────────────────────────
// Dev-only verification channel. Listens on a Windows named pipe
// (QLocalServer) and answers newline-delimited JSON commands: grab a
// screenshot, dump the QML scene as text, read QObject properties, switch
// tabs, click an item by objectName.
//
// The point is cost: a text scene dump answers "is the button there, is it
// enabled, what does it say" for a fraction of what reading a screenshot
// costs. Screenshots stay reserved for genuinely visual checks.
//
// NEVER starts unless main() was given --debug-bridge, so a normal run (and
// the jury demo) has no pipe and no extra surface.
class DebugBridge : public QObject
{
    Q_OBJECT

public:
    explicit DebugBridge(QQuickWindow *window, QObject *parent = nullptr);
    ~DebugBridge() override;

    // Objects reachable from the "props" command, addressed by name.
    void registerObject(const QString &name, QObject *object);

    bool listen(const QString &pipeName);
    QString pipeName() const { return m_pipeName; }

private slots:
    void onNewConnection();

private:
    void handleLine(QLocalSocket *client, const QByteArray &line);
    QJsonObject dispatch(const QJsonObject &request);

    QJsonObject cmdPing() const;
    QJsonObject cmdShot(const QJsonObject &request);
    QJsonObject cmdDump(const QJsonObject &request);
    QJsonObject cmdProps(const QJsonObject &request) const;
    QJsonObject cmdNavigate(const QJsonObject &request);
    QJsonObject cmdClick(const QJsonObject &request);
    QJsonObject cmdInvoke(const QJsonObject &request) const;

    QQuickWindow *m_window = nullptr;
    QLocalServer *m_server = nullptr;
    QString m_pipeName;
    QHash<QString, QObject *> m_objects;
};
