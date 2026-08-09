# CLAUDE.md — STM32 AI Deployer

Bu dosya Claude Code tarafından her oturumda otomatik okunur.
Projeye yeni başlarken veya bağlamı kaybettiğinde bu dosyayı referans al.

---

## Proje Kimliği

**Uygulama adı:** STM32 AI Deployer
**Tür:** Windows masaüstü uygulaması
**Framework:** Qt 6.11.0 / C++17 / CMake
**Amaç:** STM32 mikrodenetleyicilerde çalışan yapay zeka modellerini
karta yüklemek, UART üzerinden gelen inference metriklerini gerçek
zamanlı izlemek ve modelleri karşılaştırmalı olarak analiz etmek.

**Ekip:**
- Muhammet Ali Şeker
- Furkan Talha Kasım
- Kadir Mert Abatay

**Danışman:** Ali Sarıkaş
**Kurum:** Marmara Üniversitesi — Bilgisayar Mühendisliği Bitirme Projesi 2025-2026

---

## Hedef Donanım

| Kart     | Flash   | RAM    | Hız     | Hedef Model |
|----------|---------|--------|---------|-------------|
| STM32F4  | 1024 KB | 192 KB | 168 MHz | MLP INT8    |
| STM32H7  | 2048 KB | 1024 KB| 480 MHz | 1D CNN INT8 |
| STM32N6  | 4096 KB | 4096 KB| 800 MHz | LSTM / KWS  |

> **STM32N6 (deneysel):** LRUN external-flash boot çalışıyor; UART tarafında
> komut-cevap yerine reset + pasif yakalama (`LPUART1 @ 209700`) kullanılıyor.
> Doğrulanmış startup/linker/HAL şablon seti hâlâ açık iş. Detay:
> [`docs/n6_kaldigimiz_yer.md`](docs/n6_kaldigimiz_yer.md).

---

## Teknik Yığın

| Bileşen          | Teknoloji                       |
|------------------|---------------------------------|
| Framework        | Qt 6.11.0 (mingw_64)            |
| UI               | **Qt Quick / QML** (ana arayüz, `qml/`) + Qt Widgets (yalnızca `SplashScreen`) |
| Derleyici        | MinGW 13.1.0 (GCC)              |
| Derleme sistemi  | CMake 3.30.5 (QMake kullanılmaz)|
| Dil              | C++17 + QML/JavaScript          |
| Veritabanı       | SQLite — Qt SQL modülü (tek esnek `analysis_records` tablosu, bkz. aşağıda) |
| Seri port        | Qt Serial Port modülü           |
| Grafikler        | Qt Charts modülü                |
| Ayar depolama    | QSettings                       |
| Süreç yönetimi   | QProcess — ST-Link CLI çağrısı  |
| Debug bağlantısı | `ST-LINK_gdbserver.exe` + GDB RSP (QTcpSocket) — Register Inspector'ın GDB arka ucu + Değişken İzleyici |
| Thread mimarisi  | QThread + Worker pattern        |
| Birim test       | Qt Test (`tests/`, `ctest`)     |
| Platform         | Windows 10/11 (yalnızca)        |

> **Not:** Aktif UI tamamen QML'dir. `Backend` (`src/bridge/Backend.*`) QML ↔
> C++ arasındaki tek cephe (facade) sınıfıdır — QML doğrudan manager'lara
> erişmez. `src/ui/*Tab.*` ve `src/mainwindow.*` erken bir Qt Widgets
> denemesinden kalan **ölü koddur** (CMake hâlâ derliyor, çalışma zamanında
> kullanılmaz). Tam mimari için [`docs/PROJECT.md`](docs/PROJECT.md).

---

## Klasör Yapısı

