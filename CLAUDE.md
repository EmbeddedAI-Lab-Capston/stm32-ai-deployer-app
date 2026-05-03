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
│   │   └── Database.h / .cpp        ← SQLite yöneticisi
│   │
│   ├── modules/
│   │   ├── board/
│   │   │   ├── BoardManager.h / .cpp
│   │   │   └── BoardPresets.h       ← F4/H7/N6 sabit tanımları
│   │   ├── flash/
│   │   │   ├── FlashManager.h / .cpp
│   │   │   └── CliRunner.h / .cpp   ← QProcess wrapper
│   │   ├── serial/
│   │   │   ├── SerialWorker.h / .cpp ← QThread worker
│   │   │   └── PacketParser.h / .cpp ← § JSON protokol parser
│   │   └── analysis/
│   │       ├── AnalysisManager.h / .cpp
│   │       └── SessionModel.h       ← SQLite oturum veri modeli
│   │
│   └── ui/
│       ├── BoardTab.h / .cpp
│       ├── FlashTab.h / .cpp
│       ├── MonitorTab.h / .cpp
│       ├── AnalysisTab.h / .cpp
│       └── SettingsDialog.h / .cpp
│
├── resources/
│   ├── app.qrc
│   ├── style.qss                    ← Merkezi Qt stil dosyası
│   └── icons/
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
AppSettings::ProgrammerCliPath   // "tools/programmer_cli_path"

// Son kullanılan COM port
AppSettings::LastComPort         // "serial/last_com_port"

// Son seçilen baud rate
AppSettings::LastBaudRate        // "serial/last_baud_rate"  default: 115200

// Son seçilen kart adı
AppSettings::LastBoard           // "board/last_board"

// Uygulama teması
AppSettings::Theme               // "ui/theme"  default: "dark"

// STM32_Programmer_CLI otomatik arama yapıldı mı
AppSettings::CliAutoDetected     // "tools/cli_auto_detected"
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

## Bu Dosyayı Güncelleme Kuralı

Yeni bir aşama tamamlandığında:
1. Aşama tablosunda durumu ✅ yap
2. Yeni eklenen sınıflar varsa klasör yapısına ekle
3. Yeni AppSettings anahtarları varsa listeye ekle
4. Değişen protokol varsa UART bölümünü güncelle
