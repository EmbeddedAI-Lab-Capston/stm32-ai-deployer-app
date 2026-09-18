# STM32N6 Referans AI Projesi — Araştırma Bulguları ve Yol Haritası

> **Durum:** Araştırma tamamlandı, uygulama başlamadı. Tarih: 2026-09-16.
>
> **Amaç:** ST'nin kendi araçlarıyla, elde, NPU kullanan gerçekçi bir STM32N6
> projesi üretmek — ve bu projeyi **STM32 AI Deployer'ın nihai testi** olarak
> kullanmak. Bugüne kadar İzleyici ve Register Inspector yalnızca *kendi*
> şablonlarımızdan çıkan firmware üzerinde doğrulandı; bu kapalı devre bir
> testtir. Yabancı firmware üzerinde çalıştığını kanıtlamak, aracın asıl
> iddiasının kanıtıdır.
>
> Bu belgedeki her teknik iddianın kaynağı belirtilmiştir. "Doğrulanmadı"
> işaretli satırlar uygulanmadan önce kontrol edilecektir.

---

## 1. Ortam — doğrulandı, eksik yok

Bu makinede kurulu olanlar (2026-09-16, yerel dosya sistemi üzerinden doğrulandı):

| Bileşen | Sürüm / Yol | Not |
|---|---|---|
| STM32CubeIDE | `C:\ST\STM32CubeIDE_2.2.0` | CubeMX gömülü gelir |
| STM32Cube_FW_N6 | **`V1.4.0`** + eski `V1.0.0` yan yana | 1.4.0 elle kuruldu (2026-09-16). Tam paket: `Projects/`, Nucleo BSP, `Middlewares/` dahil |
| ST Edge AI Core | `C:\ST\STEdgeAI\4.0` | |
| ST Neural-ART (NPU derleyici) | `stedgeai0400.stneuralart` kurulu | `configs/stm32n6.mdesc` + `stm32n6.mpool` mevcut |
| NPU runtime (ll_aton) | `C:\ST\STEdgeAI\4.0\Middlewares\ST\AI\Npu\` | `ATON.h` tam register haritasıyla |
| N6 referans uygulamaları | `C:\ST\STEdgeAI\4.0\Projects\STM32N6570-DK\Applications\` | `hello_world`, `NPU_Validation`, `CM55_Validation`, `SNS` |
| STM32CubeProgrammer | `C:\Program Files\STMicroelectronics\STM32Cube\` | `STM32_SigningTool_CLI` de burada |

**Sonuç: NPU derlemesi için ek kurulum gerekmiyor.** Bu, planın en büyük
bilinmeyeniydi ve kapandı.

> **Sürüm notu (çözüldü):** ST'nin Nucleo AI kılavuzu FW_N6 v1.1.1 istiyordu,
> bizde v1.0.0 vardı. **v1.4.0 kuruldu**, sorun kapandı — kılavuzun uyardığı
> "NPU, RIF sekmesinde görünmüyor" durumu bu sürümde beklenmiyor.
>
### Regresyon testi — yapıldı, sonuç: risk yok (2026-09-16)

`PipelineRunner::findSdkDir()` SDK'ları isme göre *ters* sıralayıp ilkini
seçtiği için uygulama kurulumdan sonra otomatik olarak v1.4.0'a geçti. Bunun
mevcut N6 pipeline'ını bozup bozmadığı ölçüldü:

Var olan `out/pipeline_test_n6` projesi (anomaly_mlp_int8) **aynı kaynaktan iki
kez** derlendi, sadece `CUBE_SDK_PATH` değiştirilerek. Sonuç: iki `.bin`
**bayt bayt aynı** (`md5 e6006246fe8d511674c4e00548984add`).

Testin kendisi de doğrulandı — `CUBE_SDK_PATH` sahte bir yola ayarlanınca
derleme düşüyor, yani değişken gerçekten etkili (aksi hâlde "aynı çıktı"
anlamsız olurdu).

**Sürprizli asıl bulgu:** `STM32Cube_FW_N6_V1.0.0` adlı klasör aslında
**HAL v1.4.0 içeriyor** — her iki paketin `stm32n6xx_hal.h`'ı da
`MAIN=1, SUB1=4, SUB2=0` diyor ve kullandığımız HAL kaynakları satır sonu
(CRLF/LF) dışında birebir aynı. Yani **hiçbir zaman eski bir HAL üzerinde
değildik**; klasör adı yanıltıcıydı, CubeMX yalnızca `Drivers/`'ı kırpılmış
biçimde kurmuştu. Yeni kurulumun getirdiği şey HAL değil, eksik olanlar:
`Projects/NUCLEO-N657X0-Q`, Nucleo BSP ve `Middlewares/`.

> **Öneri:** yanıltıcı adlı `STM32Cube_FW_N6_V1.0.0` klasörü ileride kafa
> karıştırır (içeriği 1.4.0). Silinebilir — ama uygulama zaten 1.4.0'ı seçtiği
> için acil değil.

---

## 2. Donanım gerçeği: NUCLEO-N657X0-Q bellek haritası ve boot zinciri

N6'nın diğer STM32'lerden ayrıldığı temel nokta: **dahili kullanıcı flash'ı
yoktur.** Kod harici flash'ta durur, çalışmak için RAM'e kopyalanır.

| Bölge | Adres | Kaynak |
|---|---|---|
| AXISRAM (bitişik ~4 MB) | `0x34000000` – `0x34400000` | `src/modules/flash/N6RamImage.h`, canlı doğrulanmış |
| Harici Octo-SPI flash (XSPI2, 512 Mbit = 64 MB) | `0x70000000` | ST ürün sayfası + Nucleo AI kılavuzu |
| → FSBL (imzalı) | `0x70000000` | ST kılavuzu |
| → Uygulama (imzalı) | `0x70100000` | ST kılavuzu |
| → Model ağırlıkları | `0x71000000` | ST kılavuzu |
| NPU (ATON) register alanı | `0x480E0000` (NS) / `0x580E0000` (S), 128 KB | CMSIS `stm32n657xx.h` üzerinden hesaplandı (§3) |

### Boot zinciri (LRUN)

```
Güç → Boot ROM
        → FSBL'i harici flash'tan (0x70000000) iç SRAM'e kopyalar, çalıştırır
        → FSBL uygulamayı (0x70100000) SRAM'e (0x34000000) kopyalar, çalıştırır
        → Uygulama ağırlıkları 0x71000000'dan memory-mapped okur
