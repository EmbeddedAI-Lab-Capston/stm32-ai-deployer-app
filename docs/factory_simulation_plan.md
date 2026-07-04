# Fabrika Simülasyonu (Factory Simulation) — Uygulama Planı

> **Not:** Bu doküman özelliğin **uygulanmadan önce yazılmış tasarım
> planıdır** — tarihi bir referanstır, güncel durum değil. `FactorySimulator`
> artık tamamen implemente edilmiş ve aktif kullanımdadır; güncel mimari için
> [`docs/PROJECT.md`](PROJECT.md) Bölüm 8'e bakın.

## Context

STM32 AI Deployer şu an tek kart üzerinde model yükleme, UART metrik izleme ve
analiz yapıyor. Bitirme sunumunda "bu uygulama ürüne dönüşürse nasıl bir endüstriyel
sistem olur?" sorusunu somut göstermek için **Fabrika Simülasyonu** adında yeni bir
mod ekliyoruz. Bu mod, gerçek donanım gerektirmeden büyük çaplı bir akıllı fabrikayı
(~20 STM32 node, ~68 sensör, 5 bölge) gerçeğe yakın canlı verilerle simüle eder:
genel dashboard, fabrika krokisi (harita), bölge detayı ve node/alarm detayı.

Kullanıcı kararları:
- **Giriş:** Header'daki (TitleBar) bir butona basınca **ayrı bir pencere** açılır —
  sanki bağımsız bir uygulamaymış gibi (kendi başlık çubuğu + kendi iç navigasyonu).
- **Sim motoru:** C++ `FactorySimulator` (QObject, QML'e expose edilir) — projenin
  C++ konvansiyonuna uyar, 20×68 güncellemede performanslı.
- **Ekranlar:** Genel Dashboard + Fabrika Krokisi + Bölge Detayı + Node/Alarm Detayı (hepsi).

## Mevcut Mimari (keşif sonucu)

- Aktif UI **tamamen QML**: `src/main.cpp` → `engine.loadFromModule("STM32AiDeployer", "Main")`.
  (`src/mainwindow.cpp` ve `src/ui/*Tab.*` legacy/ölü kod — dokunulmayacak.)
- `qml/Main.qml`: `ApplicationWindow` (frameless) → `TitleBar` + `TopTabBar` + `StackLayout`(6 ekran).
- Tasarım dili: `qml/Theme.qml` singleton (renkler, radii, spacing, tipografi, `statusColor()`).
- Hazır bileşenler (yeniden kullanılacak): `Card`, `InfoCard`, `MetricBar`, `StatusPill`,
  `DataTable`, `SectionHeader`, `AppButton`, `Terminal` — `qml/components/`.
- C++ bridge deseni: `src/bridge/Backend.{h,cpp}`, `main.cpp`'de
  `rootContext()->setContextProperty("backend", backend)` ile expose. Aynı desenle
  `factorySim` expose edilecek.
- CMake: `qt_add_qml_module()` QML dosyalarını **açıkça** listeler; yeni `.qml`'ler buraya eklenecek.

## Fabrika Modeli (FactorySimulator içeriği)

5 bölge / 20 node / ~68 sensör (kullanıcı spesifikasyonu temel alındı):

| # | Bölge | Node | Kart | Sensörler |
|---|-------|------|------|-----------|
| 1 | Çevresel Güvenlik & Erken Uyarı | 5 | STM32F4 | MQ-135/MQ-2 (gaz/duman), DHT22/SHT31 (sıcaklık/nem), LDR — ~20 |
| 2 | Sıvı/Gaz Akış & Basınç Kontrol | 5 | F4/H7 | Basınç, debimetre, ultrasonik seviye, solenoid valf — ~15 |
| 3 | Kestirimci Bakım & Ağır Makine | 4 | STM32H7 | MPU6050/ADXL345 (titreşim), termokupl, ACS712 (akım) — ~16 |
| 4 | Uç Yapay Zeka & Kalite Kontrol | 4 | H7/N6 | Kamera (DCMI)/TOF, YOLO TinyML inference — ~12 |
| 5 | Merkezi Ağ Geçitleri (Gateway) | 2 | H7 | Wi-Fi/Ethernet, SD kart, CAN/RS485/LoRa toplama — ~5 |

**Veri yapıları (C++):**
- `SensorSpec`: `id, name, type, unit, value, normalMin, normalMax, status`
- `NodeSpec`: `id, name, zoneId, board(F4/H7/N6), modelName, infUs, freeRamB, uptimeS, status, x, y, sensors[]`
- `ZoneSpec`: `id, name, color, nodeIds[]`
- `AlarmEvent`: `time, nodeId, nodeName, zoneName, severity(info/warning/critical), message`

**Gerçekçilik:** her tick'te `value = base + sinusoidalDrift + gauss noise`; düşük olasılıkla
anomali enjekte edilir (gaz piki, titreşim rezonansı, basınç aşımı, ısınma) → sensör/node
status'u `warning`/`critical`'a döner ve `AlarmEvent` üretilir. Status `Theme.statusColor()`
ile uyumlu string'ler (`ok`/`warning`/`error`).

## Yapılacaklar

### 1) C++ — Simülasyon motoru
**Yeni:** `src/modules/simulation/FactorySimulator.h` + `.cpp` (QObject)
- `QTimer` tabanlı tick (varsayılan 1 Hz). `Q_PROPERTY`: `running`, `intervalMs`, `speed`,
  `tickCount` ve toplu KPI'lar (`nodeCount`, `sensorCount`, `okCount`, `warningCount`,
  `criticalCount`, `activeAlarmCount`, `avgInfUs`).
