# Doğrulama Ekosistemi — Uygulama Planı

> Bu plan **uygulanmak** için yazıldı. Adımlar sırayla, olduğu gibi izlenmelidir.
> Her adımın sonunda bir **Doğrulama** bloğu var; o blok geçmeden bir sonraki
> adıma geçilmez.

---

## 1. Problem

Şu an UI'ın doğru çalışıp çalışmadığını anlamanın tek yolu, uygulamayı açıp
elle tıklayarak ekran görüntüsü almak ve görüntüyü yorumlamak. Bu:

- **pahalı** — her ekran görüntüsü çok fazla token harcar,
- **yavaş** — doğru ekrana ulaşmak için birden çok tıklama adımı gerekir,
- **güvenilmez** — "buton doğru yerde mi, tablo doğru satırı mı gösteriyor,
  işlemden sonra ekran güncellendi mi" soruları göz kararı cevaplanır.

Buna karşılık **arka plan doğrulaması zaten ucuz**: SQLite kayıtları, UART
çıktısı ve `app_trace.log` doğrudan okunabiliyor.

**Hedef:** UI doğrulamasını da arka plan doğrulaması kadar ucuz, tekrarlanabilir
ve metin tabanlı hale getirmek; ekran görüntüsünü ise *tek komutla* ve
*navigasyon derdi olmadan* alınabilir kılmak.

---

## 2. Çözümün özeti

Üç katman. Her katman tek başına da değerli, sırayla inşa edilecek.

| Katman | Ne yapar | Faz |
|---|---|---|
| **DebugBridge** | Uygulamanın içine gömülü, yalnızca geliştirmede açılan bir komut kanalı (Windows named pipe). Ekran görüntüsü alır, QML sahnesini **metin** olarak döker, QObject property'lerini okur, sekme değiştirir, öğeye tıklar. | Faz 1 |
| **uiprobe.ps1** | Bu kanalı tek satırlık komutlara indiren PowerShell sürücüsü. | Faz 2 |
| **objectName + click** | UI öğelerine kararlı isimler vererek akışların insansız sürülmesini sağlar. | Faz 3 |
| **QML birim testleri** | Uygulamayı hiç açmadan bileşen seviyesinde otomatik doğrulama. | Faz 4 (opsiyonel) |
| **Görsel regresyon** | Ekran görüntüsünü referansla karşılaştırıp tek sayı döndürme. | Faz 5 (gelecek) |

### En kritik fikir: `dump` komutu

`dump`, ekranda görünen her QML öğesini **düz metin JSON** olarak verir:
tip, `objectName`, konum, boyut, görünürlük, etkinlik, varsa `text`.

Yani "Başlat butonu ekranda var mı, aktif mi, nerede duruyor, üstünde ne
yazıyor" sorusu **ekran görüntüsü almadan**, bir görüntünün maliyetinin çok
altında cevaplanabilir hale gelir. Ekran görüntüsü yalnızca gerçekten görsel
bir şey (renk, hizalama, taşma) doğrulanacağında kullanılır.

---

## 3. Uygulayıcı için kurallar

Bunlar tercih değil, kuraldır:

1. **Sırayla git.** Faz 1 → 2 → 3 → 4. Bir fazın Doğrulama bloğu geçmeden
   sonrakine başlama.
2. **Kapsam dışına çıkma.** Bu plan yalnızca listelenen dosyaları oluşturur/
   değiştirir. Başka hiçbir dosyayı "yol üstünde" düzeltme, yeniden
   yapılandırma, yeniden adlandırma yapma.
3. **Şu dosyalara dokunma:** `src/modules/serial/PacketParser.*`,
   `src/modules/debug/GdbRspCodec.*` — dokümantasyonda "değiştirme" olarak
   işaretli.
4. **İki denemede olmuyorsa dur.** Bir adım iki denemede geçmiyorsa, o adımda
   ne yaptığını ve hatanın tam metnini yazıp **dur ve kullanıcıya sor**.
   Mimariyi değiştirerek "etrafından dolaşmaya" çalışma.
5. **Commit atarken `Co-Authored-By` satırı ekleme.** Bu projede yasak.
6. Kod ve kod yorumları **İngilizce**, dokümantasyon **Türkçe** (proje kuralı).
7. Yeni sınıflarda `#pragma once`, pointer-to-member `connect` sözdizimi,
   her sınıf kendi `.h`/`.cpp` çiftinde (proje kuralı, `CLAUDE.md`).

