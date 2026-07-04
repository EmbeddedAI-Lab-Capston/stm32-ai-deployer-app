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
| Thread mimarisi  | QThread + Worker pattern        |
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
│       └── simulation/
│           └── FactorySimulator.h / .cpp ← Fabrika demo veri motoru
│
├── qml/                              ← AKTİF UI — tamamı burada
│   ├── Main.qml · Theme.qml · MockData.qml
│   ├── screens/                     ← Dashboard/Board/Flash/Monitor/Benchmark/Analysis
│   ├── components/                  ← AppButton/Card/DataTable/Terminal/TitleBar …
│   ├── dialogs/                     ← SettingsDialog/PipelineWizard/AboutDialog
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
│       └── uart_report.c / .h       ← Protokol v1.0 UART raporlama
│
└── docs/
    ├── PROJECT.md                   ← Tüm mimari, uçtan uca (ana referans)
    ├── protocol_v1.md               ← UART protokol referansı
    ├── n6_kaldigimiz_yer.md         ← STM32N6 boot/flash geçmişi + güncel durum
    ├── factory_simulation_plan.md   ← Fabrika Sim. orijinal tasarım planı (tarihi)
    └── lstm_stm32_export.md         ← LSTM → X-CUBE-AI uyumlu TFLite export rehberi
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
