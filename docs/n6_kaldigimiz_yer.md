# STM32N6 — Kaldığımız Yer

Son güncelleme: **2026-09-15** (baştan yazım: 2026-09-14, öncesi: 2026-06-06)

> **Bu dosya 2026-09-14'te baştan yazıldı.** Önceki sürüm "flash/boot çözüldü,
> sorun UART RX'te" diyordu ve bağlantı sorunlarını kalıcı bir ST-Link kısıtı
> sanıyordu. **İkisi de yanlışmış.** Gerçek engel bir BOOT jumper pozisyonuydu;
> bulunduktan sonra N6'da canlı bellek/register erişimi tamamen açıldı. Tarihsel
> kayıt (UART komut-cevap serüveni) §11'de korundu.
>
> **2026-09-15 eki:** §9'daki "İzleyici N6'da kapalı" sonucu artık geçerli değil.
> gdbserver hâlâ N6'da oturum açamıyor, ama İzleyici ikinci bir transport
> kazandı ve N6'da tam hızda çalışıyor — bkz. [`memread_sidecar.md`](memread_sidecar.md).

---

## 1. Otuz saniyelik özet

| Konu | Durum |
|---|---|
| Canlı bellek okuma (çalışırken) | ✅ Çalışıyor |
| Pin / peripheral register okuma | ✅ Çalışıyor |
| Model yükleyip çalıştırma | ✅ Çalışıyor — RAM'e, imzalama yok |
| Inference | ✅ 42 µs @ 600 MHz |
| Register Inspector | ✅ Yol açık (CLI arka ucu) |
| Değişken İzleyici | ✅ 200 Hz / 0 kaçırılan (memread arka ucu, §9) |
| — gdbserver yolu | ❌ N6'da oturum açamıyor (kalıcı, §9) |
| NPU iç register'ları | ⚠️ Adres ve tanımlar bulundu, henüz okunmadı |

**Ön koşul:** kart **geliştirme boot** pozisyonunda olmalı (§3). Bu tek şart
sağlanmazsa yukarıdakilerin hiçbiri çalışmaz.

---

## 2. Neden N6 diğer kartlardan farklı

F4 ve H7'de çipin içinde kalıcı flash var (F4: 1 MB @ `0x08000000`). Program
oraya yazılır, güç gelince CPU oradan çalışır, kalıcıdır.

**N6'da iç flash yoktur.** Çipin içinde sadece RAM (4 MB AXISRAM) ve ST'nin
fabrikada yazdığı bir boot ROM vardır. Program ya kartın üzerindeki **harici
flash yongasında** durur, ya da her açılışta **dışarıdan RAM'e yüklenir**.

Bu tek mimari fark, aşağıdaki her şeyin sebebidir.

---

## 3. Boot modları ve BOOT jumper'ları — EN KRİTİK BÖLÜM

Güç gelince CPU her zaman önce boot ROM'a gider. ROM ilk iş olarak BOOT
pinlerine bakar:

| Mod | ROM ne yapar | Debug erişimi |
|---|---|---|
| **Geliştirme boot** | ROM'da bekler, dışarıdan program almayı bekler | **Tam açık** |
| **Flash boot (LRUN)** | Harici flash'ı okur, **imzayı doğrular**, RAM'e kopyalar, atlar | **Kapalı** |

Karttaki iki jumper (BOOT0 / BOOT1, 3 pinli header) bu pinleri fiziksel olarak
ayarlar.

**Flash boot bir güvenli boot'tur.** İmza doğrulandıktan sonra ROM, güvenlik
gereği debug biriminin belleğe erişimini (AHB-AP) kapatır — sahadaki imzalı bir
ürüne debugger takılıp içeriğinin okunmasını engellemek için. Bu tasarım gereği
böyledir, arıza değildir.