```
stm32-ai-deployer-app/
├── CLAUDE.md                        ← Bu dosya
├── CMakeLists.txt
├── README.md
├── TODO.md                          ← Build komutları + açık yapılacaklar
├── .gitignore
│
├── src/
│   ├── main.cpp                     ← Bootstrap, QML motoru, context property'ler
│   ├── mainwindow.h / .cpp          ← ÖLÜ KOD (Widgets denemesi, kullanılmıyor)
│   ├── ui/                          ← ÖLÜ KOD — BoardTab/FlashTab/MonitorTab/
│   │                                   AnalysisTab/BenchmarkTab/Sidebar/
│   │                                   SettingsDialog/PipelineWizard (Widgets)
│   │                                   İSTİSNA: SplashScreen.h/.cpp hâlâ aktif
│   │
│   ├── bridge/
│   │   └── Backend.h / .cpp         ← QML ↔ C++ TEK cephe (facade) sınıfı
│   │
│   ├── core/
│   │   ├── AppSettings.h / .cpp     ← QSettings wrapper
│   │   ├── AppState.h / .cpp        ← Merkezi çalışma zamanı durumu
│   │   ├── ToolDetector.h / .cpp    ← GCC/Make/CLI otomatik tespit
│   │   └── TemplateEngine.h / .cpp  ← {{PLACEHOLDER}} template sistemi
│   │
│   └── modules/
│       ├── board/
│       │   ├── BoardManager.h / .cpp
│       │   └── BoardPresets.h       ← F4/H7/N6 sabit tanımları
│       ├── flash/
│       │   ├── FlashManager.h / .cpp
│       │   ├── CliRunner.h / .cpp   ← QProcess wrapper
│       │   ├── XCubeAIRunner.h / .cpp ← stedgeai CLI wrapper
│       │   ├── PipelineConfig.h     ← Pipeline yapılandırma struct
│       │   └── PipelineRunner.h / .cpp ← .tflite→C→GCC→Flash orkestrasyonu
│       ├── serial/
│       │   ├── SerialManager.h / .cpp ← Thread'li seri port yöneticisi
│       │   ├── SerialWorker.h / .cpp ← QThread worker
│       │   ├── SerialSimulator.h / .cpp ← Donanımsız sahte § veri kaynağı
│       │   └── PacketParser.h / .cpp ← § JSON protokol parser
│       ├── analysis/
│       │   └── AnalysisManager.h / .cpp ← SQLite (tek esnek tablo)
│       ├── simulation/
│       │   └── FactorySimulator.h / .cpp ← Fabrika demo veri motoru
│       ├── registers/                ← Register Inspector (SvdCatalog, RuleEngine,
│       │                                RegisterInspector, CliRegisterReader,
│       │                                GdbServerReader, RegisterAdvisor …)
│       ├── debug/                    ← DebugLink modülü (Register Inspector +
│       │   ├── DebugLinkTypes.h        Değişken İzleyici'nin PAYLAŞTIĞI tek ST-Link)
│       │   ├── GdbRspCodec.h / .cpp  ← RSP çerçeveleme + gözlemci beyaz listesi
│       │   ├── GdbServerProcess.h / .cpp ← ST-LINK_gdbserver.exe süreç yönetimi
│       │   ├── DebugLinkWorker.h / .cpp ← QThread worker (QTcpSocket)
│       │   └── DebugLink.h / .cpp    ← Ana thread cephesi, retain()/release()
│       └── watcher/                  ← Değişken İzleyici (Faz 1-8)
│           ├── SymbolModel.h · NmSymbolParser.h/.cpp ← nm çıktısı ayrıştırma
│           ├── ElfSymbolSource.h/.cpp ← arm-none-eabi-nm QProcess sarmalayıcı
│           ├── ElfTargetMatcher.h/.cpp ← ELF↔hedef VTOR/vektör tablosu eşleşmesi
│           ├── WatchModel.h · ValueCodec.h/.cpp ← WatchItem/WatchStats + decode/format
│           ├── WatchPlanBuilder.h/.cpp ← okuma isteklerini birleştirir
│           ├── TraceBuffer.h/.cpp    ← halka arabellek + decimate + rawWindow
│           ├── WatchSampler.h/.cpp   ← bir örneğin ham yanıtlarını decode eder
│           ├── VariableWatcher.h/.cpp ← ana orkestratör (örnekleme+kayıt+oynatma)
│           ├── TraceEventLog.h/.cpp  ← ortak zaman ekseni olayları
│           ├── TraceRecorder.h/.cpp · TracePlayer.h/.cpp ← CSV kayıt/oynatma
│           ├── WatchProfile.h/.cpp   ← analysis_records "watch_profile" satırları
│           ├── TimeSeriesRuleModel.h · TimeSeriesRuleEngine.h/.cpp ← Faz 8 kural motoru
│           └── WatchPresetMatcher.h/.cpp ← AI-farkındalıklı preset eşleştirme
│
├── src/quick/
│   └── TracePlot.h / .cpp           ← QQuickPaintedItem, piksel-sütunu min/max grafik
│
├── qml/                              ← AKTİF UI — tamamı burada
│   ├── Main.qml · Theme.qml · MockData.qml
│   ├── screens/                     ← Dashboard/Board/Flash/Monitor/Benchmark/Analysis/
│   │                                   Register/Watch
│   ├── components/                  ← AppButton/Card/DataTable/Terminal/TitleBar …
│   │   └── watch/                   ← WatchItemTable/WatchToolbar/WatchLinkStatus/
│   │                                   TracePlotView/WatchEventLane/WatchRecordingBar/
│   │                                   WatchPlaybackBanner/WatchRuleFeed
│   ├── dialogs/                     ← SettingsDialog/PipelineWizard/AboutDialog/
│   │                                   SymbolPickerDialog/WatchItemDialog/
│   │                                   ProfileCompareDialog
│   └── factory/                     ← FactorySimWindow/Dashboard/Map/ZoneDetail/NodeDetail
│
├── resources/
│   ├── app.qrc
│   ├── style.qss                    ← Yalnızca Widgets SplashScreen için stil
│   └── icons/
│
├── templates/                       ← STM32 proje şablonları
│   ├── README.md
│   ├── base/
│   │   ├── STM32F4/                 ← Makefile, ld, startup, Inc/, Src/
│   │   ├── STM32H7/
│   │   └── STM32N6/                 ← deneysel (bkz. n6_kaldigimiz_yer.md)
│   ├── sensors/
│   │   ├── MPU6050/                 ← I2C IMU (HAR)
│   │   ├── BME280/                  ← I2C çevre sensörü
│   │   └── PDM_MIC/                 ← SAI PDM mikrofon (KWS)
│   └── ai_glue/
│       ├── ai_runner.c / .h         ← X-CUBE-AI inference wrapper
│       ├── uart_report.c / .h       ← Protokol v1.0 UART raporlama
│       └── stack_paint.c / .h       ← Stack watermark boyama (0xA5A5A5A5)
│
├── watch/                            ← Değişken İzleyici veri dosyaları (§ aşağıda)
│   ├── README.md · watch_rules.json · watch_presets.json
│   └── demo/h7_demo_trace.csv       ← Gerçek H7 kaydı, ST-Link'siz demo için
│
├── tests/                            ← Qt Test, saf sınıflar (donanım/QObject bağımlılığı yok)
│   ├── CMakeLists.txt · main.cpp
│   └── Test*.h / .cpp                ← bkz. docs/variable_watcher_plan.md Bölüm 13
│
└── docs/
    ├── PROJECT.md                   ← Tüm mimari, uçtan uca (ana referans)
    ├── protocol_v1.md               ← UART protokol referansı
    ├── n6_kaldigimiz_yer.md         ← STM32N6 boot/flash geçmişi + güncel durum
    ├── factory_simulation_plan.md   ← Fabrika Sim. orijinal tasarım planı (tarihi)
    ├── lstm_stm32_export.md         ← LSTM → X-CUBE-AI uyumlu TFLite export rehberi
    ├── register_inspector_plan.md / _findings.md ← Register Inspector tasarım + doğrulama
    └── variable_watcher_plan.md / _findings.md   ← Değişken İzleyici tasarım + doğrulama
```