- `Q_INVOKABLE`: `start()`, `stop()`, `setSpeed(double)`,
  `zones()→QVariantList`, `nodes()→QVariantList`, `nodesInZone(id)→QVariantList`,
  `node(id)→QVariantMap`, `sensorsOf(nodeId)→QVariantList`, `alarms(limit)→QVariantList`,
  `zoneSummary(id)→QVariantMap`.
- Sinyal: `tick()` (her güncellemede), `alarmRaised(QVariantMap)`, `runningChanged()`.
- Konvansiyon: `#pragma once`, tek sınıf/dosya, yeni connect syntax, yorumlar İngilizce.

**Değişiklik:** `src/main.cpp`
- `auto *factorySim = new FactorySimulator(&app);`
- `engine.rootContext()->setContextProperty("factorySim", factorySim);` (backend ile yan yana)

### 2) QML — Ayrı simülasyon penceresi
**Yeni:** `qml/factory/FactorySimWindow.qml` — kendi `ApplicationWindow`'u (Main.qml ile
aynı frameless + Theme stilini taklit eder, "ayrı uygulama" hissi). İçinde:
- Kendi başlık çubuğu (logo "FACTORY SIM" rozeti + min/max/close) ve bir iç nav bar
  (TopTabBar deseninde): `Genel` · `Kroki` · `Bölge` · `Node & Alarm`.
- `StackLayout` ile 4 ekran; `factorySim.start()` açılışta, `stop()` kapanışta.
- `property int rev: 0; Connections { target: factorySim; function onTick(){ rev++ } }`
  deseniyle ekranlar canlı yenilenir (mevcut Backend fonksiyon-tabanlı yenileme deseni gibi).

**Yeni ekranlar:** `qml/factory/`
- `FactoryDashboard.qml` — KPI özet kartları (`InfoCard`), bölge bazlı sağlık `MetricBar`'ları,
  son alarmlar `DataTable`. (DashboardScreen.qml düzenini referans alır.)