```

İki binary de **imzalı olmak zorunda** (`STM32_SigningTool_CLI`); imzasız
image'ı ROM reddeder. BOOT anahtarları geliştirme modu ile flash-boot modu
arasında seçim yapar.

**Bizim uygulamamızla ilişkisi:** `flash/n6_deploy_mode` ayarımız zaten iki
modu biliyor — `"ram"` (varsayılan: image'ı doğrudan SWD ile `0x34000400`'e
yazar, FSBL'i atlar) ve `"lrun"`. Yani deploy yolunu sıfırdan kurmuyoruz.
Ama referans proje **ağırlıkları harici flash'ta** tutacağı için burada
muhtemelen LRUN yolunu kullanacağız; `"ram"` modu ağırlıkları kapsamaz.
**Doğrulanmadı:** bizim LRUN modumuzun imzalama adımını yapıp yapmadığı.

---

## 3. NPU'ya erişim — ne görebiliriz, ne göremeyiz

**NPU register'ları okunabilir.** CMSIS başlığından hesaplandı:

```
PERIPH_BASE_NS                    0x40000000
AHB5PERIPH_BASE_NS  + 0x08020000 → 0x48020000
NPU_BASE_NS         + 0x000C0000 → 0x480E0000     (güvenli alias: 0x580E0000)
ATON_ADDR_SPACE_SIZE             = 0x20000 (128 KB)
```

`ATON.h` her birimin register offset'lerini veriyor: `ATON_ACTIV_CTRL_OFFSET`,
`ATON_ARITH_*`, `ATON_STRENG_*`, `ATON_DEBUG_TRACE_*`… Yani **Değişken
İzleyici NPU register'larını ham adres olarak izleyebilir**
(`Backend::addWatchAddress`). Canlı NPU aktivitesi göstermek için bu yeterli.

**Ama Register Inspector göremez.** `svd/STM32N657.svd` içindeki 248
peripheral tarandı: ATON/NPU kontrol bloğu **yok**. Yalnızca `CACHEAXI`
(`0x480DFC00`, NPU cache'i) ve RCC'deki NPU clock/reset bitleri var.

Üç seçenek:

1. NPU'yu İzleyici'de ham adresle izle — en ucuz, hemen çalışır.
2. `ATON.h`'dan türetilmiş **küçük bir ek SVD** yazıp `svd/`'ye koy. Register
   Inspector mimarisi zaten "yeni kart = yeni SVD + `boards.json` kaydı, sıfır
   C++ değişikliği" ilkesi üzerine kurulu; bunu yapmak o ilkenin de kanıtı olur.
   Orta maliyet, yüksek gösteri değeri.
3. Register hikâyesini CACHEAXI + RCC üzerinden kur (NPU'nun içi değil, çevresi).

**Öneri:** 1 ile başla, vakit kalırsa 2'yi yap.

---

## 4. Başlangıç projesi — karar

v1.4.0 kurulunca tablo değişti: paket **NUCLEO-N657X0-Q için resmî projeler**
getiriyor (`Projects/NUCLEO-N657X0-Q/`), 1.0.0'da bunların hiçbiri yoktu.

| Aday | Kamera/ekran | Nucleo | Not |
|---|---|---|---|
| **★ `Templates/Template_FSBL_LRUN`** (FW_N6 v1.4.0) | Hayır | **Yerel — Nucleo için yazılmış** | `.ioc` **CubeMX projesi** + FSBL/Appli ayrımı, ST kılavuzunun anlattığı mimarinin birebir kendisi |
| ST Edge AI `hello_world` | Hayır | DK hedefli, port gerek | NPU inference kodunun **referansı** olarak kullanılacak |
| `NPU_Validation` | Hayır | DK hedefli | armgcc + Makefile örneği |
| STM32N6-GettingStarted-ImageClassification | **Evet** (IMX335 + UVC/SPI ekran) | Resmî Nucleo desteği | Donanım bizde yok |

**Karar: `Template_FSBL_LRUN` iskelet alınır.** Gerekçe: Nucleo'ya özgü,
`.ioc` taşıdığı için CubeMX'te üzerine I2C (BME280) ve X-CUBE-AI eklenebilir,
ve FSBL/Appli ayrımı ST'nin AI kılavuzunun tarif ettiği yapının aynısı — yani
kılavuzu bu proje üzerinde birebir takip edebiliriz. NPU inference kodunu ise
`hello_world`'den ödünç alırız.

Şablonun kendi tarifi: *"bootROM'dan sonra FSBL iç RAM'de çalışır, MPU/cache/
clock'u kurar, uygulama binary'sini harici flash'tan iç RAM'e kopyalar."*
FSBL sistem saatini **600 MHz**'e ayarlıyor; çalıştığında yeşil LED (PG.00)
250 ms periyotla yanıp söner — ilk "çalışıyor mu" göstergemiz bu olacak.

### Öğretim için hazır gelen örnekler

`Projects/NUCLEO-N657X0-Q/` altında öğrenme planımızın neredeyse her maddesi
için resmî örnek var: `Examples/RIF` (izolasyon), `Examples/RAMCFG`
(SRAM2-6 açma), `Examples/XSPI` (harici flash), `Examples/I2C` (BME280),
`Examples/BSEC` (OTP), `Templates/Template_Isolation_LRUN` (TrustZone),
`ROT_Provisioning` (güvenli boot). Yani her konuyu teoriden sonra ST'nin
kendi çalışan örneğiyle pekiştirebiliriz.

`hello_world`'ün derleme seçenekleri (kendi `README_FIRST.md`'sinden):
`LL_ATON_RT_MODE` (ASYNC/POLLING), `USE_NPU_CACHE`, `USE_MCU_DCACHE/ICACHE`,
`VDDCORE_OVERDRIVE`, `USE_EXTERNAL_MEMORY_DEVICES` (DK'da 1 olmalı — Nucleo
portunda gözden geçirilecek ilk bayrak).

---

## 5. Model seçimi

ST'nin kendi N6 ölçümleri:

| Model | Süre | Nerede |
|---|---|---|
| `mobilenet_v1_0.25_96_tfs_int8.tflite` | **1–2 ms** | NUCLEO-N657X0-Q (resmî config) |
| `efficientnet_v2B1_240_fft_qdq_int8.onnx` | **44 ms** | STM32N6570-DK |

**Öneri: iki model birden.**

- `mobilenet_v1_0.25_96` → "çalışıyor mu" referansı. Nucleo'da resmî olarak
  doğrulanmış; sorun çıkarsa suçlu bizim kurulumumuzdur, model değil.
- `efficientnet_v2B1_240` → istenen "ağır model". NPU'da 44 ms; CPU'da ya çok
  yavaş kalacak ya hiç sığmayacak — **NPU'lu/NPU'suz karşılaştırmasının**
  anlamlı olduğu nokta tam burası.

Elimizdeki 13 `.tflite` modelin hepsi küçük, NPU'yu yormazlar; model zoo'dan
indirmemiz gerekecek.

---

## 6. Projenin şekli

```
BME280 (I2C1, ~10 Hz)  ─┐
                        ├─→ seqlock'lu TelemetryBlock ──→ SWD ──→ İzleyici (canlı)
Statik girdi tensörü ───┴─→ NPU inference ──→ class_id / confidence / inf_us
```

- **Sensör hafif akar**, kartı yormaz — "gerçek hayat" kolu.
- **YZ statik veriyle koşar** — ölçüm tekrarlanabilir olur, kamera gerekmez.
- **Her şey telemetri bloğundan okunur**, UART gerekmez. N6'da UART zaten
  kırık (reset + pasif yakalama) — ve bu, demonun en güçlü hikâyesi: *kartın
  UART'ı yarı yolda bıraktı, ölçümü belleğe taşıyıp kurtardık.*
- **Kasıtlı hata enjeksiyonu varyantı:** örneğin I2C1 clock'u kapalı ya da NPU
  için RIF izolasyonu ayarlanmamış bir derleme. Firmware sessizce yanlış
  çalışır; biz Register Inspector'la snapshot alıp reset değerinden farkı
  gösterip teşhis koyarız. Aracın asıl iddiası budur — hatasız bir sistemde
  register'lara bakmak hiçbir şey anlatmaz.

### Bizim araçla kesişim — şimdiden bilinen üç nokta

1. **Stack watermark yanıltacak.** ST'nin startup'ı `0xA5A5A5A5` boyamaz →
   İzleyici "~0 B" gösterir (sahte kritik). Çözüm: `templates/ai_glue/stack_paint.c`
   referans projeye elle eklenir. Yan fayda: yaklaşımın taşınabilirliğini kanıtlar.
2. **ELF↔hedef eşleşme kontrolü (VTOR + vektör tablosu) ilk kez yabancı
   firmware'de sınanacak.** N6'da image RAM'e kopyalandığı için vektör tablosu
   RAM'dedir — kontrolün "meşru RAM vektör tablosu" senaryosunu tam olarak test eder.
3. **memread sidecar N6'da zaten 200 Hz'de 0 kayıpla doğrulandı**, okuma yolu hazır.

---

## 7. Öğrenme planı

Sırayla; her adımda önce "neden böyle", sonra kartta uygulama:

1. **Dahili flash yok** → boot ROM / FSBL / imzalama / BOOT pinleri / LRUN
2. **Bellek haritası** → AXISRAM, NPU RAM'leri, XSPI2, MCE, üç ayrı cache
   (ICACHE / DCACHE / **CACHEAXI**)
3. **TrustZone + RIF** → "peripheral'a erişemiyorum" hatalarının asıl kaynağı;
   NPU için `HAL_RIF_RISC_SetSlaveSecureAttributes(..., RIF_RISC_PERIPH_INDEX_NPU, ...)`
4. **Saat ve güç** → VDDCORE overdrive, VDDIO3 1.8 V, SMPS, PB12/EXT_SMPS_MODE
   (kılavuz bu pinin **saat konfigürasyonundan önce** init edilmesini özellikle
   uyarıyor)
5. **AI zinciri** → `stedgeai --target stm32n6 --st-neural-art`, `mdesc`/`mpool`
   nedir, ağırlıklar nereye gider, epoch controller ne yapar
6. **Cache maintenance** → inference öncesi `mcu_cache_clean_invalidate_range()`
   + `npu_cache_invalidate()`; atlanırsa NPU eski veriyi okur ve **sessizce
   yanlış sonuç** verir
7. **Debug** → N6'da gdbserver'ın hedefi durduramaması, memread sidecar'ın
   varlık sebebi

---

## 8. Açık riskler

### OTP fuse — okundu, ZATEN YANMIŞ (2026-09-16)

`Template_FSBL_LRUN`'ın README'si uyarıyor:

> *"The following OTP fuses are set in this template: **VDDIO3_HSLV=1**…
> **WARNING — When OTP fuses are set, they can not be reset.**"*

Karttan salt-okuma ile kontrol edildi (hiçbir kalıcı işlem yapılmadı):

```
STM32_Programmer_CLI -c port=SWD sn=001A0027... mode=UR -otp displ word=124
  HCONF1 | Data124: 0x00018000 | Status124: 0x00000000