---

## UART Protokolü (Aşama 0 — Kesinleşti)

Firmware → Qt yönünde her paket bu formattadır:

```
§{JSON}\r\n
```

- `§` = UTF-8 0xC2 0xA7 — start marker, her paketin ilk 2 byte'ı
- Gövde: tek satır compact JSON
- Bitiş: `\r\n`
- Tüm sayısal değerler **integer** — `float` / `%f` kullanılmaz
- UART gönderimi firmware tarafında **DMA** (HAL_UART_Transmit_DMA) — blocking yasak

### Mesaj Tipleri

```json
// Inference metriği — model çalıştıktan sonra
{"t":"inf","model":"MLP_INT8","inf_us":8200,"ram_b":3072,"acc_pct":96,"label":"walking","card":"STM32F4"}

// Gerçek sensör örneği + isteğe bağlı inference
{"t":"sensor","sensor":"BME280","seq":42,"values":[25120,100840,43600],"unit":"milli","model":"anomaly_cnn_int8","inf_us":956,"ram_b":6594,"acc_pct":90,"label":"normal","card":"STM32H7"}

// Sistem durumu — 1 Hz periyodik
{"t":"sys","uptime_s":42,"temp_c":38,"free_ram_b":185000,"state":"running"}

// Başlangıç — reset sonrası bir kez
{"t":"boot","card":"STM32F4","sdk":"EdgeAI_v1.0","model":"MLP_INT8","baud":115200}

// Hata
{"t":"err","code":3,"msg":"sensor_timeout"}
```