- `FactoryMap.qml` — 2D kroki: 5 `ZoneBlock` (renk kodlu kutu), içlerinde `NodeMarker`'lar
  (x/y konumlu, status'a göre yeşil/sarı/kırmızı, hover/tıkla → NodeDetail). Ana görsel cazibe.
- `ZoneDetail.qml` — seçili bölgenin node listesi + sensör tablosu + canlı `MetricBar`'lar.
- `NodeDetail.qml` — tek node drill-down: sensörler, canlı değerler, model/inference metrikleri
  + sağda fabrika geneli `AlarmFeed`.

**Yeni bileşenler:** `qml/factory/components/`
- `ZoneBlock.qml`, `NodeMarker.qml` (pulse animasyonlu durum noktası), `SensorRow.qml`,
  `AlarmFeed.qml`, `KpiTile.qml` (gerekirse; mümkünse `InfoCard` yeniden kullanılır).

### 3) Giriş noktası — Header butonu
**Değişiklik:** `qml/components/TitleBar.qml`
- Menü ile pencere kontrolleri arasına bir `AppButton` / özel buton: **"🏭 Fabrika Simülasyonu"**.
- Yeni sinyal `signal openFactorySim()`; buton `onClicked: root.openFactorySim()`.

**Değişiklik:** `qml/Main.qml`
- `TitleBar { ... onOpenFactorySim: factorySimWindow.show() }`
- Dialog'ların yanına `FactorySimWindow { id: factorySimWindow; visible: false }`.

### 4) CMake — dosya kaydı
**Değişiklik:** `CMakeLists.txt`
- `target_sources`/source listesine `FactorySimulator.h/.cpp` ekle.
- `qt_add_qml_module(... QML_FILES ...)` listesine tüm yeni `qml/factory/**` dosyalarını ekle.
- Yeni Qt modülü gerekmiyor (Quick/Controls/Layouts zaten linkli). `MultiEffect` için
  `QtQuick.Effects` zaten Card.qml'de kullanılıyor → ek bağımlılık yok.

## Kritik Dosyalar (özet)
- Yeni: `src/modules/simulation/FactorySimulator.{h,cpp}`
- Yeni: `qml/factory/FactorySimWindow.qml`, `qml/factory/{FactoryDashboard,FactoryMap,ZoneDetail,NodeDetail}.qml`
- Yeni: `qml/factory/components/{ZoneBlock,NodeMarker,SensorRow,AlarmFeed}.qml`
- Değişiklik: `src/main.cpp`, `qml/Main.qml`, `qml/components/TitleBar.qml`, `CMakeLists.txt`
- Yeniden kullanım: `qml/Theme.qml`, `qml/components/{Card,InfoCard,MetricBar,StatusPill,DataTable,SectionHeader,AppButton}.qml`

## Doğrulama (end-to-end)
1. Yapılandır + derle:
   ```powershell
   $env:PATH = "C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\mingw1310_64\bin;$env:PATH"
   cmake -B build -S . -DCMAKE_PREFIX_PATH="C:/Qt/6.11.0/mingw_64" -DCMAKE_BUILD_TYPE=Release -G "MinGW Makefiles"
   cmake --build build
   ```
2. Çalıştır: `.\build\stm32-ai-deployer.exe` → konsolda QML hata/uyarısı olmamalı.
3. Header'daki "🏭 Fabrika Simülasyonu" butonu → ayrı pencere açılır, sim otomatik başlar.
4. **Genel**: KPI'lar (node/sensör/alarm sayıları) 1 Hz güncellenir.
5. **Kroki**: 5 bölge + 20 node işareti görünür; bazıları zamanla sarı/kırmızıya döner;
   node'a tıklayınca Node detayına geçer.
6. **Bölge**: bölge seçimi node/sensör tablolarını ve canlı barları doğru gösterir.
7. **Node & Alarm**: sensör değerleri akar; anomali olunca alarm feed'e yeni satır düşer.
8. Pencere kapanınca `factorySim.stop()` çağrılır (ana uygulama etkilenmez).

## Kapsam Dışı (bu sürümde değil)
- Simülasyon verisinin SQLite'a kalıcı yazımı / Analiz sekmesiyle entegrasyon.
- Gerçek UART/donanım ile köprü; LoRa/CAN gibi protokol simülasyonunun byte seviyesi.
- Sim senaryolarını dosyadan yükleme/kaydetme (ileride eklenebilir).
