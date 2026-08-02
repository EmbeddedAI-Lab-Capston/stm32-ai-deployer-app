# Değişken İzleyici (Variable Watcher) — Uygulama Planı

> **Durum:** Onay bekliyor · **Dal:** `feature/register-inspector` (main'e merge yok)
> **Faz A (fizibilite):** ✅ tamamlandı — ölçümler bu dokümanda veri olarak kabul edilir, tekrar ölçülmez
> **Yürütücü:** Sonnet · Bu doküman karar bırakmaz; her adım dosya/tip/kriter seviyesinde tanımlıdır
>
> Bağlı doküman: [`CLAUDE.md`](../CLAUDE.md) · [`docs/PROJECT.md`](PROJECT.md) ·
> [`docs/register_inspector_plan.md`](register_inspector_plan.md)

---

## 0. Bir bakışta

STM Studio benzeri, **çalışan hedeften** (reset yok, halt yok) periyodik bellek
okumasıyla değişken değerlerini canlı zaman serisi olarak izleyen bir ekran.
Okuma yolu: `ST-LINK_gdbserver.exe` + GDB Remote Serial Protocol (RSP) üzerinden
`m<addr>,<len>` paketleri. Sembol yolu: `arm-none-eabi-nm -S --defined-only <elf>`.

Yan fayda: aynı okuma katmanı `IRegisterReader`'ı gerçekleyerek mevcut Register
Inspector snapshot'larını **9 Hz → ~500 Hz** bandına taşır.

Farklılaştırıcılar (bu araca özgü, STM Studio'da olmayan): deploy edilen modelden
**AI-farkındalıklı otomatik izleme listesi**, **canlı bellek watermark + sızıntı
trendi**, **model A/B kaynak profili karşılaştırması**, **UART metrikleri ve
register olaylarıyla ortak zaman ekseni**.

---

## 1. Faz A sonuçlarının plana yansıyan halleri (yeniden ölçülmeyecek)

| Bulgu | Plandaki karşılığı |
|---|---|
| `-g -e -p <port> -d -i <SN> -cp <dir>` çalışıyor; `-k` / `--halt` reset atıyor | `GdbServerProcess` argümanları sabit; `-k`/`--halt` kod düzeyinde yasak |
| `qSupported` → `QStartNoAckMode` → `QNonStop:1` → `vCont;c` sırası zorunlu | `DebugLinkWorker::handshake()` bu sırayı birebir uygular |
| Sonrasında `DHCSR = 0x01010000` → `S_HALT`(17)=0, `S_RETIRE_ST`(24)=1 | Handshake sonrası + **4 Hz** DHCSR sağlık okuması; `S_RETIRE_ST` birincil canlılık, `S_HALT` durdurur, `S_RESET_ST`(25) reset olayı düşer (§4.5) |
| `PacketSize=4000` (hex) = 16384 bayt | Tek okuma tavanı `kMaxReadBytes = 4096` (hex cevap 2× olduğu için güvenli marj) |
| t ≈ 0.31 ms sabit + boyut/550 KB/s | `WatchPlanBuilder` **round-trip sayısını** minimize eder; boyut ikincil |
| CLI, `DEV_CONNECT_ERR` durumunda bile exit code 0 döner | Çıkış koduna güvenilmez; `HexDumpParser::errorMarkers` deseni korunur ve genişletilir |
| `nm` bazı satırlarda boyut alanı vermiyor (3 alan) | `NmSymbolParser` 3 ve 4 alanlı satırların ikisini de kaldırır |
| `A` tipi sembolün "adresi" aslında **değeridir** | `Symbol::addressIsValue = true`; watch listesine adres olarak eklenmesi engellenir |
| `_sstack` sadece N6'da var | Fallback `_estack - _Min_Stack_Size`, **veri olarak** `watch_presets.json`'da |
| ELF her kart için kendi linker script'iyle derlenir | RAM tablosu C++'a **hardcode edilmez**; semboller + `qXfer:memory-map:read` kullanılır |
| `analysis_records` 1 sn çözünürlük | Ham seri dosyaya; DB'ye sadece özet profil (`kind="watch_profile"`) |
| `ai_runner.c`'deki ilginç değerler fonksiyon-yerel | Faz 5'te static'e terfi + stack boyama (firmware değişikliği) |

---

## 2. Mimari — üç katman

```
┌─ src/modules/debug/  (YENİ, paylaşılan düşük seviye SWD bağlantısı)
│   GdbRspCodec        saf: çerçeveleme, checksum, RLE, hex, giden paket beyaz listesi
│   GdbServerProcess   ana thread: QProcess ömrü, port seçimi, hata işareti taraması
│   DebugLinkWorker    worker thread: QTcpSocket + handshake + istek kuyruğu + örnekleyici
│   DebugLink          ana thread cephe (SerialManager/SerialWorker deseninin aynısı)
│
├─ src/modules/registers/
│   GdbServerReader    IRegisterReader'ın 2. gerçeklemesi (CliRegisterReader silinmez)
│
└─ src/modules/watcher/  (YENİ, izleyici mantığı)
    NmSymbolParser · ElfSymbolSource · ElfTargetMatcher · ValueCodec · WatchPlanBuilder
    TraceBuffer · TraceEventLog · WatchSampler · VariableWatcher
    TimeSeriesRuleEngine · WatchPresetMatcher · TraceRecorder · TracePlayer · WatchProfile
```

`Backend` yine **tek cephe**: QML `DebugLink`'i veya `VariableWatcher`'ı görmez.

### 2.1 Neden yeni bir `debug/` modülü

Hem Register Inspector (snapshot) hem Değişken İzleyici (sürekli örnekleme) aynı
tek ST-Link bağlantısını kullanır. İkisi ayrı bağlantı açarsa çakışırlar. Tek
`DebugLink` örneği `main.cpp`'de yaratılır, hem `RegisterInspector`'a hem
`VariableWatcher`'a verilir.

### 2.2 Thread modeli

| Nesne | Thread | Gerekçe |
|---|---|---|
| `GdbServerProcess` (QProcess) | ana | QProcess zaten async/sinyal tabanlı, bloklamıyor |
| `DebugLinkWorker` (QTcpSocket + örnekleme) | worker | 3000 Hz döngü ana thread'de olamaz |
| `TraceBuffer`, `WatchProfile`, kural motoru | ana | Worker ≤30 Hz'de **toplu batch** yayar, sinyal başına örnek yok |

**Kritik kural:** worker örnek başına sinyal yaymaz. `QElapsedTimer` kapısıyla
en fazla 30 Hz'de veya 512 örnek biriktiğinde `samplesReady(WatchSampleBatch)`
yayar. 3000 Hz × 8 değişken → emit başına ~100 örnek, saniyede 30 sinyal.

---

## 3. Faz planı (yürütme sırası)

Her faz kendi commit'ini alır, `feature/register-inspector` dalında kalır.
**Commit mesajlarında AI attribution / co-author satırı YASAK.**

| Faz | Başlık | Donanım gerekli mi |
|---|---|---|
| 1 | Debug link altyapısı (RSP + gdbserver süreç yönetimi) | Kısmen (birim testler donanımsız) |
| 2 | `GdbServerReader` — Register Inspector hızlanması | Evet (H7) |
| 3 | Sembol katmanı (ELF → nm → sembol tablosu) | Hayır |
| 4 | Örnekleyici + ring buffer + Backend + tablo UI | Evet (H7) |
| 5 | Firmware: static terfi + stack boyama | Evet (H7, flash) |
| 6 | Grafik + zaman ekseni + olay korelasyonu | Evet (H7) |
| 7 | Kayıt / oynatma / dışa aktarma / güvenli mod | Kısmen |
| 8 | `TimeSeriesRuleEngine` + watermark + AI preset + profil karşılaştırma | Kısmen |
| 9 | Dokümantasyon + CLAUDE.md + ertelenmiş kart doğrulamaları | Hayır |

Detaylar aşağıda faz faz.

---

## 4. Faz 1 — Debug link altyapısı

### 4.1 Oluşacak dosyalar

| Dosya | İçerik |
|---|---|
| `src/modules/debug/DebugLinkTypes.h` | `MemoryRequest`, `MemoryReply`, `WatchSampleBatch`, `DebugLinkState` |
| `src/modules/debug/GdbRspCodec.h/.cpp` | **Saf** (QObject değil) — çerçeveleme/checksum/RLE/hex/beyaz liste |
| `src/modules/debug/GdbServerProcess.h/.cpp` | QProcess ömrü, boş port seçimi, hazırlık tespiti, hata işaretleri |
| `src/modules/debug/DebugLinkWorker.h/.cpp` | QTcpSocket, handshake, istek kuyruğu, örnekleme zamanlayıcısı |
| `src/modules/debug/DebugLink.h/.cpp` | Ana thread cephe: süreç + thread + worker sahibi |

### 4.2 `DebugLinkTypes.h`

```cpp
#pragma once
#include <QByteArray>
#include <QMetaType>
#include <QString>
#include <QVector>

enum class DebugLinkState {
    Closed, StartingServer, Connecting, Handshaking, Open, Failed
};

// One contiguous read request (worker executes them strictly serially).
struct MemoryRequest { quint32 id = 0; quint64 addr = 0; quint32 len = 0; };

struct MemoryReply {
    quint32    id = 0;
    quint64    addr = 0;
    QByteArray data;        // raw bytes, memory order
    bool       ok = false;
    QString    error;       // e.g. "E01" or a transport message
};

// Column-major sample batch: series[i][k] is item i's value at times[k].
// One allocation per item per emit instead of one per sample.
//
// ── Timestamp contract (binding — see plan Bolum 4.2) ────────────────────
// times[k]: taken from the session monotonic clock immediately BEFORE the
//           FIRST 'm' packet of that sample is written to the socket.
// skewUs:   the span that starts at times[k] and ends when the LAST reply of
//           that same sample has been fully parsed, in microseconds.
// Meaning:  "these values were read within the window [t, t + skewUs/1e6]."
//           For single-round-trip samples skewUs is simply that read's RTT.
// This is why a sample with many read blocks is less simultaneous, not just
// slower — the UI reports skewUs so the user can see it.
struct WatchSampleBatch {
    QVector<double>          times;      // see timestamp contract above
    QVector<QVector<double>> series;     // size == watch item count
    quint32 dropped = 0;                 // missed deadlines since last emit
    double  actualRateHz = 0.0;
    double  rttMsAvg = 0.0;
    double  skewUs = 0.0;                // t -> last reply parsed, per sample
    bool    coreRunning = true;          // DHCSR S_RETIRE_ST==1 && S_HALT==0
    bool    targetReset = false;         // DHCSR S_RESET_ST seen since last check
};
Q_DECLARE_METATYPE(MemoryReply)
Q_DECLARE_METATYPE(WatchSampleBatch)
```

> **Zaman damgası tanımı bağlayıcıdır.** Bir örnek birden fazla round-trip
> gerektirdiğinde `t` **ilk** `m` paketinin yazılma anıdır, `skewUs` ise son
> cevabın ayrıştırılmasına kadar geçen süredir. `uwTick` eğimiyle zaman tabanı
> doğrulayan Faz 4/6 adımları bu tanıma dayanır; başka bir yorum (ör. batch
> ortası veya son cevap anı) eğimde sistematik kayma üretir.

### 4.3 `GdbRspCodec` — saf, birim test edilebilir

```cpp
class GdbRspCodec
{
public:
    enum class Extract { NeedMore, Packet, Notification, Ack, Nak, Garbage };

    static quint8     checksum(const QByteArray &payload);
    static QByteArray frame(const QByteArray &payload);      // "$" + payload + "#" + 2 hex

    // Consumes one unit from `buffer`. '%' non-stop notifications are reported
    // separately so the request/reply matcher never confuses them with replies.
    static Extract    extract(QByteArray &buffer, QByteArray &payloadOut);

    // RSP run-length: "<c>*<n>" => c repeated (ASCII(n) - 29) extra times.
    // "0* " (n=' '=32) => 32-29 = 3 extra => "0000". Repeat chars '#' and '$'
    // never occur. Returns false in *ok on malformed input.
    static QByteArray expandRunLength(const QByteArray &payload, bool *ok);
    static QByteArray unescape(const QByteArray &payload);   // 0x7d escape

    static QByteArray hexDecode(const QByteArray &hex, bool *ok);
    static QByteArray memoryReadPacket(quint64 addr, quint32 len); // "m<hex>,<hex>"
    static bool       isErrorReply(const QByteArray &payload, QString *codeOut); // "E01" | "E 01"

    // ── Observer guarantee ───────────────────────────────────────────────
    // The ONLY packets this application is ever allowed to transmit. Anything
    // that could halt, reset, write, or breakpoint the target is rejected here,
    // not by convention. See CLAUDE.md "Degisken Izleyici" decision record.
    static bool isAllowedOutgoing(const QByteArray &payload);
};
```

`isAllowedOutgoing` beyaz listesi (tam liste, başka hiçbir şey geçmez):
`qSupported:*`, `QStartNoAckMode`, `QNonStop:1`, `vCont;c`, `m<...>`,
`qXfer:memory-map:read:*`, `qXfer:features:read:*`, `D`.
Reddedilenler arasında açıkça: `?`, `Z*`, `z*`, `M*`, `X*`, `G*`, `P*`,
`vCont;t`, `vCont;s`, `r`, `R*`, `k`, `\x03` (Ctrl-C).

`DebugLinkWorker` gönderim yolunda:

```cpp
if (!GdbRspCodec::isAllowedOutgoing(payload)) {
    qWarning() << "BLOCKED outgoing RSP packet:" << payload;   // never sent
    return;
}
```

### 4.4 `GdbServerProcess`

```cpp
class GdbServerProcess : public QObject
{
    Q_OBJECT
public:
    void setServerPath(const QString &path);          // ST-LINK_gdbserver.exe
    void setCubeProgrammerBinDir(const QString &dir); // -cp argument
    void setStlinkSerial(const QString &sn);          // -i ; empty = let server pick
    void setPreferredPort(quint16 port);              // 0 = auto

    void start();     // emits ready(port) or failed(msg)
    void stop();      // graceful: caller detaches first, then terminate() -> kill()
    bool isRunning() const;
    quint16 port() const;
signals:
    void ready(quint16 port);
    void failed(const QString &message);
    void logLine(const QString &line);   // surfaced in the UI log pane
    void crashed(const QString &message);
};
```

Uygulama kuralları:

1. **Port seçimi:** `QTcpServer` ile 0 portuna bind → `serverPort()` oku → kapat →
   o portu gdbserver'a ver. Bağlanma başarısız olursa (yarış) 3 kez tekrar dene.
2. **Argümanlar (bu sırayla, başka argüman eklenmez):**
   `-g -e -p <port> -d [-i <SN>] -cp <cubeprogrammer_bin_dir>`
   `-k` ve `--halt` `start()` içinde `Q_ASSERT` + runtime kontrolüyle yasaklı.
3. **Hazırlık tespiti:** stdout metnine güvenme. 100 ms aralıklarla, 10 s
   boyunca TCP bağlanmayı dene; ilk başarılı bağlantı = hazır. Paralelde
   stdout/stderr'de hata işaretlerini tara: `ST-LINK error`, `Error in
   initializing`, `Cannot`, `DEV_CONNECT_ERR`, `already in use`, `No ST-LINK
   detected`. İşaret görülürse beklemeyi kes, `failed()` yay.
4. **Kalıntı süreç:** başlatma "address in use" ile başarısız olursa başka port
   dene. Bağlanma "ST-LINK meşgul" ile başarısız olursa kullanıcıya
   *"Kalıntı bir ST-LINK_gdbserver süreci ST-Link'i tutuyor olabilir"* mesajı
   göster. **Otomatik `taskkill` YAPMA** — yalnızca kullanıcının açıkça
   onayladığı bir "Kalıntı süreci sonlandır" butonu (Faz 9, opsiyonel).
5. **Kapatma sırası:** önce worker `D` (detach) gönderir → soket kapanır →
   `terminate()` → 2 s sonra hâlâ çalışıyorsa `kill()`. `main.cpp`'de
   `qApp->aboutToQuit` bağlantısıyla garanti temizlik.

### 4.5 `DebugLinkWorker` handshake (birebir sıra)

```
1. TCP connect 127.0.0.1:<port>
2. -> $qSupported:swbreak+;hwbreak+#..     <- reply; PacketSize=<hex> ayrıştır
   (bu aşamada ack modu AÇIK: her alınan pakete '+' gönder)
3. -> $QStartNoAckMode#..                  <- '+' sonra "OK"
   "OK" alındıktan SONRA ack gönderme/bekleme TAMAMEN kapanır (m_ackMode=false)
4. -> $QNonStop:1#..                       <- "OK"
5. -> $vCont;c#..                          <- "OK"
6. DOĞRULAMA: m E000EDF0,4 -> DHCSR (asagidaki bit tablosu)
```

`%Stop:...` bildirimleri (non-stop modu) `Extract::Notification` olarak ayrılır
ve **yok sayılır** — asla bir `m` cevabıyla eşleştirilmez.

#### DHCSR (0xE000EDF0) — tek okumada üç bilgi

Bu üç bit **aynı 4 baytlık okumadan** gelir; ek round-trip maliyeti **yoktur**.

| Bit | Ad | Anlamı ve tepki |
|---|---|---|
| 17 | `S_HALT` | 1 → çekirdek **DURDU**. Örnekleme durur, rozet kırmızı, `failed("Hedef durmus durumda")`. |
| 24 | `S_RETIRE_ST` | 1 → son DHCSR okumasından beri **en az bir komut emekliye ayrıldı**. Mimari düzeyde pozitif çalışma kanıtı; DWT'nin açık olmasını **gerektirmez**. **Birincil canlılık göstergesi budur.** |
| 25 | `S_RESET_ST` | 1 → son okumadan beri çekirdek **RESET edildi**. Zaman eksenine `kind="targetReset"`, `severity="warning"` olayı düşülür; örnekleme **DURMAZ** (adresler geçerli kalır) ama kullanıcı uyarılır, çünkü değerler süreksiz sıçrar. |

Bu bitler **sticky / okununca temizlenir**: her okuma "son kontrolden beri"
semantiği verir. Handshake'teki 6. adımda `S_HALT == 0` **ve**
`S_RETIRE_ST == 1` beklenir.

**Faz A referans değeri (ölçüldü, veri olarak kabul edilir):**
handshake sonrası `DHCSR = 0x01010000` → `S_REGRDY(16)=1`, `S_RETIRE_ST(24)=1`,
`S_HALT(17)=0`. Faz 1 canlı doğrulaması bu değerle karşılaştırılır.

> **DWT_CYCCNT birincil canlılık göstergesi DEĞİLDİR.** DWT'nin etkin olması
> firmware'e bağlıdır (`DEMCR.TRCENA` + `DWT_CTRL` bit0). Bizim
> `ai_runner.c` şablonumuz açıyor, ama başkasının firmware'inde kapalı olabilir
> — o durumda sayaç donuk görünür ve araç yanlışlıkla "hedef durdu" derdi.
> `DWT_CYCCNT` yalnızca **isteğe bağlı bir izleme kalemi** olarak kalır.

#### Sağlık kontrolü sıklığı (sayıyla)

- DHCSR sağlık okuması **4 Hz**'de, **kendi `MemoryRequest`'i** olarak yapılır.
  `0xE000EDF0` çekirdek özel bölgededir ve RAM planından uzaktır; `WatchPlanBuilder`
  birleştirme eşiği (256 bayt) bunu asla birleştiremez — ayrı round-trip zorunludur.
- **Maliyet hesabı:** 4 × 0.31 ms = **~1.24 ms/s**, yani örnekleme bandının
  **%0.2'sinden azı**. Örnek başına kontrol yapılsaydı 3000 Hz'de bant **yarıya**
  düşerdi; bu yüzden sabit 4 Hz seçilmiştir.
- Örnekleme durduğunda sağlık kontrolü de durur (boşta gdbserver'a trafik yok).

### 4.6 `DebugLink` (ana thread cephesi)

```cpp
class DebugLink : public QObject
{
    Q_OBJECT
public:
    void setPaths(const QString &gdbServerPath, const QString &cubeProgrammerBinDir);
    void setStlinkSerial(const QString &sn);

    DebugLinkState state() const;
    QString  lastError() const;
    bool     isOpen() const;
    quint32  maxReadBytes() const;      // min(4096, (PacketSize-8)/2)
    bool     coreRunning() const;

    // ── Reference-counted session ────────────────────────────────────────
    // The counter going 0->1 opens the link; 1->0 closes it. Every retain()
    // MUST be matched by exactly one release() — error and cancel paths
    // included. There is deliberately no public open()/close(): two owners
    // (Register Inspector + Variable Watcher) share one ST-Link, and a raw
    // close() from one of them would yank the link out from under the other.
    void retain();          // async; emits opened() immediately if already open
    void release();
    int  refCount() const;

    // Emergency teardown that IGNORES the reference count. The only caller is
    // qApp::aboutToQuit — process cleanup at exit must be unconditional, or a
    // leaked retain() would strand a gdbserver holding the ST-Link.
    void shutdownNow();

    // One-shot batch read (Register Inspector path). Replies arrive in order.
    void readRanges(quint32 batchId, const QVector<MemoryRequest> &requests);

    // Continuous sampling (Variable Watcher path).
    void startSampling(const QVector<MemoryRequest> &plan, int targetRateHz); // 0 = max
    void stopSampling();
    bool isSampling() const;
signals:
    void stateChanged();
    void opened();
    void closed();
    void failed(const QString &message);
    void logLine(const QString &line);
    void rangesRead(quint32 batchId, const QVector<MemoryReply> &replies);
    void rawSamplesReady(const QVector<MemoryReply> &replies, double t, double skewUs);
    void samplingStats(double actualRateHz, double rttMsAvg, quint32 dropped);
    void coreHalted();     // S_HALT set -> sampling stopped
    void coreReset();      // S_RESET_ST set -> event logged, sampling continues
};
```

#### `retain()` / `release()` sözleşmesi (bağlayıcı, ikircikli değil)

1. **`open()` / `close()` public arayüzden kalkar.** Tek erişim yolu
   `retain()` / `release()`'tir.
   `Backend::openWatchLink()` → `retain()`, `Backend::closeWatchLink()` → `release()`.
2. `retain()` sayacı **0 → 1** yaparsa asenkron açılış başlar. **`retain()`
   açılışı beklemez.** Çağıran taraf işini `opened()` sinyalini bekleyerek yapar.
3. Link zaten açıkken `retain()` çağrılırsa `opened()` **derhal** (bir sonraki
   event loop turunda) yayılır — çağıran iki farklı kod yolu yazmak zorunda kalmaz.
4. **Açılış başarısız olursa sayaç geri alınır** (`refCount()` çağrı öncesine
   döner) ve `failed(msg)` yayılır. Bu durumda **çağıran `release()` çağırmaz** —
   başarısız `retain()` sayaç tüketmemiş sayılır. Her yerde bu kural geçerlidir;
   "her ihtimale karşı release()" **yanlıştır**, sayacı negatife düşürür.
5. `release()` sayacı **1 → 0** yaparsa kapanış dizisi işler: `D` (detach) →
   soket kapat → gdbserver `terminate()` → 2 s sonra `kill()`.
6. `refCount() < 0` bir programlama hatasıdır; `Q_ASSERT` + `qWarning` ile
   yakalanır, sessizce sıfıra kırpılmaz.

> **Not:** `rawSamplesReady` ham blokları taşır; bunları `WatchItem` değerlerine
> çeviren `WatchSampler` Faz 4'te gelir. Faz 1'de `DebugLink` yalnız
> `readRanges` + handshake seviyesinde tamamlanır.

### 4.7 Değişecek dosyalar

- **`src/core/ToolDetector.h/.cpp`** — üç yeni statik tespit:
  ```cpp
  static QString detectGdbServer();            // ST-LINK_gdbserver.exe
  static QString detectArmNm();                // arm-none-eabi-nm.exe
  static QString detectCubeProgrammerBinDir(); // -cp icin dizin
  ```
  Arama sırası mevcut desenle aynı: `AppSettings` → PATH → CubeIDE plugin
  taraması → sabit yollar. **CubeIDE sürümü glob'la bulunur, hardcode edilmez**
  (kurulumda 2.1.1 var, CLAUDE.md 1.19.0 diyor — ikisi de olabilmeli). Mevcut
  `findInCubeIDE()` yardımcı fonksiyonu yeniden kullanılır. Plugin dizin
  desenleri:
  `com.st.stm32cube.ide.mcu.externaltools.stlink-gdb-server.win32_*/tools/bin/`
  `com.st.stm32cube.ide.mcu.externaltools.cubeprogrammer.win32_*/tools/bin/`
  `com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.*.win32_*/tools/bin/`
  `detectAll()` çıktısına üç yeni `ToolInfo` eklenir.

- **`src/core/AppSettings.h/.cpp`** — yeni anahtarlar:
  ```
  tools/gdbserver_path
  tools/arm_nm_path
  tools/cubeprogrammer_bin_dir
  watch/gdb_port                 (0 = otomatik)
  ```

- **`CMakeLists.txt`** — yeni kaynaklar `PROJECT_SOURCES`'a; `Qt6::Network`
  zaten bağlı (QTcpSocket için yeterli).

- **`src/main.cpp`** — `DebugLink` örneği yaratılır ve hem `RegisterInspector`'a
  hem `VariableWatcher`'a verilir. `aboutToQuit` bağlantısı **referans sayacını
  atlayan** bir acil kapatma yolu çağırır (`DebugLink::shutdownNow()`): süreç
  temizliği çıkışta koşulsuz olmalıdır, kalan sahiplere bakılmaz.

- **`svd/boards.json`** — her kayda `debug` bloğu (aile-özel bilgi veri olarak):
  ```json
  "debug": {
    "gdb": {
      "support": "stable",
      "notes": "",
      "verifiedOn": "2026-08-02 NUCLEO-H723ZG"
    }
  }
  ```
  N6 için `"support": "experimental"`, `"notes": "TrustZone/RIF: guvenli RAM
  bolgeleri non-secure DAP'tan okunamaz; okuma hatasi olarak yuzeye cikar,
  sessiz sifir DONMEZ. LRUN boot sonrasi -g attach dogrulanmadi."`
  F4 için `"support": "stable"`, `"verifiedOn": ""` (kart gelince doldurulur).
  `SvdCatalog::SvdBoardMapping`'e `QString gdbSupport; QString gdbNotes;`
  alanları eklenir.

### 4.8 Doğrulama — Faz 1

**Donanımsız (birim test, `tests/` — bu fazda kurulur):**

| Test | Başarı kriteri |
|---|---|
| `checksum("qSupported")` | GDB referans değeriyle birebir |
| `frame()` | `$qSupported#37` biçimi, 2 küçük harf hex |
| `extract()` — parçalı akış | 3 parçaya bölünmüş bir paket `NeedMore`,`NeedMore`,`Packet` verir |
| `extract()` — `%Stop:T05...` | `Notification` döner, `Packet` DEĞİL |
| `expandRunLength("0* ")` | `"0000"` (4 karakter) |
| `expandRunLength("ab*\"cd")` | `'b'` toplam 6 kez ( `"`=34, 34-29=5 ekstra ) |
| `expandRunLength("*x")` | `ok=false` (başta tekrar karakteri yok) |
| `hexDecode("78563412")` | `{0x78,0x56,0x34,0x12}` |
| `memoryReadPacket(0xE000EDF0,4)` | `"mE000EDF0,4"` (küçük harf hex kabul) |
| `isErrorReply("E01")` / `("E 01")` | ikisi de `true`, kod `"01"` |
| `isAllowedOutgoing` — beyaz liste | `m`,`qSupported`,`QNonStop:1`,`vCont;c`,`D` → `true` |
| `isAllowedOutgoing` — kara liste | `?`,`Z0,...`,`M...`,`X...`,`vCont;t`,`k`,`\x03` → `false` |

**Canlı (NUCLEO-H723ZG, ST-LINK SN 004D003B3235511837333439):**

1. `DebugLink::retain()` → `opened()` sinyali ≤ 5 s içinde gelir; `refCount()==1`.
   İkinci bir `retain()` → `opened()` **derhal** tekrar yayılır, `refCount()==2`,
   yeni süreç başlatılmaz.
2. Handshake sonrası DHCSR okunur; beklenen `0x01010000` — `S_HALT(17)==0`,
   `S_RETIRE_ST(24)==1`. Gerçek okunan değer findings dosyasına yazılır.
3. **Regresyon kanıtı:** link açıkken UART monitörü açılır; `§{"t":"sys"...}`
   paketleri akmaya devam eder. Akış kesilirse çekirdek durmuştur → faz başarısız.
4. `readRanges` ile 4 B / 1 KB / 4 KB okunur; ölçülen Hz `app_trace.log`'a yazılır
   ve Faz A değerleriyle **aynı büyüklük mertebesinde** olmalıdır
   (4 B > 2000 Hz, 1 KB > 300 Hz, 4 KB > 90 Hz). Değilse RLE/çerçeveleme hatası aranır.
5. İki `retain()`'in **birinci** `release()`'inde süreç **yaşamaya devam eder**
   (`refCount()==1`); **ikinci** `release()`'te (`refCount()==0`) gdbserver
   süreci **kaybolur** (`tasklist`) ve `STM32_Programmer_CLI -c port=SWD`
   yeniden çalışır (ST-Link serbest). Bu, sayaç semantiğinin canlı kanıtıdır.
6. Başarısız `retain()` testi: gdbserver yolu kasten bozulur → `failed()` gelir
   ve `refCount()==0` kalır (sayaç sızmadı).
7. **`S_RESET_ST` testi:** link açık ve sağlık kontrolü çalışırken karttaki
   siyah RESET butonuna basılır → bir sonraki 4 Hz okumasında `S_RESET_ST`
   yakalanır, `coreReset()` yayılır, örnekleme **durmaz**. Ardından bit kendini
   temizler (sonraki okumada 0). Bu, sticky semantiğinin canlı kanıtıdır.

**Bu fazda `docs/variable_watcher_findings.md` oluşturulur**; 1–7 adımlarının
gerçek çıktıları oraya yapıştırılır (Register Inspector'daki
`register_inspector_findings.md` deseni).

---

## 5. Faz 2 — `GdbServerReader` (Register Inspector hızlanması)

### 5.1 Dosyalar

| Dosya | Durum |
|---|---|
| `src/modules/registers/GdbServerReader.h/.cpp` | YENİ — `IRegisterReader` 2. gerçeklemesi |
| `src/modules/registers/RegisterInspector.h/.cpp` | DEĞİŞİR — arka uç seçimi |
| `src/bridge/Backend.h/.cpp` | DEĞİŞİR — `registerReadBackend()` / `setRegisterReadBackend()` |
| `qml/dialogs/SettingsDialog.qml` | DEĞİŞİR — arka uç seçici + yeni araç yolları |
| `src/core/AppSettings.h/.cpp` | DEĞİŞİR — `registers/read_backend` |

```cpp
class GdbServerReader : public IRegisterReader
{
    Q_OBJECT
public:
    explicit GdbServerReader(DebugLink *link, QObject *parent = nullptr);
    void setStlinkSn(const QString &sn) override;
    void setConnectMode(const QString &mode) override;   // NOTE: ignored, gdb attach is always live
    bool isBusy() const override;
    void read(const ReadPlan &plan) override;
};
```

Davranış kuralları:

- **Oturum ömrü (retain/release sözleşmesi, §4.6):**
  1. `read()` **başında** `retain()` çağrılır.
  2. Link henüz açık değilse plan **kuyrukta tutulur**; `opened()` gelince
     çalıştırılır. (Bloklama yok, bekleme döngüsü yok.)
  3. `readFinished` **veya** `readFailed` yayıldıktan **SONRA** `release()`.
  4. `retain()` sonrası `failed()` gelirse: sayaç zaten geri alınmıştır →
     `readFailed(msg)` yayılır ve **`release()` ÇAĞRILMAZ** (§4.6 madde 4).
  5. İzleyici oturumu zaten bir `retain` tutuyorsa sayaç 1 → 2 → 1 gider,
     yani **link kapanmaz**; snapshot açık oturumun içinden alınır.
- `ReadPlanItem` → `MemoryRequest`. `item.byteCount > maxReadBytes()` ise
  parçalanır. Sonuç `RegisterReadResult.values` içine adres-çapalı yazılır.
- Blok hatası ataması `CliRegisterReader` ile **aynı semantik**: aralıktan hiç
  değer dönmediyse `RegisterReadError` eklenir, snapshot geri kalanı yaşar.
- `connectMode` ("HOTPLUG"/"UR") bu arka uçta anlamsızdır; **sessizce yok
  saymaz** — snapshot metadata'sına `connectMode = "GDB-ATTACH"` yazar ki UI
  yalan söylemesin.

`RegisterInspector` değişikliği (arayüz bozulmaz):

```cpp
void setReaderBackend(const QString &backend);   // "cli" | "gdb" — kullanici TERCIHI
void setDebugLink(DebugLink *link);
// m_cliReader ve m_gdbReader ayrı ayrı tutulur; m_reader aktif olanı gosterir.
// Mevcut m_cliReader->setCliPath() yolu aynen kalir (backend-ozel ayar deseni).
```

#### Etkin arka ucun çözümlenmesi (her snapshot'ta, bu sırayla)

Ayardaki değer bir **tercihtir**, garanti değil. `RegisterInspector::takeSnapshot()`
her çağrıda şu zinciri işletir:

| # | Koşul | Sonuç |
|---|---|---|
| a | ayar `"gdb"` **değilse** | `CliRegisterReader` |
| b | gdbserver yolu tespit edilememişse | `CliRegisterReader` + **tek seferlik** uyarı |
| c | `DebugLink::retain()` başarısız olursa (`failed()`) | `CliRegisterReader` + **tek seferlik** uyarı |
| d | aksi halde | `GdbServerReader` |

- **"Tek seferlik uyarı" = uygulama çalışması başına bir kez** `statusMessage`,
  snapshot başına değil. `bool m_gdbFallbackWarned = false;` bayrağıyla tutulur.
  Amaç: her snapshot'ta aynı uyarıyı tekrarlayıp kullanıcıyı köreltmemek.
- (c) yolunda `retain()` sayaç tüketmediği için `release()` çağrılmaz (§4.6 madde 4).
- Bu zincir sayesinde **gdbserver kurulu olmayan bir makinede Register Inspector
  hiç bozulmaz** — sadece eski hızında çalışır.

### 5.2 Doğrulama — Faz 2

**Çapraz doğrulama testi (bu fazın asıl kanıtı):**

1. Arka uç = CLI, Snapshot A al.
2. Arka uç = CLI, Snapshot B al. **Kontrol grubu**: mevcut A→B diff'i çalıştır,
   değişen register sayısını not et → `N_control`.
3. Arka uç = GDB, Snapshot B'yi yeniden al. A→B diff'i çalıştır → `N_test`.
4. **Başarı kriteri:** `N_test <= N_control * 1.5` ve `N_test` içindeki her
   register ya `N_control` içinde de var, ya da doğası gereği oynak
   (`TIM*.CNT`, `SysTick.VAL`, `DWT.CYCCNT`, `RCC` hazır bayrakları, `*_SR`
   durum register'ları). Yeni ve açıklanamayan bir fark = ayrıştırma hatası.
5. **Süre kriteri:** varsayılan peripheral seçimiyle (H7: 16 peripheral)
   snapshot süresi CLI'ya göre **en az 5× hızlı**. `app_trace.log`'a iki süre de
   yazılır.
6. Peripheral seçimi "hepsi" (tüm SVD) ile GDB arka ucunda snapshot **tamamlanır**
   (paket boyutu / parçalama doğru).

**Varsayılan arka uç kalıcı olarak `"cli"`'dır.** `"gdb"` yalnızca Ayarlar'dan
**opt-in** seçilir. Gerekçe: Register Inspector şu an çalışır durumda ve
kullanıcı için kritik; varsayılanı gdbserver'a çevirmek çalışan bir özelliğe
süreç yönetimi ve port çakışması riskini **sessizce** eklemek olur. Hız kazancı
konfor, doğruluk ise değil — konfor için çalışan yolu riske atmıyoruz.
≥5× hız kriteri bu fazın kabul şartı olarak **aynen korunur**; değişen tek şey
varsayılan ayardır. CLI yolu hiçbir koşulda silinmez.

---

## 6. Faz 3 — Sembol katmanı

### 6.1 Dosyalar

| Dosya | İçerik |
|---|---|
| `src/modules/watcher/SymbolModel.h` | `Symbol`, `SymbolKind` |
| `src/modules/watcher/NmSymbolParser.h/.cpp` | **Saf** — `nm` çıktısı → `QList<Symbol>` |
| `src/modules/watcher/ElfSymbolSource.h/.cpp` | QProcess ile `nm` çağırır, async |
| `src/modules/watcher/ElfTargetMatcher.h/.cpp` | ELF ↔ karttaki firmware eşleşme doğrulaması (§6.2) |
| `src/modules/watcher/WatchModel.h` | `WatchItem`, `WatchValueType`, `DisplayFormat`, `WatchStats` |
| `src/modules/watcher/ValueCodec.h/.cpp` | **Saf** — bayt → sayı, biçimlendirme |

```cpp
enum class SymbolKind {
    Bss,        // B/b  — RAM, zero-init      → izlenebilir
    Data,       // D/d  — RAM, init'li        → izlenebilir
    ReadOnly,   // R/r  — flash sabit          → izlenebilir (degismez)
    Code,       // T/t  — kod                  → izlenmez (varsayilan gizli)
    Weak,       // W/w/V/v                     → tipine gore
    Absolute,   // A/a  — DEGER, ADRES DEGIL   → izleme listesine EKLENEMEZ
    Common,     // C
    Other
};

struct Symbol {
    QString    name;
    quint64    address = 0;    // Absolute icin: bu bir DEGER, adres degil
    quint64    size = 0;       // 0 = nm boyut vermedi
    char       nmType = '?';
    SymbolKind kind = SymbolKind::Other;
    bool       hasSize = false;
    bool       addressIsValue = false;   // kind == Absolute
};
```

`NmSymbolParser::parse(const QString &nmOutput)` kuralları:

- 4 alanlı satır: `<addr hex> <size hex> <type> <name>`
- 3 alanlı satır: `<addr hex> <type> <name>` → `hasSize = false`
- Ayrıştırılamayan satır sessizce atlanır (nm başlık/uyarı satırları)
- `A`/`a` → `kind = Absolute`, `addressIsValue = true`
- Tip harfi büyük/küçük ayrımı korunur (`nmType`), `kind` eşlemesi harfe göre

```cpp
enum class WatchValueType { U8,I8,U16,I16,U32,I32,U64,I64,F32,F64 };
enum class DisplayFormat  { Dec, Hex, Bin };
enum class WatchItemKind  { Scalar, RegionScan };  // RegionScan = stack watermark

struct WatchItem {
    QString         id;          // kalici, QUuid::createUuid().toString(Id128)
    QString         label;
    QString         role;        // preset rolu ("heapEnd","stackWatermark",...) veya bos
    quint64         address = 0;
    WatchItemKind   kind = WatchItemKind::Scalar;
    WatchValueType  type = WatchValueType::U32;
    DisplayFormat   format = DisplayFormat::Dec;
    quint32         regionBytes = 0;   // RegionScan icin
    double          scale = 1.0;
    double          offset = 0.0;
    QString         unit;
    bool            enabled = true;
    QString         source;      // "elf:<sembol>" | "manual"
    QString         color;       // Theme'den atanan cizgi rengi
};

// Ring buffer'dan BAGIMSIZ, artimli (Welford) tutulur — eski ornekler
// dusse bile oturum min/max/ort/stddev dogru kalir.
struct WatchStats {
    quint64 count = 0;
    double  min = 0, max = 0, last = 0, mean = 0, m2 = 0;
    double  stddev() const;
    void    push(double v);
    void    reset();
};
```

`ValueCodec` (saf, little-endian — Cortex-M mimari sabiti):

```cpp
static int    byteSize(WatchValueType t);
static double decode(const QByteArray &buf, int offset, WatchValueType t, bool *ok);
static QString format(double raw, const WatchItem &item);  // scale/offset/format/unit uygular
```

`ElfSymbolSource`:

```cpp
void setNmPath(const QString &path);
void load(const QString &elfPath);      // async QProcess, 15 s timeout
signals:
    void loaded(const QList<Symbol> &symbols, const QString &elfPath);
    void failed(const QString &message);
```
Komut: `<nm> -S --defined-only <elf>`. Çıkış kodu 0 olsa bile çıktı boşsa hata.

### 6.2 ELF ↔ hedef eşleşme doğrulaması (`ElfTargetMatcher`)

**En kötü hata modu budur:** karta flash edilmemiş bir ELF yüklenirse tüm sembol
adresleri yanlış olur ve araç **sessizce tamamen yanlış değerler** gösterir.
Grafik akar, sayılar değişir, hiçbir şey hata vermez — bu, planın kendi
dürüstlük ilkesiyle doğrudan çelişir. Bu yüzden ELF yüklendiğinde ve link
açıkken şu doğrulama çalışır. **Tamamen aile-bağımsızdır, sabit RAM/flash
adresi kullanmaz.**

```cpp
enum class ElfMatchResult { Unknown, Match, Mismatch };

struct ElfMatchReport {
    ElfMatchResult result = ElfMatchResult::Unknown;
    quint64 vtor = 0;              // okunan VTOR degeri
    quint32 targetInitialSp = 0;   // vektor tablosu [0]
    quint32 targetResetVec = 0;    // vektor tablosu [1]
    quint64 elfEstack = 0;         // ELF _estack
    quint64 elfResetHandler = 0;   // ELF Reset_Handler
    bool    spMatches = false;
    bool    resetMatches = false;
    QString detail;                // UI'da gosterilecek aciklama
};

class ElfTargetMatcher {
public:
    // 1. VTOR oku (0xE000ED08) -> aktif vektor tablosu tabani
    //    [Cortex-M mimari sabiti — aile-ozel degil, boards.json'a tasinmaz]
    // 2. O tabandan 8 bayt oku -> [initial SP, reset vector]
    // 3. ELF'ten _estack ve Reset_Handler sembollerini al
    // 4. Karsilastir:
    //      targetInitialSp == elfEstack
    //      targetResetVec  == (elfResetHandler | 1)   [Thumb biti]
    static ElfMatchReport evaluate(quint64 vtor, const QByteArray &firstEightBytes,
                                   const QList<Symbol> &symbols);   // SAF, test edilebilir
};
```

**Sonuç politikası — engelleme değil, görünür uyarı:**

| Sonuç | Davranış |
|---|---|
| `Match` (ikisi de tutuyor) | `WatchLinkStatus`'ta **yeşil** "ELF eşleşiyor" rozeti |
| `Mismatch` | **Sarı** rozet + görünür uyarı: *"Yüklenen ELF karttaki firmware ile eşleşmiyor olabilir — değerler yanlış olabilir."* Kullanıcının **açıkça onaylaması** gereken **"Yine de devam et"** seçeneği. Onaylanana kadar örnekleme başlamaz. |
| `Unknown` (link kapalı / ELF yok / semboller eksik) | **Gri** rozet, uyarı yok |

**Neden sert blok değil:** vektör tablosunu RAM'e taşıyan firmware (`VTOR`
yeniden yazılmış, bootloader'lı tasarımlar, N6 LRUN) yanlış alarm üretebilir.
Sert blok, meşru bir senaryoyu kullanılamaz hale getirirdi. Açık uyarı +
bilinçli onay, hem sessiz yanlışı hem de gereksiz engellemeyi önler.

`Match` durumunda `_estack` **zaten doğrulanmış** olur — bu, Faz 8'deki stack
watermark hesabının temel varsayımını bedavaya sağlamlaştırır.

### 6.3 Doğrulama — Faz 3

**Donanımsız birim testler** (`tests/fixtures/nm_h7.txt` — H7 ELF'inden gerçek
`nm` çıktısı, faz başında bir kez üretilip commit edilir):

| Test | Kriter |
|---|---|
| 4 alanlı satır | adres/boyut/tip/ad doğru |
| 3 alanlı satır | `hasSize == false`, ad doğru |
| `_Min_Stack_Size` (A tipi) | `kind == Absolute`, `addressIsValue == true` |
| `_estack` | `kind` doğru, adres > 0x20000000 |
| Weak sembol (`W`) | `kind == Weak` |
| Bozuk satır | atlanır, parse çökmez |
| `ValueCodec::decode` — U32 LE | `{0x78,0x56,0x34,0x12}` → `0x12345678` |
| `ValueCodec::decode` — I16 negatif | `{0xFF,0xFF}` → `-1` |
| `ValueCodec::decode` — F32 | `{0x00,0x00,0x80,0x3F}` → `1.0` |
| `ValueCodec::format` | `scale=0.001, unit="ms"` → `"8.200 ms"` |
| `WatchStats` | 1000 rastgele değerde `stddev()` referansla ≤1e-9 fark |
| `ElfTargetMatcher` — eşleşen | SP==`_estack`, vec==`Reset_Handler\|1` → `Match` |
| `ElfTargetMatcher` — Thumb biti | vec `Reset_Handler` (bit0=0) → `Mismatch`, kural doğru uygulanıyor |
| `ElfTargetMatcher` — SP tutuyor, vec tutmuyor | `Mismatch`, `spMatches=true`, `resetMatches=false` |
| `ElfTargetMatcher` — `_estack` sembolü yok | `Unknown` (uydurma karşılaştırma yok) |

**Yarı-canlı:** H7 pipeline çıktısındaki gerçek `.elf` yüklenir;
`_estack`, `_end`, `_ebss`, `_sbss` bulunur; `_Min_Stack_Size` ve
`_Min_Heap_Size` **izlenebilir listede görünmez** (veya "sabit" rozetiyle,
adres olarak eklenemez halde görünür).

**Canlı ELF eşleşme testi (bu fazın ikinci kanıtı):**

1. Karta flash edilmiş **doğru** ELF yüklenir → rozet **yeşil**, uyarı yok.
2. **Kasten farklı** bir ELF yüklenir (başka bir model/başka bir build; en
   kolayı: pipeline'ı farklı bir modelle bir kez daha çalıştırıp eski `.elf`'i
   tutmak) → rozet **sarı**, uyarı metni çıkar, "Yine de devam et"
   onaylanmadan örnekleme başlamaz.
3. Link kapalıyken ELF yüklenir → rozet **gri** (`Unknown`), yanlış bir
   "eşleşiyor" iddiası yok.

---

## 7. Faz 4 — Örnekleyici + ring buffer + Backend + tablo UI

### 7.1 Dosyalar

| Dosya | İçerik |
|---|---|
| `src/modules/watcher/WatchPlanBuilder.h/.cpp` | **Saf** — `WatchItem` listesi → birleştirilmiş `MemoryRequest` planı |
| `src/modules/watcher/TraceBuffer.h/.cpp` | **Saf** — item başına ring + `decimate()` + `WatchStats` |
| `src/modules/watcher/WatchSampler.h/.cpp` | Ham blok → item değerleri çözümleme (worker tarafı mantığı, saf fonksiyon) |
| `src/modules/watcher/VariableWatcher.h/.cpp` | Ana thread orkestratörü (manager) |
| `qml/screens/WatchScreen.qml` | Yeni sekme (bu fazda: yalnız tablo, grafik yok) |
| `qml/components/watch/WatchItemTable.qml` | İzlenen değişken tablosu |
| `qml/components/watch/WatchLinkStatus.qml` | Bağlantı + hız durum şeridi |
| `qml/components/watch/WatchToolbar.qml` | ELF yükle / hız seç / başlat-durdur |
| `qml/dialogs/SymbolPickerDialog.qml` | Sembol arama + ekleme |
| `qml/dialogs/WatchItemDialog.qml` | Manuel adres / tip / ölçek düzenleme |

### 7.2 `WatchPlanBuilder` — round-trip minimizasyonu

Faz A modeli `t ≈ 0.31 ms + boyut/550 KB/s` olduğundan **sabit maliyet
baskındır**: 8 dağınık değişkeni 8 ayrı okumayla almak 8×0.31 = 2.5 ms
(≈400 Hz), tek 256 B blokla almak 0.78 ms (≈1280 Hz).

```cpp
struct WatchPlan {
    QVector<MemoryRequest>     requests;
    // itemSlots[i] = { requestIndex, byteOffset } — cozumleme haritasi
    QVector<QPair<int,int>>    itemSlots;
    int  roundTripsPerSample() const { return requests.size(); }
};

class WatchPlanBuilder {
public:
    // Birlestirme kurali: iki aralik arasindaki bosluk <= kMergeGapBytes (256)
    // ise birlestir; olusan blok kMaxReadBytes (4096) sinirini asamaz.
    // RegionScan item'lari plana GIRMEZ — ayri, dusuk hizli planda toplanir.
    static WatchPlan build(const QList<WatchItem> &items, quint32 maxReadBytes);
    static WatchPlan buildRegionScans(const QList<WatchItem> &items, quint32 maxReadBytes);
};
```

UI, `roundTripsPerSample()` değerini gösterir — kullanıcı dağınık değişken
seçtiğinde hızın neden düştüğünü **görür**, tahmin etmez.

### 7.3 `TraceBuffer`

```cpp
struct PlotColumn { double t; double vmin; double vmax; double vlast; bool hasData; };

class TraceBuffer {
public:
    void configure(int itemCount, int capacityPerItem);   // varsayilan 120000 (40 s @3 kHz)
    void append(const WatchSampleBatch &batch);           // WatchStats'i de gunceller
    void clear();
    int      sampleCount() const;
    double   firstTime() const;
    double   lastTime() const;
    QVector<PlotColumn> decimate(int item, double t0, double t1, int columns) const;
    const WatchStats &stats(int item) const;
};
```

Kapasite politikası: item başına `capacityPerItem`, toplam bellek tavanı
**64 MB** (8 bayt/örnek × item × kapasite). Aşılırsa `capacityPerItem`
otomatik düşürülür ve UI'da *"halka arabelleği N s'ye düşürüldü"* bilgisi verilir
— sessiz kırpma yok.

### 7.4 `VariableWatcher` (manager)

```cpp
class VariableWatcher : public QObject
{
    Q_OBJECT
public:
    explicit VariableWatcher(DebugLink *link, QObject *parent = nullptr);

    void setNmPath(const QString &path);
    void loadElf(const QString &path);
    const QList<Symbol> &symbols() const;
    QString elfPath() const;

    QList<WatchItem> items() const;
    QString addSymbol(const QString &symbolName);      // "" = eklenemedi (Absolute vb.)
    QString addAddress(quint64 addr, WatchValueType t, const QString &label);
    void    updateItem(const QString &id, const QVariantMap &props);
    void    removeItem(const QString &id);
    void    clearItems();

    void start(int targetRateHz);   // 0 = max
    void stop();
    bool isRunning() const;

    const TraceBuffer &buffer() const;
    QVariantMap rateInfo() const;   // targetHz, actualHz, rttMs, blocks, skewUs, missed, coreRunning

    void saveItems(const QString &boardName);     // AppSettings "watch/items"
    void loadItems(const QString &boardName);
signals:
    void itemsChanged();
    void samplesAppended();      // ≤30 Hz
    void statsChanged();         // ≤4 Hz (durum seridi icin)
    void runningChanged();
    void symbolsLoaded(int count);
    void errorOccurred(const QString &message);
};
```

**Adres doğrulama:** yeni bir ham adres eklendiğinde:
1. gdbserver'ın `qXfer:memory-map:read` çıktısı varsa, adres bir RAM/flash
   bölgesine düşmeli. Düşmüyorsa **uyarı** (engelleme değil).
2. Memory-map yoksa, yüklü ELF sembollerinin min/max aralığı referans alınır.
3. **Hiçbir durumda C++'a sabit RAM tablosu yazılmaz.**

### 7.5 `Backend` — ST-Link hakemliği (mevcut guard'ların genişletilmesi)

Mevcut dağınık kontroller (`m_flashBusy || m_pipelineBusy || m_probeBusy` ve
`m_registers->isBusy()`) tek bir açık hakemle değiştirilir:

```cpp
// Backend.h — private
QString m_stlinkOwner;    // "" | "flash" | "pipeline" | "probe" | "register" | "watch"
bool acquireStLink(const QString &who);   // false + statusMessage(sahibi adiyla)
void releaseStLink(const QString &who);
// Backend.h — public
Q_PROPERTY(QString stlinkOwner READ stlinkOwner NOTIFY stlinkOwnerChanged)
```

Yerleştirme (mevcut guard'ların bulunduğu satırlar):
`Backend::flashFirmware` (~1526), `Backend::runPipeline` (~1629),
`Backend::probeStLinkBoardForPort` (~755), `Backend::takeRegisterSnapshot` (~2989),
ve yeni `Backend::openWatchLink`. Serbest bırakma **her** çıkış yolunda
(başarı, hata, iptal) — mevcut `m_*Busy = false` atamalarının yanına.

`openWatchLink()` sahipliği **oturum boyunca** tutar (gdbserver probe'u sürekli
tutuyor), `closeWatchLink()` bırakır. Hakem sahipliği `DebugLink` referans
sayacıyla **paralel ama ayrı** tutulur: hakem "hangi özellik ST-Link'i kullanıyor"
sorusunu, referans sayacı "link kaç sahibi var" sorusunu cevaplar. Register
snapshot'ı açık bir izleme oturumunun içinden alındığında hakem sahibi `"watch"`
kalır, referans sayacı geçici olarak 2 olur. Hata mesajı sahibi adıyla:
*"ST-Link su anda Degisken Izleyici tarafindan kullaniliyor. Once izlemeyi durdurun."*
QML `stlinkOwner` property'siyle ilgili ekranda kısayol "Durdur" butonu gösterir.

### 7.6 `Backend` — izleyici API'si (tam liste)

```cpp
// ── Properties ────────────────────────────────────────────────────────────
Q_PROPERTY(bool         watchLinkOpen    READ watchLinkOpen    NOTIFY watchLinkChanged)
Q_PROPERTY(QString      watchLinkState   READ watchLinkState   NOTIFY watchLinkChanged)
Q_PROPERTY(QString      watchLinkError   READ watchLinkError   NOTIFY watchLinkChanged)
Q_PROPERTY(bool         watchRunning     READ watchRunning     NOTIFY watchRunChanged)
Q_PROPERTY(bool         watchPlayback    READ watchPlayback    NOTIFY watchLinkChanged)
Q_PROPERTY(QVariantMap  watchRateInfo    READ watchRateInfo    NOTIFY watchStatsChanged)
Q_PROPERTY(QVariantList watchItems       READ watchItems       NOTIFY watchItemsChanged)
Q_PROPERTY(QVariantList watchViolations  READ watchViolations  NOTIFY watchViolationsChanged)
Q_PROPERTY(QString      stlinkOwner      READ stlinkOwner      NOTIFY stlinkOwnerChanged)
// ELF <-> hedef eslesmesi (Bolum 6.2): "match" | "mismatch" | "unknown"
Q_PROPERTY(QString      watchElfMatch    READ watchElfMatch    NOTIFY watchElfMatchChanged)
Q_PROPERTY(QVariantMap  watchElfMatchDetail READ watchElfMatchDetail NOTIFY watchElfMatchChanged)

// ── Link ──────────────────────────────────────────────────────────────────
Q_INVOKABLE void openWatchLink();    // -> DebugLink::retain()  (bkz. Bolum 4.6)
Q_INVOKABLE void closeWatchLink();   // -> DebugLink::release()
// NOT: retain() basarisiz olursa (failed()) sayac zaten geri alinmistir;
// closeWatchLink() cagrilmaz. UI durumu watchLinkState uzerinden takip eder.

// ── Semboller ─────────────────────────────────────────────────────────────
Q_INVOKABLE QString      watchElfPath() const;
Q_INVOKABLE QString      suggestedElfPath() const;   // deployedModelOutputDir()/build/*.elf
Q_INVOKABLE void         loadWatchElf(const QString &path);
Q_INVOKABLE QVariantList watchSymbols(const QString &filter, int limit) const;

// ── Izleme kalemleri ──────────────────────────────────────────────────────
Q_INVOKABLE void addWatchSymbol(const QString &symbolName);
Q_INVOKABLE void addWatchAddress(const QString &addrHex, const QString &type, const QString &label);
Q_INVOKABLE void updateWatchItem(const QString &id, const QVariantMap &props);
Q_INVOKABLE void removeWatchItem(const QString &id);
Q_INVOKABLE void clearWatchItems();

// ── Calistirma ────────────────────────────────────────────────────────────
Q_INVOKABLE void startWatch(int targetRateHz);   // 0 = max
Q_INVOKABLE void stopWatch();
Q_INVOKABLE void clearWatchData();
// watchElfMatch == "mismatch" iken startWatch() reddedilir; kullanici bunu
// acikca cagirmadan ornekleme baslamaz ("Yine de devam et" butonu).
Q_INVOKABLE void acknowledgeElfMismatch();

// ── Register arka ucu (Faz 2) ─────────────────────────────────────────────
Q_INVOKABLE QString registerReadBackend() const;
Q_INVOKABLE void    setRegisterReadBackend(const QString &backend);

signals:
    void watchLinkChanged();
    void watchRunChanged();
    void watchItemsChanged();
    void watchStatsChanged();
    void watchViolationsChanged();
    void watchSymbolsLoaded(int count);
    void watchError(const QString &message);
    void stlinkOwnerChanged();
    void watchElfMatchChanged();
```

> Faz 6/7/8'de eklenecek `watchPlotFrame`, `watchEvents`, kayıt/oynatma ve
> profil API'leri ilgili fazlarda listelenmiştir.

### 7.7 UI — bu fazda kapsam

- `qml/Main.qml`: yeni sekme **"İzleyici"** (Register'dan sonra, 8. sıra) →
  `WatchScreen{}` `StackLayout`'a eklenir.
- `WatchScreen.qml` yerleşimi: üstte `WatchToolbar`, altında sol `WatchItemTable`
  (bu fazda tam genişlik), en altta `WatchLinkStatus`.
- `WatchItemTable` sütunları: Etkin · Etiket · Adres · Tip · Biçim · Ölçek ·
  Birim · **Canlı Değer** · Min · Max · Ort. Canlı değer 10 Hz'de yenilenir
  (`samplesAppended` sinyalinde `Timer` ile kısılır).
- `WatchLinkStatus` göstergeleri:
  - bağlantı durumu · **Hedef X Hz / Gerçekleşen Y Hz** · RTT ms ·
    okuma bloğu/örnek · kaçırılan deadline
  - **örnek kayması (skew) µs** — bu, `WatchSampleBatch::skewUs`'tur.
    Tooltip metni birebir: *"Değerler t ile t + skew arasında okundu."*
    Çok bloklu planlarda bu sayı büyür; kullanıcı eşzamanlılık kaybını görür.
  - **çekirdek durumu** rozeti: `S_RETIRE_ST=1` → yeşil "koşuyor";
    `S_HALT=1` → kırmızı "DURDU"; `S_RESET_ST=1` görüldüğünde ayrıca
    turuncu "RESET" flaşı (örnekleme devam eder).
  - **ELF eşleşme** rozeti (§6.2): yeşil "eşleşiyor" / sarı "eşleşmiyor olabilir"
    / gri "bilinmiyor".
  - Gerçekleşen hız hedefin %80'inin altındaysa hız rozeti `Theme.warning` olur.

### 7.8 Doğrulama — Faz 4

**Donanımsız birim testler:**

| Test | Kriter |
|---|---|
| `WatchPlanBuilder` — bitişik 4 adet u32 | tek `MemoryRequest`, 16 bayt |
| `WatchPlanBuilder` — 300 bayt boşluklu iki değişken | **iki** request (boşluk > 256) |
| `WatchPlanBuilder` — 100 bayt boşluklu iki değişken | **tek** request |
| `WatchPlanBuilder` — 8 KB yayılım | ≥2 request, hiçbiri > 4096 bayt |
| `WatchPlanBuilder` — `itemSlots` | her item doğru request+offset'e eşlenir |
| `TraceBuffer::append` + `decimate` | 10000 örnek, 100 sütun → 100 sütun, min≤max |
| `TraceBuffer` taşması | kapasite×2 örnek sonrası `sampleCount()==kapasite`, `stats().count==2×kapasite` |
| `TraceBuffer::decimate` boş aralık | `hasData=false` sütunlar, çökme yok |

**Canlı (H7):**

1. `g_ai_infer_count` benzeri **artan** bir değişken izlenir (Faz 5 öncesi:
   `uwTick` HAL sayacı, 1 kHz artar). Değer monoton artar ve 1000 örnekte
   ≈1000 birim artmış olur → **zaman tabanı doğrulaması**.
   Eğim `WatchSampleBatch::times` tanımına (§4.2: ilk `m` paketinin yazılma anı)
   dayanır; başka bir damgalama yorumu burada sistematik sapma üretir.
   > **Dipnot:** `uwTick` **HAL'e özgüdür** — "her firmware'de var" değil,
   > *HAL tabanlı her firmware'de* vardır. Proje şablonlarının tamamı HAL
   > kullandığı için bu test bizim ürettiğimiz ELF'lerde geçerlidir. HAL
   > kullanmayan bir ELF izlenirken bu zaman tabanı testi **atlanır** ve yerine
   > `S_RETIRE_ST` canlılık kontrolü (§4.5) kullanılır.
2. Hedef 1000 Hz seçilir; gerçekleşen hız ≥ 800 Hz raporlanır.
3. Hedef "max" seçilir; gerçekleşen hız Faz A modeliyle uyumlu
   (tek 64 B blok → >2000 Hz).
4. 60 s boyunca 1000 Hz'de çalışırken **UI donmaz**: sekme değiştirme,
   scroll, buton tıklaması akıcı. `app_trace.log`'a periyodik olarak
   `sampleCount` yazılır; ana thread'de bloklama olsaydı log aralığı kayardı.
5. UART monitörü paralel açık kalır ve `§` paketleri akmaya devam eder.
6. Bellek: 60 s / 1000 Hz / 8 değişken sonrası uygulama RSS artışı < 100 MB.

---

## 8. Faz 5 — Firmware: static terfi + stack boyama

`templates/ai_glue/ai_runner.c` içindeki ilginç değerler fonksiyon-yereldir
(stack) → sembolle izlenemez. Bu faz onları gözlemlenebilir hale getirir.

### 8.1 `templates/ai_glue/ai_runner.c` / `.h` değişiklikleri

```c
/* ── Watch-observable AI state ──────────────────────────────────────────
 * NOTE: these are file-scope and volatile on purpose. Locals live on the
 * stack and have no stable symbol address, so the host-side Variable
 * Watcher cannot observe them. volatile keeps the stores from being
 * optimised away at -Os. Cost: a few hundred bytes of .bss.
 */
static volatile uint32_t g_ai_last_inference_us = 0;
static volatile uint32_t g_ai_infer_count       = 0;
static volatile uint8_t  g_ai_last_class        = 0;
static volatile uint8_t  g_ai_last_confidence   = 0;
static ai_i8             g_ai_input[AI_NETWORK_IN_1_SIZE];
static ai_i8             g_ai_output[AI_NETWORK_OUT_1_SIZE];
```

`AI_Runner_Infer()` içinde:
- `ai_i8 q_input[...]` → `g_ai_input` kullanılır
- `ai_i8 output_data[...]` → `g_ai_output` kullanılır
- `elapsed_us` hesaplandıktan sonra `g_ai_last_inference_us = elapsed_us;`
- `g_ai_infer_count++`, `g_ai_last_class = best`,
  `g_ai_last_confidence = (uint8_t)confidence`

**Davranış değişmez** — sadece depolama sınıfı değişir. Reentrancy notu:
`AI_Runner_Infer` zaten tek thread'den çağrılıyor (bare-metal main loop).

### 8.2 Stack boyama (watermark için ön koşul)

Yeni dosya: `templates/ai_glue/stack_paint.c` / `.h`

```c
/* Paints the unused part of the stack with a known pattern so the host can
 * compute a high-water mark by scanning for the first untouched byte.
 * Without this, nothing pre-fills the stack and watermark is impossible.
 * Called once from main(), before any deep call. Paints from _sstack (or
 * _estack - _Min_Stack_Size when _sstack is absent) up to the current SP
 * minus a safety margin, so the live frame is never touched.
 */
#define STACK_PAINT_PATTERN 0xA5A5A5A5u
#define STACK_PAINT_MARGIN  128u
void StackPaint_Init(void);
uint32_t StackPaint_Pattern(void);     /* keeps the symbol referenced */
```

`templates/base/STM32*/Src/main.c` şablonlarına `StackPaint_Init();`
`HAL_Init()` sonrası, ilk derin çağrıdan önce eklenir.

**Sınır (açıkça belgelenecek):** watermark yalnızca **bu pipeline ile
derlenmiş** firmware'de çalışır. Başka bir ELF izlenirken watermark kalemi
"kullanılamıyor — stack boyama yok" olarak gösterilir, sahte değer üretilmez.

### 8.3 Doğrulama — Faz 5

1. H7 için pipeline yeniden çalıştırılır, `.elf` üretilir, flash edilir.
2. `nm -S --defined-only` çıktısında `g_ai_last_inference_us`,
   `g_ai_infer_count`, `g_ai_last_class`, `g_ai_last_confidence`,
   `g_ai_input`, `g_ai_output` **görünür** ve RAM adreslerindedir (`b`/`d`).
3. İzleyicide `g_ai_infer_count` izlenir → inference çalışırken artar.
4. `g_ai_last_inference_us` değeri, aynı anda UART'tan gelen
   `§{"t":"inf","inf_us":...}` değeriyle **±%5 içinde uyuşur** →
   bu, hem sembol adresinin hem tip çözümlemesinin doğruluğunun kanıtıdır.
5. Boyama sonrası stack bölgesi okunduğunda 0xA5A5A5A5 dolgu görülür;
   watermark hesabı Faz 8'de kurala bağlanır.
6. Firmware boyutu artışı raporlanır (< 1 KB beklenir).

---

## 9. Faz 6 — Grafik + zaman ekseni + olay korelasyonu

### 9.1 Dosyalar

| Dosya | İçerik |
|---|---|
| `src/quick/TracePlot.h/.cpp` | `QQuickPaintedItem`, `QML_ELEMENT` — çizim motoru |
| `src/modules/watcher/TraceEventLog.h/.cpp` | Ortak zaman eksenindeki olaylar |
| `qml/components/watch/TracePlotView.qml` | Eksen + lejant + imleç + zoom sarmalayıcı |
| `qml/components/watch/WatchEventLane.qml` | Olay şeridi (dikey işaretler) |

### 9.2 Neden Qt Charts değil

Qt Charts `ChartView`/`LineSeries` `QGraphicsScene` tabanlıdır; birkaç bin
noktada kare süresi kabul edilemez hale gelir ve veri beslemesi `QPointF`
listesi kopyalamayı gerektirir. Yerine ~150 satırlık bir `QQuickPaintedItem`
kullanılır: piksel sütunu başına **min/max zarfı** çizer, yani çizilen segment
sayısı örnek sayısından değil **piksel genişliğinden** bağımsızdır.
Qt Charts bağımlılığı `CMakeLists.txt`'te kalır (başka yerde kullanılmıyor
olsa da kaldırmak bu planın kapsamı dışı).

### 9.3 `TracePlot`

```cpp
class TracePlot : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT
    // frame: [{ id, label, color, unit, points: [t,min,max, t,min,max, ...],
    //           yMin, yMax, laneIndex }]
    Q_PROPERTY(QVariantList frame READ frame WRITE setFrame NOTIFY frameChanged)
    Q_PROPERTY(QVariantList events READ events WRITE setEvents NOTIFY eventsChanged)
    Q_PROPERTY(double windowStart READ windowStart WRITE setWindowStart NOTIFY rangeChanged)
    Q_PROPERTY(double windowEnd   READ windowEnd   WRITE setWindowEnd   NOTIFY rangeChanged)
    Q_PROPERTY(int    laneCount   READ laneCount   WRITE setLaneCount   NOTIFY laneCountChanged)
    Q_PROPERTY(double cursorTime  READ cursorTime  WRITE setCursorTime  NOTIFY cursorChanged)
    Q_PROPERTY(QColor gridColor MEMBER m_gridColor NOTIFY styleChanged)
    Q_PROPERTY(QColor axisColor MEMBER m_axisColor NOTIFY styleChanged)
public:
    void paint(QPainter *painter) override;
    Q_INVOKABLE double timeAtX(double x) const;
    Q_INVOKABLE int    laneAtY(double y) const;
};
```

`QML_ELEMENT` `qt_add_qml_module(STM32AiDeployer ...)` ile aynı hedefte
derlendiği için `STM32AiDeployer` URI'sine otomatik kaydolur — `main.cpp`'de
manuel `qmlRegisterType` gerekmez. (Gerekirse geri dönüş: `qmlRegisterType`.)

**Yerleşim kararı: şeritli (lane) görünüm.** Her değişken kendi yatay şeridini
ve kendi Y ölçeğini alır, X ekseni ortaktır. Gerekçe: farklı birimleri
(µs, bayt, adres, sınıf indeksi) tek Y ekseninde göstermek yanıltıcıdır.
Aynı birimli iki değişken için `TracePlotView`'da "üst üste bindir" seçeneği
vardır (`laneIndex` aynı verilir).

**Y ölçekleme:** varsayılan otomatik; pencere içi min/max'e oturur, ancak
küçülme 1 s'lik yumuşatmayla yapılır (grafik zıplamaz). Kalem bazında
manuel min/max sabitlenebilir.

### 9.4 Veri akışı (facade kuralı korunur)

```
TracePlotView.qml  --Timer(25 Hz)-->  backend.watchPlotFrame(columns, windowSec)
                                        -> VariableWatcher -> TraceBuffer::decimate()
                   --Timer(25 Hz)-->  backend.watchEvents(t0, t1)
```

Yük: `columns ≤ 800`, kalem başına 3 sayı → 25 Hz × 8 kalem × 2400 sayı.
Örnekleme hızından **bağımsız**; 3000 Hz'de de aynı yük.

```cpp
Q_INVOKABLE QVariantList watchPlotFrame(int columns, double windowSec);
Q_INVOKABLE QVariantList watchEvents(double fromT, double toT) const;
Q_INVOKABLE QVariantMap  watchItemStats(const QString &id) const;
Q_INVOKABLE QVariantMap  watchValuesAt(double t) const;   // imlec okumasi
```

### 9.5 Ortak zaman ekseni + olay korelasyonu

`TraceEventLog` (ana thread), tek monotonik `QElapsedTimer` (oturum başında
sıfırlanır) üzerinden saniye cinsinden `t` ile olay tutar:

```cpp
struct TraceEvent {
    double  t;
    QString kind;      // "inference" | "sys" | "boot" | "uartError" |
                       // "snapshot" | "ruleViolation" | "flash" | "note" |
                       // "targetReset"   <- DHCSR S_RESET_ST (Bolum 4.5)
    QString text;
    QString severity;  // "info" | "warning" | "error"
};
```

Besleme noktaları (`Backend`'de mevcut sinyallere ek bağlantı, mevcut davranış
değişmez):
- `SerialManager::inferenceReceived` → `kind="inference"`, metin `model/inf_us/label`
- `SerialManager::sysReceived` → `kind="sys"` (1 Hz)
- `SerialManager::bootReceived` / `errorReceived`
- `Backend::registerSnapshotReady` → `kind="snapshot"`, "Snapshot A/B alındı"
- `DebugLink::coreReset` (DHCSR `S_RESET_ST`, 4 Hz sağlık okumasından) →
  `kind="targetReset"`, `severity="warning"`, metin: *"Hedef reset edildi —
  bu noktadan sonraki değerler süreksizdir."* Örnekleme durmaz; grafikte bu
  çizgi **kalın turuncu** çizilir çünkü serideki sıçramanın nedeni odur.
- Faz 8 kural ihlalleri → `kind="ruleViolation"`

> **Dürüstlük kuralı (zorunlu):** UART olayları **ana thread'de varış anında**
> damgalanır. Bu firmware zaman damgası değildir; USB-CDC + DMA gecikmesi
> nedeniyle birkaç ms kayabilir. `WatchEventLane` tooltip'inde ve
> `docs/variable_watcher_findings.md`'de bu açıkça yazılır. Grafik üzerinde
> olay çizgileri kesikli çizilir (ölçülen değil, "yaklaşık" olduklarını
> görsel olarak belli etmek için).

### 9.6 Doğrulama — Faz 6

**Donanımsız:**
- `decimate()` 1e6 örnek / 800 sütun için < 20 ms (birim testte `QElapsedTimer`).
- `TracePlot::paint` boş `frame` ile çökmez; `laneCount=0` ile çökmez.

**Canlı (H7):**
1. 3000 Hz'de (veya ulaşılabilir max) 60 s izleme; grafik akıcı, pencere
   kaydırma/zoom takılmıyor.
2. `uwTick` izlenir → grafik **düz artan doğru**; eğim ≈1000 birim/s
   (zaman ekseni doğrulaması, gözle ölçülebilir).
3. UART inference olayları `g_ai_infer_count` basamaklarıyla **hizalı** görünür
   (±birkaç ms). Hizasızlık sistematikse (ör. hep 200 ms) zaman tabanı hatası aranır.
4. İmleç bir noktaya konur; okunan değer tabloda o andaki değerle uyuşur.
5. Register snapshot alınır → olay şeridinde `snapshot` işareti belirir.

---

## 10. Faz 7 — Kayıt / oynatma / dışa aktarma / güvenli mod

### 10.1 Dosyalar

| Dosya | İçerik |
|---|---|
| `src/modules/watcher/TraceRecorder.h/.cpp` | CSV yazıcı (akış halinde, tamponlu) |
| `src/modules/watcher/TracePlayer.h/.cpp` | CSV okuyucu → canlıymış gibi `WatchSampleBatch` yayar |
| `src/modules/watcher/WatchProfile.h/.cpp` | Oturum özeti + DB kaydı |
| `watch/demo/h7_demo_trace.csv` | Bu fazda H7'den kaydedilir ve commit edilir |

### 10.2 CSV biçimi (tek doğruluk kaynağı)

```
# stm32-ai-deployer watch trace v1
# board=STM32H7 elf=D:/.../app.elf model=anomaly_cnn_int8 started=2026-08-02T14:03:11
# targetHz=1000 actualHz=947 items=3
# item,0,g_ai_infer_count,0x24000123,u32,dec,1,0,,
# item,1,g_ai_last_inference_us,0x24000127,u32,dec,0.001,0,ms,
# item,2,heapEnd,0x2400a000,u32,hex,1,0,,heapEnd
# event,12.418,inference,"MLP_INT8 8200us walking",info
t,0,1,2
0.000000,41,8200,603979776
0.001057,41,8200,603979776
...
```

- Yorum satırları (`#`) başlık/kalem/olay meta verisi taşır → oynatma
  tam yapılandırmayı geri kurar.
- `t` saniye, 6 hane; değerler **ham** (scale/offset uygulanmamış) —
  ölçekleme görüntüleme katmanındadır, kayıt ham kalır.
- Yazma: 64 KB tamponlu `QTextStream`, örnek başına flush **yok**.

### 10.3 Güvenli mod (demo sağlamlığı)

`TracePlayer` kayıtlı CSV'yi **canlı yolun aynı sinyal zincirinden** geçirir
(`VariableWatcher` `WatchSampleBatch` üretir, `TraceBuffer`'a düşer, aynı
grafik/kural motoru çalışır). Fark yalnızca kaynaktadır.

- Hız çarpanı: 0.25× / 1× / 2× / 4× / adım adım.
- UI'da kalıcı, göz ardı edilemez banner: **"KAYITTAN OYNATMA — canlı hedef yok"**
  (`Theme.warning` arka plan). Canlı ile karıştırılamaz.
- `watch/demo/h7_demo_trace.csv` uygulamayla dağıtılır (CMake `watch/` kopyalar)
  → **ST-Link takılı olmadan bile ekran tam çalışır**. Bitirme sunumu için
  birincil güvenlik ağı.
- `Backend::demoTracePath()` bu dosyayı döndürür; `WatchToolbar`'da
  "Demo kaydını oynat" butonu.

### 10.4 DB kaydı — özet profil

`analysis_records`, `kind = "watch_profile"`, `c0..c14`:

| Sütun | İçerik |
|---|---|
| c0 | model adı |
| c1 | kart |
| c2 | değişken etiketi |
| c3 | örnek sayısı |
| c4 | gerçekleşen Hz |
| c5 | min |
| c6 | max |
| c7 | ortalama |
| c8 | stddev |
| c9 | son değer |
| c10 | süre (s) |
| c11 | birim |
| c12 | rol (`heapEnd`/`stackWatermark`/...) |
| c13 | ham seri dosya yolu |
| c14 | not (kullanıcı metni) |

Değişken başına bir satır. **Ham seri asla DB'ye yazılmaz** (1 sn çözünürlük).

### 10.5 Backend API eklemeleri

```cpp
Q_INVOKABLE QString defaultWatchRecordPath() const;   // AppSettings watch/record_dir
Q_INVOKABLE QString demoTracePath() const;
Q_INVOKABLE bool startWatchRecording(const QString &path);
Q_INVOKABLE void stopWatchRecording();
Q_INVOKABLE bool startWatchPlayback(const QString &path, double speed);
Q_INVOKABLE void stopWatchPlayback();
Q_INVOKABLE bool saveWatchProfile(const QString &note);   // -> analysis_records
Q_INVOKABLE bool exportWatchCsv(const QString &path);     // gorunen pencere, decimated
Q_INVOKABLE bool exportWatchJson(const QString &path);    // oturum ozeti + yapilandirma
Q_PROPERTY(bool watchRecording READ watchRecording NOTIFY watchRunChanged)
```

`AnalysisScreen.qml`'e yeni `kind` sekmesi/filtresi: **"İzleme Profilleri"**
(mevcut `recordsForKindQml("watch_profile")` yolu yeniden kullanılır).

### 10.6 Doğrulama — Faz 7

**Donanımsız:**
| Test | Kriter |
|---|---|
| Recorder→Player gidiş-dönüş | 10000 örnek yaz → oku → `t` ve değerler bit-birebir |
| Başlık ayrıştırma | kalem tipi/ölçek/birim/rol geri kurulur |
| Bozuk CSV | hata mesajı, çökme yok |
| Olay satırları | `TraceEventLog`'a geri yüklenir |
| Profil satırı | 15 hücre, kolon eşlemesi doğru |

**Canlı/demo:**
1. H7'de 30 s kayıt alınır → `watch/demo/h7_demo_trace.csv` commit edilir.
2. **ST-Link fiziksel olarak çıkarılır**, uygulama yeniden başlatılır,
   demo kaydı oynatılır → grafik, tablo, istatistikler tam çalışır, banner görünür.
3. Aynı kayıt 4× hızda oynatılır → UI donmaz.
4. Profil kaydedilir → Analiz ekranında satırlar görünür, CSV dışa aktarılır.

---

## 11. Faz 8 — `TimeSeriesRuleEngine` + watermark + AI preset + profil karşılaştırma

### 11.1 Dosyalar

| Dosya | İçerik |
|---|---|
| `src/modules/watcher/TimeSeriesRuleModel.h` | `TsRule`, `TsRuleViolation`, `TsConditionType` |
| `src/modules/watcher/TimeSeriesRuleEngine.h/.cpp` | **Saf** — pencere üzerinde deterministik değerlendirme |
| `src/modules/watcher/WatchPresetMatcher.h/.cpp` | Semboller + preset JSON → önerilen izleme listesi |
| `watch/watch_rules.json` | Kural verisi |
| `watch/watch_presets.json` | AI-farkındalıklı preset verisi |
| `watch/README.md` | Şema açıklaması |
| `qml/components/watch/WatchRuleFeed.qml` | İhlal akışı |
| `qml/dialogs/ProfileCompareDialog.qml` | Model A vs B kaynak profili |

### 11.2 `RuleEngine` ile karıştırılmama (CLAUDE.md kuralı)

| | `RuleEngine` (mevcut) | `TimeSeriesRuleEngine` (yeni) |
|---|---|---|
| Girdi | **Tek snapshot**, decoded peripheral ağacı | **Pencere**, zaman serisi örnekleri |
| Soru | "Şu anki register durumu tutarlı mı?" | "Son N saniyedeki davranış olağan mı?" |
| Veri dosyası | `svd/rules.json` | `watch/watch_rules.json` |
| Kapsam | Peripheral içi field ilişkileri | Eşik / z-skoru / trend / olay kapısı |

Mevcut `RuleEngine` **hiç değiştirilmez**. İki motor birleştirilmez.

### 11.3 Koşul tipleri (yalnız bu üçü + kapı)

```cpp
enum class TsConditionType {
    Threshold,   // deger op sabit, opsiyonel sustainMs boyunca surekli
    ZScore,      // |x - pencereOrt| > k * pencereStdSapma, minSamples sarti
    Drift        // pencere uzerinde dogrusal egim > minSlopePerSec ve R2 > minR2
};

struct TsGate {                 // olay korelasyonu — opsiyonel
    QString eventKind;          // "inference" | "sys" | ...
    int     withinMs = 0;       // ihlal ancak bu pencerede olay varsa raporlanir
    bool    isValid() const { return !eventKind.isEmpty() && withinMs > 0; }
};

struct TsRule {
    QString id, severity, message;
    QString appliesToRole;        // preset rolu ile eslesme
    QString appliesToLabelRegex;  // veya etiket regex'i
    int     windowMs = 0;
    TsConditionType type = TsConditionType::Threshold;
    QString op;                   // "<" ">" "<=" ">=" — Threshold icin
    double  value = 0, k = 4.0, minSlopePerSec = 0, minR2 = 0.6;
    int     minSamples = 50, sustainMs = 0;
    TsGate  gate;
};

struct TsRuleViolation {
    QString ruleId, severity, itemId, label, message;
    double  t = 0;        // ihlalin gozlendigi zaman
    double  value = 0;
    QVariantMap detail;   // mean/stddev/z/slope/r2 — UI'da "neden" gostermek icin
};
```

**ML tabanlı anomali tespiti bilinçli olarak yoktur** (ground truth yok,
savunulamaz). Her ihlal, hangi sayının hangi eşiği nasıl aştığını `detail`
içinde taşır — bitirme jürisine "model öyle dedi" değil, aritmetik gösterilir.

`Drift` için `minR2` zorunludur: gürültülü bir seride eğim tesadüfen büyük
çıkabilir; R² kapısı bunu eler.

### 11.4 `watch_rules.json` (başlangıç seti)

```json
{
  "_schemaVersion": 1,
  "_comment": "Deterministic, windowed rules for the Variable Watcher. Separate from svd/rules.json (instantaneous register state) on purpose — see CLAUDE.md.",
  "rules": [
    {
      "id": "stack_headroom_critical",
      "severity": "error",
      "appliesToRole": "stackWatermark",
      "type": "threshold", "op": "<", "value": 512, "sustainMs": 1000,
      "message": "{label}: yalnizca {value} B stack bosluk kaldi (esik {threshold} B)"
    },
    {
      "id": "heap_leak_drift",
      "severity": "warning",
      "appliesToRole": "heapEnd",
      "type": "drift", "windowMs": 30000, "minSlopePerSec": 8.0, "minR2": 0.6,
      "message": "{label}: {windowSec} s icinde {slope} B/s artis egilimi (R2={r2}) — olasi bellek sizintisi"
    },
    {
      "id": "inference_time_outlier",
      "severity": "info",
      "appliesToLabelRegex": "inference_us|elapsed_us|inf_us",
      "type": "zscore", "windowMs": 5000, "k": 4.0, "minSamples": 200,
      "gate": { "eventKind": "inference", "withinMs": 50 },
      "message": "{label}: {value} (pencere ort. {mean}, z={z}) — aykiri inference suresi"
    },
    {
      "id": "watermark_downward_trend",
      "severity": "warning",
      "appliesToRole": "stackWatermark",
      "type": "drift", "windowMs": 60000, "minSlopePerSec": -4.0, "minR2": 0.5,
      "message": "{label}: stack bosluğu {windowSec} s'de {slope} B/s azaliyor — limite tirmanma"
    }
  ]
}
```

### 11.5 `watch_presets.json` — AI-farkındalıklı otomatik liste

```json
{
  "_schemaVersion": 1,
  "presets": [
    {
      "id": "core_memory", "label": "Bellek sagligi", "always": true,
      "items": [
        { "role": "heapEnd", "symbol": "__sbrk_heap_end", "type": "u32", "format": "hex" },
        { "role": "bssEnd",  "symbol": "_end",            "type": "u32", "format": "hex", "constant": true },
        { "role": "stackWatermark", "kind": "regionScan",
          "regionFrom": ["_sstack", "_estack-_Min_Stack_Size"],
          "regionTo": "_estack", "pattern": "0xA5A5A5A5", "unit": "B", "rateHz": 2 }
      ]
    },
    {
      "id": "xcubeai_runtime", "label": "X-CUBE-AI calisma zamani",
      "requiresAnySymbol": ["s_network", "net_exec_ctx", "g_npu_exec_ctx", "NN_Instance_Default"],
      "items": [
        { "role": "inferenceUs", "symbol": "g_ai_last_inference_us", "type": "u32", "scale": 0.001, "unit": "ms" },
        { "role": "inferCount",  "symbol": "g_ai_infer_count",       "type": "u32" },
        { "role": "lastClass",   "symbol": "g_ai_last_class",        "type": "u8"  },
        { "role": "confidence",  "symbol": "g_ai_last_confidence",   "type": "u8", "unit": "%" }
      ]
    },
    {
      "id": "hal_timebase", "label": "HAL zaman tabani", "always": true,
      "items": [ { "role": "hwTick", "symbol": "uwTick", "type": "u32", "unit": "ms" } ]
    }
  ]
}
```

Kurallar:
- Bulunamayan sembol **sessizce atlanır** — preset kısmen uygulanır, hata değil.
- `regionFrom` bir alternatif listesidir: `_sstack` yoksa `_estack -
  _Min_Stack_Size` ifadesi değerlendirilir (`_Min_Stack_Size` `A` tipi olduğu
  için **değer** olarak kullanılır — plan başındaki tuzağın doğru tarafı).
  Bu ifade değerlendirmesi `WatchPresetMatcher` içinde, yalnızca
  `<sembol>-<sembol>` ve `<sembol>+<sembol>` biçimlerini destekleyecek kadar
  dar tutulur (genel ifade motoru yazılmaz).
- **Turnusol testi:** yeni bir kart eklemek = `.svd` + `boards.json` kaydı +
  linker template. Bu preset dosyası kart adı hiç geçmediği için değişmez;
  semboller ELF'ten gelir. **Tek satır C++ değişmez.**

### 11.6 Model A/B kaynak profili karşılaştırması

**İsimlendirme (CLAUDE.md kuralı):** Register Inspector'daki "Snapshot A →
Snapshot B farkı" ile karıştırılmaz. Burada kullanılan terimler:
**"Oturum"** ve **"Profil Karşılaştırma"**. UI'da "A/B" harfleri kullanılmaz.

`ProfileCompareDialog.qml`: `analysis_records`'tan iki `watch_profile` oturumu
seçilir → yan yana delta tablosu:

| Metrik | Model 1 | Model 2 | Δ | Δ% |
|---|---|---|---|---|
| Ortalama inference (ms) | … | … | … | … |
| Tepe inference (ms) | … | … | … | … |
| Tepe heap kullanımı (B) | … | … | … | … |
| En düşük stack boşluğu (B) | … | … | … | … |
| Oturum süresi (s) | … | … | — | — |
| Gerçekleşen örnekleme (Hz) | … | … | — | — |

```cpp
Q_INVOKABLE QVariantList watchProfiles() const;              // secim listesi
Q_INVOKABLE QVariantMap  compareWatchProfiles(int idA, int idB);
Q_INVOKABLE void         applyWatchPresets();
Q_INVOKABLE QVariantList watchPresetSuggestions() const;     // uygulamadan once onizleme
```

Karşılaştırma **yalnızca aynı rol/etiketteki** kalemler için yapılır; eşleşmeyenler
"karşılığı yok" olarak listelenir — uydurma eşleştirme yapılmaz.

### 11.7 Doğrulama — Faz 8

**Donanımsız birim testler (sentetik seriler):**

| Test | Kriter |
|---|---|
| Threshold — eşik altı 1 örnek, `sustainMs=1000` | ihlal **yok** |
| Threshold — eşik altı 1500 ms sürekli | ihlal **var**, `t` doğru |
| ZScore — sabit seri + tek sıçrama | tam 1 ihlal, `detail.z` beklenen değerde |
| ZScore — `minSamples` altında | ihlal yok (yetersiz veri) |
| Drift — düz artan 10 B/s | ihlal var, `slope≈10`, `r2≈1` |
| Drift — beyaz gürültü, ort. sabit | ihlal **yok** (`r2` kapısı eler) |
| Gate — olay yokken ihlal koşulu sağlanır | ihlal **bastırılır** |
| Gate — olay ±withinMs içinde | ihlal raporlanır |
| Preset — `g_ai_*` sembolleri yok | preset kısmen uygulanır, hata yok |
| Preset — `_sstack` yok | fallback `_estack-_Min_Stack_Size` hesaplanır |
| Preset — `_Min_Stack_Size` | **değer** olarak kullanılır, adres olarak değil |
| ProfileCompare — eşleşmeyen kalem | "karşılığı yok" satırı, uydurma eşleşme yok |

**Canlı (H7):**
1. `applyWatchPresets()` → `uwTick`, heap, watermark, AI kalemleri otomatik gelir.
2. Watermark 0xA5 taramasıyla makul bir değer verir (0 veya `regionBytes`
   değil); derin bir çağrı yapan firmware'de değer düşer.
3. **Sızıntı demosu:** her inference'ta küçük bir `malloc` yapan (ve serbest
   bırakmayan) geçici bir firmware ile `heap_leak_drift` kuralı tetiklenir;
   `detail.slope` gerçek sızıntı hızıyla uyuşur. (Bu firmware commit edilmez,
   sadece doğrulama içindir; sonuç `variable_watcher_findings.md`'ye yazılır.)
4. İki farklı model deploy edilip iki profil kaydedilir → karşılaştırma tablosu
   anlamlı Δ üretir.

---

## 12. Faz 9 — Dokümantasyon + kalıcı kararlar + ertelenmiş doğrulamalar

### 12.1 `CLAUDE.md`'ye eklenecek bölüm (birebir, "Register Inspector — Kalıcı Mimari Kararları"nın hemen altına)

```markdown
## Değişken İzleyici — Kalıcı Mimari Kararları

> Bu bölüm bir karar kaydıdır (ADR benzeri) — Değişken İzleyici üzerinde
> çalışan her oturumda geçerlidir, unutulmamalı/çiğnenmemelidir.

- **Gözlemci ilkesi mutlaktır:** hedef ASLA durdurulmaz, reset edilmez,
  breakpoint konmaz, belleğe YAZILMAZ. Giden GDB RSP paketleri
  `GdbRspCodec::isAllowedOutgoing()` beyaz listesiyle kod düzeyinde sınırlıdır
  (`qSupported`, `QStartNoAckMode`, `QNonStop:1`, `vCont;c`, `m`, `qXfer:*:read`,
  `D`). `?`, `Z/z`, `M/X`, `G/P`, `vCont;t|s`, `k`, `\x03` gönderimi engellidir.
  gdbserver `-k` / `--halt` ile başlatılmaz. Bu, Register Inspector'daki
  "araç gözlemci" kuralının aynısının canlı okuma yoluna uygulanmasıdır.
- **Canlılık ve reset tespiti tek DHCSR (0xE000EDF0) okumasından, 4 Hz'de:**
  `S_RETIRE_ST` (bit 24) **birincil canlılık göstergesidir** — mimari düzeyde
  "komut emekliye ayrıldı" kanıtıdır ve DWT'nin açık olmasını gerektirmez;
  `DWT_CYCCNT` canlılık için KULLANILMAZ (başkasının firmware'inde DWT kapalı
  olabilir, sayaç donuk görünür ve araç yanlışlıkla "hedef durdu" derdi).
  `S_HALT` (bit 17) set ise örnekleme durur ve kullanıcı uyarılır.
  `S_RESET_ST` (bit 25) set ise zaman eksenine `kind="targetReset"` olayı
  düşülür ve örnekleme **devam eder** — adresler geçerli kalır, ama değerler
  süreksiz sıçradığı için kullanıcı görünür biçimde uyarılır. Üç bit de
  sticky/okununca-temizlenir; sağlık okuması kendi `MemoryRequest`'idir
  (~%0.2 bant maliyeti) ve örnekleme durunca o da durur.
- **Yüklenen ELF ile karttaki firmware'in eşleştiği VTOR + vektör tablosu
  üzerinden doğrulanır** (`VTOR` → tablo[0]==`_estack`, tablo[1]==`Reset_Handler|1`);
  eşleşmezse değerler gösterilir ama **görünür uyarı** verilir ve kullanıcının
  açık onayı istenir — **sessizce yanlış veri gösterilmez**. Sert blok değildir,
  çünkü vektör tablosunu RAM'e taşıyan meşru firmware yanlış alarm üretebilir.
- **Tek ST-Link, tek sahip:** `Backend` içindeki açık hakem (`m_stlinkOwner`)
  flash / pipeline / probe / register / watch arasında hakemlik eder. Sahiplik
  hata mesajlarında ada göre bildirilir. STM32_Programmer_CLI bağlantı hatasında
  bile exit code 0 döndüğü için çıkış koduna değil, çıktıdaki hata işaretlerine
  bakılır (`HexDumpParser::errorMarkers` deseni).
- **Aile-özel hiçbir şey C++'a hardcode edilmez:** RAM bölgeleri ELF
  sembollerinden ve gdbserver'ın `qXfer:memory-map:read` çıktısından gelir;
  kart-özel debug ayarları `svd/boards.json` içindeki `debug` bloğundadır;
  izleme presetleri `watch/watch_presets.json`'dadır. Turnusol testi: yeni kart
  eklemek = `.svd` + `boards.json` kaydı + linker template; TEK SATIR C++
  değişmemeli. (Cortex-M mimari sabitleri — DHCSR adresi gibi — aile-özel
  değildir ve bu kuralın istisnası sayılmaz.)
- **`nm` `A` tipi sembollerin "adresi" aslında DEĞERİdir** (`_Min_Stack_Size`,
  `_Min_Heap_Size`). `Symbol::addressIsValue` ile işaretlenir; izleme listesine
  adres olarak eklenmesi engellidir. Yalnızca preset ifadelerinde sayı olarak
  kullanılır.
- **Ham zaman serisi veritabanına yazılmaz.** `analysis_records` 1 saniye
  çözünürlüklüdür ve 15 TEXT sütunludur; 500–3000 Hz seri için uygunsuzdur.
  Ham seri dosyaya (CSV v1 başlıklı biçim), yalnızca özet profil DB'ye
  (`kind = "watch_profile"`).
- **Anomali tespiti yalnızca deterministiktir:** eşik (`sustainMs` ile),
  kayan pencere z-skoru, doğrusal trend/drift (R² kapısıyla) ve olay
  korelasyon kapısı. ML tabanlı anomali tespiti YAPILMAZ — ground truth yok,
  savunulamaz. Her ihlal, hangi sayının hangi eşiği nasıl aştığını `detail`
  alanında taşır.
- **İki kural motoru birleştirilmez:** `RuleEngine` anlık register durumunu
  (`svd/rules.json`), `TimeSeriesRuleEngine` pencere üzerindeki davranışı
  (`watch/watch_rules.json`) değerlendirir. Mevcut `RuleEngine` değiştirilmez.
- **İsimler karıştırılmaz:** Register Inspector = "Snapshot A / Snapshot B /
  A→B farkı". Değişken İzleyici = "Oturum / Profil / Profil Karşılaştırma".
  İzleyici UI'sinde A/B harfleri kullanılmaz.
- **UART olayları ana thread'de VARIŞ anında damgalanır** — firmware zaman
  damgası değildir. USB-CDC + DMA gecikmesi birkaç ms kayma yaratır; UI'da
  kesikli çizgiyle ve tooltip ile açıkça belirtilir.
- **3000 Hz örnekleme UI thread'ine sinyal başına taşınmaz:** worker en fazla
  30 Hz'de toplu `WatchSampleBatch` yayar. Oturum istatistikleri (min/max/
  ortalama/stddev) halka arabelleğinden bağımsız, artımlı (Welford) tutulur —
  eski örnekler düşse bile oturum istatistiği doğru kalır. Grafiğe basılan
  nokta sayısı örnekleme hızından değil piksel genişliğinden gelir
  (min/max zarf decimation).
- **`IRegisterReader` arayüzü korunur:** `GdbServerReader` ikinci gerçeklemedir;
  `CliRegisterReader` silinmez. `registers/read_backend` **varsayılanı kalıcı
  olarak `"cli"`'dır**; `"gdb"` Ayarlar'dan opt-in seçilir. Ayar `"gdb"` olsa
  bile etkin arka uç her snapshot'ta çözümlenir: gdbserver yolu yoksa veya
  `retain()` başarısızsa sessizce CLI'ya düşülür ve **uygulama çalışması başına
  bir kez** (snapshot başına değil) uyarı verilir. Gerekçe: çalışır durumdaki
  Register Inspector'a süreç/port riskini varsayılan olarak eklemeyiz; hız
  kazancı konfordur, doğruluk değil.
- **`DebugLink` referans sayımlıdır; public `open()`/`close()` YOKTUR.** Tek
  ST-Link'i iki sahip (Register Inspector + Değişken İzleyici) paylaştığı için
  yalnızca `retain()`/`release()` vardır ve her `retain()` tam olarak bir
  `release()` ile eşleşir — hata ve iptal yolları dahil. **Başarısız `retain()`
  sayaç tüketmez; o durumda `release()` çağrılmaz.** "Her ihtimale karşı
  release()" yanlıştır ve sayacı negatife düşürür (`Q_ASSERT` ile yakalanır).
- **Zaman damgası sözleşmesi bağlayıcıdır:** `times[k]`, o örneğin **ilk** `m`
  paketi sokete yazılmadan hemen önce alınır; `skewUs` ise o örneğin **son**
  cevabı ayrıştırılana kadar geçen süredir. Anlamı: "bu değerler
  `[t, t + skewUs/1e6]` penceresinde okundu." Çok bloklu planlar eşzamanlı
  değildir ve bu, UI'da `skewUs` olarak **gösterilir**, gizlenmez.
- **Demo güvenliği birinci sınıf yoldur:** kayıttan oynatma modu canlı yolun
  aynı sinyal zincirini kullanır; uygulamayla birlikte dağıtılan
  `watch/demo/h7_demo_trace.csv` sayesinde ekran ST-Link olmadan tam çalışır.
  Oynatma sırasında göz ardı edilemez bir banner gösterilir.
- **Stack watermark yalnızca bu pipeline ile derlenmiş firmware'de geçerlidir**
  (startup'ta 0xA5A5A5A5 boyama gerekir). Başka bir ELF izlenirken watermark
  kalemi "kullanılamıyor" olarak gösterilir; sahte değer üretilmez.
```

### 12.2 `CLAUDE.md`'de güncellenecek diğer yerler

- **Klasör yapısı** ağacına: `src/modules/debug/`, `src/modules/watcher/`,
  `src/quick/`, `watch/`, `tests/`, `qml/components/watch/` ve yeni dialoglar.
- **Teknik Yığın** tablosuna: `Debug bağlantısı | ST-LINK_gdbserver + GDB RSP (QTcpSocket)`
  ve `Birim test | Qt Test (tests/, ctest)`.
- **Modüller ve Sorumluluklar**'a Modül 6 (`DebugLink`) ve Modül 7 (`VariableWatcher`).
- **AppSettings — Anahtar Listesi**'ne yeni anahtarlar (§12.4).
- **Önemli Dosyalar** tablosuna: `src/modules/debug/GdbRspCodec.h`
  (gözlemci beyaz listesi), `src/modules/debug/DebugLink.h` (retain/release
  sözleşmesi), `src/modules/watcher/ElfTargetMatcher.h` (ELF ↔ hedef eşleşmesi),
  `watch/watch_rules.json`, `watch/watch_presets.json`.
- **Geliştirme Aşamaları** tablosuna yeni satır:
  `| — | Register Inspector + Değişken İzleyici | ✅ Tamamlandı |`

### 12.3 `docs/PROJECT.md`

Yeni bölüm: **"16. Değişken İzleyici"** — mimari şema, thread modeli, RSP
handshake sırası, veri akışı, CSV biçimi, kural motoru, dosya haritası.
Bölüm 3 (Mimari Genel Bakış) ve Bölüm 15 (Dosya Haritası) güncellenir.

### 12.4 `AppSettings` — yeni anahtarların tam listesi

```
tools/gdbserver_path              ST-LINK_gdbserver.exe tam yolu
tools/arm_nm_path                 arm-none-eabi-nm.exe tam yolu
tools/cubeprogrammer_bin_dir      gdbserver -cp argümanı için dizin
watch/gdb_port                    0 = otomatik boş port seç
watch/last_elf_path               son yüklenen ELF
watch/items                       JSON object: kart adı -> [WatchItem]
watch/target_rate_hz              varsayılan 200
watch/window_sec                  grafik pencere genişliği, varsayılan 10
watch/record_dir                  kayıt klasörü
registers/read_backend            "cli" | "gdb" — VARSAYILAN KALICI OLARAK "cli"
                                  ("gdb" yalnizca Ayarlar'dan opt-in; etkin arka
                                   uc her snapshot'ta Bolum 5.1 zinciriyle cozulur)
```

### 12.5 Ertelenmiş doğrulamalar — "kart gelince" listesi

`docs/variable_watcher_findings.md` sonunda açık kontrol listesi:

**STM32F4 (kart elde yok):**
- [ ] gdbserver `-g` attach çalışıyor mu (F4 ST-Link V2-1)
- [ ] Handshake sonrası `S_HALT == 0` **ve** `S_RETIRE_ST == 1`
- [ ] `ElfTargetMatcher` doğru ELF'te yeşil veriyor mu (VTOR flash'ta, 0x08000000)
- [ ] RAM (0x20000000) ve RAM2 (0x20020000) **iki ayrı bölge** okunabiliyor mu
- [ ] `_sstack` yok → fallback `_estack - _Min_Stack_Size` doğru sonuç veriyor mu
- [ ] Örnekleme hızı H7'ye göre nasıl (F4 168 MHz, daha yavaş SWD beklenir)
- [ ] `boards.json` `debug.gdb.verifiedOn` doldurulur

**STM32N6 (kart elde yok, deneysel):**
- [ ] LRUN external-flash boot sonrası `-g` attach çalışıyor mu
- [ ] `S_RETIRE_ST` okunabiliyor mu (TrustZone altında DHCSR erişimi)
- [ ] `ElfTargetMatcher`: LRUN'da VTOR RAM'e taşınmış olabilir → **yanlış alarm**
      beklenir mi? Beklense bile sert blok yok (§6.2); uyarı metni N6 için
      anlamlı mı, kontrol edilir
- [ ] TrustZone/RIF: 0x34000400 RAM non-secure DAP'tan okunabiliyor mu
- [ ] Güvenli bölge okuması **hata olarak** yüzeye çıkıyor mu (sessiz sıfır DEĞİL)
- [ ] `_sstack` var → watermark doğrudan çalışıyor mu
- [ ] `g_npu_exec_ctx` / `NN_Instance_Default` sembolleri preset'e takılıyor mu
- [ ] Başarısızsa `boards.json` `debug.gdb.support` `"unsupported"` yapılır ve
      UI'da neden gösterilir (sessiz başarısızlık yok)

---

## 13. Test altyapısı (Faz 1'de kurulur)

Projede şu an test yok. Qt Test eklenir; **uygulama derlemesi etkilenmez**
(ayrı hedef).

```
tests/
  CMakeLists.txt
  main.cpp                    tum suite'leri QTest::qExec ile sirayla calistirir
  TestGdbRspCodec.h/.cpp
  TestNmSymbolParser.h/.cpp
  TestElfTargetMatcher.h/.cpp
  TestValueCodec.h/.cpp
  TestWatchPlanBuilder.h/.cpp
  TestTraceBuffer.h/.cpp
  TestTimeSeriesRuleEngine.h/.cpp
  TestTraceRecorder.h/.cpp
  fixtures/
    nm_h7.txt                 gercek `nm -S --defined-only` ciktisi
    rsp_replies.txt           RLE'li / parcali / hatali cevap ornekleri
    trace_sample.csv
```

Kök `CMakeLists.txt`:

```cmake
option(STM32AID_BUILD_TESTS "Build unit tests" ON)
if(STM32AID_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
```

`tests/CMakeLists.txt` **yalnızca saf sınıfları** derler (QObject/donanım
bağımlılığı yok) — bu, "hangi sınıf test edilebilir" sorusunun cevabını
mimariye gömer:

`GdbRspCodec.cpp`, `NmSymbolParser.cpp`, `ElfTargetMatcher.cpp`,
`ValueCodec.cpp`, `WatchPlanBuilder.cpp`, `TraceBuffer.cpp`,
`TimeSeriesRuleEngine.cpp`, `TraceRecorder.cpp`, `TracePlayer.cpp`.

`tests/main.cpp` deseni:

```cpp
int main(int argc, char **argv)
{
    int status = 0;
    { TestGdbRspCodec t;         status |= QTest::qExec(&t, argc, argv); }
    { TestNmSymbolParser t;      status |= QTest::qExec(&t, argc, argv); }
    // ...
    return status;
}
```

Çalıştırma:

```powershell
$env:PATH = "D:\Qt\Tools\CMake_64\bin;D:\Qt\Tools\mingw1310_64\bin;$env:PATH"
cmake -B build -S . -DCMAKE_PREFIX_PATH="D:/Qt/6.11.1/mingw_64" `
  -DCMAKE_BUILD_TYPE=Release -G "MinGW Makefiles"
cmake --build build
ctest --test-dir build --output-on-failure
```

> Not: `CMakeLists.txt`'e `Qt6::Test` bileşeni `find_package`'a eklenir
> (yalnızca `STM32AID_BUILD_TESTS` açıkken gerekli, ama koşulsuz aramak
> zararsızdır ve CMake mantığını basit tutar).

**Donanım doğrulaması ayrıdır:** birim testler donanıma asla bağlanmaz.
Canlı doğrulama adımları faz faz yukarıda tanımlıdır ve sonuçları
`docs/variable_watcher_findings.md`'ye yazılır. Bir faz, birim testleri geçse
bile canlı kriterleri karşılamadan "tamamlandı" sayılmaz.

---

## 14. Kapsam dışı bırakılanlar (gerekçeli)

| Bırakılan | Gerekçe |
|---|---|
| **TouchPoint (X/Y) görüntüleyici** | Kullanıcı kararı. Zaman serisinin üzerine ölçülebilir teşhis değeri katmıyor; ikinci bir görselleştirme modunun bakım maliyeti kazancından büyük. |
| **Canlı değişken YAZMA** | Gözlemci ilkesinin ihlali. Çalışan bir inference sırasında yanlış bir yazma model tamponunu bozar ve semptomu teşhisi imkânsız kılar. Bitirme demosunda savunulamaz bir risk. |
| **Trigger / koşullu yakalama** | Ya hedef tarafında destek ya da sürekli tam hızda geriye dönük tamponlama gerektirir. "Bir şey tuhaflaştığında haber ver" ihtiyacını `TimeSeriesRuleEngine` olaydan sonra karşılıyor. Ertelendi. |
| **ML tabanlı anomali tespiti** | Ground truth yok. Etiketlenmiş arıza verisi olmadan eğitilen bir dedektörün doğruluğu ölçülemez; jüriye "model öyle dedi" demek savunulamaz. Deterministik kurallar aritmetiğini gösterebiliyor. |
| **ITM / SWO trace** | Farklı taşıma katmanı (SWO pini + ayrı çözücü), ayrı firmware enstrümantasyonu. İkinci bir protokol yığını demek; RSP yolu zaten ihtiyacı karşılıyor. |
| **RTOS-farkındalıklı görünüm (task listesi, task stack'leri)** | Şablonlarda RTOS yok (bare-metal main loop). Kullanılmayan bir özelliğin altyapısı. |
| **Çoklu probe / eşzamanlı çok kart izleme** | Tek ST-Link hakemi mimarinin temel varsayımı. Çoklu probe, hakem modelinin ve tüm guard'ların yeniden tasarımını gerektirir. |
| **Ham serinin SQLite'a yazılması** | `analysis_records` 1 sn çözünürlüklü, 15 TEXT sütunlu. 3000 Hz seri için yapısal olarak uygunsuz. (Ölçüldü, Faz A.) |
| **Peripheral-arası register kuralları** | `svd/rules.json` içinde zaten gerekçesiyle kapsam dışı (DMA stream yönlendirme tablosu ve pin/AF eşlemesi hiçbir SVD dosyasında yok). Karar tekrarlanır, değiştirilmez. |
| **`Qt Charts` ile grafik** | `QGraphicsScene` tabanlı; birkaç bin noktada kare süresi kabul edilemez. Yerine min/max zarf çizen `QQuickPaintedItem` (~150 satır). Bağımlılık `CMakeLists.txt`'te kalır. |
| **Otomatik kalıntı `gdbserver` süreç öldürme** | Kullanıcının başka bir CubeIDE oturumunu sessizce öldürmek yıkıcı. Tespit + adlandırılmış uyarı + açık kullanıcı onayıyla buton (opsiyonel, Faz 9). |

---

## 15. Riskler ve azaltmaları

| Risk | Etki | Azaltma |
|---|---|---|
| RSP run-length kodlaması (`X*n`) 4 KB sıfır bloklarında kesin devreye girer; Faz A küçük okumalarla bunu görmemiş olabilir | Bozuk veri, sessiz yanlış değerler | `expandRunLength` Faz 1'de **birim testli**; Faz 1 canlı adımında 4 KB okuma ölçülür ve içerik doğrulanır |
| `QNonStop:1` sonrası `%Stop:` bildirimleri cevaplarla karışır | Kilitlenme veya yanlış eşleşme | `GdbRspCodec::extract` bildirimi ayrı `Extract::Notification` olarak döndürür ve yok sayılır |
| gdbserver ST-Link'i tutarken flash/probe sessizce başarısız (CLI exit 0) | Kullanıcı neden olduğunu anlamaz | Adlandırılmış `m_stlinkOwner` hakemi + çıktı işareti ayrıştırma; hata mesajında sahibin adı |
| gdbserver süreç sızıntısı (uygulama çökerse) | ST-Link kilitli kalır | `aboutToQuit` temizliği + port çakışmasında otomatik yeni port + kullanıcıya adlandırılmış uyarı |
| **Karta flash edilmemiş bir ELF yüklenir** → tüm sembol adresleri yanlış | **En kötü hata modu:** araç sessizce tamamen yanlış değer gösterir; grafik akar, hiçbir şey hata vermez | `ElfTargetMatcher` (§6.2): VTOR + vektör tablosu ile `_estack`/`Reset_Handler` karşılaştırması; eşleşmezse sarı rozet + açık onay istenir. Faz 3'te kasten yanlış ELF ile test edilir |
| Hedef gerçekte duruyor ama fark edilmiyor | "Canlı" grafik yalan söyler | Birincil gösterge `S_RETIRE_ST` (mimari, DWT'den bağımsız); `S_HALT` set olursa örnekleme durur, rozet kırmızı |
| DWT firmware'de kapalıysa `DWT_CYCCNT` donuk görünür | Canlılık göstergesi olarak kullanılsaydı **yanlış "hedef durdu"** alarmı | `DWT_CYCCNT` canlılık için kullanılmaz; yalnızca isteğe bağlı izleme kalemi |
| Hedef oturum ortasında reset edilir (kullanıcı butona basar, watchdog, brown-out) | Seri süreksiz sıçrar, kullanıcı bunu veri sanır | `S_RESET_ST` bitiyle tespit → `kind="targetReset"` olayı + grafikte kalın turuncu çizgi; örnekleme durmaz |
| `retain()`/`release()` dengesizliği (özellikle hata/iptal yollarında) | Link asla kapanmaz (ST-Link kilitli) veya erken kapanır (izleme oturumu düşer) | Sözleşme §4.6'da altı maddeyle bağlayıcı; başarısız `retain()`'de `release()` **çağrılmaz** kuralı açık; `refCount()<0` `Q_ASSERT` ile yakalanır |
| Çok bloklu planlarda değişkenler eşzamanlı okunmuyor | Aralarındaki ilişki yanlış yorumlanır | `skewUs` sözleşmesi (§4.2) + UI'da açık gösterim + `roundTripsPerSample()` |
| Dağınık değişken seçimi hızı beklenmedik biçimde düşürür | Kullanıcı aracı yavaş sanır | `roundTripsPerSample()` UI'da gösterilir; `WatchPlanBuilder` agresif birleştirir |
| Firmware `-Os` ile static'leri eleyebilir | Semboller yok, preset boş | `volatile` niteleyici + Faz 5 doğrulamasında `nm` çıktısında varlık kontrolü |
| Stack boyama derin bir çağrı içinden yapılırsa canlı frame'i bozar | Çökme | `StackPaint_Init` `main()` başında, mevcut SP − 128 bayt marja kadar boyar |
| N6 TrustZone okumayı reddeder | Özellik N6'da çalışmaz | `boards.json`'da `experimental`; okuma hatası **yüzeye çıkar**, sıfır olarak gösterilmez; "kart gelince" listesi |
| Yeni `tests/` hedefi ekip derlemesini yavaşlatır | Sürtünme | `STM32AID_BUILD_TESTS=OFF` ile kapatılabilir; testler saf sınıfları derler, Qt Quick/Serial bağlamaz |

---

## 16. Yürütme kontrol listesi (Sonnet için özet)

1. **Faz 1** → `debug/` modülü (**`retain()`/`release()`, public `open()`/`close()`
   YOK**) + DHCSR üç bit sağlık kontrolü (4 Hz) + `tests/` altyapısı +
   `ToolDetector` + `AppSettings` + `boards.json` `debug` bloğu. Birim testler
   yeşil, canlı 5 adım `variable_watcher_findings.md`'ye yazıldı, DHCSR referans
   değeri `0x01010000` ile karşılaştırıldı. Commit.
2. **Faz 2** → `GdbServerReader` + `RegisterInspector` arka uç **çözümleme
   zinciri** (a→d) + Ayarlar UI. **Varsayılan `"cli"` kalır.** Çapraz doğrulama
   (CLI vs GDB snapshot) geçti, ≥5× hız kanıtlandı, gdbserver yolu silinip
   CLI'ya sessiz düşüş + tek seferlik uyarı test edildi. Commit.
3. **Faz 3** → sembol katmanı + `ValueCodec` + **`ElfTargetMatcher`**. Birim
   testler yeşil, gerçek ELF'te `_estack` bulundu, `_Min_Stack_Size` adres
   olarak eklenemedi, doğru ELF yeşil / kasten yanlış ELF sarı uyarı verdi. Commit.
4. **Faz 4** → `WatchPlanBuilder` + `TraceBuffer` + `VariableWatcher` + ST-Link
   hakemi + tablo UI. `uwTick` monoton arttı, 1000 Hz'de ≥800 Hz gerçekleşti,
   UI donmadı. Commit.
5. **Faz 5** → `ai_runner.c` static terfi + `stack_paint.c` + template main.c.
   `nm` sembolleri gördü, `g_ai_last_inference_us` UART `inf_us` ile ±%5 uyuştu.
   Commit.
6. **Faz 6** → `TracePlot` + `TraceEventLog` + grafik UI. 60 s / 3000 Hz akıcı,
   UART olayları hizalı. Commit.
7. **Faz 7** → kayıt/oynatma/profil/dışa aktarma + `watch/demo/h7_demo_trace.csv`.
   ST-Link çıkarılmış halde demo tam çalıştı. Commit.
8. **Faz 8** → `TimeSeriesRuleEngine` + presetler + profil karşılaştırma.
   Sentetik seri testleri yeşil, sızıntı demosu kuralı tetikledi. Commit.
9. **Faz 9** → `CLAUDE.md` ADR bölümü + `PROJECT.md` + findings + "kart gelince"
   listesi. Commit.

**Her commit'te:** AI attribution / co-author satırı **yok**;
`feature/register-inspector` dalında kalınır; `main`'e merge edilmez.

---

## 17. Kapsam önceliği — takvim sıkışırsa ne düşer

9 faz + her fazda canlı donanım doğrulaması büyük bir iştir. Bitirme takvimi
sıkışırsa hangi fazın düşeceği **önceden yazılıdır** ki karar anlık ve panikle
verilmesin.

| Öncelik | Fazlar | Gerekçe |
|---|---|---|
| **Çekirdek (vazgeçilmez)** | 1, 3, 4, 6 | Özelliğin kendisi: link + sembol + örnekleyici + grafik. Biri düşerse ortada "Değişken İzleyici" kalmaz. |
| **Kritik** | 7 | Demo güvenlik ağı. Jüri önünde ST-Link çalışmazsa kayıttan oynatma tek kurtarıcıdır. |
| **Zorunlu** | 9 | Dokümantasyon + ADR. Yazılmazsa sonraki oturum kararları yeniden tartışır. |
| **Farklılaştırıcı, ertelenebilir** | 5, 8 | AI telemetrisi + kural motoru. Araç bunlarsız da çalışır; "STM Studio benzeri"nden "STM Studio + AI" ayrımı burada kaybolur. |
| **En ertelenebilir** | 2 | Çalışan Register Inspector'a dokunuyor; kazancı hız/konfor, yeni yetenek değil. |

**Düşürme sırası (sıkışırsa): 2 → 8 → 5.**

**Faz 6 DÜŞÜRÜLMEZ.** Grafik olmadan elde sayı tablosu kalır; "canlı zaman
serisi" iddiası ortadan kalkar ve özellik amacını kaybeder. Faz 6 düşecek kadar
sıkışıldıysa doğru karar özelliği tamamen ertelemektir, yarısını teslim etmek değil.

**Faz 1 + 3 + 4 tamamlanmadan hiçbir şey gösterilebilir değildir** — bu üçü
bölünemez bir blok olarak planlanmalıdır (link olmadan sembol, sembol olmadan
örnekleyici anlamsızdır).

**Faz 2 düşerse ne kaybedilir:** Register Inspector eski hızında (9 Hz) kalır.
Varsayılan zaten `"cli"` olduğu için **hiçbir regresyon yoktur** — bu, Faz 2'yi
en güvenli düşürülebilir faz yapan şeydir (madde 3'teki varsayılan kararının
ikinci faydası).
