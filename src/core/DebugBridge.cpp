#include "DebugBridge.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMetaMethod>
#include <QMetaObject>
#include <QMetaProperty>
#include <QMouseEvent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QVariant>

namespace {

// QML-declared types get a generated class name like "Card_QMLTYPE_42".
// The suffix is noise for a reader, so strip it.
QString cleanTypeName(const QObject *object)
{
    QString name = QString::fromLatin1(object->metaObject()->className());
    const int marker = name.indexOf(QStringLiteral("_QMLTYPE_"));
    if (marker > 0)
        name.truncate(marker);
    return name;
}

// Responses are read by a human or a model, so they must stay small: long
// strings and long lists are truncated rather than dumped whole.
QJsonValue clampValue(const QVariant &value)
{
    if (value.typeId() == QMetaType::QVariantList) {
        // Tail, not head: every list this hits so far (pipelineLines,
        // monitorLines, flashLines, ...) is an accumulating log, where the
        // most recent entries - not the first 20 written at app start - are
        // what a caller actually wants when the list is too long to show in
        // full.
        const QVariantList list = value.toList();
        QJsonArray array;
        const int start = qMax(0, list.size() - 20);
        if (start > 0)
            array.append(QStringLiteral("...(%1 earlier)").arg(start));
        for (int i = start; i < list.size(); ++i)
            array.append(QJsonValue::fromVariant(list.at(i)));
        return array;
    }
    if (value.typeId() == QMetaType::QString) {
        QString text = value.toString();
        if (text.size() > 400)
            text = text.left(400) + QStringLiteral("...");
        return text;
    }
    const QJsonValue json = QJsonValue::fromVariant(value);
    if (json.isNull() && value.isValid())
        return QStringLiteral("<%1>").arg(QString::fromLatin1(value.typeName()));
    return json;
}

void collectItems(QQuickItem *item, int depth, int maxDepth, bool onlyVisible,
                  const QString &filter, QJsonArray &out)
{
    if (!item || depth > maxDepth)
        return;

    const QList<QQuickItem *> children = item->childItems();
    for (QQuickItem *child : children) {
        const bool visible = child->isVisible();
        // Skipping the whole subtree is intentional: nothing under an
        // invisible item is on screen either.
        if (onlyVisible && !visible)
            continue;

        const QString type = cleanTypeName(child);
        const QString name = child->objectName();
        const bool matches = filter.isEmpty()
                             || type.contains(filter, Qt::CaseInsensitive)
                             || name.contains(filter, Qt::CaseInsensitive);

        if (matches && (child->width() > 0.0 || child->height() > 0.0)) {
            const QPointF scenePos = child->mapToScene(QPointF(0, 0));
            QJsonObject entry;
            entry.insert(QStringLiteral("type"), type);
            if (!name.isEmpty())
                entry.insert(QStringLiteral("name"), name);
            entry.insert(QStringLiteral("depth"), depth);
            entry.insert(QStringLiteral("x"), qRound(scenePos.x()));
            entry.insert(QStringLiteral("y"), qRound(scenePos.y()));
            entry.insert(QStringLiteral("w"), qRound(child->width()));
            entry.insert(QStringLiteral("h"), qRound(child->height()));
            if (!visible)
                entry.insert(QStringLiteral("visible"), false);
            if (!child->isEnabled())
                entry.insert(QStringLiteral("enabled"), false);
            const QVariant text = child->property("text");
            if (text.isValid() && !text.toString().isEmpty())
                entry.insert(QStringLiteral("text"), text.toString().left(120));
            out.append(entry);
        }

        collectItems(child, depth + 1, maxDepth, onlyVisible, filter, out);
    }
}

QQuickItem *findByObjectName(QQuickItem *root, const QString &name)
{
    if (!root)
        return nullptr;
    const QList<QQuickItem *> children = root->childItems();
    for (QQuickItem *child : children) {
        if (child->objectName() == name)
            return child;
        if (QQuickItem *found = findByObjectName(child, name))
            return found;
    }
    return nullptr;
}

} // namespace

DebugBridge::DebugBridge(QQuickWindow *window, QObject *parent)
    : QObject(parent)
    , m_window(window)
{
}

