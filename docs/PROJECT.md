# STM32 AI Deployer — Detaylı Proje Dokümanı

> Bu doküman projenin **tüm mimarisini, modüllerini, veri akışını ve çalışma
> mantığını** uçtan uca açıklar. Yeni bir geliştiriciye veya bitirme jürisine
> projeyi sıfırdan tanıtmak için hazırlanmıştır.

---

## 1. Proje Özeti

**STM32 AI Deployer**, STM32 mikrodenetleyiciler üzerinde çalışan yapay zeka
modellerinin tüm yaşam döngüsünü tek bir masaüstü uygulamasından yöneten bir
araçtır. Uygulama dört temel işi bir arada yapar:

1. **Dağıtım (Deploy):** Bir `.tflite` modelini alır, X-CUBE-AI ile C koduna
   çevirir, GCC ile derler ve STM32 kartına flash'lar — tek tıkla uçtan uca.
2. **İzleme (Monitor):** Karttan UART üzerinden gelen inference metriklerini
   (`§{JSON}` protokolü) gerçek zamanlı olarak ekranda gösterir.
3. **Analiz & Benchmark:** Çalıştırılan modellerin hız/RAM/doğruluk değerlerini
   SQLite veritabanında saklar, karşılaştırmalı tablolar ve CSV/PDF dışa
   aktarımı üretir.
4. **Fabrika Simülasyonu:** Gerçek donanım olmadan, 5 bölgeli / 20 düğümlü /
   ~68 sensörlü büyük bir akıllı fabrikayı sentetik canlı veriyle simüle eden
   bir demo modu.