---

## 4. Ortam — her build öncesi bunu oku

Bu makineye özgü iki tuzak var:

**A) Türkçe karakterli yol sorunu.** Proje `D:\Yazılım\stm32-ai-deployer-app`
altında ama "Yazılım" içindeki `ı` harfi `qmlimportscanner`'ı çökertiyor.
**Build daima junction üzerinden yapılır:**

```powershell
$env:PATH = "C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\mingw1310_64\bin;$env:PATH"
Set-Location "C:\dev\stm32-ai-deployer-app"
cmake --build build -j 12
```

Junction yoksa oluştur:
`cmd /c mklink /J "C:\dev\stm32-ai-deployer-app" "D:\Yazılım\stm32-ai-deployer-app"`

**B) Çalışan exe link'i bloklar.** Build'den önce uygulamayı kapat:

```powershell
Stop-Process -Name STM32AiDeployer -Force -ErrorAction SilentlyContinue
```

**Standart build döngüsü** (her kod değişikliğinden sonra):

```powershell
Stop-Process -Name STM32AiDeployer -Force -ErrorAction SilentlyContinue
$env:PATH = "C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\mingw1310_64\bin;$env:PATH"
Set-Location "C:\dev\stm32-ai-deployer-app"
cmake --build build -j 12
```

> CMakeLists.txt'e **yeni dosya eklendiğinde** `cmake --build` yeniden
> yapılandırmayı kendi tetikler; ayrıca `cmake -B build` çalıştırmak gerekmez.
> Gerekirse tam komut Bölüm 12'de.

---

## FAZ 1 — DebugBridge çekirdeği

Hedef: uygulama `--debug-bridge` ile açıldığında bir named pipe dinlesin;
`ping`, `shot`, `dump`, `props`, `navigate`, `quit` komutlarına cevap versin.

Neden named pipe (TCP değil): Windows Güvenlik Duvarı TCP dinleyicisi için
uyarı penceresi açabiliyor; named pipe açmıyor. `QLocalServer` zaten
`Qt6::Network` içinde ve bu modül projeye **halihazırda bağlı** — yeni bağımlılık yok.

Neden `src/core/` altında: bu, uygulama geneline ait bir altyapı.
`src/modules/debug/` klasörü **hedef MCU'nun** debug'ı (GDB/ST-Link) içindir,
buraya konursa karışır.

---

### S1.1 — `src/core/DebugBridge.h` oluştur

```cpp
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

    QQuickWindow *m_window = nullptr;
    QLocalServer *m_server = nullptr;
    QString m_pipeName;
    QHash<QString, QObject *> m_objects;
};
```

---

### S1.2 — `src/core/DebugBridge.cpp` oluştur

Aşağıdaki dosyayı **olduğu gibi** yaz:

```cpp
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
        const QVariantList list = value.toList();
        QJsonArray array;
        for (int i = 0; i < list.size() && i < 20; ++i)
            array.append(QJsonValue::fromVariant(list.at(i)));
        if (list.size() > 20)
            array.append(QStringLiteral("...(%1 more)").arg(list.size() - 20));
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
```

---

### S1.3 — `qml/Main.qml`'e `debugNavigate()` ekle

`Main.qml` içinde `readonly property int edge: 6` satırının **hemen altına**
şunu ekle:

```qml
    // Programmatic tab switch for the dev-only DebugBridge (C++ calls this by
    // name). Mirrors exactly what TopTabBar.onSelected does.
    function debugNavigate(index) {
        tabBar.currentIndex = index
        stack.currentIndex = index
    }
```

> `tabBar.currentIndex`'e atama yapmak zaten dosyada kullanılan bir kalıp
> (bkz. `DashboardScreen.onOpenPipelineRequested`), yeni bir şey icat edilmiyor.

---

### S1.4 — `src/main.cpp`'yi güncelle

**(a) include ekle.** `#include "bridge/Backend.h"` satırının altına:

```cpp
#include "core/DebugBridge.h"
```

**(b) argüman ayrıştırma.** `app.setWindowIcon(QIcon(":/app_icon.png"));`
satırının **hemen altına**:

```cpp
    // Dev-only switches. Both default to off, so a normal run is unchanged.
    //   --no-splash          : show the main window immediately (automation)
    //   --debug-bridge[=name]: open the DebugBridge named pipe
    const QStringList args = QCoreApplication::arguments();
    const bool noSplash = args.contains(QStringLiteral("--no-splash"));
    QString debugPipe;
    for (const QString &arg : args) {
        if (arg == QLatin1String("--debug-bridge"))
            debugPipe = QStringLiteral("stm32aid-debug");
        else if (arg.startsWith(QLatin1String("--debug-bridge=")))
            debugPipe = arg.section(QLatin1Char('='), 1);
    }
```

**(c) köprüyü kur.** Şu satırın altına:

```cpp
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
```

şunu ekle:

```cpp
    // ── Debug bridge (dev-only verification channel) ───────────────────────
    if (!debugPipe.isEmpty()) {
        auto *bridge = new DebugBridge(window, &app);
        bridge->registerObject(QStringLiteral("appState"), appState);
        bridge->registerObject(QStringLiteral("backend"), backend);
        bridge->registerObject(QStringLiteral("factorySim"), factorySim);
        bridge->listen(debugPipe);
    }
```

**(d) splash'i atlanabilir yap.** Mevcut splash bloğunu:

```cpp
    auto *splash = new SplashScreen();
    splash->show();
    app.processEvents();

    QObject::connect(splash, &SplashScreen::done, &app, [splash, window]() {
        ...
    });
    splash->startClosingSequence(3500);
```

şununla değiştir:

```cpp
    if (noSplash) {
        // Automation path: the 3.5s splash would leave the main window hidden,
        // and a hidden window cannot be grabbed.
        if (window) {
            window->show();
            window->raise();
            window->requestActivate();
        }
    } else {
        auto *splash = new SplashScreen();
        splash->show();
        app.processEvents();

        QObject::connect(splash, &SplashScreen::done, &app, [splash, window]() {
            if (window) {
                window->show();
                window->raise();
                window->requestActivate();
            }
            splash->close();
            splash->deleteLater();
        });
        splash->startClosingSequence(3500);
    }
```

---

### S1.5 — `CMakeLists.txt`'e dosyaları ekle

Kök `CMakeLists.txt` içinde `PROJECT_SOURCES` listesinde şu satırların:

```cmake
    src/core/TemplateEngine.h
    src/core/TemplateEngine.cpp
```

**hemen altına** ekle:

```cmake
    src/core/DebugBridge.h
    src/core/DebugBridge.cpp
```