DebugBridge::~DebugBridge()
{
    if (m_server) {
        m_server->close();
        QLocalServer::removeServer(m_pipeName);
    }
}

void DebugBridge::registerObject(const QString &name, QObject *object)
{
    if (object)
        m_objects.insert(name, object);
}

bool DebugBridge::listen(const QString &pipeName)
{
    m_pipeName = pipeName;
    // A previous run that crashed can leave the name registered.
    QLocalServer::removeServer(pipeName);

    m_server = new QLocalServer(this);
    connect(m_server, &QLocalServer::newConnection, this, &DebugBridge::onNewConnection);

    if (!m_server->listen(pipeName)) {
        qWarning() << "DebugBridge: listen failed:" << m_server->errorString();
        return false;
    }
    qInfo() << "DebugBridge listening on pipe" << pipeName;
    return true;
}

void DebugBridge::onNewConnection()
{
    while (QLocalSocket *client = m_server->nextPendingConnection()) {
        connect(client, &QLocalSocket::readyRead, this, [this, client]() {
            while (client->canReadLine())
                handleLine(client, client->readLine());
        });
        connect(client, &QLocalSocket::disconnected, client, &QObject::deleteLater);
    }
}

void DebugBridge::handleLine(QLocalSocket *client, const QByteArray &line)
{
    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(line.trimmed(), &error);

    QJsonObject reply;
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        reply.insert(QStringLiteral("ok"), false);
        reply.insert(QStringLiteral("error"),
                     QStringLiteral("bad json: ") + error.errorString());
    } else {
        reply = dispatch(document.object());
    }

    client->write(QJsonDocument(reply).toJson(QJsonDocument::Compact) + '\n');
    client->flush();
}

QJsonObject DebugBridge::dispatch(const QJsonObject &request)
{
    const QString command = request.value(QStringLiteral("cmd")).toString();

    if (command == QLatin1String("ping"))
        return cmdPing();
    if (command == QLatin1String("shot"))
        return cmdShot(request);
    if (command == QLatin1String("dump"))
        return cmdDump(request);
    if (command == QLatin1String("props"))
        return cmdProps(request);
    if (command == QLatin1String("navigate"))
        return cmdNavigate(request);
    if (command == QLatin1String("click"))
        return cmdClick(request);
    if (command == QLatin1String("invoke"))
        return cmdInvoke(request);
    if (command == QLatin1String("quit")) {
        QMetaObject::invokeMethod(qApp, "quit", Qt::QueuedConnection);
        return QJsonObject{{QStringLiteral("ok"), true}};
    }

    return QJsonObject{{QStringLiteral("ok"), false},
                       {QStringLiteral("error"), QStringLiteral("unknown cmd: ") + command}};
}

QJsonObject DebugBridge::cmdPing() const
{
    QJsonObject reply{{QStringLiteral("ok"), true}};
    reply.insert(QStringLiteral("app"), QCoreApplication::applicationName());
    reply.insert(QStringLiteral("version"), QCoreApplication::applicationVersion());
    reply.insert(QStringLiteral("windowVisible"), m_window && m_window->isVisible());
    reply.insert(QStringLiteral("objects"), QJsonArray::fromStringList(m_objects.keys()));
    return reply;
}

QJsonObject DebugBridge::cmdShot(const QJsonObject &request)
{
    if (!m_window)
        return QJsonObject{{QStringLiteral("ok"), false},
                           {QStringLiteral("error"), QStringLiteral("no window")}};
    // Grabbing a hidden window yields blank pixels; fail loudly instead of
    // silently saving a black image.
    if (!m_window->isVisible())
        return QJsonObject{{QStringLiteral("ok"), false},
                           {QStringLiteral("error"), QStringLiteral("window not visible")}};

    QString path = request.value(QStringLiteral("path")).toString();
    if (path.isEmpty())
        path = QDir(QCoreApplication::applicationDirPath())
                   .filePath(QStringLiteral("shots/shot.png"));
    QDir().mkpath(QFileInfo(path).absolutePath());

    const QImage image = m_window->grabWindow();
    if (image.isNull() || !image.save(path))
        return QJsonObject{{QStringLiteral("ok"), false},
                           {QStringLiteral("error"), QStringLiteral("grab/save failed: ") + path}};

    QJsonObject reply{{QStringLiteral("ok"), true}};
    reply.insert(QStringLiteral("path"), QFileInfo(path).absoluteFilePath());
    reply.insert(QStringLiteral("w"), image.width());
    reply.insert(QStringLiteral("h"), image.height());
    return reply;
}