```

`0x18000` = bit 15 + bit 16 = **`HSLV_VDDIO3` ve `HSLV_VDDIO2` ikisi de set.**
Yani NUCLEO-N657X0-Q bu sigortalar **yanmış hâlde geliyor** (ya da kutudan
çıkan demo yakmış). Beklenen de buydu: kartın boot flash'ı XSPIM Port 2
üzerinde ve 1.8 V ile besleniyor.

**Sonuçları:**

- **Bizim yakacağımız bir şey yok.** Şablonun `OTP_Config()`'i biti önce
  okuyup zaten setliyse dokunmuyor — yani `NO_OTP_FUSE` olsun olmasın
  davranış aynı, hiçbir geri dönüşsüz işlem olmayacak.
- **XSPI tam hızda çalışacak**, dolayısıyla ölçeceğimiz inference süreleri
  ST'nin yayınladığı referans sayılarla kıyaslanabilir olacak.
- ⚠ **Kalıcı donanım kısıtı artık yürürlükte:** VDDIO2/VDDIO3 domain'indeki
  pinlere bir daha asla ~2.5 V üzeri verilmemeli (ST: HSLV setliyken VDDIO
  3.3 V aralığındaysa çip zarar görür). **BME280'i bağlamadan önce, kullanacağımız
  I2C pinlerinin hangi VDDIO domain'inde olduğu ve kaç volt beslendiği kartın
  kullanım kılavuzundan/şemasından doğrulanmalıdır.** Bu artık teorik bir
  uyarı değil, kartın mevcut durumu.

### Diğer riskler

| Risk | Etki | Ne zaman çözülür |
|---|---|---|
| ~~FW_N6 sürüm sıçraması pipeline'ı bozabilir~~ | — | ✅ **Kapandı** — bayt bayt aynı binary, §1 |
| Bizim LRUN modumuz imzalama yapıyor mu | Deploy yolu | Koda bakılacak |
| RAMCFG SRAM2-6 açılmazsa inference **askıda kalıyor** (topluluk raporu) | Sessiz kilitlenme | Port sırasında |
| Cache maintenance sırası yanlışsa sessizce yanlış sonuç | Yanlış demo | Port sırasında |
| BME280 henüz takılı değil, N6 pinleri seçilmedi | Sensör kolu | Donanım adımında |

---

## 8.5 Adım 3 tamamlandı — boot zinciri kartta çalışıyor (2026-09-16)

`Template_FSBL_LRUN` kopyalandı (`n6_ai_node/`), derlendi, imzalandı, karta
yüklendi ve **yeşil LED'in yanıp söndüğü register'dan doğrulandı.** Yolda üç
gerçek engel çıktı; üçü de aşağıda, çünkü tekrar karşımıza çıkacaklar.

### Engel 1 — proje SDK'ya 7 seviye yukarıdan bakıyor

Şablon, paketin içinde durduğu varsayımıyla yazılmış:
`.project` içinde `PARENT-6-PROJECT_LOC/Drivers`, `.cproject` içinde
`../../../../../../../Drivers`. Paketin dışına kopyalayınca hepsi kırılıyor.

**Çözüm (mutlak yol yazmadan):** projenin içine SDK'ya bir junction kondu
(`n6_ai_node/sdk` → `STM32Cube_FW_N6_V1.4.0`) ve referanslar
`PARENT-2-PROJECT_LOC/sdk/...` ile `../../../sdk/...` olacak şekilde
güncellendi. Böylece proje dosyaları makineden bağımsız kaldı; junction
`.gitignore`'da, yeni makinede tek komutla kurulur:

```powershell
New-Item -ItemType Junction -Path n6_ai_node\sdk `
         -Target $HOME\STM32Cube\Repository\STM32Cube_FW_N6_V1.4.0
```

### Engel 2 — ST'nin şablonunda eksik var

`Appli/Src/main.c` `BSP_LED_Init()`/`BSP_LED_Toggle()` çağırıyor, BSP include
yolu da tanımlı — ama **`stm32n6xx_nucleo.c` kaynağı projeye hiç eklenmemiş**,
dolayısıyla link `undefined reference` ile düşüyor. `AppS/.project`'e linked
resource olarak eklendi.

### Engel 3 (asıl öğretici olan) — HOTPLUG ile FSBL çalışmıyor, UR ile çalışıyor

FSBL'i `mode=HOTPLUG` ile RAM'e yükleyip çalıştırınca çekirdek **kilitlendi**:

```
PC = 0xEFFFFFFE (lockup)   XPSR = 0x81000003 (IPSR=3 → HardFault)
CFSR = 0x00008301 → MMFSR.IACCVIOL + BFSR.IBUSERR + PRECISERR + BFARVALID
addr2line(LR-1) → FSBL/Src/main.c:94  (SystemClock_Config)
```

Sebep: HOTPLUG çipe **çalışır hâldeyken** bağlanır; FSBL ise reset sonrası
durum varsayarak saat ağacını sıfırdan kurar. `mode=UR` (under reset) ile
bağlanınca çekirdek reset durumunda yakalanıyor ve aynı ikili sorunsuz
çalışıyor. ST'nin EWARM makrosu (`EWARM/FSBL/setup.mac`) da ayrıca vektör
tablosundaki değeri değil sabit bir yığın adresi kullanıyor
(`SP = 0x341FFD00`) — biz de onu kullandık.

> Bu, kendi uygulamamızdaki N6 "ram" deploy yolu için de geçerli bir ders:
> `N6RamImage::armArgs()` `connectArgsWithMode(..., "HOTPLUG")` kullanıyor.
> Kendi şablonumuz saat ağacını bu kadar derin kurmadığı için sorun çıkmamış
> olabilir; **ama yabancı firmware'de çıkıyor.** İncelenmeli.

### Çalışan prosedür (tekrarlanabilir)