> `Qt6::Network` (QLocalServer'ın geldiği modül) zaten `find_package` ve
> `target_link_libraries` içinde — **ekleme yapma.**

---

### Faz 1 Doğrulama

```powershell
Stop-Process -Name STM32AiDeployer -Force -ErrorAction SilentlyContinue
$env:PATH = "C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\mingw1310_64\bin;$env:PATH"
Set-Location "C:\dev\stm32-ai-deployer-app"
cmake --build build -j 12
```

Build hatasız bitmeli. Sonra:

```powershell
Start-Process -FilePath "C:\dev\stm32-ai-deployer-app\build\STM32AiDeployer.exe" -ArgumentList "--debug-bridge","--no-splash"
Start-Sleep -Seconds 3
Get-Content "C:\dev\stm32-ai-deployer-app\build\app_trace.log" -Tail 5
```

**Beklenen:** log'un son satırlarında
`DebugBridge listening on pipe "stm32aid-debug"` görünmeli.

Görünmüyorsa: uygulama açıldı mı (`Get-Process STM32AiDeployer`), argümanlar
doğru geçti mi kontrol et. İki denemede olmazsa **dur ve sor**.

Uygulamayı açık bırak, Faz 2'de kullanılacak.

---

## FAZ 2 — `uiprobe.ps1` sürücüsü

Hedef: pipe protokolünü tek satırlık komutlara indirmek.

### S2.1 — `tools/uiprobe.ps1` oluştur

Önce klasörü oluştur (`tools/` henüz yok), sonra dosyayı **olduğu gibi** yaz:

```powershell
<#
  uiprobe.ps1 - drives the app's DebugBridge (dev-only named pipe).

  The app must already be running with:
      STM32AiDeployer.exe --debug-bridge --no-splash

  Examples:
      .\tools\uiprobe.ps1 ping
      .\tools\uiprobe.ps1 navigate -Tab 7
      .\tools\uiprobe.ps1 shot -Path C:\temp\watch.png
      .\tools\uiprobe.ps1 dump -Filter Button
      .\tools\uiprobe.ps1 props -Object appState
      .\tools\uiprobe.ps1 click -Name watch.startButton
      .\tools\uiprobe.ps1 log -Lines 30
      .\tools\uiprobe.ps1 quit
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true, Position = 0)]
    [ValidateSet("ping", "shot", "dump", "props", "navigate", "click", "quit", "log")]
    [string]$Command,

    [string]$Path,
    [int]$Tab = -1,
    [string]$Object = "appState",
    [string]$Name,
    [string]$Filter,
    [int]$MaxDepth = 12,
    [int]$Lines = 40,
    [string]$Pipe = "stm32aid-debug",
    [string]$ExeDir = "C:\dev\stm32-ai-deployer-app\build"
)

function Send-BridgeCommand([string]$JsonBody) {
    $client = New-Object System.IO.Pipes.NamedPipeClientStream(".", $Pipe, [System.IO.Pipes.PipeDirection]::InOut)
    try {
        $client.Connect(3000)
    } catch {
        Write-Output "ERROR: cannot connect to pipe '$Pipe'. Is the app running with --debug-bridge?"
        return $null
    }
    $encoding = New-Object System.Text.UTF8Encoding($false)
    $writer = New-Object System.IO.StreamWriter($client, $encoding)
    $writer.AutoFlush = $true
    $reader = New-Object System.IO.StreamReader($client, $encoding)
    $writer.WriteLine($JsonBody)
    $response = $reader.ReadLine()
    $client.Dispose()
    return $response
}

# "log" never touches the pipe - it just tails the app's own trace file.
if ($Command -eq "log") {
    $logPath = Join-Path $ExeDir "app_trace.log"
    if (-not (Test-Path $logPath)) {
        Write-Output "no app_trace.log at $logPath"
        exit 1
    }
    Get-Content $logPath -Tail $Lines
    exit 0
}

$request = @{ cmd = $Command }
switch ($Command) {
    "shot" {
        if ($Path) { $request.path = $Path }
    }
    "dump" {
        $request.maxDepth = $MaxDepth
        if ($Filter) { $request.filter = $Filter }
    }
    "props" {
        $request.object = $Object
        if ($Filter) { $request.filter = $Filter }
    }
    "navigate" {
        if ($Tab -lt 0) { Write-Output "ERROR: -Tab required"; exit 1 }
        $request.tab = $Tab
    }
    "click" {
        if (-not $Name) { Write-Output "ERROR: -Name required"; exit 1 }
        $request.name = $Name
    }
}

$response = Send-BridgeCommand ($request | ConvertTo-Json -Compress)
if ($null -eq $response) { exit 1 }
Write-Output $response
```

### S2.2 — Sekme indeks tablosunu not et

`navigate -Tab N` için indeksler (`Main.qml`'deki `TopTabBar.tabs` sırası):

| N | Ekran |
|---|---|
| 0 | Dashboard |
| 1 | Kartlar |
| 2 | Flash |
| 3 | Monitör |
| 4 | Benchmark |
| 5 | Analiz |
| 6 | Register |
| 7 | İzleyici (Watch) |

---

### Faz 2 Doğrulama

Uygulama hâlâ `--debug-bridge --no-splash` ile açık olmalı.

```powershell
Set-Location "C:\dev\stm32-ai-deployer-app"
.\tools\uiprobe.ps1 ping
.\tools\uiprobe.ps1 navigate -Tab 7
.\tools\uiprobe.ps1 shot -Path C:\temp\uiprobe_test.png
.\tools\uiprobe.ps1 dump -Filter Tab
.\tools\uiprobe.ps1 props -Object appState -Filter board
```

**Beklenen:**
- `ping` → `{"ok":true,"app":"STM32 AI Deployer",...,"windowVisible":true,...}`
- `navigate` → `{"ok":true,"tab":7}`
- `shot` → `{"ok":true,"path":"C:/temp/uiprobe_test.png","w":1500,"h":940}`
  (dosya gerçekten oluşmuş olmalı: `Test-Path C:\temp\uiprobe_test.png`)
- `dump` → `{"ok":true,"count":N,"items":[...]}` — N > 0
- `props` → `appState`'in board ile ilgili property'leri

Hepsi geçtiyse **Faz 1+2 tamamdır ve asıl kazanç burada elde edilmiştir.**
Bu noktada `quit` ile uygulamayı kapat, commit at (bkz. Bölüm 11).

---

## FAZ 3 — `objectName` konvansiyonu + tıklama

Şu an QML'de **hiç** `objectName` yok (doğrulandı). `click` komutunun ve
`dump`'ın işe yaraması için kararlı isimler gerekli.

### S3.1 — Konvansiyon

```
objectName: "<ekran>.<öğe>"
```

Örnekler: `"tabbar.tab7"`, `"watch.startButton"`, `"monitor.terminal"`,
`"analysis.exportCsvButton"`.

Kurallar:
- Yalnızca **etkileşimli** öğeler (Button, TextField, ComboBox, CheckBox) ve
  **doğrulanacak** kapsayıcılar (tablo, terminal, grafik) isim alır. Her
  Rectangle'a isim verilmez.
- Mevcut `id:` değerleri **değiştirilmez**, sadece `objectName` eklenir.
- İsimler İngilizce ve camelCase.

### S3.2 — İlk uygulama kapsamı

Bu fazda **yalnızca** şunlara `objectName` eklenir:

1. `qml/components/TopTabBar.qml` — sekme butonlarına
   `objectName: "tabbar.tab" + index`
2. `qml/components/watch/WatchToolbar.qml` — bağlan / ELF yükle / sembol ekle /
   başlat / durdur butonlarına `watch.*` isimleri
3. `qml/screens/WatchScreen.qml` — ana tablo, grafik ve kural akışı
   kapsayıcılarına `watch.itemTable`, `watch.plot`, `watch.ruleFeed`

> Neden bunlar: bekleyen iş listesindeki (bkz. `TODO.md`) **uçtan uca izleyici
> senaryosu** tam olarak bu ekranda koşulacak. Diğer ekranlara isim eklemek
> ihtiyaç doğdukça, aynı konvansiyonla yapılır — hepsini şimdi eklemeye
> çalışma.

Uygulayıcı notu: dosyaları aç, ilgili öğeleri bul, `objectName:` satırını
ekle. Öğe adları farklı çıkarsa **var olanı kullan**, dosyayı yeniden
yapılandırma.

---

### Faz 3 Doğrulama

```powershell
# yeniden derle + başlat (Bölüm 4'teki döngü)
.\tools\uiprobe.ps1 dump -Filter "watch."
.\tools\uiprobe.ps1 click -Name "tabbar.tab7"
.\tools\uiprobe.ps1 props -Object backend -Filter watch
```

**Beklenen:** `dump` isimlendirilmiş öğeleri `"name":"watch.…"` alanıyla
listeler; `click` `{"ok":true,...}` döner ve ardından alınan `dump`/`shot`
İzleyici ekranını gösterir.

---

## FAZ 4 — QML birim testleri (opsiyonel, düşük öncelik)

Hedef: uygulamayı hiç açmadan bileşen mantığını doğrulamak.
`Qt6QuickTest` kurulu (doğrulandı).

**Bu faz risklidir** (QML modülünün test tarafından import edilmesi
fiddly olabilir). Kural 4 burada özellikle geçerli: **iki denemede olmuyorsa
dur ve sor.** Uygulamanın QML modül yapısını değiştirmeye çalışma.

### S4.1 — `tests/qml/tst_smoke.qml`

```qml
import QtQuick
import QtTest

// Smoke test: proves the QML test harness itself runs. Real component tests
// get added next to it once this passes.
TestCase {
    name: "Smoke"

    function test_arithmetic() {
        compare(1 + 1, 2)
    }
}
```

### S4.2 — `tests/qml/main.cpp`

```cpp
#include <QtQuickTest>

QUICK_TEST_MAIN(qmltests)
```

### S4.3 — `tests/CMakeLists.txt` sonuna ekle

```cmake
# QML-side tests run in their own executable: the pure C++ suite above must
# stay free of a QML engine / event loop dependency.
find_package(Qt6 REQUIRED COMPONENTS QuickTest Qml)

qt_add_executable(STM32AiDeployerQmlTests qml/main.cpp)
set_target_properties(STM32AiDeployerQmlTests PROPERTIES WIN32_EXECUTABLE FALSE)
target_compile_definitions(STM32AiDeployerQmlTests PRIVATE
    QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml"
)
target_link_libraries(STM32AiDeployerQmlTests PRIVATE Qt6::QuickTest Qt6::Qml)
add_test(NAME STM32AiDeployerQmlTests COMMAND STM32AiDeployerQmlTests)
```

### Faz 4 Doğrulama

```powershell
Set-Location "C:\dev\stm32-ai-deployer-app"
cmake --build build -j 12
ctest --test-dir build --output-on-failure
```

**Beklenen:** hem `STM32AiDeployerTests` hem `STM32AiDeployerQmlTests` PASS.

---

## FAZ 5 — Görsel regresyon (gelecek, şimdi yapma)

Fikir: `shot` ile alınan görüntüyü `docs/ui_baseline/<ekran>.png` ile
karşılaştırıp **farklı piksel yüzdesini tek sayı olarak** döndürmek. Böylece
"UI beklenmedik şekilde değişti mi" sorusu görüntü okumadan cevaplanır.

Uygulanması: `DebugBridge`'e `{"cmd":"diff","baseline":"..."}` komutu; iki
`QImage`'ı piksel piksel karşılaştırıp yüzde döndürür. Font/antialiasing
farkları için eşik (ör. %0.5) gerekir.

**Bu faz bu planın kapsamında değildir.** İhtiyaç netleştiğinde ayrı ele alınır.

---

## 11. Kapanış işleri (Faz 3'ten sonra)

1. **`CLAUDE.md`** → "Klasör Yapısı" bölümüne `tools/` ve
   `src/core/DebugBridge.*` satırlarını ekle; "Sık Kullanılan Komutlar"
   bölümüne kısa bir "Doğrulama / UI kontrolü" alt başlığı ekleyip
   `uiprobe.ps1` kullanımını 3-4 satırda göster.
2. **`TODO.md`** → "Şu an nerede kaldık" bölümüne doğrulama ekosisteminin
   kurulduğunu ve nasıl kullanıldığını 2-3 satırla yaz.
3. **Commit.** Ayrı ayrı, anlamlı commit'ler:
   - `feat(debug): DebugBridge - dev-only UI verification channel`
   - `feat(tools): uiprobe.ps1 driver for DebugBridge`
   - `feat(qml): objectName convention for watch screen + tab bar`
   - `docs: verification ecosystem usage`

   **`Co-Authored-By` satırı ekleme.** Dal: `feature/register-inspector`
   (bu projede main'e merge edilmez, bkz. `TODO.md`).

---

## 12. Hangi doğrulama için hangi araç

Bu tablo ekosistemin özüdür — bir sonraki oturumda "nasıl kontrol edeyim"
sorusunun cevabı burada.

| Soru | Araç | Maliyet |
|---|---|---|
| Arka planda işlem doğru mu oldu? | SQLite / UART / `props` | çok ucuz |
| Ekranda hangi öğeler var, aktif mi, ne yazıyor? | `dump` (gerekirse `-Filter`) | ucuz |
| Uygulama ne yaptı, hata verdi mi? | `log` (`app_trace.log`) | ucuz |
| Bir akış çalışıyor mu (tıkla → sonuç)? | `click` + `props`/`dump` | ucuz |
| Görsel/yerleşim/renk doğru mu? | `shot` + görüntüyü oku | **pahalı — son çare** |
| Bileşen mantığı doğru mu (uygulama açmadan)? | `ctest` (Faz 4) | ucuz |

**Kural:** önce `dump`/`props`/`log`. `shot` yalnızca gerçekten *görsel* bir
şey doğrulanacaksa.

---

## 13. Tam yeniden yapılandırma (gerekirse)

`cmake --build` yetmezse (ör. CMakeCache bozulduysa):

```powershell
Stop-Process -Name STM32AiDeployer -Force -ErrorAction SilentlyContinue
$env:PATH = "C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\mingw1310_64\bin;$env:PATH"
Set-Location "C:\dev\stm32-ai-deployer-app"
cmake -B build -S . -DCMAKE_PREFIX_PATH="C:/Qt/6.11.0/mingw_64" -DCMAKE_BUILD_TYPE=Release -G "MinGW Makefiles"
cmake --build build -j 12
```