QJsonObject DebugBridge::cmdDump(const QJsonObject &request)
{
    if (!m_window)
        return QJsonObject{{QStringLiteral("ok"), false},
                           {QStringLiteral("error"), QStringLiteral("no window")}};

    const int maxDepth = request.value(QStringLiteral("maxDepth")).toInt(12);
    const bool onlyVisible = request.value(QStringLiteral("onlyVisible")).toBool(true);
    const QString filter = request.value(QStringLiteral("filter")).toString();

    QJsonArray items;
    collectItems(m_window->contentItem(), 0, maxDepth, onlyVisible, filter, items);

    QJsonObject reply{{QStringLiteral("ok"), true}};
    reply.insert(QStringLiteral("count"), items.size());
    reply.insert(QStringLiteral("items"), items);
    return reply;
}

QJsonObject DebugBridge::cmdProps(const QJsonObject &request) const
{
    const QString name = request.value(QStringLiteral("object")).toString();
    QObject *target = m_objects.value(name, nullptr);
    if (!target)
        return QJsonObject{{QStringLiteral("ok"), false},
                           {QStringLiteral("error"), QStringLiteral("unknown object: ") + name}};

    const QString filter = request.value(QStringLiteral("filter")).toString();

    QJsonObject values;
    const QMetaObject *meta = target->metaObject();
    // propertyOffset() skips QObject's own properties (objectName), which are
    // never what the caller is asking about.
    for (int i = meta->propertyOffset(); i < meta->propertyCount(); ++i) {
        const QMetaProperty property = meta->property(i);
        if (!property.isReadable())
            continue;
        const QString propertyName = QString::fromLatin1(property.name());
        if (!filter.isEmpty() && !propertyName.contains(filter, Qt::CaseInsensitive))
            continue;
        values.insert(propertyName, clampValue(property.read(target)));
    }

    QJsonObject reply{{QStringLiteral("ok"), true}};
    reply.insert(QStringLiteral("object"), name);
    reply.insert(QStringLiteral("props"), values);
    return reply;
}

QJsonObject DebugBridge::cmdNavigate(const QJsonObject &request)
{
    if (!m_window)
        return QJsonObject{{QStringLiteral("ok"), false},
                           {QStringLiteral("error"), QStringLiteral("no window")}};

    const int tab = request.value(QStringLiteral("tab")).toInt(-1);
    if (tab < 0)
        return QJsonObject{{QStringLiteral("ok"), false},
                           {QStringLiteral("error"), QStringLiteral("tab index required")}};

    const bool invoked = QMetaObject::invokeMethod(m_window, "debugNavigate",
                                                   Q_ARG(QVariant, QVariant(tab)));
    if (!invoked)
        return QJsonObject{{QStringLiteral("ok"), false},
                           {QStringLiteral("error"),
                            QStringLiteral("debugNavigate() not found in Main.qml")}};

    return QJsonObject{{QStringLiteral("ok"), true}, {QStringLiteral("tab"), tab}};
}

