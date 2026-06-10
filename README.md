<div align="center">

# STM32 AI Deployer

**STM32 mikrodenetleyiciler için uçtan uca yapay zeka dağıtım, izleme ve analiz aracı**

`.tflite` modelini al → C koduna çevir → derle → karta flash'la → UART'tan gelen
inference metriklerini gerçek zamanlı izle → karşılaştırmalı analiz et — hepsi tek masaüstü uygulamasından.

[![Platform](https://img.shields.io/badge/platform-Windows%2010%2F11-0078D6?logo=windows)](#)
[![Qt](https://img.shields.io/badge/Qt-6.11.0-41CD52?logo=qt&logoColor=white)](#)
[![Language](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus)](#)
[![UI](https://img.shields.io/badge/UI-Qt%20Quick%20%2F%20QML-41CD52)](#)
[![Build](https://img.shields.io/badge/build-CMake-064F8C?logo=cmake)](#)
[![Status](https://img.shields.io/badge/status-active%20development-orange)](#)

</div>

---

## İçindekiler

- [Genel Bakış](#-genel-bakış)
- [Öne Çıkan Özellikler](#-öne-çıkan-özellikler)
- [Hedef Donanım](#-hedef-donanım)
- [Mimari](#-mimari)
- [Teknik Yığın](#-teknik-yığın)
- [Hızlı Başlangıç](#-hızlı-başlangıç)
- [Pipeline Akışı](#-pipeline-akışı)
- [UART Protokolü](#-uart-protokolü-v10)
- [Klasör Yapısı](#-klasör-yapısı)
- [Dokümantasyon](#-dokümantasyon)
- [Ekip](#-ekip)

---

## Genel Bakış

**STM32 AI Deployer**, STM32 mikrodenetleyiciler üzerinde çalışan yapay zeka
modellerinin tüm yaşam döngüsünü tek bir masaüstü uygulamasından yöneten bir
araçtır. Dört temel işi bir arada yapar:


| İş                     | Açıklama                                                                                                                               |
| ------------------------ | ---------------------------------------------------------------------------------------------------------------------------------------- |
| **Dağıtım**           | Bir`.tflite` modelini X-CUBE-AI ile C koduna çevirir, GCC ile derler ve STM32 kartına flash'lar — tek tıkla uçtan uca               |
| **İzleme**              | Karttan UART üzerinden gelen inference metriklerini (`§{JSON}` protokolü) gerçek zamanlı gösterir                                  |
| **Analiz & Benchmark**   | Modellerin hız / RAM / doğruluk değerlerini SQLite'ta saklar, karşılaştırmalı tablolar ve CSV/PDF dışa aktarımı üretir      |
| **Fabrika Simülasyonu** | Gerçek donanım olmadan 5 bölgeli / 20 düğümlü / ~68 sensörlü akıllı fabrikayı sentetik canlı veriyle canlandıran demo modu |

---

## Öne Çıkan Özellikler

- **Tek tıkla pipeline** — `.tflite`'tan flash'lanmış karta kadar 5 adımlı otomatik orkestrasyon
- **Otomatik araç tespiti** — `arm-none-eabi-gcc`, `make`, `STM32_Programmer_CLI`, `stedgeai` ilk açılışta otomatik bulunur
- **Thread-güvenli seri port** — UART işlemleri ayrı `QThread` worker'ında; UI asla bloklanmaz
- **Gerçek zamanlı izleme** — `§{JSON}` protokol parser, tip bazlı sinyaller, 500 satırlık dairesel terminal tamponu
- **Donanımsız demo** — `SerialSimulator` ve `FactorySimulator` ile gerçek kart olmadan tam sunum
- **Modern QML arayüz** — çerçevesiz pencere, merkezi `Theme.qml`, yeniden kullanılabilir bileşen kütüphanesi
- **Üç STM32 sınıfı** — F4 / H7 / N6 preset'leri + özel kart desteği

---

## Hedef Donanım


| Kart    | Flash   | RAM     | Hız    | Tipik Model | Not                                               |
| ------- | ------- | ------- | ------- | ----------- | ------------------------------------------------- |
| STM32F4 | 1024 KB | 192 KB  | 168 MHz | MLP INT8    | Giriş seviyesi, düşük güç                   |
| STM32H7 | 2048 KB | 1024 KB | 480 MHz | 1D CNN INT8 | Orta sınıf, çoğu düğüm                     |
| STM32N6 | 4096 KB | 4096 KB | 800 MHz | LSTM / KWS  | NPU'lu, en güçlü; özel boot/flash prosedürü |

> **STM32N6 notu:** N6 (NUCLEO-N657X0-Q) "LRUN" boot moduyla çalışır; binary'nin
> imzalanması (`-align 0x400`) ve özel re-flash adımları gerekir. Detay için
> [`docs/n6_kaldigimiz_yer.md`](docs/n6_kaldigimiz_yer.md).

---

## Mimari

Uygulama katmanlı bir mimari kullanır. QML arayüzü iş mantığını doğrudan
yapmaz; her şey tek bir cephe (facade) sınıfı olan **`Backend`** üzerinden
C++ tarafına iletilir.

```
┌─────────────────────────────────────────────────────────────┐
│                      QML Arayüz Katmanı                       │
│  Main.qml → TopTabBar → [Dashboard | Board | Flash |          │
│             Monitor | Benchmark | Analysis] + FactorySimWindow│
└───────────────┬─────────────────────────────┬────────────────┘
                │ context properties           │
                ▼                               ▼
        ┌───────────────┐              ┌──────────────────┐
        │   Backend     │              │ FactorySimulator │
        │ (QML cephesi) │              └──────────────────┘
        └───┬───┬───┬───┘
            ▼   ▼   ▼
    SerialManager · FlashManager · AnalysisManager · PipelineRunner
            │            │                            │
    QThread+Worker   CliRunner               XCubeAIRunner + CliRunner
            │       (QProcess)              (stedgeai + gcc + programmer)
    PacketParser (§{JSON} protokol)
```

Tam mimari, sınıf sorumlulukları ve veri akışı için → [`docs/PROJECT.md`](docs/PROJECT.md)

---

## Teknik Yığın


| Bileşen          | Teknoloji                                                         |
| ----------------- | ----------------------------------------------------------------- |
| Framework         | Qt 6.11.0 (mingw_64)                                              |
| UI                | Qt Quick / QML (ana arayüz) + Qt Widgets (yardımcı pencereler) |
| Dil               | C++17 + QML / JavaScript                                          |
| Derleyici         | MinGW 13.1.0 (GCC)                                                |
| Derleme sistemi   | CMake 3.21+ (QMake kullanılmaz)                                  |
| Veritabanı       | SQLite (Qt SQL)                                                   |
| Seri port         | Qt SerialPort                                                     |
| Grafikler         | Qt Charts                                                         |
| Süreç yönetimi | QProcess (CLI araçları)                                         |
| Thread modeli     | QThread + Worker                                                  |
| Platform          | Windows 10 / 11 (yalnızca)                                       |

---

## Hızlı Başlangıç

### Ön Koşullar

- **Qt 6.11.0** (mingw_64 kit) + CMake + MinGW araçları
- **STM32CubeProgrammer** (`STM32_Programmer_CLI.exe`)
- **X-CUBE-AI 10.2.0** (`stedgeai.exe`)
- **arm-none-eabi-gcc** + **make** (STM32CubeIDE ile gelir)

> Araç yolları ilk açılışta `ToolDetector` tarafından otomatik aranır;
> bulunamazsa **Ayarlar → Araçlar** ekranından elle girilebilir.

### Derleme

```powershell
# Qt araç yollarını PATH'e ekle
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

> Build sonrası `templates/` klasörü exe'nin yanına kopyalanır ve `windeployqt`
> ile Qt runtime DLL'leri + QML plugin'leri dağıtılır (standalone çalışma).

---

## Pipeline Akışı

`PipelineRunner` tek bir `run(PipelineConfig)` çağrısıyla beş adımı sırayla,
asenkron yürütür:

```
.tflite
  │  1) stepAnalyze()  → stedgeai analyze   (model metrikleri, doğrulama)
  ▼
  │  2) stepXCubeAI()  → stedgeai generate  (C kodu üretimi)
  ▼
  │  3) stepPrepare()  → TemplateEngine     (base + sensor + ai_glue şablonları)
  ▼
  │  4) stepBuild()    → arm-none-eabi-gcc  (Makefile → .elf)
  ▼
  │  5) stepFlash()    → STM32_Programmer_CLI (karta yükle)
  ▼
finished(success)
```

UI tarafında bu akış **4 sayfalı `PipelineWizard`** ile yönlendirilir:
Model → Sensör → Özet/Araç Doğrulama → Derleme+Flash.

---

## UART Protokolü v1.0

Firmware → Qt yönünde her paket:

```
§{JSON}\r\n
```

- `§` = UTF-8 `0xC2 0xA7` — başlangıç işareti (her paketin ilk 2 byte'ı)
- Gövde: tek satır compact JSON · tüm sayısal değerler **integer** (float yok)
- UART gönderimi firmware'de **DMA** ile (`HAL_UART_Transmit_DMA`) — blocking yasak

```json
{"t":"inf","model":"MLP_INT8","inf_us":8200,"ram_b":3072,"acc_pct":96,"label":"walking","card":"STM32F4"}
{"t":"sys","uptime_s":42,"temp_c":38,"free_ram_b":185000,"state":"running"}
{"t":"boot","card":"STM32F4","sdk":"EdgeAI_v1.0","model":"MLP_INT8","baud":115200}
{"t":"err","code":3,"msg":"sensor_timeout"}
```

Tam referans → [`docs/protocol_v1.md`](docs/protocol_v1.md)

---

## Klasör Yapısı

```
stm32-ai-deployer-app/
├── CMakeLists.txt
├── src/
│   ├── main.cpp                  Uygulama bootstrap + QML motoru
│   ├── bridge/Backend.*          QML ↔ C++ tek cephe sınıfı
│   ├── core/                     AppState · AppSettings · ToolDetector · TemplateEngine
│   └── modules/
│       ├── board/                BoardManager · BoardPresets
│       ├── serial/               SerialManager · SerialWorker · PacketParser
│       ├── flash/                FlashManager · CliRunner · XCubeAIRunner · PipelineRunner
│       ├── analysis/             AnalysisManager (SQLite)
│       └── simulation/           FactorySimulator (demo motoru)
├── qml/
│   ├── Main.qml · Theme.qml
│   ├── screens/                  Dashboard · Board · Flash · Monitor · Benchmark · Analysis
│   ├── components/               AppButton · Card · DataTable · Terminal · TitleBar …
│   ├── dialogs/                  SettingsDialog · PipelineWizard · AboutDialog
│   └── factory/                  FactorySimWindow · FactoryDashboard · FactoryMap …
├── templates/                    STM32 firmware şablonları (base / sensors / ai_glue)
├── resources/                    style.qss · icons/
└── docs/                         PROJECT.md · protocol_v1.md · n6_kaldigimiz_yer.md
```

---

## Dokümantasyon


| Doküman                                                 | İçerik                                                                    |
| -------------------------------------------------------- | --------------------------------------------------------------------------- |
| [`docs/PROJECT.md`](docs/PROJECT.md)                     | Tüm mimari, modüller, veri akışı ve çalışma mantığı (uçtan uca) |
| [`docs/protocol_v1.md`](docs/protocol_v1.md)             | UART protokol referansı                                                    |
| [`docs/n6_kaldigimiz_yer.md`](docs/n6_kaldigimiz_yer.md) | STM32N6 boot/flash prosedürü                                              |
| [`CLAUDE.md`](CLAUDE.md)                                 | Geliştirme kuralları ve proje kimliği                                    |

---

## Ekip

**Muhammet Ali Şeker** · **Furkan Talha Kasım** · **Kadir Mert Abatay**

**Danışman:** Ali Sarıkaş
**Kurum:** Marmara Üniversitesi — Bilgisayar Mühendisliği Bitirme Projesi 2025–2026

---

<div align="center">