### Qt Parser Özeti

```cpp
// SerialWorker — QThread içinde çalışır
// § ile başlamayan satırlar → rawLog sinyali (ham terminal)
// § ile başlayan satırlar  → JSON parse → tip bazlı sinyal
emit inferenceReceived(QJsonObject);
emit sysReceived(QJsonObject);
emit bootReceived(QJsonObject);
emit errorReceived(QJsonObject);
```

---

## Modüller ve Sorumluluklar

### Modül 0 — Backend (`src/bridge/`)
- QML'in gördüğü **tek cephe** sınıf; tüm manager'ları sarar
- Verileri QML dostu tiplere (`QVariantList`/`QVariantMap`) çevirir
- Araçlar, seri port, kart seçimi, monitör, simülasyon, flash, pipeline,
  benchmark, analiz — hepsi `Q_INVOKABLE` metotlarla buradan geçer

### Modül 1 — BoardManager
- STM32F4 / H7 / N6 preset yönetimi
- Custom kart JSON config okuma/yazma
- Aktif kart bilgisini uygulama genelinde yayar

### Modül 2 — FlashManager + CliRunner + XCubeAIRunner + PipelineRunner
- `.hex` / `.bin` dosya doğrulama
- STM32_Programmer_CLI çağrısı (QProcess)
- Flash log çıktısını UI'ya iletir
- CLI yolu AppSettings'ten okunur
- `PipelineRunner`: `.tflite → stedgeai → TemplateEngine → gcc → flash` beş adımını orkestre eder

### Modül 3 — SerialManager + SerialWorker + PacketParser
- `SerialManager` ana thread'de yaşar, `QThread` içinde `SerialWorker`'ı taşır
- QSerialPort işlemleri worker thread'inde (ana thread asla bloklanmaz)
- VID/PID ile ST-Link otomatik tespiti (VID: 0x0483)
- § protokol parser — `inf`/`sys`/`boot`/`err`/`bench`/`sensor` tipleri
- Circular buffer — son 500 inference kaydı tutulur
- `SerialSimulator`: donanımsız demo için sahte § veri kaynağı

### Modül 4 — AnalysisManager
- Esnek `kind` + `cells` modeliyle tek tablo (`analysis_records`) kullanır
- benchmark / simulation / sensor / compiled kayıtlarını aynı şemada saklar
- CSV/PDF dışa aktarım, kayıt silme

### Modül 5 — FactorySimulator (`src/modules/simulation/`)
- Gerçek donanım olmadan 5 bölge / 20 düğüm / ~68 sensörlü fabrikayı simüle eder
- Detay: [`docs/PROJECT.md`](docs/PROJECT.md) Bölüm 8

### Modül 6 — DebugLink (`src/modules/debug/`)
- Register Inspector'ın GDB arka ucu (`GdbServerReader`) VE Değişken
  İzleyici'nin **PAYLAŞTIĞI TEK** ST-Link bağlantısı — `retain()`/`release()`
  referans sayımlı, public `open()`/`close()` yok
- `GdbRspCodec`: RSP çerçeveleme + gözlemci beyaz listesi (`isAllowedOutgoing()`)
- `GdbServerProcess`: `ST-LINK_gdbserver.exe` süreç yönetimi (QProcess)
- `DebugLinkWorker`: ana thread'i asla bloklamayan `QThread` + `QTcpSocket`
- 4 Hz DHCSR sağlık kontrolü: `S_RETIRE_ST` canlılık, `S_HALT` durma,
  `S_RESET_ST` → `targetReset` olayı

### Modül 7 — VariableWatcher (`src/modules/watcher/`)
- "STM Studio benzeri" canlı değişken izleme — hedef ASLA durdurulmaz/
  yazılmaz (gözlemci ilkesi)