QJsonObject DebugBridge::cmdClick(const QJsonObject &request)
{
    if (!m_window)
        return QJsonObject{{QStringLiteral("ok"), false},
                           {QStringLiteral("error"), QStringLiteral("no window")}};

    const QString name = request.value(QStringLiteral("name")).toString();
    if (name.isEmpty())
        return QJsonObject{{QStringLiteral("ok"), false},
                           {QStringLiteral("error"), QStringLiteral("name required")}};

    QQuickItem *item = findByObjectName(m_window->contentItem(), name);
    if (!item)
        return QJsonObject{{QStringLiteral("ok"), false},
                           {QStringLiteral("error"),
                            QStringLiteral("no item with objectName: ") + name}};
    if (!item->isVisible() || !item->isEnabled())
        return QJsonObject{{QStringLiteral("ok"), false},
                           {QStringLiteral("error"),
                            QStringLiteral("item hidden or disabled: ") + name}};

    const QPointF local(item->width() / 2.0, item->height() / 2.0);
    const QPointF scenePos = item->mapToScene(local);
    const QPointF globalPos = m_window->mapToGlobal(scenePos);

    QMouseEvent press(QEvent::MouseButtonPress, scenePos, globalPos,
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent release(QEvent::MouseButtonRelease, scenePos, globalPos,
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(m_window, &press);
    QCoreApplication::sendEvent(m_window, &release);

    QJsonObject reply{{QStringLiteral("ok"), true}};
    reply.insert(QStringLiteral("name"), name);
    reply.insert(QStringLiteral("x"), qRound(scenePos.x()));
    reply.insert(QStringLiteral("y"), qRound(scenePos.y()));
    return reply;
}

// Calls a Q_INVOKABLE (or slot) by name on a registered object, with 0-4 JSON
// args. Added on demand (2026-09-07) when re-triggering Backend::scanTools()
// mid-session was the only way to verify a ToolDetector fix without a full
// app relaunch — the same need will come up for other zero/few-arg actions,
// so this stays generic rather than one method at a time.
QJsonObject DebugBridge::cmdInvoke(const QJsonObject &request) const
{
    const QString objName = request.value(QStringLiteral("object")).toString();
    const QString methodName = request.value(QStringLiteral("method")).toString();

    QObject *target = m_objects.value(objName, nullptr);
    if (!target)
        return QJsonObject{{QStringLiteral("ok"), false},
                           {QStringLiteral("error"), QStringLiteral("unknown object: ") + objName}};
    if (methodName.isEmpty())
        return QJsonObject{{QStringLiteral("ok"), false},
                           {QStringLiteral("error"), QStringLiteral("method required")}};

    QVariantList args;
    const QJsonArray jsonArgs = request.value(QStringLiteral("args")).toArray();
    for (const QJsonValue &value : jsonArgs)
        args.append(value.toVariant());

    if (args.size() > 4)
        return QJsonObject{{QStringLiteral("ok"), false},
                           {QStringLiteral("error"), QStringLiteral("at most 4 args supported")}};

    const QMetaObject *meta = target->metaObject();
    int foundIndex = -1;
    for (int i = 0; i < meta->methodCount(); ++i) {
        const QMetaMethod candidate = meta->method(i);
        if (QString::fromLatin1(candidate.name()) == methodName
            && candidate.parameterCount() == args.size()) {
            foundIndex = i;
            break;
        }
    }
    if (foundIndex < 0)
        return QJsonObject{{QStringLiteral("ok"), false},
                           {QStringLiteral("error"),
                            QStringLiteral("no method '%1' with %2 arg(s) on '%3'")
                                .arg(methodName).arg(args.size()).arg(objName)}};

    const QMetaMethod method = meta->method(foundIndex);

    // JSON numbers always arrive as `double`; a target `int`/`bool`/... param
    // needs an exact QMetaType match for QGenericArgument, so coerce here.
    for (int i = 0; i < args.size(); ++i) {
        const QMetaType targetType = method.parameterMetaType(i);
        if (args[i].metaType() != targetType)
            args[i].convert(targetType);
    }

    QGenericArgument genArgs[4];
    for (int i = 0; i < args.size(); ++i)
        genArgs[i] = QGenericArgument(args[i].typeName(), args[i].constData());

    const bool invoked = method.invoke(target, Qt::DirectConnection,
                                       genArgs[0], genArgs[1], genArgs[2], genArgs[3]);
    if (!invoked)
        return QJsonObject{{QStringLiteral("ok"), false},
                           {QStringLiteral("error"), QStringLiteral("invoke failed")}};

    QJsonObject reply{{QStringLiteral("ok"), true}};
    reply.insert(QStringLiteral("object"), objName);
    reply.insert(QStringLiteral("method"), methodName);
    return reply;
}
