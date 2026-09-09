# Bellek-Öncelikli Telemetri Planı (Faz 10)

> **Bu dosya kendi kendine yeterlidir.** Bu planı uygulayan oturum, planın
> yazıldığı sohbeti hiç görmemiş olabilir. Ortam kurulumundan kabul
> kriterlerine kadar ihtiyaç duyulan her şey burada. Tahmin yürütme; bir şey
> yazılı değilse **dur ve kullanıcıya sor**.

**Oluşturulma:** 2026-09-09
**Dal:** `feature/register-inspector` (main'e merge yok — bkz. `TODO.md`)

---

## 0. Bu planın kuralları (ÖNCE BUNU OKU)

### 0.1 Doğrulama için sıfırdan araç yazma — mevcut olanı kullan

Bu projede UI'ı doğrulamak için **zaten kurulmuş bir çerçeve var**:
`--debug-bridge` bayrağıyla açılan named-pipe kanalı + `tools/uiprobe.ps1`
sürücüsü. Tasarımı: [`docs/verification_ecosystem_plan.md`](verification_ecosystem_plan.md).

**Yasak:** yeni bir ekran görüntüsü alma aracı, yeni bir UI otomasyon
mekanizması, yeni bir test sürücüsü yazmak.
**Zorunlu:** aşağıdaki komutları kullanmak.

```powershell
# Ekrandaki isimlendirilmiş öğeleri metin olarak listele (EN UCUZ, ÖNCE BUNU DENE)
.\tools\uiprobe.ps1 dump -Filter "watch."

# Backend/AppState özelliklerini oku
.\tools\uiprobe.ps1 props -Object backend -Filter "watchItems"

# Q_INVOKABLE metot çağır (en fazla 6 argüman; dönüş değeri "result" alanında gelir)
.\tools\uiprobe.ps1 invoke -Object backend -Method selectBoard -MethodArgs "NUCLEO-H723ZG"

# Sekmeye geç (indeksler §1.4'te)
.\tools\uiprobe.ps1 navigate -Tab 7

# objectName'i olan bir öğeye tıkla
.\tools\uiprobe.ps1 click -Name "watch.startButton"

# Ekran görüntüsü — SADECE görsel bir şey doğrulanacaksa
.\tools\uiprobe.ps1 shot -Path C:\dev\stm32-ai-deployer-app\out\ornek_e2e\01_durum.png

# Uygulamayı kapat
.\tools\uiprobe.ps1 quit
```

**Öncelik sırası:** `props`/`dump`/`invoke` (metin, ucuz) → `shot` (sadece
görsel doğrulama veya kanıt fotoğrafı gerektiğinde).

### 0.2 Ekran görüntüsü kuralları

- Her fazın **başarılı** ana adımlarında ekran görüntüsü al.
- Konum: `out/<faz_adı>_e2e/NN_kısa_açıklama.png` (örn.
  `out/sensor_memory_e2e/03_canli_degerler.png`)
- Numaralandırma iki haneli ve sıralı: `01_`, `02_`, `03_`...
- `out/` gitignore'da — **dosyaları silme**, diskte kalsınlar, kanıt onlar.
- Ekran görüntüsünü aldıktan sonra **Read aracıyla aç ve gerçekten
  beklediğin şeyi gösterdiğini doğrula.** Boş/yanlış ekran yakalamış olabilirsin.

### 0.3 Bir adım başarısız olursa ne yapılacak (teşhis oyun kitabı)

Sırayla, atlamadan:

1. **Uygulama durumunu metin olarak oku.**
   `props -Object backend -Filter "<ilgili>"` ve `dump -Filter "<ekran>."`
   Genellikle hata mesajı bir property'de veya bir banner'da duruyordur.

2. **Uygulama log'una bak.** `C:\dev\stm32-ai-deployer-app\build\app_trace.log`
   — QML uyarıları ve `qDebug` çıktısı burada. (Register/Watch hataları
   buraya YAZILMAZ, onlar sinyalle UI'ya gider — 1. adıma bak.)

3. **Firmware sessizse / karta dair şüphe varsa: geçici GDB oturumu.**
   Bu, uygulamanın kendi gözlemci-kısıtlı yolundan ayrı, **teşhis bitince
   kapatılan** harici bir oturumdur. Uygulamanın `DebugLink`'ini kullanma.

   ```bash
   # 1) gdbserver'ı başlat (kart seri numarası §1.3'te)
   "/c/ST/STM32CubeIDE_2.2.0/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.stlink-gdb-server.win32_2.2.500.202604010938/tools/bin/ST-LINK_gdbserver.exe" \
     -g -p 61234 -d -i <KART_SERI_NO> \
     -cp "/c/Program Files/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin" \
     > /tmp/gdbserver.log 2>&1 &
   sleep 3 && cat /tmp/gdbserver.log      # "Waiting for debugger connection..." görmelisin
   ```

   Sonra bir GDB script dosyası yaz (scratchpad'e) ve çalıştır:
   ```
   target remote localhost:61234
   info registers pc sp lr
   bt
   print/x *(unsigned int*)0xE000ED28
   print/x *(unsigned int*)0xE000ED2C
   quit
   ```
   ```bash
   cd "<ELF'in bulunduğu build klasörü>" && \
   "/c/ST/STM32CubeIDE_2.2.0/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.14.3.rel1.win32_1.0.100.202602081740/tools/bin/arm-none-eabi-gdb.exe" \
     -batch -x <script_yolu> <elf_dosyası>
   ```

   **Okuma kılavuzu:**
   | Adres | Register | Anlamı |
   |---|---|---|
   | `0xE000ED28` | CFSR | `0x80000` = UNDEFINSTR (genelde FPU açılmamış), `0x100` = INVSTATE, `0x8200` = PRECISERR (kötü adres) |
   | `0xE000ED2C` | HFSR | `0x40000000` = FORCED (alt bir fault yükseltilmiş) |

   `bt` çıktısındaki en üst kullanıcı frame'i (`main()` gibi) sana firmware'in
   **nerede takıldığını** söyler. `HardFault_Handler`'da duruyorsa firmware
   çökmüştür; normal bir HAL fonksiyonundaysa çalışıyordur (sadece o an
   orada yakalanmıştır).

   **Bitince gdbserver'ı kapat** (`-e` kullanmadığımız için detach edince
   kendi kapanır; `tasklist | grep -i gdbserver` ile doğrula).

4. **Hâlâ çözülmediyse dur ve kullanıcıya sor.** Ne denediğini, hangi
   çıktıyı aldığını yaz. **Tahminle kod değiştirme.**

### 0.4 Genel çalışma kuralları

- **Commit mesajlarında ASLA `Co-Authored-By` kullanma.** (Proje kuralı.)
- Her faz sonunda: `cmake --build` + `ctest` yeşil olmalı, sonra commit.
- Saf (donanımsız/QObject'siz) sınıf değiştirdiysen `tests/` altına test
  ekle — proje kuralı bu (`docs/variable_watcher_plan.md` Bölüm 13).
- Firmware template'i değiştirdiysen **üç kart için de** (F4/H7/N6) aynı
  değişikliği yap, yoksa aile-özel sapma oluşur (bu tam olarak F4'ün
  `SystemInit` hatasının sebebiydi — bkz. §2.3).
- `CLAUDE.md` ve `TODO.md`'yi güncel tut.

---

## 1. Ortam kurulumu (her oturumda önce bu)

### 1.1 Yollar — ASCII junction ZORUNLU

Proje gerçekte `D:\Yazılım\stm32-ai-deployer-app` altında ama yolda Türkçe
karakter var ve bu bazı ST/Qt araçlarını (qmlimportscanner, stedgeai, python)
kırıyor. Bu yüzden bir ASCII junction var:

```
C:\dev\stm32-ai-deployer-app  →  D:\Yazılım\stm32-ai-deployer-app
```

- **Derleme, uygulamayı çalıştırma, uiprobe, ST araçları: DAİMA `C:\dev\...`**
- Dosya okuma/yazma/git: her iki yol da olur (aynı depo).
- Python ile Türkçe yollu dosya açma **çalışmaz** — `C:\dev\...` kullan.

### 1.2 Derleme ve çalıştırma

```powershell
# PATH (her yeni PowerShell oturumunda)
$env:PATH = "C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\mingw1310_64\bin;$env:PATH"

# Derle
cmake --build C:\dev\stm32-ai-deployer-app\build -j

# Test (ctest için Qt DLL'leri de PATH'te olmalı)
$env:PATH = "C:\Qt\6.11.0\mingw_64\bin;$env:PATH"
ctest --test-dir C:\dev\stm32-ai-deployer-app\build --output-on-failure

# Uygulamayı doğrulama kanalıyla başlat
Start-Process -FilePath "C:\dev\stm32-ai-deployer-app\build\STM32AiDeployer.exe" `
  -ArgumentList "--debug-bridge","--no-splash" `
  -WorkingDirectory "C:\dev\stm32-ai-deployer-app\build"
Start-Sleep -Seconds 2
```

**Tuzak:** uygulama çalışırken derlersen link adımı "Permission denied" verir.
Önce `Get-Process -Name STM32AiDeployer | Stop-Process -Force` veya
`uiprobe.ps1 quit`.

### 1.3 Kart envanteri

| Kart | Preset adı (`selectBoard` argümanı) | ST-Link PID | ST-Link seri no | COM | Baud | UART durumu |
|---|---|---|---|---|---|---|
| F4 | `STM32F407 Discovery` | `374B` | `066AFF3332584B3043214814` | COM4 | 115200 | **YOK** — VCP hedef USART'a köprülü değil (§2.4) |
| H7 | `NUCLEO-H723ZG` | `374E` | `004D003B3235511837333439` | COM3 | 115200 | Çalışıyor |
| N6 | `NUCLEO-N657X0-Q` | `3754` | `001A00273434511734313937` | COM5 | 209700 | Pasif yakalama (komut-cevap yok) |

COM numarası USB portu değişirse kayabilir. Uygulamadan doğrula:
```powershell
.\tools\uiprobe.ps1 invoke -Object backend -Method detectedStLinkPort
```
(Önce `selectBoard` ile doğru kartı seçmiş olmalısın — bu metot **aktif
karta** göre eşleştirir.)

### 1.4 Sekme indeksleri

`0`=Dashboard `1`=Kartlar `2`=Flash `3`=Monitör `4`=Benchmark `5`=Analiz
`6`=Register `7`=İzleyici

### 1.5 Mevcut `objectName`'ler (uiprobe `click` için)

```
tabbar.tab0 … tabbar.tab7
watch.connectButton  watch.loadElfButton  watch.addSymbolButton
watch.addAddressButton  watch.startButton  watch.clearDataButton
watch.plot  watch.itemTable  watch.ruleFeed
watch.symbolDialogCloseButton  watch.compareProfilesButton
watch.compareDialogCloseButton
register.snapshotAButton  register.snapshotBButton
register.diffButton  register.diffPopupCloseButton
analysis.subTab0 … analysis.subTab4
```

Yeni bir butona tıklaman gerekiyorsa **QML'e `objectName` ekle** (yukarıdaki
`<ekran>.<eleman>` desenini izle), derle, sonra tıkla. Koordinatla tıklama yok.

### 1.6 `uiprobe invoke`'un sınırları ve ham JSON kaçış yolu

- En fazla **6 argüman**.
- `-MethodArgs` sadece **skaler** değer gönderebilir (string/sayı/bool).
- **Dizi veya iç içe nesne** göndermek gerekiyorsa (örn.
  `takeRegisterSnapshot(int, QStringList)` veya `runPipeline(QVariantMap)`),
  ham JSON'u doğrudan pipe'a yaz. Şablon (scratchpad'e kaydet, depoya değil):

```powershell
param([int]$Slot, [string[]]$Peripherals)
$request = @{
    cmd = "invoke"; object = "backend"; method = "takeRegisterSnapshot"
    args = @($Slot, $Peripherals)
}
$json = $request | ConvertTo-Json -Compress -Depth 5
$client = New-Object System.IO.Pipes.NamedPipeClientStream(".", "stm32aid-debug", [System.IO.Pipes.PipeDirection]::InOut)
$client.Connect(3000)
$enc = New-Object System.Text.UTF8Encoding($false)
$w = New-Object System.IO.StreamWriter($client, $enc); $w.AutoFlush = $true
$r = New-Object System.IO.StreamReader($client, $enc)
$w.WriteLine($json); Write-Output ("REPLY: " + $r.ReadLine()); $client.Dispose()
```

### 1.7 Dosya konumları

| Ne | Nerede |
|---|---|
| Modeller | `Models/environmental/`, `Models/motion/`, `Models/audio/` (gitignored) |
| F4 için model | `Models/environmental/anomaly_mlp_int8.tflite` |
| H7 için model | `Models/environmental/anomaly_cnn_int8.tflite` |
| H7 referans ELF | `out/pipeline_test_h7/build/anomaly_cnn_int8_STM32H7.elf` |
| F4 referans ELF | `out/pipeline_test_f4/build/anomaly_mlp_int8_STM32F4.elf` |
| HAL SDK'ları | `~/STM32Cube/Repository/STM32Cube_FW_F4_V1.28.0`, `..._H7_V1.13.0` |
| Firmware şablonları | `templates/base/STM32{F4,H7,N6}/`, `templates/ai_glue/`, `templates/sensors/` |
| İzleyici presetleri | `watch/watch_presets.json` |
| İzleyici kuralları | `watch/watch_rules.json` |

**N6 için SDK yoksa** (`STM32Cube_FW_N6_*` klasörü yoksa) şu tarifle indir —
F4/H7 için de aynı desen (`docs/dev_machine_setup.md` §7):
```bash
mkdir -p ~/STM32Cube/Repository && cd ~/STM32Cube/Repository
git clone --depth 1 --filter=blob:none --sparse \
  https://github.com/STMicroelectronics/STM32CubeN6.git STM32Cube_FW_N6_V1.0.0
cd STM32Cube_FW_N6_V1.0.0
git sparse-checkout set Drivers
cat .gitmodules      # submodule yollarını buradan doğrula, sonra:
git submodule update --init --depth 1 \
  Drivers/CMSIS/Device/ST/STM32N6xx Drivers/STM32N6xx_HAL_Driver
```

### 1.8 Ortamın çalıştığını doğrula (duman testi — HER OTURUM BAŞINDA)

```powershell
$env:PATH = "C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\mingw1310_64\bin;$env:PATH"
cmake --build C:\dev\stm32-ai-deployer-app\build -j          # hatasız bitmeli
Start-Process -FilePath "C:\dev\stm32-ai-deployer-app\build\STM32AiDeployer.exe" `
  -ArgumentList "--debug-bridge","--no-splash" -WorkingDirectory "C:\dev\stm32-ai-deployer-app\build"
Start-Sleep -Seconds 2
cd C:\dev\stm32-ai-deployer-app
.\tools\uiprobe.ps1 dump -Filter "tabbar."     # 8 sekme dönmeli
```
8 sekme dönmüyorsa **devam etme** — ortam bozuk, §0.3'e git.

---

## 2. Mevcut durum — neyin çalıştığı, neyin çalışmadığı

### 2.1 Çalışan ve kanıtlanmış (dokunma, üzerine inşa et)

- **Pipeline:** `.tflite → stedgeai → şablon → gcc → flash`. H7 ve F4'te
  gerçek donanımda koştu.
- **Değişken İzleyici:** ELF sembollerinden veya elle girilen adresten SWD
  üzerinden canlı bellek okuma (≈200–1000 Hz), grafik, CSV kayıt/oynatma,
  profil kaydetme/karşılaştırma, pencereli kural motoru.
  **Gözlemci ilkesi kod düzeyinde zorunlu:** hedef durdurulmaz, reset
  edilmez, belleğe yazılmaz (`GdbRspCodec::isAllowedOutgoing()` beyaz listesi).
- **RegionScan / stack watermark:** canlı çalışıyor (H7'de gerçek `2896 B`).
- **Register Inspector:** SVD kataloğu (peripheral→register→field→enum),
  Snapshot A/B, A→B farkı, kural motoru, JSON export, CLI + GDB arka uçları.
  **Ama snapshot tabanlı — canlı akış yok.**
- **Doğrulama çerçevesi:** DebugBridge + `uiprobe.ps1` (§0.1).

### 2.2 İzleyici'nin bilinen ve KABUL EDİLMİŞ maliyeti

RegionScan istekleri ana örnekleme planına giriyor (ayrı düşük-hızlı bir
zamanlayıcı yok). H7'de 4 KB'lık stack bölgesi izlenirken hedef 200 Hz
**gerçek ~100 Hz**'e düşüyor. Bu bilinen ve dokümante bir maliyet
(`WatchPlanBuilder.h`), hata değil. Bu plan bunu **değiştirmiyor**.

### 2.3 F4'te düzeltilmiş kritik hata (tekrar etmesin)

`templates/base/STM32F4/startup_stm32f407xx.s` içindeki `Reset_Handler`
**`SystemInit()`'i hiç çağırmıyordu** → FPU açılmıyordu → `-mfloat-abi=hard`
derlemesi ilk VFP komutunda UsageFault veriyordu (CFSR `0x80000`), firmware
`main()`'in ilk satırında donuyordu. H7 ve N6'nınki doğruydu; sadece F4
sapmıştı. Düzeltildi (commit `9a41bc2`).

> **Ders:** bir kart ailesinde firmware hiç UART çıktısı vermiyorsa, o
> ailenin `startup_*.s` dosyasını diğerlerininkiyle karşılaştır.

### 2.4 F4'ün UART'ı YOK — bu plan bunu varsayar

F4 Discovery'de ST-Link'in VCP'si hedefin USART2'sine köprülü değil.
Firmware'in kendisi doğru çalışıp UART'a yazıyor (GDB ile kanıtlandı), ama
PC'ye **hiçbir bayt ulaşmıyor** (ham `System.IO.Ports.SerialPort` ile de
doğrulandı — 3 saniyede 0 bayt). Harici USB-seri adaptör kullanılmayacak
(kullanıcı kararı).

**Sonuç:** F4'te tüm doğrulama **SWD/bellek yoluyla** yapılacak. UART ile
çapraz doğrulama **sadece H7'de** mümkün.

### 2.5 N6'nın bilinen engelleri — zaman kutulu, best-effort

- Register snapshot **başarısız** ("RCC read failed (exit 1)") — güvenli
  firmware üzerinde HOTPLUG bağlantısı çalışmıyor (TrustZone/RIF).
  `svd/boards.json` bunu zaten not ediyor. **Yeni bir hata değil.**
- Şablon seti "kanıtlanmış" sayılmıyor (`docs/n6_kaldigimiz_yer.md`).
- UART pasif yakalama ile çalışıyor (LPUART1 @ 209700).

**Kural:** her fazda N6 adımlarını **dene**; çıkmazsa "N6'da deneysel /
doğrulanamadı" diye işaretle, gerekçesini yaz ve **devam et**. N6 takvimi
rehin almayacak.

### 2.6 Bu planın kapattığı boşluklar

| # | Eksik | Kaynak |
|---|---|---|
| 1 | Sensörün **ham fiziksel değerleri** bellekten okunamıyor (yerel değişkende, ELF'te sembolü yok) | Kullanıcı hedefi |
| 2 | **Canlı peripheral/register izleme** yok (Register tab snapshot; İzleyici SVD'yi bilmiyor) | Kullanıcı hedefi |
| 3 | **RAM bütçesi** görselleştirmesi yok | Öneri |
| 4 | Firmware'in bildirdiği `inf_us` **bağımsız doğrulanmıyor** | Öneri |
| 5 | "Bu model bu karta **sığar mı**" ön kontrolü yok | Öneri |
| 6 | **Çok-modelli otomatik süpürme** yok (elle yapılıyor) | Öneri |

---

## 3. Faz 10.1 — Sensör ham değerlerini bellekten okunur yapmak (seqlock ile)

**Amaç:** UART'a hiç ihtiyaç duymadan, sensörün ham fiziksel değerlerini ve
inference sonucunu canlı bellekten, **yırtık okuma olmadan** okumak.

### 3.1 Sorun

`templates/base/STM32*/Src/main.c` içindeki döngüde:

```c
float input[AI_INPUT_SIZE];          /* ← yığında (stack), ELF'te sembolü YOK */
AI_InferenceResult result;           /* ← yığında, sembolü YOK */
```

Yerel değişkenlerin sabit adresi olmadığı için İzleyici bunları göremez.
`ai_runner.c`'deki `g_ai_*` global'leri görülebiliyor (bu doğru desen), ama
ham sensör değerleri ve etiket dışarıda kalıyor.

### 3.2 Seqlock neden gerekli

Host asenkron örneklerken firmware yazmanın ortasında olabilir → yarısı eski
yarısı yeni veri. Çözüm: bloğun **başında ve sonunda** birer sayaç. Firmware
yazmadan önce baştakini artırır, yazdıktan sonra sondakini eşitler. Host tüm
bloğu tek okumada alır; **iki sayaç eşit değilse o örneği atar.**

### 3.3 Yeni dosya: `templates/ai_glue/telemetry.h`

```c
#pragma once
#include <stdint.h>

/* Host (Degisken Izleyici) tarafindan SWD uzerinden okunan telemetri blogu.
 * Butun alanlar tek bir struct icinde ve BITISIK -> WatchPlanBuilder bunlari
 * tek bir okuma blogunda birlestirir, boylece seq_begin/seq_end ayni SWD
 * okumasinda gelir ve seqlock dogrulamasi anlamli olur.
 *
 * Seqlock sozlesmesi:
 *   yazan  : seq_begin++ -> DMB -> alanlari yaz -> DMB -> seq_end = seq_begin
 *   okuyan : blogu oku; seq_begin == seq_end degilse ORNEGI AT.
 */

#define TELEMETRY_MAX_SENSOR 8
#define TELEMETRY_LABEL_LEN  24

typedef struct {
    volatile uint32_t seq_begin;                       /* offset 0  */
    volatile float    sensor[TELEMETRY_MAX_SENSOR];    /* ham fiziksel deger */
    volatile uint32_t sensor_count;                    /* gecerli sensor[] adedi */
    volatile uint32_t sensor_ok;                       /* 1 = son okuma basarili */
    volatile uint32_t inf_us;                          /* son inference suresi */
    volatile uint32_t infer_count;                     /* toplam inference */
    volatile uint32_t cycle;                           /* ana dongu sayaci */
    volatile uint32_t class_id;
    volatile uint32_t confidence_pct;
    volatile char     label[TELEMETRY_LABEL_LEN];
    volatile uint32_t seq_end;                         /* seq_begin ile ESIT olmali */
} TelemetryBlock;

extern volatile TelemetryBlock g_telemetry;

void Telemetry_BeginWrite(void);
void Telemetry_EndWrite(void);
```

### 3.4 Yeni dosya: `templates/ai_glue/telemetry.c`

```c
#include "telemetry.h"
#include "main.h"     /* __DMB() icin CMSIS */

volatile TelemetryBlock g_telemetry;

void Telemetry_BeginWrite(void)
{
    g_telemetry.seq_begin++;
    __DMB();                     /* seq once gorunur olsun */
}

void Telemetry_EndWrite(void)
{
    __DMB();                     /* veri once gorunur olsun */
    g_telemetry.seq_end = g_telemetry.seq_begin;
}
```

### 3.5 `main.c` değişikliği — ÜÇ KART İÇİN DE AYNI

`templates/base/STM32F4/Src/main.c`, `.../STM32H7/Src/main.c`,
`.../STM32N6/Src/main.c`.

1. Başa ekle: `#include "telemetry.h"`
2. Döngüde, `AI_Runner_Infer` çağrısından **sonra**, `UART_Report_*`
   çağrılarından **önce** şunu ekle:

```c
        /* Telemetriyi bellege yaz — host bunu SWD ile okuyor (UART sart degil) */
        Telemetry_BeginWrite();
        for (uint32_t ti = 0; ti < AI_INPUT_SIZE && ti < TELEMETRY_MAX_SENSOR; ++ti)
            g_telemetry.sensor[ti] = input[ti];
        g_telemetry.sensor_count   = (AI_INPUT_SIZE < TELEMETRY_MAX_SENSOR)
                                        ? AI_INPUT_SIZE : TELEMETRY_MAX_SENSOR;
        g_telemetry.sensor_ok      = 1;
        g_telemetry.inf_us         = inf_us;
        g_telemetry.infer_count    += 1;
        g_telemetry.cycle          = cycle;
        g_telemetry.class_id       = result.class_id;
        g_telemetry.confidence_pct = result.confidence_pct;
        for (uint32_t li = 0; li < TELEMETRY_LABEL_LEN; ++li)
            g_telemetry.label[li] = result.label[li];
        Telemetry_EndWrite();
```

3. Sensör okuması başarısız olan dalda da (`Sensor_Read(...) != HAL_OK`
   olan `continue` dalında) durumu işaretle — **sessizce eski değeri
   bırakma**:

```c
        if (Sensor_Read(input, AI_INPUT_SIZE) != HAL_OK) {
            Telemetry_BeginWrite();
            g_telemetry.sensor_ok = 0;
            Telemetry_EndWrite();
            HAL_Delay(200);
            continue;
        }
```

### 3.6 Makefile değişikliği — ÜÇ KART İÇİN DE

`templates/base/STM32{F4,H7,N6}/Makefile` içinde `Src/stack_paint.c`
satırının yanına ekle:

```
  Src/telemetry.c \
```

> `templates/ai_glue/*.c|h` dosyaları `TemplateEngine` tarafından projenin
> `Src/`/`Inc/` klasörlerine kopyalanıyor. Yeni dosyanın kopyalandığını
> §3.9'daki derleme adımında doğrulayacaksın; kopyalanmıyorsa
> `src/core/TemplateEngine.cpp` ve `src/modules/flash/PipelineRunner.cpp`
> içindeki `ai_glue` kopyalama listesine `telemetry` dosyalarını ekle.

### 3.7 Host tarafı: `WatchItem`'a guard alanları

`src/modules/watcher/WatchModel.h` — `WatchItem` struct'ına ekle:

```cpp
    // Seqlock koruması (0 = korumasiz). Ikisi de doluysa WatchSampler bu
    // iki u32'yi okur ve ESIT DEGILSE bu ornegi ok=false yapar — host
    // yazmanin ortasinda yakalanmis demektir (torn read).
    quint64        guardBeginAddr = 0;
    quint64        guardEndAddr   = 0;
```

### 3.8 Host tarafı: plan + decode

**`src/modules/watcher/WatchPlanBuilder.cpp` — `build()`:**
Korumalı bir kalem için istenen aralığı, guard adreslerini de kapsayacak
şekilde genişlet. Yani o kalemin `Range`'i:
- `start = min(item.address, guardBeginAddr)`
- `end   = max(item.address + boyut, guardEndAddr + 4)`

Böylece guard'lar **aynı cevap paketinde** gelir.

`WatchPlan`'a paralel bir vektör ekle:
```cpp
    // itemSlots ile ayni indeksleme. {-1,-1,-1} = korumasiz.
    // {requestIndex, beginOffset, endOffset}
    QVector<std::array<int,3>> guardSlots;
```
ve `build()` içinde korumalı kalemler için doldur (`merge()` de bunu
taşımalı — region planı guard kullanmıyor, sadece skaler plandan kopyala).

**`src/modules/watcher/WatchSampler.cpp` — `decodeSample()`:**
Skaler decode'dan **önce** guard kontrolü:

```cpp
        // Seqlock kapisi: iki sayac esit degilse host yazmanin ortasinda
        // yakalanmistir; bu ornek icin deger UYDURMA, ok=false birak.
        if (i < plan.guardSlots.size() && plan.guardSlots.at(i)[0] >= 0) {
            const auto &g = plan.guardSlots.at(i);
            const MemoryReply &gr = replies.at(g[0]);
            if (!gr.ok) continue;
            bool okB = false, okE = false;
            const double b = ValueCodec::decode(gr.data, g[1], WatchValueType::U32, &okB);
            const double e = ValueCodec::decode(gr.data, g[2], WatchValueType::U32, &okE);
            if (!okB || !okE || b != e)
                continue;      // ok[i] false kalir
        }
```

### 3.9 Presetler: `watch/watch_presets.json`

`xcubeai_runtime` presetine yeni kalemler ekle. **Adresler `g_telemetry`
sembolünden ve alan offset'lerinden çözülecek** — `WatchPresetMatcher`
şu an sadece düz sembol adı çözüyor, bu yüzden ona **offset desteği** ekle:

Preset formatına yeni alanlar:
```json
{ "role": "sensor0", "symbol": "g_telemetry", "offset_bytes": 4,   "type": "f32", "unit": "" ,
  "guardBegin": { "symbol": "g_telemetry", "offset_bytes": 0 },
  "guardEnd":   { "symbol": "g_telemetry", "offset_bytes": 96 } }
```

> **Offset'leri tahmin etme.** Derlenmiş ELF'ten gerçek offset'leri şu
> komutla çıkar ve preset'e onları yaz:
> ```bash
> arm-none-eabi-gdb -batch -ex "print/x (int)&((TelemetryBlock*)0)->seq_end" <elf>
> ```
> veya daha basiti: `arm-none-eabi-nm <elf> | grep g_telemetry` ile taban
> adresi al, `pahole`/`gdb ptype /o TelemetryBlock` ile alan offset'lerini
> doğrula. **Doğrulamadan yazma.**

`WatchPresetMatcher::resolveSuggestions()` içinde:
- `offset_bytes` varsa çözülen adrese ekle,
- `guardBegin`/`guardEnd` varsa `item.guardBeginAddr/guardEndAddr` doldur.

### 3.10 Birim testleri (ZORUNLU)

`tests/TestWatchSampler.cpp` (mevcut desene uy — dosyanın başındaki
`namespace { ... }` yardımcılarını kullan):

1. `guardsMatchingDecodesNormally` — `seq_begin == seq_end` → `ok=true`, doğru değer.
2. `guardsMismatchedReportsNotOk` — `seq_begin != seq_end` → `ok=false`, değer `0.0`.
3. `guardChunkFailedReportsNotOk` — guard'ın olduğu cevap `ok=false` → `ok=false`.

`tests/TestWatchPlanBuilder.cpp`:
4. `guardedItemRangeCoversBothGuards` — üretilen `MemoryRequest` aralığı hem
   `guardBeginAddr`'ı hem `guardEndAddr+4`'ü kapsıyor, `guardSlots` offset'leri doğru.

`tests/TestWatchPresetMatcher.cpp`:
5. `offsetBytesIsAddedToSymbolAddress` — `offset_bytes` adrese ekleniyor.

Hepsi `ctest` ile yeşil olmalı.

### 3.11 Canlı doğrulama — H7 (önce burada, çünkü UART var → çapraz doğrulama)

```powershell
# 1) Pipeline'i H7 icin kosur (ham JSON scriptiyle, §1.6 sablonu; method=runPipeline)
#    config: modelPath=Models/environmental/anomaly_cnn_int8.tflite
#            modelName=anomaly_cnn_int8, targetBoard=STM32H7,
#            sensorType=BME280, outputDir=.../out/pipeline_test_h7
# 2) Pipeline bitene kadar bekle:
.\tools\uiprobe.ps1 props -Object backend -Filter "pipeline"   # busy=False, stage="Pipeline tamamlandı"

# 3) Izleyici'ye gec, bagla, ELF yukle, presetleri uygula, baslat
.\tools\uiprobe.ps1 invoke -Object backend -Method selectBoard -MethodArgs "NUCLEO-H723ZG"
.\tools\uiprobe.ps1 navigate -Tab 7
.\tools\uiprobe.ps1 click -Name "watch.connectButton"
Start-Sleep -Seconds 3
.\tools\uiprobe.ps1 invoke -Object backend -Method loadWatchElf -MethodArgs "C:/dev/stm32-ai-deployer-app/out/pipeline_test_h7/build/anomaly_cnn_int8_STM32H7.elf"
.\tools\uiprobe.ps1 props -Object backend -Filter "watchElfMatch"     # "match" olmali
.\tools\uiprobe.ps1 invoke -Object backend -Method applyWatchPresets
.\tools\uiprobe.ps1 click -Name "watch.startButton"
Start-Sleep -Seconds 5
.\tools\uiprobe.ps1 props -Object backend -Filter "watchItems"
```

**Kabul kriterleri (H7):**
- `sensor0/1/2` kalemleri `hasValue: true` ve **fiziksel olarak makul**
  değerler (BME280: sıcaklık ~20–35, nem ~20–70, basınç ~95000–105000
  civarı — birimler firmware'in ölçeğine göre).
- `watchRateInfo.readErrors` **artmıyor** (seqlock her örneği atmıyorsa).
- **Çapraz doğrulama:** UART'ı da bağla (`connectSerial "COM3" 115200`),
  Monitör'deki `§sensor` paketindeki `values[]` ile İzleyici'deki
  `sensor0/1/2` **aynı büyüklükte** olmalı (UART milli-birimde tam sayı
  gönderiyor: `input[0]*1000`; yani İzleyici'deki `22.92` ↔ UART'taki `22920`).
  Bu eşleşme, **SWD yolunun doğruluğunun kanıtıdır** — bunu mutlaka
  ekran görüntüsüyle belgele.

**Ekran görüntüleri:** `out/sensor_memory_e2e/`
`01_h7_presetler_uygulandi.png`, `02_h7_canli_sensor_degerleri.png`,
`03_h7_uart_vs_bellek_capraz_dogrulama.png`

### 3.12 Canlı doğrulama — F4 (UART yok, sadece bellek)

Aynı akış, ama:
- `selectBoard "STM32F407 Discovery"`, model `anomaly_mlp_int8.tflite`,
  `targetBoard=STM32F4`, `outputDir=.../out/pipeline_test_f4`
- **UART adımı yok.** Doğrulama tamamen İzleyici üzerinden.
- Değerlerin makullüğü tek kontrol (H7'deki BME280 ile aynı ortamda
  benzer büyüklükte olmalı).

**Ekran görüntüleri:** `04_f4_canli_sensor_degerleri.png`

### 3.13 Canlı doğrulama — N6 (zaman kutulu)

Dene; `applyWatchPresets` sonrası kalemler `hasValue: true` oluyorsa
belgele. Bağlantı/şablon sorunundan çıkmıyorsa **§2.5 uyarınca "N6'da
doğrulanamadı: <gerçek hata mesajı>" diye not düş ve devam et.**

### 3.14 Faz 10.1 bitti sayılır ki

- [ ] `telemetry.h/.c` üç kartın da Makefile'ında, derleniyor
- [ ] `main.c` üç kartta da telemetriyi yazıyor (başarısız sensör dalı dahil)
- [ ] `guardBeginAddr/guardEndAddr` + `guardSlots` + `decodeSample` kapısı yazıldı
- [ ] 5 birim test eklendi, `ctest` yeşil
- [ ] H7'de canlı sensör değerleri okundu **ve UART ile çapraz doğrulandı**
- [ ] F4'te canlı sensör değerleri okundu (UART'sız)
- [ ] N6 denendi (sonuç ne olursa olsun yazıldı)
- [ ] Ekran görüntüleri `out/sensor_memory_e2e/` altında
- [ ] `TODO.md` + `CLAUDE.md` güncellendi, commit atıldı

---

## 4. Faz 10.2 — RAM bütçesi görselleştirmesi

**Amaç:** "Bu model bu karta sığdı ve en dar anda X KB payı kaldı" diyebilmek.

### 4.1 KRİTİK İNCELİK — yanlış sayı üretme tuzağı

Linker sembolleri (`_end`, `_sstack`, `_estack`) için **anlamlı olan
sembolün ADRESİDİR, o adresteki bellek içeriği DEĞİL.**

Şu an `watch_presets.json`'da `_end` bir kalem olarak izleniyor ve gösterilen
değer (`0x2c760172` gibi) **çöp** — `_end` adresindeki rastgele bellek
içeriği. RAM bütçesi için kullanılması gereken sayı `_end`'in **adresi**
(`0x20002c00`), ki bunu uygulama zaten ELF sembol tablosundan biliyor,
okumaya gerek yok.

| Büyüklük | Kaynak | Bellek okuması gerekir mi? |
|---|---|---|
| Statik veri sonu (.bss sonu) | `_end` sembolünün **adresi** | Hayır |
| Stack tabanı | `_sstack` sembolünün **adresi** | Hayır |
| RAM tepesi | `_estack` sembolünün **adresi** | Hayır |
| Heap tepesi (anlık) | `__sbrk_heap_end` **değişkeninin değeri** | **Evet** |
| Stack en dar boşluk | `stackWatermark` RegionScan **değeri** (B) | **Evet** |

### 4.2 Hesap

```
ram_total      = adres(_estack) - RAM_ORIGIN        (veya AppState.boardRamKb)
static_end     = adres(_end)
heap_top       = deger(__sbrk_heap_end)             (0 ise heap kullanilmiyor)
stack_dip      = adres(_sstack) + deger(stackWatermark)   /* en derin kullanim noktasi */
serbest_bayt   = stack_dip - max(static_end, heap_top)
```

`serbest_bayt` **negatifse** → çakışma var, kırmızı uyarı göster
(sessizce gösterme).

### 4.3 Yapılacaklar

1. **`src/bridge/Backend.h/.cpp`:**
   `Q_INVOKABLE QVariantMap ramBudget() const;`
   Döndürdüğü map: `{ ok, ramTotal, staticEnd, heapTop, stackDip, freeBytes,
   usedPct, warning }`. Sembol adreslerini `m_watcher->symbols()`'tan,
   değerleri `m_watcher->buffer().stats(i).last`'tan (role'e göre bularak) al.
   Gerekli sembol/rol yoksa `ok=false` + `warning` doldur — **uydurma sayı
   döndürme.**
2. **`qml/components/watch/RamBudgetBar.qml`** (yeni): yatay yığılmış bar —
   statik / heap / boş / stack. `Theme` renklerini kullan, inline stil yazma.
3. **`qml/screens/WatchScreen.qml`:** barı `WatchLinkStatus`'ın üstüne ekle.
   `objectName: "watch.ramBudget"` ver.
4. **Birim test** `tests/TestRamBudget.cpp` (saf hesap fonksiyonunu
   `Backend`'den ayrı, test edilebilir bir yardımcıya çıkarırsan): normal
   durum, heap=0 durumu, çakışma (negatif) durumu.

### 4.4 Canlı doğrulama

H7 ve F4'te: presetleri uygula, örneklemeyi başlat, bar dolmalı.
`ramBudget()` çıktısındaki `freeBytes` ile `stackWatermark` kaleminin canlı
değeri **tutarlı** olmalı.

**Ekran görüntüleri:** `out/ram_budget_e2e/01_h7_bar.png`, `02_f4_bar.png`

### 4.5 Bitti sayılır ki
- [ ] `ramBudget()` sembol **adresi** ile **değeri** ayrımını doğru yapıyor
- [ ] Eksik veri varsa `ok=false` (uydurma yok)
- [ ] Bar H7 ve F4'te canlı doldu, ekran görüntüsü var
- [ ] Testler yeşil, commit atıldı

---

## 5. Faz 10.3 — Firmware'in bildirdiği süreyi bağımsız doğrulama

**Amaç:** `inf_us` değerini firmware kendisi ölçüp kendisi söylüyor. Host
tarafından **bağımsız bir üst sınır kontrolü** yaparak bu beyanı sınamak.

### 5.1 Neyin doğrulanabildiği konusunda dürüst ol

- **Doğrulanabilir:** çıkarım **hızı** (inference/saniye) — host kendi
  saatiyle `infer_count`'un artışını ölçebilir.
- **Doğrudan doğrulanamaz:** tek bir inference'ın mikrosaniye süresi
  (firmware döngüsünde `HAL_Delay(16)` var, hız zaten ona bağlı).
- **Ama anlamlı bir sınır kontrolü var:** gözlenen hız, firmware'in
  iddia ettiği süreden türeyen teorik üst sınırı **aşamaz**:
  `gözlenen_hz <= 1e6 / inf_us` olmalı. Aşıyorsa firmware'in beyanı
  **kanıtlanabilir şekilde yanlıştır**.

Bunu UI'da **"tutarlı" / "TUTARSIZ"** olarak göster; "doğrulandı" deme
(yanlış güven verir).

### 5.2 Yapılacaklar

1. **`src/bridge/Backend.h/.cpp`:**
   `Q_INVOKABLE QVariantMap inferenceRateCheck(double windowSec) const;`
   Dönen map: `{ ok, observedHz, reportedInfUs, theoreticalMaxHz,
   consistent, detail }`.
   Hesap: `infer_count` rolündeki kalemin `TraceBuffer`'daki ilk/son
   değerinden ve zaman damgalarından `observedHz = Δcount / Δt`.
   `windowSec` kadar veri yoksa `ok=false`.
2. **UI:** `WatchLinkStatus.qml`'e küçük bir rozet: `"12.4 Hz gözlendi ·
   beyan 125 µs ile tutarlı"` veya tutarsızsa `Theme.danger` renginde uyarı.
   `objectName: "watch.rateCheckBadge"`.
3. **Birim test** (saf hesap): tutarlı durum, tutarsız durum (gözlenen hız
   teorik sınırı aşıyor), yetersiz veri durumu.

### 5.3 Canlı doğrulama

H7'de örneklemeyi 30 sn çalıştır → `inferenceRateCheck(10)` çağır.
`observedHz` ≈ 60 (firmware `HAL_Delay(16)` yüzünden), `theoreticalMaxHz`
= `1e6/125` = 8000 → `consistent: true`.

**Ekran görüntüsü:** `out/rate_check_e2e/01_h7_tutarli.png`

### 5.4 Bitti sayılır ki
- [ ] Rozet "doğrulandı" değil "tutarlı/tutarsız" diyor
- [ ] Yetersiz veride `ok=false`
- [ ] H7'de canlı çalıştı, ekran görüntüsü var
- [ ] Testler yeşil, commit atıldı

---

## 6. Faz 10.4 — Canlı peripheral/register izleme (SVD → İzleyici)

**Amaç:** Kullanıcının DMA/timer/I2C gibi register'ları **isimleriyle
seçip canlı grafikte** izleyebilmesi.

### 6.1 Anahtar tespit — motor zaten var

Bir register, sonuçta bir bellek adresidir. İzleyici şu an bile
"Adres Ekle" ile `0x40026010` girilirse onu 200 Hz'de okuyup çizer.
**Eksik olan okuma yeteneği değil, isimlendirme/gezinme katmanı.**

Doğrulandı: `src/modules/watcher/` altında SVD'ye **tek bir kod referansı
yok**. Register tab'i SVD'yi biliyor ama snapshot okuyor; İzleyici canlı
okuyor ama hiçbir şeyin adını bilmiyor. **Yapılacak iş bu ikisini bağlamak.**

### 6.2 GÜVENLİK — okununca temizlenen register'lar

Bazı register'lar okununca durum bayraklarını temizler. Onları 200 Hz'de
okumak **firmware'in davranışını değiştirir** ve CLAUDE.md'deki
**gözlemci ilkesini çiğner** (snapshot'ta bir kez okumak zararsız,
saniyede 200 kez okumak değil).

SVD bu bilgiyi zaten taşıyor: `SvdParser` `<readAction>` alanını okuyup
`SvdRegister::readAction` içinde saklıyor (şu an kimse kullanmıyor).

**Kural:** `readAction` boş değilse (`clear`, `modify`, `modifyExternal`)
o register **canlı izlemeye varsayılan olarak eklenemez**. Deseni ELF
uyuşmazlığındaki gibi kur: **görünür uyarı + kullanıcının açık onayı**
(sessizce izin verme, sessizce yasaklama).

Ayrıca peripheral'ın **saati kapalıysa** (Register tab bunu zaten
`clock off` olarak gösteriyor) uyar — okuma çöp döner veya bazı ailelerde
fault üretir.

### 6.3 Yapılacaklar

1. **`src/modules/registers/RegisterInspector.h/.cpp`:**
   `QVariantList registersOfPeripheral(const QString &name) const;`
   Her eleman: `{ name, addr, size, access, readAction, description,
   clockEnabled }`. SVD kataloğu yüklü değilse boş liste.
2. **`src/bridge/Backend.h/.cpp`:** iki cephe metodu:
   ```cpp
   Q_INVOKABLE QStringList  watchPeripheralList() const;               // = registerPeripheralList()
   Q_INVOKABLE QVariantList watchRegistersOf(const QString &p) const;  // yukaridakini sarar
   Q_INVOKABLE QString      addWatchRegister(const QString &peripheral,
                                             const QString &registerName,
                                             bool acknowledgeReadAction);
   ```
   `addWatchRegister`:
   - SVD'den adresi çöz,
   - `readAction` doluysa **ve** `acknowledgeReadAction == false` ise
     **ekleme**, hata mesajı döndür (`statusMessage` ile UI'ya git),
   - ekleyeceği kalem: `kind=Scalar`, `type=u32`, `format=hex`,
     `label="<PERIPHERAL>.<REGISTER>"`, `role="peripheral"`,
     `source="svd:<peripheral>.<register>"`.
3. **`qml/dialogs/RegisterPickerDialog.qml`** (yeni):
   - Sol: peripheral listesi (arama kutusu ile filtrelenir)
   - Sağ: seçili peripheral'ın register'ları (ad, adres, açıklama)
   - `readAction` dolu olanlar **sarı uyarı ikonu** ile işaretli; seçilirse
     "Bu register okununca temizlenir, sürekli okumak firmware davranışını
     değiştirebilir. Yine de ekle?" onayı çıkar.
   - Saati kapalı peripheral'lar soluk + "clock off" etiketi.
   - `objectName`'ler: `watch.registerPickerDialog`,
     `watch.registerPickerCloseButton`, `watch.registerPickerAddButton`
4. **`qml/components/watch/WatchToolbar.qml`:** yeni buton
   `objectName: "watch.addRegisterButton"`, metin `"Register Ekle"`,
   `RegisterPickerDialog`'u açar.
5. **Kapsam sınırı (ÖNEMLİ):** genel amaçlı bir SFR tarayıcısı yapma.
   `watch/watch_presets.json`'a **AI veri yolu ile ilgili** bir hazır
   grup ekle (sensörün bağlı olduğu I2C, onu besleyen DMA stream'i,
   inference'ı ölçen timer). Gerisi elle seçilebilir kalsın.

### 6.4 Yeni kural (kural motoru zaten hazır)

`watch/watch_rules.json`'a ekle — canlı peripheral izleme açılınca bu
**teşhis** mümkün hale geliyor:

```json
{
  "id": "dma_stall_during_inference",
  "severity": "warning",
  "appliesToLabelRegex": "DMA[0-9]+\\.S[0-9]+NDTR|DMA[0-9]+\\.CNDTR",
  "type": "threshold", "op": "==", "value": 0, "sustainMs": 500,
  "gate": { "eventKind": "inference", "withinMs": 1000 },
  "message": "{label}: {windowSec} s boyunca DMA sayaci ilerlemedi — olasi transfer takilmasi"
}
```
> Kuralı eklemeden önce `TimeSeriesRuleEngine`'in `op: "=="` destekleyip
> desteklemediğini **koddan doğrula**; desteklemiyorsa ya desteği ekle ya da
> mevcut bir tiple (örn. `drift` ile "azalma durdu") ifade et. **Çalışmayan
> kural ekleme** — `_note_disabled` konvansiyonuna bak.

### 6.5 Canlı doğrulama

H7'de (BME280 I2C üzerinden okunuyor):
1. Bağlan, ELF yükle, `addWatchRegister("I2C1", "ISR", false)` → I2C durum
   register'ı `readAction` içeriyorsa **reddedilmeli** (kabul kriteri!).
2. `addWatchRegister("DMA1", "S0NDTR", false)` gibi güvenli bir sayaç ekle →
   eklenmeli, canlı grafikte **değişen** bir değer göstermeli.
3. Aynı grafikte `g_ai_last_inference_us` ile birlikte görünmeli —
   korelasyon hikâyesi bu.

F4'te aynısını tekrarla (peripheral adları farklı: F4'te `DMA1` stream
register'ları `S0NDTR` biçiminde, SVD'den doğrula — **tahmin etme**).

N6: dene, `prepareRegisters` başarısız olursa §2.5 uyarınca not düş, geç.

**Ekran görüntüleri:** `out/live_register_e2e/`
`01_picker_dialog.png`, `02_readaction_uyarisi.png`,
`03_h7_dma_canli_grafik.png`, `04_f4_canli_grafik.png`

### 6.6 Bitti sayılır ki
- [ ] `readAction` dolu register onaysız eklenmiyor (test edildi)
- [ ] Saati kapalı peripheral uyarı gösteriyor
- [ ] H7'de canlı bir register grafiği alındı, inference metriğiyle aynı eksende
- [ ] F4'te tekrarlandı
- [ ] N6 denendi ve sonucu yazıldı
- [ ] Genel SFR tarayıcısına dönüşmedi (AI veri yolu preseti var)
- [ ] Testler yeşil, commit atıldı

---

## 7. Faz 10.5 — "Bu model bu karta sığar mı" ön kontrolü

**Amaç:** 20 dakika derleyip linker hatasıyla öğrenmek yerine, **derlemeden
önce** söylemek.

### 7.1 Durum

Böyle bir kontrol **yok** (doğrulandı: `flashKb`/`ramKb` sadece boot
mesajından okunup saklanıyor, hiçbir yerde model gereksinimiyle
karşılaştırılmıyor).

Veri zaten mevcut:
- `stedgeai analyze` çıktısı (pipeline 1/5 adımı) `weights`, `activations`,
  `ram (total)` sayılarını basıyor — bu satırlar `pipelineLines`'a düşüyor.
- `BoardPresets::all()` kartın `flashKb`/`ramKb` değerlerini biliyor.

### 7.2 Yapılacaklar

1. **`src/modules/flash/XCubeAIRunner.h/.cpp`:** `analyze` çıktısından
   `weights`, `activations`, `macc` sayılarını **ayrıştıran** saf bir
   fonksiyon ekle: `static ModelFootprint parseAnalyzeOutput(const QString&)`.
   Ayrıştırılamazsa alanlar `-1` (yani "bilinmiyor") — 0 döndürme.
2. **`src/modules/flash/PipelineRunner.cpp`:** 1/5 adımından sonra
   footprint'i kartın kapasitesiyle karşılaştır:
   - `activations + ~firmware_ram_payı > board.ramKb*1024` → **uyar**
   - `weights + ~firmware_flash_payı > board.flashKb*1024` → **uyar**
   - Ayrıştırılamadıysa "kapasite kontrolü yapılamadı" de, **sessiz geçme**.
   Uyarı **bloklamaz** (kullanıcı yine de denemek isteyebilir), ama
   `pipelineLines`'a `type:"warn"` satırı olarak düşer.
3. **Birim test** `tests/TestModelFootprint.cpp`: gerçek bir `analyze`
   çıktısı metnini (H7 ve F4 koşularından kopyala) ayrıştırıyor mu,
   bozuk metinde `-1` dönüyor mu.

### 7.3 Canlı doğrulama

- Sığan durum: `anomaly_mlp_int8` + F4 → uyarı **çıkmamalı**.
- Sığmayan durum: büyük bir model (örn. `Models/audio/kws_tcresnet_direct_int8.tflite`)
  + F4 → uyarı **çıkmalı**. (Gerçekten sığmadığını `analyze` çıktısındaki
  sayılarla doğrula; sığıyorsa başka bir model dene, uyarıyı zorlama.)

**Ekran görüntüsü:** `out/fit_check_e2e/01_uyari.png`

### 7.4 Bitti sayılır ki
- [ ] Ayrıştırma saf fonksiyonda ve test edilmiş
- [ ] Bilinmeyen değerde uydurma yok (`-1`, "kontrol yapılamadı")
- [ ] Uyarı bloklamıyor
- [ ] Hem sığan hem sığmayan senaryo canlı denendi
- [ ] Testler yeşil, commit atıldı

---

## 8. Faz 10.6 — Otomatik çok-modelli süpürme

**Amaç:** Projenin başlığındaki işi (karşılaştırmalı model analizi) elle
değil, araç otomatik yapsın.

> **Bu fazı en sona bırak** — 10.1–10.5'in hepsine dayanır.

### 8.1 Ne yapacak

Kullanıcı bir model listesi ve bir kart seçer; araç sırayla her model için:
1. `runPipeline` (derle + flash)
2. İzleyici'ye bağlan, ELF yükle, presetleri uygula
3. N saniye örnekle
4. `saveWatchProfile("<model adı>")` ile profili kaydet
5. Sonraki modele geç

Sonunda tüm profiller tek tabloda karşılaştırılır.

### 8.2 Yapılacaklar

1. **`src/modules/flash/ModelSweepRunner.h/.cpp`** (yeni sınıf, `QObject`):
   durum makinesi — `Idle → Compiling → Flashing → Watching → Saving → (sonraki)`.
   Sinyaller: `progress(int modelIndex, QString stage)`, `finished(bool ok)`,
   `failed(QString)`.
   **Bir model başarısız olursa dur ma — o modeli "başarısız" işaretle ve
   sıradakine geç**, sonuç tablosunda görünsün.
2. **`src/bridge/Backend.h/.cpp`:**
   ```cpp
   Q_INVOKABLE void startModelSweep(const QVariantList &modelPaths,
                                     const QString &board,
                                     const QString &sensorType,
                                     int secondsPerModel);
   Q_INVOKABLE void cancelModelSweep();
   Q_PROPERTY(QVariantMap sweepStatus READ sweepStatus NOTIFY sweepChanged)
   ```
3. **UI:** `qml/dialogs/ModelSweepDialog.qml` — model seçimi (çoklu),
   kart, süre; ilerleme listesi (her model için durum rozeti).
   `objectName`'ler: `sweep.startButton`, `sweep.cancelButton`,
   `sweep.progressList`.
   Giriş noktası: Benchmark ekranına `objectName: "benchmark.sweepButton"`.
4. **Sonuç:** süpürme bitince `AnalysisScreen`'in **İzleme Profilleri**
   sekmesinde (zaten var) tüm profiller görünür. Ek olarak
   `compareWatchProfiles` ile ikili karşılaştırma yapılabilir.

### 8.3 Canlı doğrulama

F4'te 2 model ile (`anomaly_mlp_int8`, `weather_mlp_int8`),
model başına 20 sn:
- İki profil de `analysis_records`'a düşmeli
- İzleme Profilleri sekmesinde ikisi de görünmeli
- Süre toplamı makul olmalı (model başına ~1.5–3 dk, çoğu derleme)

**Ekran görüntüleri:** `out/model_sweep_e2e/`
`01_dialog.png`, `02_ilerleme.png`, `03_sonuc_profilleri.png`

### 8.4 Bitti sayılır ki
- [ ] Bir modelin başarısızlığı süpürmeyi durdurmuyor
- [ ] İptal düğmesi gerçekten iptal ediyor (yarım flash bırakmıyor)
- [ ] F4'te 2 modelle uçtan uca koştu
- [ ] Profiller Analiz ekranında görünüyor
- [ ] Testler yeşil, commit atıldı

---

## 9. Tüm fazlar için ortak kabul kriterleri

Her faz için, istisnasız:

1. **Derleme temiz:** `cmake --build ... -j` hatasız.
2. **Testler yeşil:** `ctest --test-dir ... --output-on-failure`.
3. **Saf sınıf değiştiyse test eklenmiş** (proje kuralı).
4. **Canlı donanımda doğrulanmış** — en az H7 **ve** F4'te; N6 denenmiş ve
   sonucu (başarı veya gerekçeli başarısızlık) yazılmış.
5. **Ekran görüntüleri** `out/<faz>_e2e/` altında, Read ile açılıp
   doğrulanmış, silinmemiş.
6. **Belgeler güncel:** `TODO.md` (madde durumu + bulunan gerçek hatalar),
   `CLAUDE.md` (yeni sınıf/klasör/ayar anahtarı/mimari karar varsa).
7. **Commit atılmış**, `Co-Authored-By` **yok**.
8. **Dürüstlük:** doğrulanmamış bir şeye "doğrulandı" deme. Bir şey
   çalışmıyorsa veya ölçülmediyse, bunu belgelere açıkça yaz. Bu projede
   yanlış "tamamlandı" etiketi, eksik özellikten daha büyük sorun sayılır.

---

## 10. Sıra ve bağımlılıklar

```
10.1 Sensör telemetrisi + seqlock   ← firmware degisikligi, once bu
      │  (üç kartı da yeniden flash'lar)
      ├─→ 10.2 RAM bütçesi          (bagimsiz, ucuz)
      ├─→ 10.3 Hız tutarlılık kontrolü (10.1'in infer_count'una dayanir)
      └─→ 10.4 Canlı peripheral izleme (bagimsiz ama en buyuk is)
                    │
                    ├─→ 10.5 Sığar mı ön kontrolü (bagimsiz, ucuz)
                    └─→ 10.6 Çok-modelli süpürme  (HEPSINE dayanir, EN SON)
```

**Önerilen sıra:** 10.1 → 10.2 → 10.3 → 10.5 → 10.4 → 10.6
(10.5 ucuz olduğu için 10.4'ün önüne alındı; 10.4 en büyük iş.)

---

## 11. Bu planı uygularken sık yapılan hatalar (kaçın)

| Hata | Doğrusu |
|---|---|
| Yeni bir ekran görüntüsü/otomasyon aracı yazmak | `uiprobe.ps1` kullan (§0.1) |
| Türkçe yollu (`D:\Yazılım\...`) komut çalıştırmak | `C:\dev\...` junction (§1.1) |
| Linker sembolünün **değerini** okumak | Adresi kullan (§4.1) |
| Bir kartta düzeltip diğerlerini atlamak | Üç şablonu da güncelle (§2.3) |
| `readAction` register'ını canlı izlemeye eklemek | Onay iste (§6.2) |
| Ölçmediğin şeye "doğrulandı" demek | "denendi/tutarlı/ölçülmedi" de (§9.8) |
| N6 takılınca saatlerce uğraşmak | Zaman kutulu, not düş, geç (§2.5) |
| Koordinatla tıklamak | `objectName` ekle, `click -Name` (§1.5) |
| `invoke` ile dizi/nesne argümanı göndermeye çalışmak | Ham JSON pipe scripti (§1.6) |
| Uygulama açıkken derlemek | Önce kapat (§1.2) |

---

## 12. Referanslar

| Konu | Dosya |
|---|---|
| Mimari (uçtan uca) | `docs/PROJECT.md` |
| Nerede kaldık / açık işler | `TODO.md` |
| Kalıcı mimari kararlar | `CLAUDE.md` |
| Doğrulama çerçevesi | `docs/verification_ecosystem_plan.md` |
| İzleyici tasarımı + bulgular | `docs/variable_watcher_plan.md`, `_findings.md`, `_review.md` |
| Register Inspector | `docs/register_inspector_plan.md`, `_findings.md` |
| Yeni makine kurulum tuzakları | `docs/dev_machine_setup.md` |
| N6 geçmişi ve güncel durum | `docs/n6_kaldigimiz_yer.md` |
| UART protokolü | `docs/protocol_v1.md` |