- Sembol katmanı (`nm` çıktısı) → `WatchPlanBuilder` (okuma birleştirme) →
  `WatchSampler` (decode) → `TraceBuffer` (halka arabellek + istatistik)
- `TraceRecorder`/`TracePlayer`: CSV kayıt + "güvenli mod" (ST-Link'siz) oynatma
- `TimeSeriesRuleEngine`: pencereli deterministik kural motoru (eşik/z-skoru/
  drift) — `src/modules/registers/RuleEngine.h` ile **karıştırılmaz**
- `WatchPresetMatcher`: AI-farkındalıklı otomatik izleme listesi (`watch/watch_presets.json`)
- Detay ve kalıcı mimari kararlar: yukarıdaki "Değişken İzleyici — Kalıcı
  Mimari Kararları" bölümü + [`docs/variable_watcher_plan.md`](docs/variable_watcher_plan.md)

---

## Register Inspector — Kalıcı Mimari Kararları

> Bu bölüm bir karar kaydıdır (ADR benzeri) — Register Inspector üzerinde
> çalışan her oturumda geçerlidir, unutulmamalı/çiğnenmemelidir.

- LLM teşhis katmanına ASLA tam snapshot dump'ı (20k+ satır / tüm
  register/field) gönderme. LLM girdisi daima damıtılmış olacak: (1) diff
  sonucu (sadece değişen field'lar, decode+enum), (2) kural motoru
  ihlalleri, (3) yalnızca ilgili/seçili peripheral subset'i. Sebep: token
  maliyeti + alakasız register'ların teşhis doğruluğunu bozması.
- Tam JSON export (arşiv/downstream) AYRI bir çıktıdır ve tam kalır; LLM
  girdisiyle karıştırılmaz. İki ayrı fonksiyon.
- LLM'den hex değil, field+enum seviyesinde ÖNERİ istenir; çıktı "hipotez",
  "fix" değil.
- Okuma katmanı `IRegisterReader` arayüzü arkasında; mevcut CLI
  implementasyonu (`CliRegisterReader`) onun bir gerçeklemesi. Native
  (probe-rs/pyOCD) arka uç ileride eklenebilir olmalı.
- "Reset'ten farklı" = tek snapshot, SVD reset değeriyle kıyas. "A → B
  farkı" = iki snapshot arası diff. Farklı kavramlar, isimleri/UI etiketleri
  karıştırılmaz.
- Araç gözlemci: register YAZMA yok, canlı polling yok, snapshot modeli
  korunur.

---

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
- **Olay ekseni ile örnek ekseni TEK bir orijini paylaşır.** Örnek zaman
  damgaları `DebugLinkWorker`'ın soket bağlanınca başlayan saatinden gelir;
  `TraceEventLog` ise link açıldığında sıfırlanır — yani doğal olarak
  handshake süresi kadar GERİDEDİR. Bu yüzden `TraceEventLog::reset()`
  **daima** `DebugLink::sessionElapsedAtOpen()` ile çağrılır. Sıfırdan
  başlatılırsa iki eksen kayar (H723ZG'de ölçülen: 61 ms) ve bu tek başına
  `watch_rules.json`'daki ±50 ms'lik olay kapısını her zaman reddettirir.
  Denetim kaydı: [`docs/variable_watcher_review.md`](docs/variable_watcher_review.md) K-2.
- **`WatchSampler`'ın `ok` bayrağı yok sayılamaz.** Başarısız okuma 0.0
  döndürür ve bu gerçek bir sıfırdan ayırt edilemez; çağıran son geçerli
  değeri korumak zorundadır, yoksa yanlış alarm üretir (review K-4).
- **gdbserver `-e` (persistent) ile başlatılmaz.** Persistent modda sunucu
  biz ayrıldıktan sonra da dinlemeye devam eder, dolayısıyla onu bizim
  öldürmemiz gerekir; Windows'ta `QProcess::terminate()` konsol sürecine
  ulaşamadığı için bu fiilen `TerminateProcess()` olur ve ST-Link'in USB
  ucunu **fiziksel çıkar-tak gerektirecek** şekilde kilitler
  (`DEV_USB_COMM_ERR`, tüm ST araçlarını etkiler). `-e` olmadan sunucu biz
  detach edince kendiliğinden ve temiz kapanır. Hazır olma tespiti bu yüzden
  TCP yoklamasıyla değil, sunucunun kendi "Waiting for debugger connection"
  satırıyla yapılır (yoklama bağlantısı persistent olmayan sunucuyu
  kapatırdı). Review K-3.
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
  kalemi "kullanılamıyor" olarak gösterilir; sahte değer üretilmez. **Bilinen
  durum (2026-08):** `WatchPresetMatcher` adres aralığını doğru çözer ve
  `WatchPlanBuilder::buildRegionScans()` okuma planını doğru kurar, ama bu
  ikisi arasındaki bayt-tarama DECODE adımı henüz `WatchSampler`'a
  bağlanmadı — RegionScan kalemleri şu an `0.0/ok=false` döner. Bunun
  sonucu olarak `watch/watch_rules.json`'daki iki `stackWatermark` kuralı
  `"enabled": false` ile **kapatılmıştır**: hiçbir kalem `role=stackWatermark`
  taşıyamayacağı için etkin bırakmak, gerçekleşemeyecek bir tespit vaat
  etmek olurdu. RegionScan decode'u yazıldığında ikisi birlikte açılır.
  Detay: `docs/variable_watcher_findings.md` Bölüm 17.3 ve
  `docs/variable_watcher_review.md`.

---

## Veritabanı Şeması (SQLite) — GERÇEK ŞEMA

`AnalysisManager` **tek, esnek bir tablo** kullanır — aşağıdaki normalize 4
tablolu yapı kodda **implemente edilmemiştir**, yalnızca Aşama 5 için
düşünülen bir hedeftir:

```sql
CREATE TABLE IF NOT EXISTS analysis_records (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    kind       TEXT NOT NULL,      -- "benchmark" | "simulation" | "sensor" | "compiled"
    created_at TEXT NOT NULL,      -- ISO 8601
    c0 TEXT, c1 TEXT, c2 TEXT, c3 TEXT, c4 TEXT,
    c5 TEXT, c6 TEXT, c7 TEXT, c8 TEXT, c9 TEXT,
    c10 TEXT, c11 TEXT, c12 TEXT, c13 TEXT, c14 TEXT
);
```

`kind` satırın hangi ekrandan geldiğini, `c0..c14` ise `Backend`'in doldurduğu
sıralı hücre listesini (tipsiz, `kind`'a göre anlam kazanır) tutar.

> **Aşama 5 hedefi (henüz yok):** `boards` / `sessions` / `inference_logs` /
> `sys_logs` şeklinde normalize edilmiş 4 tablo. Bu şema Aşama 5 başladığında
> ya implemente edilmeli ya da bu bölümdeki hedef mevcut esnek modele göre
> revize edilmelidir.

---

## Geliştirme Kuralları

### C++ Kuralları
- Header guard: `#pragma once` (ifndef/define kullanılmaz)
- Namespace: `using namespace std` veya `using namespace Qt` global scope'ta yasak
- Her sınıf kendi `.h` / `.cpp` çiftine sahip — tek dosyada birden fazla sınıf yok
- Connect syntax: her zaman yeni pointer-to-member syntax
  ```cpp
  // DOĞRU
  connect(obj, &Class::signal, this, &Class::slot);
  // YANLIŞ
  connect(obj, SIGNAL(signal()), this, SLOT(slot()));
  ```
- Smart pointer tercih edilir: `std::unique_ptr`, `std::shared_ptr`
- Qt ownership varsa raw pointer kabul edilebilir (parent-child)

### Qt Kuralları
- `QMake` / `.pro` dosyası oluşturulmaz — yalnızca CMake
- Thread: `QSerialPort` ve uzun süren işlemler ana thread'de çalışmaz
- UI güncellemesi: her zaman ana thread'den — worker'dan `emit` ile
- `QSettings` doğrudan erişim yerine `AppSettings` wrapper üzerinden

### Dosya Kuralları
- Kaynak dosyalar `src/` altında, modül klasörlerine göre ayrılmış
- UI dosyaları `qml/` altında (`screens/`, `components/`, `dialogs/`, `factory/`)
- Görünüm `qml/Theme.qml` üzerinden yönetilir; `resources/style.qss` yalnızca
  Widgets tabanlı `SplashScreen` için kullanılır — inline stil yazılmaz
- İkonlar `resources/icons/` altında

### Yorum Kuralları
- Kod İngilizce, yorumlar İngilizce
- Tamamlanmamış özellikler: `// TODO(aşama-N): açıklama`
- Kritik davranış: `// NOTE: açıklama`
- Bilinen sınırlama: `// FIXME: açıklama`

---

## AppSettings — Anahtar Listesi

```cpp
// STM32_Programmer_CLI.exe tam yolu
"programmer/cli_path"

// stedgeai.exe tam yolu (X-CUBE-AI)
"tools/xcubeai_cli_path"

// arm-none-eabi-gcc tam yolu
"tools/gcc_path"

// make.exe tam yolu
"tools/make_path"

// STM32Cube SDK kök yolu
"tools/cube_sdk_path"

// İlk açılış araç taraması yapıldı mı
"tools/auto_detected"

// Son kullanılan COM port
"serial/last_com_port"

// Son kullanılan baud
"serial/last_baud"

// Son seçilen kart adı
"board/last_board"

// Kullanıcı tanımlı özel kartlar (JSON array)
"boards/custom"

// Uygulama teması
"ui/theme"  // default: "dark"

// Son firmware klasörü
"flash/last_firmware_dir"

// Son model klasörü
"flash/last_model_dir"

// Son pipeline çıktı klasörü
"flash/last_output_dir"

// Son deploy edilen model bilgisi (Benchmark ekranı için)
"benchmark/deployed_model_name"
"benchmark/deployed_model_path"
"benchmark/deployed_output_dir"
"benchmark/deployed_sensor_type"

// ST-LINK_gdbserver.exe tam yolu (Register Inspector GDB arka ucu + Değişken İzleyici)
"tools/gdbserver_path"

// arm-none-eabi-nm.exe tam yolu (Değişken İzleyici sembol katmanı)
"tools/arm_nm_path"

// STM32_Programmer_CLI.exe'yi içeren dizin (gdbserver'ın -cp argümanı)
"tools/cubeprogrammer_bin_dir"

// Register Inspector okuma arka ucu tercihi: "cli" | "gdb"
// VARSAYILAN KALICI OLARAK "cli" — "gdb" yalnızca Ayarlar'dan opt-in
"registers/read_backend"

// gdbserver TCP portu; 0 = otomatik boş port seç
"watch/gdb_port"

// Son yüklenen ELF yolu (Değişken İzleyici)
"watch/last_elf_path"

// Kart adı -> [WatchItem] JSON eşlemesi
"watch/items"

// Varsayılan örnekleme hızı (Hz), varsayılan 200
"watch/target_rate_hz"

// Yeni izleme kayıtları için önerilen klasör
"watch/record_dir"
```

> Tam ve güncel anahtar tanımları için tek doğruluk kaynağı:
> `src/core/AppSettings.h`.

---

## Geliştirme Aşamaları

| Aşama | Başlık                  | Durum       |
|-------|-------------------------|-------------|
| 0     | Protokol standardı      | ✅ Tamamlandı |
| 1     | Qt proje iskeleti       | ✅ Tamamlandı |
| 2     | Ana UI iskeleti         | ✅ Tamamlandı |
| 3     | Serial port modülü      | ✅ Tamamlandı |
| 4     | Flash modülü            | ✅ Tamamlandı |
| 4.6   | X-CUBE-AI CLI entegrasyonu | ✅ Tamamlandı |
| 4.8   | Template Framework + Pipeline Wizard | ✅ Tamamlandı |
| —     | QML arayüze geçiş + Fabrika Simülasyonu | ✅ Tamamlandı (aktif geliştirme) |
| —     | Register Inspector + Değişken İzleyici | ✅ Tamamlandı |
| 5     | Veritabanı ve kayıt     | ⏳ Bekliyor  |
| 6     | Canlı dashboard         | ⏳ Bekliyor  |
| 7     | Model karşılaştırma     | ⏳ Bekliyor  |
| 8     | Bitirme demo hazırlığı  | ⏳ Bekliyor  |

---

## Sık Kullanılan Komutlar

```powershell
# CMake yolu (Qt ile birlikte gelir)
$env:PATH = "C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\mingw1310_64\bin;$env:PATH"

# Projeyi yapılandır
cmake -B build -S . `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.11.0/mingw_64" `
  -DCMAKE_BUILD_TYPE=Release `
  -G "MinGW Makefiles"

# Derle
cmake --build build

# Temizle
cmake --build build --target clean

# Doğrudan çalıştır
.\build\STM32AiDeployer.exe
```

---

## Önemli Dosyalar

| Dosya                      | Ne Yapar                                    |
|----------------------------|---------------------------------------------|
| `src/core/AppSettings.h`   | Tüm QSettings anahtarlarının merkezi noktası|
| `src/bridge/Backend.h`     | QML ↔ C++ tek cephe sınıfı                  |
| `src/modules/analysis/AnalysisManager.h` | SQLite bağlantısı ve tek-tablo şema yönetimi |
| `src/modules/serial/PacketParser.h` | § protokol parser — değiştirme    |
| `qml/Theme.qml`            | Merkezi QML tema/renk tanımları              |
| `resources/style.qss`      | Yalnızca Widgets `SplashScreen` için stil    |
| `docs/PROJECT.md`          | Ana mimari referansı — güncel durumu yansıtır |
| `docs/protocol_v1.md`      | UART protokol referansı                     |
| `docs/n6_kaldigimiz_yer.md` | STM32N6 boot/flash geçmişi + güncel durum   |
| `src/modules/debug/GdbRspCodec.h` | RSP çerçeveleme + gözlemci beyaz listesi (`isAllowedOutgoing()`) |
| `src/modules/debug/DebugLink.h` | Paylaşılan ST-Link — `retain()`/`release()` sözleşmesi |
| `src/modules/watcher/ElfTargetMatcher.h` | ELF ↔ hedef eşleşmesi (VTOR + vektör tablosu) |
| `watch/watch_rules.json`   | `TimeSeriesRuleEngine` kuralları (pencereli, deterministik) |
| `watch/watch_presets.json` | AI-farkındalıklı otomatik izleme listesi     |
| `docs/variable_watcher_plan.md` / `_findings.md` | Değişken İzleyici tasarım + canlı doğrulama sonuçları |

---

## Araç Yolları — Otomatik Tespit (Aşama 4.8)

`ToolDetector` sınıfı ilk açılışta tüm araçları arar ve `AppSettings`'e kaydeder.

| Araç                  | Arama sırası                                         |
|-----------------------|------------------------------------------------------|
| arm-none-eabi-gcc     | PATH → STM32CubeIDE plugins → sabit yollar          |
| make                  | PATH → STM32CubeIDE plugins → sabit yollar          |
| STM32_Programmer_CLI  | PATH → Program Files/ST/.../bin/                    |
| stedgeai              | PATH → ~/STM32Cube/Repository/.../Utilities/windows/ |

**Bilinen sabit yollar:**
- GCC: `C:\ST\STM32CubeIDE_1.19.0\...\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1...\tools\bin\arm-none-eabi-gcc.exe`
- Make: `C:\ST\STM32CubeIDE_1.19.0\...\com.st.stm32cube.ide.mcu.externaltools.make.win32_2.2.0...\tools\bin\make.exe`

## Template Yapısı (Aşama 4.8)

```
templates/base/       → Kart başına HAL iskeleti (Makefile, ld, startup, Src/, Inc/)
templates/sensors/    → Sensör başına okuma kodu (MPU6050, BME280, PDM_MIC)
templates/ai_glue/    → X-CUBE-AI wrapper (ai_runner) + UART raporlama (uart_report)
```

**Placeholder sistemi:** `{{KEY}}` → TemplateEngine tarafından wizard değerleriyle değiştirilir.

## Pipeline Akışı (Aşama 4.8)

```
.tflite → stedgeai generate (C kodu)
       → TemplateEngine (board + sensor + ai_glue şablonları kopyala)
       → arm-none-eabi-gcc via Makefile (derleme)
       → .elf
       → STM32_Programmer_CLI (flash)
```

`PipelineWizard` (4 sayfa): Model → Sensör → Özet/Araç Doğrulama → Derleme+Flash
`PipelineRunner`: Tüm adımları sırayla `XCubeAIRunner` ve `CliRunner` ile çalıştırır.

---

## Bu Dosyayı Güncelleme Kuralı

Yeni bir aşama tamamlandığında:
1. Aşama tablosunda durumu ✅ yap
2. Yeni eklenen sınıflar varsa klasör yapısına ekle
3. Yeni AppSettings anahtarları varsa listeye ekle
4. Değişen protokol varsa UART bölümünü güncelle