| Özellik | Değer |
|---------|-------|
| Tür | Windows masaüstü uygulaması (yalnızca Win 10/11) |
| Framework | Qt 6.11.0 (mingw_64) |
| UI teknolojisi | **Qt Quick / QML** (ana arayüz) + Qt Widgets (yardımcı pencereler) |
| Derleyici | MinGW 13.1.0 (GCC) |
| Derleme sistemi | CMake 3.21+ (QMake kullanılmaz) |
| Dil | C++17 + QML/JavaScript |
| Veritabanı | SQLite (Qt SQL) |
| Seri port | Qt SerialPort |
| Grafikler | Qt Charts |
| Süreç yönetimi | QProcess (CLI araçlarını çağırmak için) |
| Thread modeli | QThread + Worker (seri port ana thread'i bloklamaz) |

**Ekip:** Muhammet Ali Şeker · Furkan Talha Kasım · Kadir Mert Abatay
**Danışman:** Ali Sarıkaş — Marmara Üniversitesi Bilgisayar Mühendisliği, Bitirme Projesi 2025-2026

---

## 2. Hedef Donanım

Uygulama üç STM32 sınıfını hedefler; her sınıf farklı bir model tipine optimize edilmiştir:

| Kart    | Flash   | RAM     | Hız     | Tipik Model | Not |
|---------|---------|---------|---------|-------------|-----|
| STM32F4 | 1024 KB | 192 KB  | 168 MHz | MLP INT8    | Giriş seviyesi, düşük güç |
| STM32H7 | 2048 KB | 1024 KB | 480 MHz | 1D CNN INT8 | Orta sınıf, çoğu düğüm |
| STM32N6 | 4096 KB | 4096 KB | 800 MHz | LSTM / KWS  | NPU'lu, en güçlü; özel boot/flash prosedürü |

> **STM32N6 notu:** N6 (NUCLEO-N657X0-Q) "LRUN" boot moduyla çalışır; binary'nin
> imzalanması (`-align 0x400`) ve özel re-flash adımları gerekir. Detay:
> `docs/n6_kaldigimiz_yer.md`.

---

## 3. Mimari Genel Bakış

Uygulama klasik bir **katmanlı mimari** kullanır. QML arayüzü hiçbir iş
mantığını doğrudan yapmaz; her şey tek bir cephe (facade) sınıfı olan
`Backend` üzerinden C++ tarafına iletilir.

```
┌─────────────────────────────────────────────────────────────┐
│                      QML Arayüz Katmanı                       │
│  Main.qml → TopTabBar → [Dashboard | Board | Flash |          │
│             Monitor | Benchmark | Analysis] + FactorySimWindow│
└───────────────┬─────────────────────────────┬────────────────┘
                │ context properties           │
                ▼                               ▼
        ┌───────────────┐              ┌──────────────────┐
        │   Backend     │              │ FactorySimulator │  (bağımsız demo)
        │ (QML cephesi) │              └──────────────────┘
        └───┬───┬───┬───┘
            │   │   │
   ┌────────┘   │   └──────────┐
   ▼            ▼              ▼
┌────────┐ ┌──────────┐ ┌──────────────┐
│AppState│ │ Managers │ │ ToolDetector │
└────────┘ └────┬─────┘ └──────────────┘
                │
   ┌────────────┼─────────────┬──────────────┐
   ▼            ▼             ▼              ▼
SerialManager FlashManager AnalysisManager PipelineRunner
   │            │                            │
QThread+     CliRunner                  XCubeAIRunner + CliRunner
SerialWorker (QProcess)                 (stedgeai + gcc + programmer)
   │
PacketParser (§{JSON} protokol)
```

### Çalışma zamanı nesneleri (`main.cpp`)

`main.cpp` uygulamayı şu sırayla kurar:

1. `QApplication` + uygulama meta verileri (isim, sürüm, ikon).
2. `QQuickStyle::setStyle("Basic")` — QtQuick.Controls'ün özelleştirilebilmesi için yerel olmayan stil.
3. Çekirdek nesneler oluşturulur (hepsi `&app`'e parent, motor ömründen uzun yaşar):
   - `AppState`  — merkezi çalışma zamanı durumu
   - `SerialManager` — seri port yönetimi (kendi thread'ini açar)
   - `FlashManager` — flash işlemleri
   - `AnalysisManager` — SQLite veritabanı
4. `AppSettings`'ten son baud ve programmer CLI yolu geri yüklenir (yoksa otomatik tespit).
5. `Backend` cephesi, yukarıdaki yöneticileri sararak oluşturulur.
6. `FactorySimulator` bağımsız demo motoru oluşturulur.
7. QML motoru başlatılır; `appState`, `backend`, `factorySim` **context property** olarak QML'e enjekte edilir.
8. `STM32AiDeployer` QML modülünden `Main` yüklenir.
9. Bir `SplashScreen` (Widgets) gösterilir; 3.5 sn sonra ana QML penceresi açılır.

---

## 4. Çekirdek Sınıflar (`src/core/`)

### 4.1 `AppState` — Tek Doğruluk Kaynağı
Uygulamanın tüm canlı durumunu tutan QObject. QML'e `Q_PROPERTY`'lerle açılır;
tüm ekranlar buradan **okur** ve sinyallerle **tepki verir**.

Tuttuğu başlıca durum:
- **Bağlantı:** `connected`, `connInfo`
- **Aktif kart:** `boardName`, `boardSpec`, `boardFlashKb`, `boardRamKb`, `boardClockMhz`, `boardInfoRows`
- **Port/baud:** `activePort`, `activeBaud`
- **Son model metrikleri:** `lastModel`, `lastInfMs`, `lastAcc`, `lastRamKb`, `lastLabel`
- **Sistem metrikleri:** `lastTempC`, `lastUptime`, `lastFreeRamKb`
- **Aktif sensör:** `lastSensor`

Yazma işlemleri `public slots` üzerinden yapılır (`setActiveBoard`,
`setConnected`, `setLiveMetrics`, `setSystemMetrics`, …); her değişiklik ilgili
`*Changed` sinyalini yayar ve QML otomatik güncellenir.

### 4.2 `AppSettings` — Kalıcı Ayarlar
`QSettings` wrapper'ı. Tüm anahtarlara doğrudan değil bu sınıf üzerinden erişilir.

| Anahtar | Açıklama |
|---------|----------|
| `programmer/cli_path` | STM32_Programmer_CLI.exe yolu |
| `tools/xcubeai_cli_path` | stedgeai.exe (X-CUBE-AI) yolu |
| `tools/gcc_path` | arm-none-eabi-gcc yolu |
| `tools/make_path` | make.exe yolu |
| `tools/auto_detected` | İlk açılış taraması yapıldı mı |
| `serial/last_com_port` | Son kullanılan COM port |
| `board/last_board` | Son seçilen kart |
| `ui/theme` | Tema (varsayılan "dark") |
| `flash/last_firmware_dir`, `flash/last_model_dir`, `flash/last_output_dir` | Son kullanılan klasörler |

### 4.3 `ToolDetector` — Otomatik Araç Tespiti
İlk açılışta dört aracı sırayla arar ve `AppSettings`'e yazar:

| Araç | Arama sırası |
|------|--------------|
| arm-none-eabi-gcc | PATH → STM32CubeIDE plugins → sabit yollar |
| make | PATH → STM32CubeIDE plugins → sabit yollar |
| STM32_Programmer_CLI | PATH → Program Files/ST/.../bin |
| stedgeai | PATH → ~/STM32Cube/Repository/.../Utilities/windows |

### 4.4 `TemplateEngine` — `{{PLACEHOLDER}}` Sistemi
Şablon dosyalarındaki `{{KEY}}` yer tutucularını wizard'dan gelen değerlerle
değiştirir. Pipeline sırasında board/sensor/ai_glue şablonlarını çıktı
klasörüne kopyalayıp özelleştirir.

---

## 5. QML ↔ C++ Köprüsü: `Backend`

`Backend` (`src/bridge/Backend.cpp`), QML'in gördüğü **tek cephe** sınıfıdır.
Tüm yöneticileri (managers) sarar, verileri QML dostu tiplere (`QVariantList`,
`QVariantMap`) dönüştürür ve `Q_INVOKABLE` metotlarla QML'den çağrılabilir
işlevler sunar. Sorumluluk alanları:

| Alan | Q_INVOKABLE / Property örnekleri |
|------|----------------------------------|
| **Araçlar** | `scanTools()`, `setToolPath()`, `toolPaths`, `scanning` |
| **Seri port** | `availablePorts()`, `connectSerial()`, `disconnectSerial()`, `detectedStLinkPort()` |
| **Kart seçimi** | `selectBoard()`, `addCustomBoard()`, `customBoards()` |
| **Monitör** | `monitorLines`, `clearMonitor()`, `startSensorAnalysis()`, `saveMonitorLog()` |
| **Simülasyon** | `startSimulation()`, `startHardwareSimulation()`, `stopSimulation()` |
| **Flash** | `flashFirmware()`, `cancelFlash()`, `flashLines`, `flashProgress`, `flashBusy` |
| **Pipeline** | `runPipeline()`, `cancelPipeline()`, `pipelineLines`, `pipelineProgress`, `pipelineStage` |
| **Benchmark** | `startBenchmark()`, `startBenchmarkWithInputs()`, `deployedModelInfo()`, `benchmarkMetrics` |
| **Kart probe** | `probeStLinkBoard()`, `probeBusy`, `probeStatus` |
| **Analiz** | `benchmarkRecords`, `simulationRecords`, `sensorRecords`, `compiledRecords`, `deleteAnalysisRecord()`, `exportAnalysisCsv()`, `exportAnalysisPdf()`, `flashCompiledModel()` |

Backend ayrıca seri/flash/analiz yöneticilerinin sinyallerini iç slot'lara
bağlar (`wireSerial()`, `wireFlash()`, `wireAnalysis()`), gelen verileri
işleyip QML'e uygun sinyaller (`statusMessage`, `pipelineFinished`, …) yayar.
Monitör terminali en fazla `kMaxMonitorLines = 500` satır tutar.

---

## 6. Modüller (`src/modules/`)

### 6.1 Board — `BoardManager`, `BoardPresets`
- `BoardPresets.h`: F4 / H7 / N6 için sabit `BoardInfo` tanımları (flash, RAM, clock).
- `BoardManager`: preset yönetimi, özel kart JSON config okuma/yazma, aktif kartı uygulamaya yayar.

### 6.2 Serial — `SerialManager` + `SerialWorker` + `PacketParser`
Bu modül **thread-güvenli** tasarlanmıştır:

- `SerialManager` ana thread'de yaşar, bir `QThread` açar ve içine
  `SerialWorker`'ı taşır. UI ile worker arasındaki tüm haberleşme **queued
  connection**'larla yapılır.
- `SerialWorker` gerçek `QSerialPort` işlemlerini worker thread'inde yapar
  (ana thread asla bloklanmaz).
- ST-Link otomatik tespiti **VID 0x0483** ile yapılır (`findStLink()`).
- Gelen ham byte'lar `PacketParser::feed()`'e verilir.

#### Protokol Parser — `PacketParser`
`§{JSON}\r\n` formatındaki paketleri çözer. `§` ile başlamayan satırlar ham log
olarak (`rawLineReceived`), başlayanlar JSON parse edilip **tip bazlı struct**
sinyalleriyle yayılır:

| Tip | Struct | Sinyal |
|-----|--------|--------|
| `inf` | `InferenceData` (model, inf_us, ram_b, acc_pct, label, card) | `inferenceReceived` |
| `sys` | `SysData` (uptime_s, temp_c, free_ram_b, state) | `sysReceived` |
| `boot` | `BootData` (card, sdk, model, sensor, baud, flash/ram/clock) | `bootReceived` |
| `err` | `ErrorData` (code, msg) | `errorReceived` |
| `bench` | `BenchData` (samples, avg/min/max_us, ram_b, …) | `benchReceived` |
| `sensor` | `SensorData` (seq, samples, values, unit, …) | `sensorReceived` |

> Protokol kuralları: tüm sayısal değerler **integer** (float yok), UART
> gönderimi firmware tarafında **DMA** ile (blocking yasak). Tam referans:
> `docs/protocol_v1.md`.

#### `SerialSimulator` + `CircularBuffer`
- `SerialSimulator`: gerçek kart olmadan `§{JSON}` paketleri üreten sahte veri kaynağı.
- `CircularBuffer`: son 500 inference kaydını dairesel tamponda tutar.

### 6.3 Flash — `FlashManager`, `CliRunner`, `XCubeAIRunner`, `PipelineRunner`

- **`CliRunner`**: `QProcess` wrapper'ı. Bir CLI'yı asenkron çalıştırır, satır
  satır stdout/stderr yayar, bitiş kodunu raporlar.
- **`FlashManager`**: `.hex` / `.bin` doğrulama + `STM32_Programmer_CLI` çağrısı.
- **`XCubeAIRunner`**: `stedgeai` (X-CUBE-AI 10.2.0) CLI sarmalayıcı —
  `analyze` (model doğrula/metrik çıkar) ve `generate` (C kodu üret) modları.
- **`PipelineRunner`**: Uçtan uca orkestrasyon. Tek `run(PipelineConfig)`
  çağrısıyla beş adımı sırayla, asenkron yürütür ve `stageChanged` /
  `progressChanged` / `finished` sinyalleriyle ilerlemeyi bildirir.

#### Pipeline Akışı (5 adım)
```
.tflite
  │  1) stepAnalyze()  → stedgeai analyze (model metrikleri, doğrulama)
  ▼
  │  2) stepXCubeAI()  → stedgeai generate (C kodu üretimi)
  ▼
  │  3) stepPrepare()  → TemplateEngine: base + sensor + ai_glue şablonlarını
  │                       çıktı klasörüne kopyala, {{PLACEHOLDER}}'ları doldur
  ▼
  │  4) stepBuild()    → arm-none-eabi-gcc (Makefile ile) → .elf
  ▼
  │  5) stepFlash()    → STM32_Programmer_CLI → karta yükle
  ▼
finished(success)
```
`PipelineConfig` (struct) tüm parametreleri taşır: model yolu, board, sensör,
çıktı klasörü, araç yolları, vb.

### 6.4 Analysis — `AnalysisManager`
SQLite veritabanını açar, şemayı migrate eder ve genel amaçlı kayıt saklar.
Veri modeli sade ve esnektir:

```cpp
struct AnalysisRecord {
    int id;
    QString kind;          // "benchmark" | "simulation" | "sensor" | "compiled"
    QStringList cells;     // tabloya yazılacak hücreler
};
```
API: `addRecord(kind, cells)`, `records(kind)`, `deleteRecord(id)`,
`deleteRecordsForKindOnDate(kind, datePrefix)`. Backend bu kayıtları
`QVariantList`'e çevirip QML tablolarına besler ve CSV/PDF dışa aktarır.

### 6.5 Simulation — `FactorySimulator`
Bkz. **Bölüm 8** (kendi başına büyük bir alt sistem).

---

## 7. QML Arayüz Katmanı (`qml/`)

### 7.1 Giriş ve Çerçeve — `Main.qml`
- **Çerçevesiz pencere** (`Qt.FramelessWindowHint`) — özel `TitleBar` + kenar
  resize handle'ları.
- Üstte `TopTabBar`, altında `StackLayout` ile 6 ana ekran arasında geçiş.
- `appState` / `backend` tanımsızsa `MockData` singleton'ına düşer (QML'i
  C++ olmadan tasarımda önizleyebilmek için).

### 7.2 Ana Ekranlar (`qml/screens/`)
| Ekran | İçerik |
|-------|--------|
| **DashboardScreen** | Genel durum, hızlı eylemler ("Pipeline aç", "Loglar") |
| **BoardScreen** (Kartlar) | Kart seçimi, özel kart ekleme, ST-Link probe, kart bilgi satırları |
| **FlashScreen** | Firmware/model seçimi, flash logu, ilerleme çubuğu, Pipeline Wizard girişi |
| **MonitorScreen** | Canlı UART terminali, simülasyon başlat/durdur, sensör analizi, log kaydetme |
| **BenchmarkScreen** | Model benchmark (örnek sayısı, giriş aralıkları), metrik kartları |
| **AnalysisScreen** | benchmark/simülasyon/sensör/compiled kayıt tabloları, CSV/PDF dışa aktarım, kayıt silme |

### 7.3 Yeniden Kullanılabilir Bileşenler (`qml/components/`)
`AppButton`, `Card`, `DataTable`, `ComboField`, `FormField`, `InfoCard`,
`MetricBar`, `MetricIcon`, `SectionHeader`, `StatusPill`, `Terminal`,
`TitleBar`, `TopTabBar` — tutarlı görünüm için merkezi yapı taşları.
`Theme.qml` (singleton) tüm renk/aralık tanımlarını taşır.

### 7.4 Dialoglar (`qml/dialogs/`)
- **SettingsDialog**: 4 araç yolu + "Tümünü Tara" (ToolDetector tetikler).
- **PipelineWizard**: 4 sayfalı sihirbaz — Model → Sensör → Özet/Araç Doğrulama → Derleme+Flash.
- **AboutDialog**: ekip/proje bilgisi.

---

## 8. Fabrika Simülasyonu — `FactorySimulator`

Gerçek donanım olmadan, büyük bir akıllı fabrikayı sentetik canlı veriyle
canlandıran bağımsız demo motoru. QML'e `factorySim` context property'si olarak
açılır ve kendi penceresinde (`FactorySimWindow.qml`) görüntülenir.

### 8.1 Modellenen Fabrika
**5 bölge, 20 düğüm (node), ~68 sensör:**

| # | Bölge | Düğüm | Sensörler |
|---|-------|-------|-----------|
| 1 | Çevresel Güvenlik & Erken Uyarı | 5× F4 | Gaz, Duman, Sıcaklık, Nem (4×5=20) |
| 2 | Sıvı/Gaz Akış & Basınç | 5× F4/H7 | Basınç, Debi, Seviye (3×5=15) |
| 3 | Kestirimci Bakım & Ağır Makine | 4× H7 | Titreşim, Sıcaklık, Akım, Devir (4×4=16) |
| 4 | Uç Yapay Zeka & Kalite Kontrol | 4× N6/H7 | Güven, Tespit/dk, TOF mesafe (3×4=12) |
| 5 | Merkezi Ağ Geçitleri | 2× H7 | Ağ Yükü, Kuyruk, SD, Gecikme (~5) |

Her düğüm bir STM32 sınıfına ve bir modele bağlıdır; `baseInfUs` ve `totalRamB`
MCU sınıfına göre ölçeklenir (N6 en hızlı/en çok RAM).

### 8.2 Veri Üretim Mantığı (`advance()` — her tick)
Simülasyonun **gerçekçi** görünmesi için değerler her tick sıfırdan
hesaplanmaz; bir **alçak-geçiren filtreyle** hedeflerine doğru yumuşakça
hareket eder. Her sensör için:

1. **Hedef değer (baz salınım):** `baseline + amplitude × (0.78·sin(t·f+φ) +
   0.22·sin(2.3·t·f+…))` — her sensörün kendi frekansı `f` ve fazı `φ` vardır,
   böylece düğümler senkron salınmaz; ikinci harmonik eğriyi temiz sinüsten kurtarır.
2. **Arıza (fault) seviyesi:** Düğüm düşük olasılıkla (~%0.4/tick) bir arızaya
   girer ve 8–22 tick sürer. `faultLevel` arıza başlayınca **kademeli yükselir**,
   bitince **yavaşça söner**. Bu sayede bir hata oluştuğunda sonraki değerler de
   bir süre bozuk kalır ve sonra normale döner (anlık sıçrama yok).
3. **Gürültü (random-walk):** `noiseState = 0.82·noiseState + randNoise(...)` —
   ortalamaya geri çeken rastgele yürüyüş; bağımsız tek-tick sıçramaları yerine
   organik titreşim.
4. **Yumuşatma:** `value += (target − value) × 0.18` — görüntülenen değer
   hedefe doğru süzülür.

Değer normal bandın dışına çıkınca durum `warning`/`error` olur; bir geçiş
anında **alarm** üretilir (`alarmRaised`), son 200 alarm saklanır.

### 8.3 KPI'lar (her tick yeniden hesaplanır)
`okCount`, `warningCount`, `criticalCount`, `activeAlarmCount`, `avgInfUs`,
`throughput` (toplam inference/sn). Inference süresi ve boş RAM de hedeflerine
doğru yumuşatılır; arıza sırasında ikisi de bir miktar kötüleşir.

### 8.4 Kontroller
- `start()` / `stop()`, `setIntervalMs(120–5000)`, `setSpeed(0.25–8.0)`
- QML erişimcileri: `zones()`, `nodes()`, `nodesInZone(z)`, `node(id)`,
  `sensorsOf(id)`, `alarms(limit)`, `zoneSummary(z)`

### 8.5 Fabrika QML Ekranları (`qml/factory/`)
`FactorySimWindow` (ayrı pencere) → `FactoryDashboard` (KPI özeti) /
`FactoryMap` (düğüm haritası, `NodeMarker`/`ZoneBlock`) → `ZoneDetail` →
`NodeDetail` (sensör satırları `SensorRow`, alarm akışı `AlarmFeed`).

---

## 9. UART Protokolü v1.0

Firmware → Qt yönünde her paket:
```
§{JSON}\r\n
```
- `§` = UTF-8 `0xC2 0xA7` — başlangıç işareti (her paketin ilk 2 byte'ı)
- Gövde: tek satır compact JSON
- Tüm sayısal değerler **integer**
- UART gönderimi firmware'de **DMA** (`HAL_UART_Transmit_DMA`) — blocking yasak

**Örnek mesajlar:**
```json
{"t":"inf","model":"MLP_INT8","inf_us":8200,"ram_b":3072,"acc_pct":96,"label":"walking","card":"STM32F4"}
{"t":"sys","uptime_s":42,"temp_c":38,"free_ram_b":185000,"state":"running"}
{"t":"boot","card":"STM32F4","sdk":"EdgeAI_v1.0","model":"MLP_INT8","baud":115200}
{"t":"err","code":3,"msg":"sensor_timeout"}
```
Tam referans: `docs/protocol_v1.md`.

---

## 10. Veritabanı Şeması (SQLite)

Kavramsal şema (kart, oturum, inference ve sistem logları):

```sql
CREATE TABLE boards (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL, flash_kb INTEGER, ram_kb INTEGER,
    clock_mhz INTEGER, is_preset INTEGER DEFAULT 0
);
CREATE TABLE sessions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    board_id INTEGER REFERENCES boards(id),
    model_name TEXT NOT NULL, architecture TEXT, quantization TEXT,
    hex_path TEXT, flashed_at TEXT, notes TEXT
);
CREATE TABLE inference_logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    session_id INTEGER REFERENCES sessions(id),
    inf_us INTEGER, ram_b INTEGER, acc_pct INTEGER,
    label TEXT, recorded_at TEXT
);
CREATE TABLE sys_logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    session_id INTEGER REFERENCES sessions(id),
    uptime_s INTEGER, temp_c INTEGER, free_ram_b INTEGER, recorded_at TEXT
);
```
> Çalışma zamanında `AnalysisManager`, esnek `kind` + `cells` modeliyle genel
> bir kayıt tablosu üzerinden benchmark/simülasyon/sensör/compiled kayıtlarını
> saklar (Bölüm 6.4).

---

## 11. Firmware Şablonları (`templates/`)

Pipeline'ın kart üzerinde çalışacak C projesini üretmek için kullandığı
şablon ağacı:

```
templates/
├── base/            → Kart başına HAL iskeleti (Makefile, .ld, startup, Src/, Inc/)
│   ├── STM32F4/     (STM32F407VGTx)
│   ├── STM32H7/     (STM32H723ZGTx)
│   └── STM32N6/     (STM32N6xx — system_clock_stub dahil)
├── sensors/         → Sensör okuma sürücüleri
│   ├── MPU6050/     (I2C IMU — HAR)
│   ├── BME280/      (I2C çevre sensörü)
│   └── PDM_MIC/     (SAI PDM mikrofon — KWS)
└── ai_glue/         → X-CUBE-AI yapıştırıcı kodu
    ├── ai_runner.c/.h    (inference wrapper)
    └── uart_report.c/.h  (Protokol v1.0 UART raporlama)
```
`{{PLACEHOLDER}}` yer tutucuları `TemplateEngine` tarafından wizard değerleriyle
doldurulur. Build sonrası CMake `templates/` klasörünü exe'nin yanına kopyalar
(PipelineRunner çalışma zamanında `applicationDirPath()/templates` ile bulur).

---

## 12. Derleme ve Çalıştırma

```powershell
# Qt araç yollarını ekle
$env:PATH = "C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\mingw1310_64\bin;$env:PATH"

# Yapılandır
cmake -B build -S . `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.11.0/mingw_64" `
  -DCMAKE_BUILD_TYPE=Release `
  -G "MinGW Makefiles"

# Derle
cmake --build build

# Çalıştır
.\build\STM32AiDeployer.exe
```

CMake özellikleri:
- `AUTOMOC` / `AUTORCC` / `AUTOUIC` açık.
- QML modülü `qt_add_qml_module` ile tanımlanır; `Theme.qml` ve `MockData.qml`
  singleton işaretlenir.
- Build sonrası `templates/` kopyalanır ve `windeployqt` ile Qt runtime DLL'leri
  + QML plugin'leri exe yanına dağıtılır (standalone çalışma).

---

## 13. Geliştirme Kuralları (özet)

- **C++:** `#pragma once`; global `using namespace` yasak; her sınıf kendi
  `.h/.cpp` çiftinde; daima yeni pointer-to-member `connect` sözdizimi; akıllı
  pointer tercih (Qt parent-child'da raw pointer kabul).
- **Qt:** Sadece CMake (`.pro` yok); uzun işlemler ve `QSerialPort` ana
  thread'de çalışmaz; UI güncellemesi yalnızca ana thread'den (`emit` ile);
  `QSettings`'e doğrudan değil `AppSettings` üzerinden erişim.
- **Stil:** Tüm görünüm `qml/Theme.qml` ve `resources/style.qss` üzerinden;
  inline stil yazılmaz.
- **Yorumlar:** Kod ve yorumlar İngilizce; `// TODO(aşama-N):`, `// NOTE:`,
  `// FIXME:` etiketleri.

---

## 14. Geliştirme Aşamaları

| Aşama | Başlık | Durum |
|-------|--------|-------|
| 0 | Protokol standardı | ✅ |
| 1 | Qt proje iskeleti | ✅ |
| 2 | Ana UI iskeleti | ✅ |
| 3 | Serial port modülü | ✅ |
| 4 | Flash modülü | ✅ |
| 4.6 | X-CUBE-AI CLI entegrasyonu | ✅ |
| 4.8 | Template Framework + Pipeline Wizard | ✅ |
| — | QML arayüze geçiş + Fabrika Simülasyonu | ✅ (aktif geliştirme) |
| 5 | Veritabanı ve kayıt | ⏳ |
| 6 | Canlı dashboard | ⏳ |
| 7 | Model karşılaştırma | ⏳ |
| 8 | Bitirme demo hazırlığı | ⏳ |

---

## 15. Önemli Dosya Haritası

| Dosya | Sorumluluk |
|-------|-----------|
| `src/main.cpp` | Uygulama bootstrap, QML motoru, context property'ler |
| `src/bridge/Backend.*` | QML ↔ C++ tek cephe sınıfı |
| `src/core/AppState.*` | Merkezi çalışma zamanı durumu |
| `src/core/AppSettings.*` | QSettings anahtarları (tek merkez) |
| `src/core/ToolDetector.*` | GCC/Make/CLI otomatik tespiti |
| `src/core/TemplateEngine.*` | `{{PLACEHOLDER}}` şablon motoru |
| `src/modules/serial/PacketParser.*` | `§{JSON}` protokol parser — **değiştirme** |
| `src/modules/serial/SerialManager.*` | Thread'li seri port yönetimi |
| `src/modules/flash/PipelineRunner.*` | Uçtan uca dağıtım orkestrasyonu |
| `src/modules/flash/XCubeAIRunner.*` | stedgeai CLI sarmalayıcı |
| `src/modules/simulation/FactorySimulator.*` | Fabrika demo veri motoru |
| `qml/Main.qml` | QML giriş noktası, sekme/ekran yönetimi |
| `qml/Theme.qml` | Merkezi tema/renk tanımları |
| `resources/style.qss` | Widgets stil dosyası |
| `docs/protocol_v1.md` | UART protokol referansı |
| `docs/n6_kaldigimiz_yer.md` | STM32N6 boot/flash prosedürü |

---

*Bu doküman projenin mevcut durumunu yansıtır. Yeni bir modül veya aşama
eklendiğinde güncellenmelidir.*