```bash
# 1) Derle (headless CubeIDE; -importAll Windows ters bolu isterse ver)
stm32cubeidec.exe --launcher.suppressErrors -nosplash \
  -application org.eclipse.cdt.managedbuilder.core.headlessbuild \
  -data <workspace> -importAll 'C:\dev\stm32-ai-deployer-app\n6_ai_node\STM32CubeIDE' -build all

# 2) Imzala (CubeProgrammer >= 2.21 ise -align sart)
STM32_SigningTool_CLI.exe -bin Template_LRUN_AppS.bin -nk -of 0x80000000 -t fsbl \
    -o Appli-trusted.bin -hv 2.3 -dump Appli-trusted.bin -align

# 3) Appli'yi harici flash'a yaz (Nucleo external loader)
STM32_Programmer_CLI -c port=SWD sn=<SN> mode=HOTPLUG \
    -el ".../ExternalLoader/MX25UM51245G_STM32N6570-NUCLEO.stldr" \
    -w Appli-trusted.bin 0x70100000

# 4) FSBL'i RAM'e koy ve calistir  --  mode=UR SART
STM32_Programmer_CLI -c port=SWD sn=<SN> mode=UR -halt \
    -w Template_LRUN_FSBL.bin 0x34180400 \
    -w32 0xE000ED08 0x34180400 \
    -coreReg MSP=0x341FFD00 PC=0x3418F5B8 -run
```

> ⚠ `PC`, FSBL'in **o anki** derlemesinin `Reset_Handler`'ıdır — FSBL her
> yeniden derlendiğinde değişebilir (ilk yazımda `0x3418F530` idi; 2026-09-17
> 00:05 derlemesinde `0x3418F5B8`). Ezbere yazmayın, ELF'ten okuyun:
> `arm-none-eabi-nm Boot/Debug/Template_LRUN_FSBL.elf | grep Reset_Handler`.
> Kart güç kesilip açıldığında (geliştirme boot modunda) Appli kendiliğinden
> başlamaz — adım 4 yeniden çalıştırılır.

### Doğrulama (gözle değil, register'dan)

| Ne | Adres | Beklenen |
|---|---|---|
| Zincir nereye geldi | `SCB->VTOR` `0xE000ED08` | `0x34000400` = Appli çalışıyor (`0x34180400` = FSBL'de takılı) |
| LED pini yapılandı mı | `GPIOG_MODER` `0x56021800` | PG0 bitleri `01` (çıkış) |
| LED yanıp sönüyor mu | `GPIOG_ODR` `0x56021814` | bit 0 örnekler arasında değişiyor |

Ölçülen: VTOR `0x34000400`, MODER `0xFFDFFFFD`, ODR bit0 sekiz örnekte
1/1/0/0/1/0/0/1 → **toggling doğrulandı.**

> Not: LED_GREEN, BSP'de `LED3` = **GPIOG pin 0** (README'deki PG.00 ile
> uyumlu). Peripheral'lar güvenli alias'tan (`0x56...`) okunuyor; güvensiz
> alias (`0x46...`) sıfır dönüyor.

---

## 8.6 NPU derlemesi çalıştı (2026-09-16)

**Araç seçimi kararı: X-CUBE-AI değil, `stedgeai` CLI.** İkisi rakip değil —
X-CUBE-AI, ST Edge AI Core'u saran CubeMX eklentisidir ve arka planda aynı
`stedgeai generate --target stm32n6 --st-neural-art` komutunu çağırır. Bu
makinede **X-CUBE-AI paketi hiç kurulu değil** (`Packs/STMicroelectronics/`
yok), ST Edge AI Core 4.0.1 ise kurulu ve `stm32n6 [--st-neural-art]` hedefini
destekliyor. Ayrıca CLI betiklenebilir (CubeMX GUI değil) ve zaten bizim
`XCubeAIRunner`'ımızın da çağırdığı araç.

> **Tuzak (yine Türkçe yol):** `atonn.exe` alt süreci `D:\Yazılım\...` yolunu
> cp1252'ye çevirirken bozuyor (`Yazılım` → `Yazilim`) ve
> `--json-quant-file: File does not exist` ile düşüyor. **Çözüm: komutu
> `C:\dev\stm32-ai-deployer-app` junction'ı üzerinden çalıştır.** Bu durumda
> konsola hâlâ `UnicodeEncodeError` log hataları basılıyor ama bunlar yalnızca
> log yazdırma hatasıdır, derleme başarılı olur.

### Komut

```bash
stedgeai generate \
  --model "C:/dev/stm32-ai-deployer-app/n6_ai_node/Model/mobilenet_v1_0.25_96_tfs_int8.tflite" \
  --target stm32n6 --st-neural-art \
  --output "C:/dev/stm32-ai-deployer-app/n6_ai_node/AI/generated" --no-workspace
```

### Sonuç — model NPU'ya neredeyse tamamen düşüyor

| | |
|---|---|
| Toplam epoch | **32** |
| Saf donanım (NPU) | **30** |
| Saf yazılım (CPU fallback) | 2 |
| Hibrit | 0 |
| MACC | 7.782.410 |

### Bellek yerleşimi (derleyicinin kendi seçtiği)

| Havuz | Adres | Kullanım |
|---|---|---|
| `octoFlash` (ağırlıklar) | **`0x71000000`** | 222.86 kB |
| `npuRAM5` (aktivasyonlar) | `0x342E0000` | 54.00 kB |
| Toplam | | 276.86 kB |

Ağırlıkların `0x71000000`'a düşmesi ST'nin kılavuzundaki adresle birebir aynı
ve FSBL (`0x70000000`) ile Appli (`0x70100000`) ile çakışmıyor — varsayılan
havuz bu kart için doğru çalışıyor.

### Üretilen dosyalar

