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

---

## Teknik Yığın

| Bileşen          | Teknoloji                       |
|------------------|---------------------------------|
| Framework        | Qt 6.11.0 (mingw_64)            |
| Derleyici        | MinGW 13.1.0 (GCC)              |
| Derleme sistemi  | CMake 3.30.5 (QMake kullanılmaz)|
| Dil              | C++17                           |
| Veritabanı       | SQLite — Qt SQL modülü          |
| Seri port        | Qt Serial Port modülü           |
| Grafikler        | Qt Charts modülü                |
| Ayar depolama    | QSettings                       |
| Süreç yönetimi   | QProcess — ST-Link CLI çağrısı  |
| Thread mimarisi  | QThread + Worker pattern        |
| Platform         | Windows 10/11 (yalnızca)        |

---

## Klasör Yapısı

```
stm32-ai-deployer/
├── CLAUDE.md                        ← Bu dosya
├── CMakeLists.txt
├── README.md
├── .gitignore
│
├── src/
│   ├── main.cpp
│   ├── mainwindow.h / .cpp
│   │
│   ├── core/
│   │   ├── AppSettings.h / .cpp     ← QSettings wrapper
│   │   ├── AppState.h / .cpp        ← Merkezi çalışma zamanı durumu
│   │   ├── ToolDetector.h / .cpp    ← GCC/Make/CLI otomatik tespit
│   │   └── TemplateEngine.h / .cpp  ← {{PLACEHOLDER}} template sistemi
│   │
│   ├── modules/
│   │   ├── board/
│   │   │   ├── BoardManager.h / .cpp
│   │   │   └── BoardPresets.h       ← F4/H7/N6 sabit tanımları
│   │   ├── flash/
│   │   │   ├── FlashManager.h / .cpp
│   │   │   ├── CliRunner.h / .cpp   ← QProcess wrapper
│   │   │   ├── XCubeAIRunner.h / .cpp ← stedgeai CLI wrapper
│   │   │   ├── PipelineConfig.h     ← Pipeline yapılandırma struct
│   │   │   └── PipelineRunner.h / .cpp ← .tflite→C→GCC→Flash orkestrasyonu
│   │   ├── serial/
│   │   │   ├── SerialWorker.h / .cpp ← QThread worker
│   │   │   └── PacketParser.h / .cpp ← § JSON protokol parser
│   │   └── analysis/
│   │       └── AnalysisManager.h / .cpp
│   │
│   └── ui/
│       ├── BoardTab.h / .cpp
│       ├── FlashTab.h / .cpp
│       ├── MonitorTab.h / .cpp
│       ├── AnalysisTab.h / .cpp
│       ├── SettingsDialog.h / .cpp  ← 4 araç + Tümünü Tara
│       └── PipelineWizard.h / .cpp  ← 4 adımlı QWizard
│
├── resources/
│   ├── app.qrc
│   ├── style.qss                    ← Merkezi Qt stil dosyası
│   └── icons/
│
├── templates/                       ← STM32 proje şablonları
│   ├── README.md
│   ├── base/
│   │   ├── STM32F4/                 ← Makefile, ld, startup, Inc/, Src/
│   │   ├── STM32H7/
│   │   └── STM32N6/
│   ├── sensors/
│   │   ├── MPU6050/                 ← I2C IMU (HAR)
│   │   ├── BME280/                  ← I2C çevre sensörü
│   │   └── PDM_MIC/                 ← SAI PDM mikrofon (KWS)
│   └── ai_glue/
│       ├── ai_runner.c / .h         ← X-CUBE-AI inference wrapper
│       └── uart_report.c / .h       ← Protokol v1.0 UART raporlama
│
└── docs/
    ├── protocol_v1.md               ← UART protokol referansı
    └── roadmap.md                   ← Geliştirme yol haritası
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

### Modül 1 — BoardManager
- STM32F4 / H7 / N6 preset yönetimi
- Custom kart JSON config okuma/yazma
- Aktif kart bilgisini uygulama genelinde yayar

### Modül 2 — FlashManager + CliRunner
- `.hex` / `.bin` dosya doğrulama
- STM32_Programmer_CLI çağrısı (QProcess)
- Flash log çıktısını UI'ya iletir
- CLI yolu AppSettings'ten okunur

### Modül 3 — SerialWorker + PacketParser
- QSerialPort — QThread worker içinde (ana thread'de çalışmaz)
- VID/PID ile ST-Link otomatik tespiti (VID: 0x0483)
- § protokol parser
- Circular buffer — son 500 inference kaydı tutulur

### Modül 4 — AnalysisManager + SessionModel
- Her flash işlemi bir "oturum" açar
- Gelen inf metriği otomatik SQLite'a yazılır
- Oturumlar arası karşılaştırma sorguları

---

## Veritabanı Şeması (SQLite)

```sql
-- Kayıtlı kartlar
CREATE TABLE boards (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    flash_kb INTEGER,
    ram_kb INTEGER,
    clock_mhz INTEGER,
    is_preset INTEGER DEFAULT 0
);

-- Model oturumları
CREATE TABLE sessions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    board_id INTEGER REFERENCES boards(id),
    model_name TEXT NOT NULL,
    architecture TEXT,       -- MLP / CNN / LSTM
    quantization TEXT,       -- float32 / int8
    hex_path TEXT,
    flashed_at TEXT,         -- ISO 8601
    notes TEXT
);

-- Inference kayıtları
CREATE TABLE inference_logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    session_id INTEGER REFERENCES sessions(id),
    inf_us INTEGER,          -- mikrosaniye
    ram_b INTEGER,           -- byte
    acc_pct INTEGER,         -- 0-100
    label TEXT,
    recorded_at TEXT         -- ISO 8601
);

-- Sistem durum kayıtları
CREATE TABLE sys_logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    session_id INTEGER REFERENCES sessions(id),
    uptime_s INTEGER,
    temp_c INTEGER,
    free_ram_b INTEGER,
    recorded_at TEXT
);
```

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
- UI dosyaları `src/ui/` altında
- `.qss` stil dosyası `resources/style.qss` — inline stil yazılmaz
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

// arm-none-eabi-gcc tam yolu (Aşama 4.8)
"tools/gcc_path"

// make.exe tam yolu (Aşama 4.8)
"tools/make_path"

// İlk açılış araç taraması yapıldı mı (Aşama 4.8)
"tools/auto_detected"

// Son kullanılan COM port
"serial/last_com_port"

// Son seçilen kart adı
"board/last_board"

// Uygulama teması
"ui/theme"  // default: "dark"

// Son firmware klasörü
"flash/last_firmware_dir"

// Son model klasörü (Aşama 4.8)
"flash/last_model_dir"

// Son pipeline çıktı klasörü (Aşama 4.8)
"flash/last_output_dir"
```

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
.\build\stm32-ai-deployer.exe
```

---

## Önemli Dosyalar

| Dosya                      | Ne Yapar                                    |
|----------------------------|---------------------------------------------|
| `src/core/AppSettings.h`   | Tüm QSettings anahtarlarının merkezi noktası|
| `src/core/Database.h`      | SQLite bağlantısı ve şema yönetimi          |
| `src/modules/serial/PacketParser.h` | § protokol parser — değiştirme    |
| `resources/style.qss`      | Tüm UI stili buradan yönetilir              |
| `docs/protocol_v1.md`      | UART protokol referansı                     |

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