**Flash boot pozisyonundaki belirti (2026-09-14'te canlı ölçüldü):**

```
mode=HOTPLUG → Error: Unable to get core ID
mode=UR      → bağlanır (Device ID 0x486) ama HER bellek okuması reddedilir,
               standart Cortex-M CPUID adresi 0xE000ED00 dahil
-halt        → "Core halted" der, yine de bellek okunamaz
```

Benzetme: bina interkomu cevap veriyor ama bütün kapılar kilitli.

**Debug Authentication bu işin parçası DEĞİL.** `debugauth=2` (discovery)
çalıştırıldığında cihaz **OPEN mode**'da olduğunu bildiriyor:

```
The target is unable to boot on RSS_DA or is in OPEN mode
```

Yani kilitli/closed bir cihazla uğraşmıyoruz; kapatan şey boot modunun kendisi.

**Geliştirme boot pozisyonuna alındığında** cihaz tam adıyla tanınır
(`ST32N657`, sadece `STM32N6xx` değil) ve tüm bellek/register okumaları çalışır.

> **Pin numarası uyarısı:** header'ın hangi ucunun pin 1 olduğu doğrulanmadı, bu
> yüzden burada "1-2 / 2-3" yerine fiziksel tarif kullanılıyor. 2026-09-14
> oturumunda çalışan pozisyon: **BOOT1 jumper'ı USB-C konnektörüne yakın iki
> pinde**, BOOT0 uzaktaki iki pinde. Şüphe varsa BOOT1'i diğer pozisyona alıp
> `mode=HOTPLUG -r32 0xE000ED00 0x4` ile test edin — gerçek bir değer dönüyorsa
> (`411FD221`) doğru pozisyondasınız.

---

## 4. İki yol: LRUN vs RAM boot

```
LRUN (eski):  derle → imzala → harici flash'a yaz → reset
              → ROM imzayı doğrular → FSBL → RAM'e kopyalar → çalış
              ⇒ kalıcı, ama DEBUG KAPALI, ve imzalama/external loader gerekir

RAM (yeni):   derle → ST-Link doğrudan RAM'e yazar → MSP/PC kur → çalış
              ⇒ güç kesilince uçar, ama DEBUG TAM AÇIK, yükleme 0.13 saniye
```

**RAM boot bir hile değildir.** Linker script (`templates/base/STM32N6/STM32N6xx_FLASH.ld`)
programı zaten AXISRAM'e bağlıyor:

```
MEMORY { RAM (xrw) : ORIGIN = 0x34000400, LENGTH = 2047K }
```

LRUN yolunda da FSBL'in görevi programı **tam o adrese** kopyalayıp oraya
atlamaktı. RAM boot sadece aradaki adımları atlar — aynı varış noktası.

Bu araç için RAM boot kesinlikle daha iyi: host zaten biziz, imzalama zinciri
gereksiz, ve debug açık kalıyor. **UART de kaybolmuyor** — uygulama yine kendi
LPUART1'ini kuruyor. Kaybedilen tek şey host'suz otonom açılış.

---

## 5. Çalışan yükle-ve-başlat dizisi

İki adım hâlinde olmak zorunda. Sebepleri aşağıda.

```powershell
$cli = "C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"
$sn  = "001A00273434511734313937"

# ADIM 1 — temiz donanım reset
& $cli -c port=SWD sn=$sn mode=UR -run

# ADIM 2 — yükle + çekirdek durumunu kur + çalıştır
& $cli -c port=SWD sn=$sn mode=HOTPLUG -halt `
    -w app.bin 0x34000400 `
    -w32 0xE000ED08 0x34000400 `   # VTOR  → vektör tablomuz
    -w32 0xE000ED88 0x00F00000 `   # CPACR → FPU/MVE aç (§6)
    -coreReg MSP=0x34200000 PC=0x340095E8 `
    -run
```

`MSP` ve `PC` **imajın kendisinden** okunur, sabit değildir:

```
imajın ilk 4 baytı  = başlangıç MSP   (bu build'de 0x34200000)
sonraki 4 bayt      = Reset_Handler   (bu build'de 0x340095E9 → PC'ye 0x340095E8)
```

Son bitteki `1` Thumb bitidir; `PC`'ye yazarken düşürülür.

**Neden `-g <adres>` kullanılmıyor:** `-g 0x34000400` vektör tablosunun kendisine
atlar, yani tabloyu komut sanıp çalıştırır. Denendi, çalışmıyor.

**Neden Adım 1 şart:** bkz. §7.

---

## 6. Bulunan ve düzeltilen gerçek hata — FPU açılmıyordu

Cortex-M55'in FPU/Helium birimi reset'te **kapalı** gelir; `CPACR`
(`0xE000ED88`) yazılarak açılır. CubeN6 SDK'sının `SystemInit`'i bunu bilerek
yapmaz:

```c
/* Vector table location and FPU setup done by secure application */
```

Yani FSBL'in yapmasını bekler. RAM boot'ta FSBL yoktur → FPU açılmaz → build
`-mfloat-abi=hard` olduğu için ilk float komutunda çöker.

Fault register'larından kanıtlandı (tahmin değil):

```
CFSR = 0x00080000   → UFSR bit 3 = NOCP  ("No CoProcessor")
HFSR = 0x40000000   → FORCED (UsageFault'tan yükseltilmiş)
CPACR= 0x00000000   → FPU gerçekten kapalı
```

**Düzeltme:** `templates/base/STM32N6/startup_stm32n6xx.s` içinde `Reset_Handler`
başına CP10/CP11 açma eklendi, `bl SystemInit`'ten önce. Idempotent — LRUN
yolunda FSBL zaten açmışsa zarar vermez.

Bu, F4'te bulunan `SystemInit` hatasının kardeşidir (bkz. commit `9a41bc2`):
aynı aile, aynı belirti (hard-float ABI + açılmamış FPU = anında fault).

---

## 7. Bir tur yakan incelik — PC yazmak reset değildir

İlk çökme sonrası FPU düzeltilip program "yeniden başlatıldığında" (sadece
`PC` yazılarak) program yine takıldı. Sebep:

CPU hâlâ **HardFault handler'ının içinde olduğunu sanıyordu** —
`SHCSR = 0x4`, yani `HARDFAULTACT` biti set. HardFault önceliği (-1) normal
kesmeleri maskeler, **SysTick dahil**. Zincir:

```
SysTick kesmesi gelmiyor → HAL'in uwTick'i hiç artmıyor
→ her HAL timeout'u sonsuza kadar bekliyor
→ program I2C_WaitOnTXISFlagUntilTimeout içinde asılı kalıyor
```

`mode=UR -run` ile gerçek donanım reset'i atıldığında `SHCSR` temizlendi
(`0x00000000`) ve program normal çalıştı.

**Kural:** `-coreReg PC=...` yazmak istisna durumunu temizlemez. Yükleme
öncesinde daima gerçek reset.

---

## 8. Canlı doğrulanan ölçümler (2026-09-14)

Donanım: NUCLEO-N657X0-Q, ST-LINK SN `001A00273434511734313937`, FW `V3J17M10`,
VCP **COM5** (eski doküman COM20 diyordu — artık geçersiz).
Araçlar: STM32CubeProgrammer **v2.23.0**, SDK `STM32Cube_FW_N6_V1.0.0`,
ST Edge AI Core **4.0**.
Firmware: `out/pipeline_test_n6/build/anomaly_mlp_int8_STM32N6.bin` (66.416 bayt).

| Ölçüm | Değer | Kanıt |
|---|---|---|
| Cihaz | `ST32N657`, Device ID `0x486`, Rev Z | CLI connect |
| CPU çekirdeği | Cortex-M55, CPUID `411FD221` | `0xE000ED00` |
| Saat | 600 MHz | SysTick `LOAD = 599999` = 600000 çevrim/ms |
| Inference | **42 µs** | `g_ai_last_inference_us` |
| HAL tick | 1 ms/tick, doğru artıyor | `uwTick` iki okuma farkı |
| Canlılık | `DHCSR = 0x01110000` | `S_HALT=0`, `S_RETIRE_ST=1`, `S_SDE=1` |
| Telemetry seqlock | geçerli (`seq_begin == seq_end`) | `g_telemetry` 92 bayt |
| Pin okuma | `GPIOE_IDR = 0x60` → PE5/PE6 yüksek | USART1 pinleriyle tutarlı |
| NPU clock | `RCC_AHB5ENR.NPUEN = 0` | model NPU'yu kullanmıyor |

Karşılaştırma: inference F4'te 125 µs, H7'de 943 µs, **N6'da 42 µs**.

**Secure / non-secure alias:** N6'nın her peripheral'ı iki adresten görünür
(`0x46028000` non-secure ↔ `0x56028000` secure). İkisi de okunabiliyor ve
**aynı değeri döndürüyor** — yani SVD'deki non-secure adresler doğrudan
kullanılabilir, ekstra bir eşleme gerekmiyor.

---

## 9. Çalışmayan: ST-LINK_gdbserver N6 çekirdeğini durduramıyor

Değişken İzleyici'nin kullandığı yol budur (`DebugLink` → gdbserver → GDB RSP).

```
-g  (attach)     → "Try halt..." → "Session manager. Fail starting session."
-k  (under reset)→ "Target not halted after reset. Force halt"
                   "Failed to halt target" → "Error in initializing ST-LINK device."
```

**Denenen ve hepsi aynı şekilde başarısız olan varyantlar:**
`-g`, `-k`, `--halt`, `-m 0`, `--frequency 1000`, `--pend-halt-timeout 30`,
`-t` (shared), ve `-cp` olarak hem STM32CubeProgrammer 2.23.0 hem STM32CubeIDE
2.2.0'ın kendi paketlediği CubeProgrammer.

**Bunun gdbserver sınırı olduğunun kanıtı:**
- Aynı gdbserver aynı makinede H7'ye sorunsuz bağlanıyor (CPUID `0x411fc272` okundu)
- STM32_Programmer_CLI aynı N6 çekirdeğini sorunsuz durduruyor (`-halt` → "Core halted")

Yani donanım halt'ı destekliyor; gdbserver'ın halt yöntemi N6'da işlemiyor.

> Önceki dokümanlardaki "bilinen N6 ST-Link kısıtı" ifadesinin gerçek sebebi
> budur. Genel bir ST-Link sorunu değil, gdbserver'a özgü.

**Sonuç (2026-09-15 itibarıyla ÇÖZÜLDÜ):** gdbserver yolu N6'da kapalı kaldı,
ama İzleyici artık ikinci bir transport kullanıyor ve N6'da **tam hızda
çalışıyor**.

ST, `STM32_Programmer_CLI`'nin içeride kullandığı C API'sini resmî olarak
dağıtıyor (`STM32CubeProgrammer/api/` — header, DLL, dokümantasyon, örnek
projeler). Bağlantı açık tutulunca 530 Hz çıkıyor. Süreç içine yüklenemiyor
(ST'nin DLL'i Qt 6.10.2'ye bağlı, uygulama Qt 6.11), bu yüzden **Qt'ye hiç
bağlanmayan `stm32aid-memread` yardımcı süreci** yazıldı.

N6'da ölçülen: 200 Hz hedefiyle **200.0 Hz, 0 kaçırılan, 0 okuma hatası**,
rtt 0.70 ms; 500 Hz hedefiyle 501.0 Hz. `SysTick_LOAD = 599999` okunuyor, yani
600 MHz — kartın gerçek saatiyle birebir.

Tam anlatım, protokol, tuzaklar ve üç kartın ölçüm tablosu:
[`memread_sidecar.md`](memread_sidecar.md).

> Tarihsel not: bu bölüm 2026-09-14'te "alternatif ~3.2 Hz'lik CLI yolu"
> diyordu. O ölçüm doğruydu (her okumada süreç başlatmanın maliyeti), ama
> kalıcı bağlantı seçeneği o gün fark edilmemişti. 3 Hz'e razı olmak
> gerekmedi.

## 10. NPU görünürlüğü

**`svd/STM32N657.svd` içinde NPU register bloğu YOKTUR.** 248 peripheral
taranmış, hiçbiri NPU değil. SVD'den elde edilebilecek NPU bilgisi yalnızca
dolaylıdır:

| Register | Adres | İçerdiği NPU alanları |
|---|---|---|
| `RCC_AHB5ENR` | `0x46028000 + 0x260` | `NPUEN`, `NPUCACHEEN` |
| `RCC_AHB5RSTR` | `0x46028000 + 0x220` | `NPURST`, `NPUCACHERST` |
| `RCC_AHB5LPENR` | `0x46028000 + 0x2A0` | `NPULPEN`, `NPUCACHELPEN` |
| `RCC_MEMENR` | `0x46028000 + 0x24C` | `NPUCACHERAMEN` |
| `SYSCFG_NPU_ICNCR` | `0x46008000 + 0x078` | NPU RAM interleaving |
| `SYSCFG_NPUNICQOSCR` | `0x46008000 + 0x028` | NPU master port QoS |
| `DBGMCU_AHB5FZ1` | `0x44001000 + 0x028` | `NPU_DBG_FREEZE` (bit 16) |

Bu kadarıyla bile "**model gerçekten NPU'yu kullanıyor mu?**" sorusu
cevaplanabilir — `NPUEN` okunur. 2026-09-14'te çalışan `anomaly_mlp_int8`
modelinde `NPUEN = 0`, yani model tamamen CPU'da koşuyor.

### NPU'nun gerçek register haritası nerede

**ST'nin NPU mimarisinin iç adı "ATON"dur**; "Neural-ART" pazarlama adıdır.
"NPU" diye aramak sonuç vermez, **"ATON" aramak gerekir.**

Tam register tanım seti makinede kurulu:

```
C:\ST\STEdgeAI\4.0\Middlewares\ST\AI\Npu\Devices\STM32N6xx\ATON.h
    853 KB, 6858 adet "#define ATON..."
C:\ST\STEdgeAI\4.0\Middlewares\ST\AI\Npu\ll_aton\ll_aton_dbgtrc.h
    NPU donanım debug/trace birimi — stall sayaçları, bus bant genişliği monitörleri
```

**NPU base adresi** (SDK header'ından türetildi, SVD'de yok):

```
PERIPH_BASE_NS          0x40000000
AHB5PERIPH_BASE_NS    + 0x08020000   = 0x48020000
NPU_BASE_NS           + 0x000C0000   = 0x480E0000      (secure: 0x580E0000)
```

Formül `CACHEAXI` ile çapraz doğrulandı: aynı taban `+ 0xBFC00` = `0x480DFC00`,
SVD'nin verdiği `CACHEAXI` adresiyle birebir aynı.

Alt bloklar `ATON_BASE`'e görelidir, örn.
`ATON_ACTIV_BASE(UNIT) = ATON_BASE + 0x15000 + 0x1000 * UNIT`.

Ayrıca NPU'nun 4 kesme hattı vardır (`NPU0_IRQn`..`NPU3_IRQn` = 53..56) ve
CACHEAXI'nin kendi kesmesi (`57`).

**Henüz yapılmadı:** bu bölge hiç okunmadı. NPU clock'u kapalıyken
(`NPUEN = 0`) okuma büyük ihtimalle hata verir. Gerçek NPU register'ı görmek
için önce NPU'yu fiilen kullanan bir model derlenmeli.

---

## 11. Tarihsel kayıt — UART komut/cevap serüveni (2026-05 … 2026-06)

Bu bölüm korunuyor çünkü hâlâ geçerli bir karar kaydı: **N6'da host→kart komut
yolu hiç çalışmadı ve terk edildi.**

Kart UART'tan TX yapabiliyordu (reset sonrası boot JSON güvenilir şekilde
geliyordu), ama host'tan gönderilen `INFO?`, `BOOT?`, `INFER ...`, `BENCH ...`
komutlarına **hiç cevap gelmedi**. Denenen ve sonuç vermeyenler: AI init'i lazy
yapmak, UART polling eklemek, HAL yerine doğrudan `USART1->ISR` / `USART1->RDR`
okumak, `PRIMASK`/`FAULTMASK`/`BASEPRI` temizliği, PE5/PE6 için GPIO
secure/privilege attribute ayarları, I2C/sensör init'ini startup'tan çıkarmak.
**RX tarafındaki kök neden hiç bulunamadı.**

**2026-06-06'da mimari değiştirildi:**
- Firmware UART'ı `USART1` → `LPUART1`, baud `115200` → `209700`
- N6'ya komut gönderilmiyor; `Backend::resetN6TargetForCapture()`
  (`boardOrPortLooksLikeN6` ile kapılı) bağlantıda kartı CLI ile resetleyip
  firmware'in kendiliğinden ürettiği JSON akışını **pasif dinliyor**
- `AppState::setActiveBoard` N6 seçilince baud'u otomatik `209700` yapıyor
  (`kN6DefaultBaud`)
- F4/H7 etkilenmedi, onlar normal komut/cevap protokolünü kullanmaya devam ediyor

> **2026-09-14 notu:** bu pasif yakalama yolu **flash boot** modunu varsayar
> (kart kendi kendine açılır). RAM boot moduna geçilirse kartı biz başlattığımız
> için "reset + pasif yakalama" mantığının gözden geçirilmesi gerekir — firmware
> zaten biz başlattığımız anda JSON basmaya başlıyor.

**Eski LRUN flash ayarları** (RAM boot kullanılmayacaksa hâlâ geçerli):
FSBL LRUN hedefi `0x34000000`, FSBL header offset `0x400`, FSBL source size
`0x00100000`, app adresi `0x70100000`, FSBL adresi `0x70000000`, external loader
`MX25UM51245G_STM32N6570-NUCLEO.stldr`. İmzalama argümanlarında `-align`
**zorunlu** — onsuz SigningTool header v2.3 payload'ı `0x400` hizasına koymuyor
ve verify geçse bile boot düzgün kalkmayabiliyor.

---

## 12. Açık işler

1. **RAM-boot'u uygulamaya bağlamak.** §5'teki dizi şu an elle koşuluyor.
   `PipelineRunner` / `Backend` N6 için flash yerine bu yolu izlemeli:
   `stedgeai → derle → AXISRAM'e yaz → VTOR/CPACR/MSP/PC kur → çalıştır`.
   `MSP`/`PC` imajın ilk 8 baytından okunmalı, sabitlenmemeli.
2. **Register Inspector'ı N6'da doğrulamak.** `CliRegisterReader` zaten
   `-c port=SWD mode=<mode> sn=<sn> -r32 ...` üretiyor — elle doğrulanan komutun
   aynısı. Muhtemelen kod değişikliği gerekmiyor, sadece canlı test + boot modu
   uyarı metinleri.
3. **İzleyici için CLI arka ucu** (~3.2 Hz) — isteğe bağlı, §9.
4. **NPU register okuma** — önce NPU kullanan bir model, sonra `0x480E0000`.
5. **`svd/boards.json` N6 kaydını güncellemek.** `notes` alanı hâlâ
   "HOTPLUG may fail on secured firmware" diyor ve `debug.gdb.notes` "LRUN boot
   sonrası -g attach doğrulanmadı" diyor — ikisi de artık kesinleşti (§3, §9).
6. **Doğrulanmış startup/linker/HAL şablon seti.** FPU düzeltmesiyle (§6)
   şablon önemli ölçüde iyileşti ama hâlâ F4/H7 kadar oturmuş değil.

---

## 13. Sorun giderme hızlı referansı

| Belirti | Muhtemel sebep |
|---|---|
| `Unable to get core ID` | BOOT jumper'ı flash boot pozisyonunda (§3) |
| Bağlanıyor ama her okuma reddediliyor | Aynı — flash boot, AHB-AP kapalı |
| Program yüklendi, `.bss` sıfırlandı, sonra takıldı | FPU açılmamış (§6) |
| `uwTick` hiç artmıyor, HAL timeout'ları dolmuyor | Bayat `HARDFAULTACT` (§7) |
| `-g` ile atladım, hiç çalışmadı | `-g` vektör tablosuna atlar; `-coreReg` kullan (§5) |
| GDB dosyayı açamıyor (`Invalid argument`) | Proje yolundaki Türkçe karakterler; ELF'i ASCII bir yola kopyala |
| gdbserver `Fail starting session` | Bilinen N6 sınırı (§9), ayar denemeyin |