| Dosya | Ne |
|---|---|
| `network.c` (537 KB) | ATON ağ kodu (epoch'lar) |
| `network.h` | arayüz |
| `stai_network.c/.h` | ST AI API sarmalayıcı |
| **`network_atonbuf.xSPI2.raw`** (223 KB) | **ağırlık blob'u** → `0x71000000`'a yazılacak |
| `network_generate_report.txt` | rapor |

> **İleriye dönük not:** varsayılan `stm32n6.mpool`, DK'da olan ama Nucleo'da
> **olmayan** `hyperRAM`'i (`0x90000000`, 32 MB) de tanımlıyor. Bu model onu
> kullanmadı (0 B), ama ağır model (`efficientnet_v2B1_240`) kullanmaya
> kalkarsa sessizce çalışmayan bir ikili üretiriz. Ağır modele geçerken
> **Nucleo'ya özel bir mpool** yazmak gerekecek.

---

## 8.7 NPU'yu uygulamaya bağlama — %90 tamam, bir açık blokaj (2026-09-16)

`stedgeai` çıktısı Appli projesine bağlandı, derlendi, karta yüklendi ve
firmware **inference döngüsüne kadar geldi**. Kalan tek sorun aşağıda.

### Yapılan bağlama işi

X-CUBE-AI'ın otomatik yapacağı şeyler elle kuruldu:

| Ne | Nasıl |
|---|---|
| NPU runtime | `n6_ai_node/ai_runtime` junction'ı → `C:\ST\STEdgeAI\4.0\Middlewares\ST\AI` |
| Kaynaklar | `network.c` + 11 `ll_aton/*.c` + `npu_cache.c` + `mcu_cache.c` |
| HAL | `HAL_RIF_MODULE_ENABLED` ve `HAL_CACHEAXI_MODULE_ENABLED` açıldı, `stm32n6xx_hal_rif.c` + `stm32n6xx_hal_cacheaxi.c` eklendi |
| Define'lar | `LL_ATON_PLATFORM=LL_ATON_PLAT_STM32N6`, `LL_ATON_OSAL=LL_ATON_OSAL_BARE_METAL`, `LL_ATON_RT_MODE=LL_ATON_RT_POLLING`, `LL_ATON_SW_FALLBACK`, `USE_NPU_CACHE` |
| Kütüphane | `ai_runtime/Lib/GCC/ARMCortexM55/NetworkRuntime1201_CM55_GCC.a` — SW fallback epoch'larının kernel'leri (`node_convert`, `forward_sm_integer`) burada |
| Uygulama kodu | `SystemInit_Post()` (RAM'leri aç), `NPU_Config()` (clock/reset/cache/RIF), DWT ile süre ölçümü, statik girdi, argmax |

> **CDT tuzağı:** `.project`'e linked resource eklemek yetmiyor — dosya
> `.cproject`'teki bir `sourcePath` altında görünmezse **sessizce derlenmez**
> (biz `Drivers/` altına koyduk). İlk denemede 41 undefined reference bunun
> yüzündendi.

### Uygulamadan gelen telemetri (İzleyici bunu okuyacak)

`g_ai_infer_count`, `g_ai_last_inference_us`, `g_ai_last_cycles`,
`g_ai_last_class`, `g_ai_last_confidence_pct`, `g_ai_status`,
`g_ai_cpu_clock_hz` — hepsi `volatile`, dosya kapsamında, UART gerekmiyor.
Ayrıca `g_boot_stage`: init'in her adımında artan bisect sayacı, hangi adımda
düşüldüğünü tek bellek okumasıyla söylüyor.

### ⚑ Bulgu: FSBL uygulamayı sessizce KIRPIYOR

`FSBL/Inc/stm32_extmem_conf.h` içinde:

```c
#define EXTMEM_LRUN_SOURCE_SIZE 0x00010000   /* = 64 KB */
```

Şablonun kendi uygulaması 5 KB olduğu için bu hiç fark edilmiyor. NPU kodu
eklenince uygulama **112 KB** oldu; FSBL ilk 64 KB'yi kopyalayıp geri kalanı
bırakıyor, çekirdek eksik koda dallanınca `CFSR=0x00000001` (MMFSR.IACCVIOL)
ile HardFault. **Hata mesajı yok, uyarı yok — sadece çöküyor.** `0x00040000`
(256 KB) yapıldı, sorun gitti.

Teşhis yöntemi kayda değer: `g_boot_stage` 0'da kalıyordu, yani main'in ilk
satırına bile gelinmiyordu; boyutu kontrol edince sebep ortaya çıktı.

### ⚠ AÇIK BLOKAJ: RISAF yapılandırması kartı kilitliyor

`g_boot_stage = 8` (tüm init bitti) ve `g_ai_status = 1` (inference başladı)
ama `g_ai_infer_count` **0'da kalıyor** — NPU epoch döngüsü bitmiyor.

Sebep büyük olasılıkla RISAF: NPU kendi başına bir bus master ve adres
filtrelerinden geçemezse saati açık olmasına rağmen belleğe ulaşamaz.
Referans uygulama bunu `RISAF_Config()` ile çözüyor.

Ama o fonksiyonun aynısını çağırınca **kart reset oluyor** (VTOR `0x18000000`
= ROM). Sebep: referans uygulama debugger'ın tek parça yüklediği bir image;
bizimki FSBL + Appli ve Appli **FLEXMEM'den çalışırken** onu koruyan
RISAF7'yi yeniden yazıyor. Kapsam daraltıldı (yalnızca RISAF4/5/6/8/15/12),
ama bu sefer de uygulama çalıştıktan sonra **debug bellek okuması** ölüyor
("failed to read the requested memory content"). Kart bozulmadı — `mode=UR`
ile her zaman geri dönülüyor.

### Denenen ve ELENEN açıklamalar (2026-09-16 akşamı)

Web araştırması + kart üzerinde ölçüm ile şunlar **elendi**:

| Hipotez | Sonuç |
|---|---|
| RISAF açılmamış, NPU belleğe erişemiyor | **Yanlış.** ST: *"RISAF yapılandırılmamışsa yalnızca secure + privileged + CID=1 erişimler geçer"* — `NPU_Config()` NPU'yu RIMC'de tam olarak öyle etiketliyor. Dahası RISAF'ı Appli'de **veya** FSBL'de yapılandırmak asılmayı çözmüyor, üstüne debug bellek erişimini öldürüyor |
| WFE'de NPU saati kesiliyor | Makul (ST topluluğu bunu bildiriyor) ama **çözmedi**. `SetClockSleepMode()` eklendi, kodda kalacak — doğru olan bu |
| NPU cache RAM'i beslenmiyor | **Yanlış.** `npu_cache_disable()` ile de aynı şekilde asılıyor |
| Ağırlıklar okunamıyor | **Yanlış.** Firmware `0x71000000`'dan `0xE6DEE804` okuyor — `.raw` dosyasının ilk kelimesiyle birebir aynı |
| NPU ölü / saatsiz | **Yanlış.** `0x480E0000` (ATON register alanı) firmware'den okunuyor |

> **Yöntem notu:** "debugger `0x71000000`'ı okuyamıyor" bulgusu bizi yanlış yöne
> sürükledi. Debugger erişimi ile firmware erişimi **farklı filtrelenir**;
> soruyu firmware'e sordurmak (`g_probe_weights`) gerçeği ortaya çıkardı.
> Aynı ders bizim kendi aracımız için de geçerli.

### Bilinen son durum

```
g_boot_stage = 8      → init'in tamamı geçti
g_ai_status  = 1      → inference başladı
g_ai_infer_count = 0  → hiç bitmiyor
g_ai_cpu_clock_hz = 600000000
CPU nerede: LL_Streng_Wait (ll_aton.c:473) — ATON_STRENG_CTRL'in RUNNING biti
            hiç düşmüyor, yani stream engine başlıyor ama transferi bitmiyor
```

Aynı belirti ST topluluğunda birebir rapor edilmiş
([Problems with running an AI Model on the NUCLEO-N657X0-Q](https://community.st.com/t5/edge-ai/problems-with-running-an-ai-model-on-the-nucleo-n657x0-q-board/td-p/818432)):
*"inference never finishes (LL_ATON_RT_DONE is never reached)"*. Verilen cevap
bizim zaten takip ettiğimiz makaleye yönlendiriyor, ek bilgi yok.

**Sıradaki denenecekler (öncelik sırasıyla):**

1. **Güç/voltaj ölçeği.** ST'nin Nucleo makalesi "Power Regulator Voltage
   Scale = 0" ve **PB12 / EXT_SMPS_MODE pininin saat yapılandırmasından ÖNCE**
   init edilmesi gerektiğini özellikle vurguluyor. Şablonun FSBL'i 600 MHz
   kuruyor ama NPU'yu düşünerek mi kurulmuş, bilmiyoruz.
2. **ATON STRENG register'larını host'tan oku** — engine neyi bekliyor,
   hata biti var mı. `ATON.h` offsetleri elimizde, İzleyici ham adres
   okuyabiliyor. (Bu aynı zamanda aracımızın NPU teşhisinde işe yaradığının
   ilk gerçek örneği olur.)
3. **`LL_ATON_RT_MODE`'u değiştir** (POLLING ↔ ASYNC/IRQ).
4. **Kontrol deneyi:** ST'nin `hello_world`'ünü hiç değiştirmeden DK hedefiyle
   derle. Derlenirse sorun bizim entegrasyonumuzda değil, kart/güç
   seviyesindedir diye ayırt edebiliriz.

### Teşhis 2 — kendi aracımızla NPU register taraması (2026-09-16 akşamı)

Tam kayıt + ekran görüntüleri: `docs/n6_npu_diag/`.

Uygulamanın Değişken İzleyici'si N6'ya bağlandı ve **200.0 Hz'de, 0 kaçırılan,
0 okuma hatasıyla** NPU register alanını (`0x480E0000`, SVD'si yok, ham adres)
tarayabildi. **ELF↔hedef eşleşme kontrolü bizim üretmediğimiz firmware'de
`match` verdi** — aracın ilk gerçek sınavı.

Ölçülenler (asılı hâldeyken):

| | |
|---|---|
| NPU saati | `RCC.DIVENR` bit 5 set → IC6 etkin ✓ |
| ATON clock control | `CTRL=1`, `AGATES0/1=0xF`, `BGATES=0x180681` ✓ |
| Stream switch | `CTRL=1` (EN) ✓ |
| CONVACC0 / ACTIV / POOL | yapılandırılmış ✓ |
| RISAF12 yasadışı erişim | bayrak yok ✓ |
| **STRENG0/7/9** | RUNNING, **POS 36'da donmuş** ✗ |
| STRENG7'nin okuduğu adres | `0x71037160` (harici flash, ağırlıklar) |
| `INTCTRL.INTREG` | `0x4` → STRENG2 tamamlanma kesmesi, temizlenmemiş |

Yani **yapılandırmada görünür bir eksik yok.** Üç motorun farklı belleklere
giderken tam aynı bayt sayısında (36) durması, tek tek erişim sorunlarıyla
açıklanamaz.

> **Metot uyarısı:** İlk turda `STRSWITCH.CTRL = 0` okundu ve "stream switch
> kapalı" diye yorumlanacaktı. Gerçekte PowerShell tek elemanlı iç içe diziyi
> düzleştirdiği için **yanlış adres** okunmuştu; düzgün okumada `CTRL = 1`.
> Ölçüm zincirinin kendisi de doğrulanmalı.

### Test 1 — `LL_ATON_RT_ASYNC` + NPU kesmesi: SONUÇSUZ

Hipotez: POLLING modunda `INTREG`'deki tamamlanma bayrağı temizlenmiyor,
donanım bir sonraki transfer için onu bekliyor olabilir.

Yapılanlar:
- `LL_ATON_RT_MODE` → `LL_ATON_RT_ASYNC`
- `NPU0_IRQn` (=53, `ATON_STD_IRQ_LINE` 0) NVIC'te açıldı
- **Öğrenilen:** `NPU0_IRQHandler`'ı uygulama **tanımlamamalı** — `ll_aton_runtime.c`
  zaten tanımlıyor (bizimki `multiple definition` + sonsuz özyineleme verdi).
  Bare-metal'de `LL_ATON_OSAL_INSTALL_IRQ` boş makro, yani uygulamaya düşen tek
  iş NVIC'i açmak.
- **Öğrenilen:** NVIC'i **runtime init'ten önce** açmak firmware'i döngüye bile
  sokmuyor (`g_ai_status` 0'da kaldı) — önceki koşumdan kalan bekleyen kesme
  handler'ı runtime hazır değilken tetikliyor. Açılış `LL_ATON_RT_Init_Network()`
  sonrasına taşındı.

**Sonuç: ölçülemedi.** ASYNC modda çekirdek epoch döngüsünde WFE'ye park
ediyor ve **debug erişim portu düşüyor** (`DEV_AP_ACCESS_ERROR`); `mode=UR` ile
bağlanmak da RAM'deki kanıtı siliyor. `DBGMCU_CR.DBG_SLEEP` + DBG saati
açılarak denendi, **düzelmedi**.

Proje **POLLING'e geri alındı** (gözlemlenebilir hâl). ASYNC'te doğru olan iki
düzeltme korundu: NVIC açılış sırası ve DBGMCU uykuda-debug.

> Bu, aracımız açısından da bir bulgu: **uyku moduna giren bir hedef bizim
> gözlem modelimizi kırıyor.** Register Inspector'ın da asılı firmware'de
> bağlanamaması ile aynı aileden bir sınır.

### Teşhis 3 — tüm pin durumlarının dökümü (2026-09-17)

12 GPIO portunun (`A–H`, `N–Q`) `MODER/OTYPER/OSPEEDR/PUPDR/IDR/ODR/AFRL/AFRH`
register'ları tek tek okundu (`docs/n6_npu_diag/gpio_dump.txt`).
**Yalnızca iki port yapılandırılmış:**

| Port | Ne |
|---|---|
| **GPIOG** | PG0 = yeşil LED (çıkış, yüksek), PG10 = açık-drenaj çıkış |
| **GPION** | PN0–PN11 hepsi **AF9 = XSPI2** (harici flash arayüzü) |
| Diğer 10 port | tamamen sıfır — saatsiz / yapılandırılmamış |

> **`STM32_Programmer_CLI -r32 <adres> <n>`'de `n` BAYT sayısıdır, kelime
> değil.** İlk dökümde 4 kelime istedim, 1 kelime aldım ve her portu "sıfır"
> sandım. Doküman için: 4 kelime = `16`.

**Bulgu:** `GPIOB` hiç yapılandırılmamış — yani **PB12 sürülmüyor**. Nucleo
BSP'sine göre (`stm32n6xx_nucleo.h`) `SMPS_GPIO_PORT = GPIOB`,
`SMPS_GPIO_PIN = GPIO_PIN_12`, `SMPS_VOLTAGE_OVERDRIVE = GPIO_PIN_SET`. Yani
kartın harici SMPS'i **nominal** seviyede kalıyor. Üstelik şablonun FSBL'i
regülatörü `PWR_REGULATOR_VOLTAGE_SCALE1` (en düşük güç) yapıyor; ST'nin Nucleo
makalesi ise Scale **0** (en yüksek frekans) diyor.

**Uygulanan düzeltme** (FSBL, saat kurulumundan önce):
`ExtSmps_SetOverdrive()` — GPIOB saati, PB12 push-pull çıkış, yüksek, 1 ms
bekleme; ardından `PWR_REGULATOR_VOLTAGE_SCALE0`.

**Doğrulandı:** `GPIOB.MODER` → PB12 çıkış, `GPIOB.ODR = 0x1000` → PB12 yüksek.

**Sonuç: NPU yine de asılıyor.** Yani güç hipotezi de elendi — ama düzeltme
**kodda kalıyor**, çünkü ST'nin kendi yönergesine göre doğru olan bu ve
şablonun gerçek bir eksiğiydi.

### Elenenlerin toplu listesi

RISAF · NPU cache · NPU saati (IC6 doğrulandı) · stream switch (EN=1
doğrulandı) · ağırlık erişimi (firmware okuyor) · yasadışı erişim (bayrak yok)
· harici SMPS overdrive + voltaj ölçeği · ASYNC/IRQ (ölçülemedi)

**Geriye kalan tek sınanmamış aday: BOOT modu** (fiziksel anahtar gerekiyor).

### Teşhis 4 — BOOT modu denemesi (2026-09-17)

**Kurgu.** Flash boot'ta debug bellek erişimi kapandığı için telemetri
okunamaz. Çözüm: firmware'i **LED üzerinden** konuşturmak. LED, `SysTick`
kesmesinden sürülüyor — ana döngü NPU epoch'unda asılı kalsa bile kesme
çalıştığı için gösterge donmuyor:

| LED | Anlamı |
|---|---|
| sönük/donuk | image hiç çalışmadı |
| yavaş (~1 s) | firmware ayakta, **hiçbir inference tamamlanmadı** |
| hızlı (~0.2 s) | inference'ler tamamlanıyor |

`App_SysTickHook()` hızı doğrudan `g_ai_infer_count`'a bağlıyor.

**Yapılanlar.** İmzalı FSBL `0x70000000`'a, imzalı Appli `0x70100000`'a
yazıldı (ağırlıklar `0x71000000`'da zaten duruyordu). BOOT1 flash-boot
konumuna alındı, güç döngüsü yapıldı.

**Boot modunun gerçekten değiştiği doğrulandı:** `mode=UR` bağlanıyor
(3.29 V, `STM32N6xx`) ama `BOOTSR` de RAM de **okunamıyor** — ROM'un güvenli
boot'u debug bellek erişimini kapatmış. Geliştirme modunda ikisi de okunuyordu.

**Sonuç — iki ayrı bulgu:**

1. ✅ **Kart artık tek başına açılıyor.** ROM → imzalı FSBL (harici flash) →
   Appli RAM'e kopyalandı → çalışıyor, LED yanıp sönüyor. Debugger yok,
   bilgisayar bağlantısı yok. **Tam LRUN zinciri kartta kanıtlandı** — bu
   şimdiye kadar hiç doğrulanmamıştı.
2. ❌ **LED yavaş kaldı → boot modu NPU asılmasının sebebi DEĞİL.** Son aday
   da elendi.

> Yan kazanım: flash boot'ta debug kapanıyor ama **LED kanalı çalışıyor.**
> Gözlemlenemeyen bir hedeften 1 bit bilgi almanın işe yarar bir yolu — demo
> için de kullanılabilir.

---

## 8.8 ✅ ÇÖZÜLDÜ — NPU çalışıyor (2026-09-17)

**Kök neden: NPU, memory-mapped harici flash'tan (xSPI2) ağırlıkları
okuyamıyor — CPU okuyabildiği hâlde.**

Kontrol deneyi: neural-art derleyicisi harici belleği olmayan bir memory pool
ile yeniden çalıştırıldı, böylece ağırlıklar iç RAM'e düştü.

```bash
stedgeai generate --model mobilenet_v1_0.25_96_tfs_int8.tflite \
  --target stm32n6 \
  --st-neural-art "nucleo-npuram@<ASCII yol>/user_neuralart.json" \
  --output <cikti> --no-workspace
```

Profil, ST'nin kendi `n6-noextmem` profilinden türetildi; ek olarak `AXISRAM2`
de kapatıldı (Appli orada çalışıyor). Sonuç yerleşim:

| | Önce (asılıyordu) | Sonra (çalışıyor) |
|---|---|---|
| Ağırlıklar | `0x71000000` — **harici flash** | **`0x342E0000`** — npuRAM5 (224 kB) |
| Aktivasyonlar | `0x342E0000` — npuRAM5 | `0x34270000` — npuRAM4 (72 kB) |

Ağırlık blob'u (`network_atonbuf.AXISRAM5.raw`, 229601 B) flash'ta duruyor;
firmware `SystemInit_Post()` sonrasında `0x71000000`'dan `0x342E0000`'a
kopyalıyor (`g_probe_ramweights` = `0xE6DEE804` ile doğrulandı).

**Ölçülen sonuç — canlı, kesintisiz:**

```
g_ai_infer_count        195 → 220 → 240   (sürekli artıyor)
g_ai_last_inference_us  3694–3695 us      (çok kararlı)
g_ai_last_cycles        2 216 917         @ 600 MHz CPU
g_ai_last_class         10                guven %84
```

### Neden bu, gözlemlerimizin hepsini açıklıyor

Teşhis 2'de asılı olan üç stream engine'den **STRENG7 tam da
`0x71037160`'ı**, yani harici flash'taki ağırlıkları okuyordu. STRENG0/9 ise
aynı boru hattının devamıydı, o yüzden hepsi **aynı noktada (POS=36)**
duruyordu — "farklı belleklere giden motorlar neden aynı yerde durdu"
sorusunun cevabı buymuş: durmuyorlardı, **bekliyorlardı**.

Ve RISAF'ta hiçbir yasadışı erişim bayrağı olmaması da tutarlı: erişim
**reddedilmiyordu**, transfer hiç tamamlanmıyordu.

> **Açık kalan alt soru:** NPU'nun xSPI2'den memory-mapped okuması neden
> çalışmıyor? FSBL'in kurduğu memory-mapped mod CPU yolu için yeterli ama NPU
> master'ı için değil. Ağır modele geçerken (ağırlıklar iç RAM'e sığmayacak)
> bu soruyu çözmek gerekecek. Şimdilik çalışan yapılandırma elimizde.

> **Performans notu:** 3.69 ms, ST'nin bu model için Nucleo'da yayınladığı
> 1–2 ms'den yüksek. Muhtemel sebep: şablon FSBL'i NPU saatini **300 MHz**'e
> kuruyor. Ayarlanabilir, ama önce çalışması gerekiyordu.

### STM32 AI Deployer'ın bu teşhisteki payı — dürüst döküm

Tez/sunum için abartmadan yazıya geçiriliyor: aracımız **kök nedeni bulan
ölçümü sağladı**, ama sürecin tamamını tek başına taşımadı.

**Aracın yaptığı — belirleyici olan kısım:**

- Değişken İzleyici, N6'ya bağlandı ve asılı firmware üzerinde **200.0 Hz,
  0 kaçırılan, 0 okuma hatası** ile örnekledi.
- NPU'nun **SVD'si yok**, yani Register Inspector onu göremiyor. İzleyici'nin
  **ham adres** desteğiyle `ATON.h`'dan türetilen offsetler üzerinden ATON
  register alanı (`0x480E0000`) tarandı: 10 stream engine'in `CTRL`, `ADDR`,
  `POS` register'ları.
- Bulunan: 3 motor RUNNING, hepsi `POS=36`'da donmuş ve **`STRENG7.ADDR =
  0x71037160`** — yani asılı motor **harici flash'tan ağırlık okuyordu**.
- **Çözüme giden hipotez doğrudan bu okumadan çıktı:** "asılan motor harici
  flash'ı okuyan motorsa, ağırlıkları iç RAM'e alalım." Kontrol deneyi bunu
  doğruladı.
- Ayrıca ELF↔hedef eşleşme kontrolü, **bizim üretmediğimiz bir firmware'de**
  `match` verdi (VTOR/SP/reset vektörü tutuyor) — aracın ilk gerçek yabancı-
  firmware sınavı.

**Aracın yapmadığı / yapamadığı:**

- Derleme, imzalama, flash'lama ve boot döngüleri `STM32_Programmer_CLI` ile
  yapıldı (aracın pipeline'ı kendi şablonlarımız için tasarlı, bu el yapımı
  ST projesini kapsamıyor).
- Tüm GPIO portlarının dökümü CLI ile alındı (tek seferlik toplu snapshot).
- **Register Inspector N6'da bu senaryoda çalışmadı** (`registerStage: error`)
  — asılı firmware üzerinde CLI arka ucu bağlanamadı.
- `closeWatchLink()` sonrası `stm32aid-memread.exe` ST-Link'i tutmaya devam
  etti; uygulama kapatılana kadar CLI bağlanamadı.
- ASYNC modda hedef WFE'ye girince aracın gözlem modeli kırıldı.

**Tek cümlelik doğru ifade:** *"Kök nedeni, kendi geliştirdiğimiz aracın
Değişken İzleyici'siyle NPU'nun register alanını canlı tarayıp asılı kalan
stream engine'in okuduğu adresi tespit ederek bulduk."*

Bu dört madde (Register Inspector, sidecar bırakma, uyku modu, liste kırpma)
aracın **gerçek bir senaryoda ortaya çıkan eksikleri** — kapatılacak işler
olarak [`TODO.md`](../TODO.md)'ye taşınmalı.

### Kartın durumu

**Kart sağlam.** RISAF denemeleri sırasında iki kez debug bellek erişimi
öldü; her seferinde `mode=UR` ile geri dönüldü (`0x34000000` → `0x324D5453`
= "STM2", imzalı image başlığı). Proje, RISAF çağrıları devre dışı bırakılmış
**hata ayıklanabilir** hâlde bırakıldı: `stage=8, status=1, count=0`.

---

## 8.9 Döngü kapandı — çalışan NPU kendi aracımızla izlendi (2026-09-17)

İlk kez araç *bozuk* değil *çalışan* firmware'i izledi. Tamamı uygulama
üzerinden (DebugBridge/uiprobe ile sürülerek), memread transport'u ile:

| | |
|---|---|
| ELF eşleşmesi | ✅ SP `0x34200000`, reset `0x34009dc9`, VTOR `0x34000400` |
| Örnekleme | **200.4 Hz**, 1 blok/örnek, RTT 0.70 ms, 0 kaçırılan, 0 okuma hatası |
| `g_ai_last_inference_us` | **3.693 – 3.706 ms** (ort. 3.695) |
| `g_ai_infer_count` | ~**8.0 inference/s** — `HAL_Delay(120)` + 3.7 ms ile tutarlı |
| `g_ai_last_class` / güven | 10 / %78–87 |
| Hız tutarlılık rozeti | "8.0 Hz gözlendi · beyan 3695 µs ile tutarlı" |
| Kayıt | 30.03 s, 6006 örnek, rol etiketli CSV; oynatmada aynı değerler |

Kayıt + ekran görüntüleri: `out/n6_npu_watch/` (gitignored).

**Yolda çıkan ve düzeltilenler:**

- **AI preset'i bu firmware'i tanımıyordu.** `xcubeai_runtime` yalnızca
  `NN_Instance_Default` arıyordu; ST'nin şablonu *adlı* örnek bildiriyor
  (`LL_ATON_DECLARE_NAMED_NN_INSTANCE_AND_INTERFACE(network)` →
  `NN_Instance_network`). Güven sembolü de burada `g_ai_last_confidence_pct`.
  İkisi `watch/watch_presets.json`'a eklendi — C++ değişmedi.
- **İzleyici araç çubuğundaki ELF yolu hep "ELF yüklenmedi" gösteriyordu.**
  Bağlama NOTIFY'sız bir `Q_INVOKABLE` çağırıyordu, yani bir kez
  değerlendirilip donuyordu. `watchSymbolsLoaded` sinyaline bağlandı.

**Gözlemler (düzeltilmedi):**

- `core_memory` preset'inin üç kalemi bu firmware'de anlamsız:
  `__sbrk_heap_end` = 0 (malloc yok), `_end` adresinde `0xFFFFFFFF`,
  `stackWatermark` = "0 B" (yığın boyanmamış — CLAUDE.md'de bilinen sınırlama).
  Üstelik 2048 baytlık bölge taraması hızı **200 → 110 Hz**'e düşürdü
  (3 blok, RTT 9 ms). Demo için bu kalemler kaldırılmalı.
- Kayıt başlığında `model=` boş: model adı yalnızca kendi pipeline'ımızla
  deploy edilince biliniyor.
- `closeWatchLink()` sonrası `stm32aid-memread` bu sefer **çıktı** — TODO'daki
  "ST-Link'i bırakmıyor" maddesi sağlıklı hedefte tekrarlanmadı; asılı
  firmware'e özgü olabilir, kapatılmış sayılmamalı.

---

## 8.10 BME280 eklendi — sensör + NPU aynı ekranda (2026-09-17)

**Bağlantı:** Arduino D15 = PH9 (I2C1_SCL), D14 = PC1 (I2C1_SDA), 3V3, GND;
adres `0x76`. Önce firmware'siz, SWD üzerinden I2C1 elle sürülerek doğrulandı
(ACK + chip ID `0x60`).

**Firmware değişikliği (`n6_ai_node/Appli`):**

- `bme280.c/h` ve `telemetry.c/h` `templates/`'ten kopyalandı (placeholder'lar
  çözüldü); `AppS/.project`'e bunlar + `stm32n6xx_hal_i2c(_ex).c` eklendi,
  `HAL_I2C_MODULE_ENABLED` açıldı.
- `HAL_I2C_MspInit`: PH9/PC1 AF4 open-drain, iç pull-up yok (modülde var),
  `HAL_PWREx_EnableVddIO4/5()`. **1.8 V aralığı hiçbir yerde seçilmiyor.**
- I2C1 zamanlaması `0xF0F6313D` (~100 kHz, PCLK1 = 200 MHz).
- Sensör hatası inference döngüsünü **durdurmaz** (`Error_Handler` yok):
  durum `g_sensor_i2c_ok / _read_ok / _read_count / _fail_count`'ta; okuma
  500 ms'de bir, başarısızsa 2 s'de bir yeniden init.
- Her inference sonrası `g_telemetry` seqlock ile dolduruluyor → İzleyici'nin
  mevcut `sensor_memory` preset'i **hiç C++/JSON değişikliği olmadan** tanıdı.

**Canlı ölçüm (İzleyici, 200 Hz, 0 kaçırılan, ELF eşleşiyor):**

| | min | max | ort |
|---|---|---|---|
| Sıcaklık | 23.02 °C | 23.05 °C | 23.03 °C |
| Nem | %46.20 | %46.32 | %46.26 |
| Basınç | 1001.15 hPa | 1001.30 hPa | 1001.23 hPa |
| Inference | 3.700 ms | 3.707 ms | 3.703 ms |

Ekran görüntüsü: `out/n6_npu_watch/watch_bme280_npu.png` (gitignored).

**Açık kalanlar:**

- **Sınıf sabit girdiye rağmen 6 (%87) ↔ 10 (%66–76) arasında salınıyor.**
  Önceki derlemede 30 s boyunca hep 10'du ama güven %78–87 arasında
  oynuyordu — yani NPU çıktısı zaten deterministik değildi; yeni derleme
  bunu sınıf değişimine taşıdı. Girdi sabit olduğuna göre beklenen tamamen
  deterministik çıktı; muhtemel şüpheliler cache temizleme/geçersizleme
  sırası veya CPU'da koşan 2 epoch'un başlatılmamış belleği. İncelenmeli.
- ~~**İzleyici'de üç sensör kalemi de `g_telemetry` etiketiyle, birimsiz
  görünüyor**~~ ✅ **Düzeltildi (2026-09-17):** preset kalemleri artık
  isteğe bağlı `label` taşıyor (yoksa `sembol+offset`), ve bir preset
  `relabels` ile başka preset'in ürettiği rolleri adlandırabiliyor.
  `bme280_labels` (`BME280_ReadAll` varsa) → "BME280 sicaklik °C / nem % /
  basinc hPa"; `mpu6050_labels` (`MPU6050_ReadAll` varsa) → ax/ay/az m/s².
  Sensör adı C++'ta yok; rol (`sensor0`…) değişmediği için profil
  karşılaştırması etkilenmez. Kartta canlı doğrulandı.
- **`readErrors` seqlock atmalarını da sayıyor.** Görülen 15/39 değeri 3'ün
  katı (yalnızca 3 korumalı kalem) — gerçek okuma hatası değil, firmware'in
  yazma ortasında yakalanan örnekler. UI bunu "okuma hatası" gibi
  gösteriyor; ayrı sayılmalı.

---

## 9. Sıradaki somut adım

1. ~~FW_N6 sürümünü çöz~~ ✅ v1.4.0 kuruldu (2026-09-16).
2. ~~Regresyon kontrolü~~ ✅ Bayt bayt aynı binary — risk yok (§1).
3. ~~`Template_FSBL_LRUN`'ı derle ve karta at, LED yanıp sönsün~~ ✅ **Tamam**
   (§8.5) — FSBL → harici flash → Appli → LED zinciri kartta doğrulandı.
4. **Sıradaki:** CubeMX'te `.ioc` üzerine X-CUBE-AI ekle,
   `mobilenet_v1_0.25_96` ile tek bir inference çalıştır.
5. Sonra: telemetri bloğu, BME280, ağır model, hata enjeksiyonu.

**Yan iş (açık):** kendi uygulamamızın `N6RamImage::armArgs()` yolu HOTPLUG
kullanıyor; §8.5 Engel 3'teki bulgu ışığında UR'ye geçmesi gerekip
gerekmediği incelenmeli.

---

## Kaynaklar

- [How to build an AI application from scratch on the NUCLEO-N657X0-Q using STM32CubeMX](https://community.st.com/t5/stm32-mcus/how-to-build-an-ai-application-from-scratch-on-the-nucleo-n657x0/ta-p/828502) — boot zinciri, adresler, imzalama, RIF, cache sırası
- [STM32N6-GettingStarted-ImageClassification](https://github.com/STMicroelectronics/STM32N6-GettingStarted-ImageClassification) — resmî Nucleo desteği, model süreleri
- [NUCLEO-N657X0-Q ürün sayfası](https://www.st.com/en/evaluation-tools/nucleo-n657x0-q.html) — 512 Mbit Octo-SPI flash, 4.2 MB SRAM
- [stm32ai-modelzoo](https://github.com/STMicroelectronics/stm32ai-modelzoo) — model kaynağı
- Yerel: `C:\ST\STEdgeAI\4.0\Projects\STM32N6570-DK\Applications\hello_world\README_FIRST.md`
- Yerel: `C:\ST\STEdgeAI\4.0\Middlewares\ST\AI\Npu\Devices\STM32N6xx\ATON.h`
