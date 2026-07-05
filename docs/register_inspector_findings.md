# Register Inspector — Faz 0 Donanım Doğrulama Bulguları

> **Amaç:** `docs/register_inspector_plan.md` Bölüm 6, Faz 0'da tanımlanan
> donanım doğrulama spike'ının sonuçları. Bu doküman, tüm implementasyonun
> (parser, okuma planı, decode) dayandığı **gerçek CLI davranışının** kaydıdır.
> **Durum:** H7 zorunlu testleri tamamlandı — MVP çıkış kriteri karşılandı.
> F4/N6 testleri kart bağlanınca koşulacak (aşağıda ertelenmiş kontrol listeleri).

---

## 0. Test Ortamı

| Öğe | Değer |
|-----|-------|
| Tarih | 2026-07-05 |
| Kart | **NUCLEO-H723ZG** (birincil doğrulama kartı, eldeki tek kart) |
| Device ID | `0x483` (STM32H72x/STM32H73x) |
| Revision ID | Rev Z |
| CPU | Cortex-M7 |
| ST-Link | SN `004D003B3235511837333439`, FW `V3J16M9` (STLINK-V3) |
| SWD frekansı | 8000 KHz |
| Voltaj | 3.27 V |
| **STM32_Programmer_CLI** | **v2.22.0** — plan beklentisiyle eşleşti |
| CLI yolu | `C:\ST\STM32CubeIDE_2.1.1\...\externaltools.cubeprogrammer.win32_2.2.400.202601091506\tools\bin\STM32_Programmer_CLI.exe` |
| UART/VCP | COM7, **115200** baud, § protokolü (`0xC2 0xA7` + `\r\n` doğrulandı) |

> **Not (CLI yolu):** Bu makinede CLI **standalone STM32CubeProgrammer** kurulumu
> değil, **STM32CubeIDE 2.1.1 plugin** paketinden geliyor. `ToolDetector`'ın
> sabit yolları (`Program Files/.../STM32CubeProgrammer/bin/`) bu makinede yok;
> araç PATH'te de değil. Register Inspector aynı `AppSettings::programmerCliPath()`
> → `FlashManager::detectCliPath()` zincirini kullanacağı için bu ortak sorun;
> ToolDetector'ın CubeIDE plugin yolunu da tarayıp taramadığı ayrıca kontrol
> edilmeli (bu bulgu Register Inspector'a özel değil, mevcut altyapıyı ilgilendirir).

Referans H7 base adresleri (SVD gelmeden önce RM0468'den, testlerde kullanıldı):
`RCC=0x58024400`, `GPIOA=0x58020000`, `GPIOB=0x58020400`, `GPIOD=0x58020C00`,
`USART3=0x40004800`, `I2C1=0x40005400`, `DMA1=0x40020000`, `LPUART1=0x58000C00`.

---

## 1. Test 1 — HOTPLUG canlı okuma, reset/halt yok ✅

**Komut:** `-c port=SWD mode=HOTPLUG -r32 0x58024400 <N>`

- HOTPLUG connect **başarılı**; reset atılmadı, core durdurulmadı.
- **Kritik doğrulama — CPU çalışmaya devam etti:** COM7 ayrı bir process
  tarafından açık tutulurken SWD okuması yapıldı. Firmware § üzerinden yaklaşık
  10 sn'de bir `err` paketi yolluyor. Okuma iki paket **arasına** düştü
  (okuma 17:54:54.5–55.9); okuma sonrası paket **firmware'in kendi zamanlayıcı
  çizelgesine tam uygun** geldi (48.290 → 58.383, ~10.09 sn). Core ~1.4 sn
  halt edilseydi bu paket gecikirdi — gecikmedi.
- **Reset olmadı kanıtı:** Pencere boyunca **hiç `boot` paketi görülmedi**
  (protokol reset sonrası bir `boot` paketi yollar). Uptime alanı bu firmware'de
  `sys` paketi olmadığından doğrudan okunamadı; `boot` paketinin yokluğu reset
  vekili olarak kullanıldı.
- **VCP ↔ SWD bağımsızlığı doğrulandı:** COM7 başka bir process tarafından
  **açık tutulurken** SWD okuması sorunsuz çalıştı — port çakışması yok. Bu,
  planın "snapshot sırasında Monitor kapatılmaz" varsayımını (Bölüm 5.3) fiilen
  doğrular.

**Sonuç:** Planın en kritik varsayımı (Bölüm 3.2 — HOTPLUG core'u durdurmadan
okur) H7'de **doğrulandı**.

---

## 2. Test 2 — Tek çağrıda çoklu `-r32` ✅

**Komut:** `... -r32 0x58024400 0x20 -r32 0x58020000 0x28 -r32 0x58020400 0x28`

- **Çoklu `-r32` tek çağrıda ÇALIŞIYOR.** Üç blok da çıktıda mevcut, her biri
  kendi `Reading 32-bit memory content` / `Size` / `Address` başlığıyla.
- **Sıra güvenilir:** çıktı, komut satırındaki sırayı korudu (RCC → GPIOA →
  GPIOB). Yine de parser **adres alanına çapalanacak** (plan Bölüm 3.5), sıraya
  güvenmeyecek — topluluk sıra-karışması raporlarına karşı savunma; bu testte
  sorun görülmedi ama garanti sayılmıyor.
- **Performans mükemmel:** 3 blok tek connect ile ~1.4 sn (aşağıda Test 4).
  Planın "snapshot başına 2 çağrı" hedefi (Bölüm 3.5) **uygulanabilir** — çağrı
  bölmeye gerek yok.

**Sonuç:** Risk tablosundaki (Bölüm 7.2) "çoklu `-r32` desteklenmiyor" riski
H7'de **gerçekleşmedi**; grup-başına-çağrı fallback'ine ihtiyaç yok.

---

## 3. Test 3 — Clock'u kapalı peripheral okuması ✅ (beklenenden iyi)

RCC dump'ından decode edilen enable durumu (`RCC_AHB4ENR` @ `0x580244E0` =
`0x0000000A` → yalnızca **GPIOB** (bit1) ve **GPIOD** (bit3) açık; GPIOA kapalı):

| Peripheral | Domain / Bus | Clock | Okuma sonucu | exit |
|-----------|--------------|-------|--------------|------|
| GPIOA `0x58020000` | AHB4 (D3) | **kapalı** | tüm word'ler `0xABFFFFFF` (tekdüze) | 0 |
| DMA1 `0x40020000` | AHB1 (D2) | **kapalı** | tüm word'ler `0x00000000` | 0 |
| LPUART1 `0x58000C00` | APB4 (D3) | **kapalı** | tüm word'ler `0x00000000` | 0 |
| GPIOB `0x58020400` | AHB4 (D3) | açık | gerçek, çeşitli değerler | 0 |

**Kritik bulgu — H7'de clock-off okuması bus-fault ÜRETMEDİ ve zinciri
DÜŞÜRMEDİ:**
- `RCC(ok) → DMA1(off) → LPUART1(off) → RCC(ok)` zinciri tek çağrıda koşuldu;
  **son RCC bloğu dahil dört blok da döndü**, exit=0. İki clock-off okuması
  ortadayken sonraki okuma düşmedi.
- Bu, planın (Bölüm 3.2 / Risk tablosu) "H7/N6'da kapalı-clock okuması bus fault
  ile zinciri düşürebilir" korkusunun **bu H7'de gerçekleşmediğini** gösterir.

**Ama RCC-first gating hâlâ ZORUNLU — sebep değişti:** Fault'tan kaçınmak için
değil, **yanlış yorumlamayı önlemek** için. Clock-off okuması sessizce makul
görünen veri döndürüyor (`0x00000000` veya GPIOA'da `0xABFFFFFF`); RCC decode'u
yapılmazsa bu çöp, gerçek register state'i sanılır. Yani:
- Plan Bölüm 3.2'deki iki-aşamalı RCC-first snapshot **korunur**.
- Clock-off peripheral UI'da "clock kapalı" rozetiyle gösterilecek (plandaki
  gibi) — ama gerekçe metnine "aksi halde `0x00000000`/`0xABFFFFFF` çöpü gerçek
  değer sanılır" eklenmeli.
- Blok-hatası toleransı yine de implemente edilecek (savunma): başka bir kart /
  domain / adres bus-fault üretebilir; H7'de görülmemesi garanti değil.

> **Not:** Gerçekten geçersiz/haritalanmamış (reserved) bir adresin zinciri
> düşürüp düşürmediği ayrıca test EDİLMEDİ — Faz 0 kriteri clock-off peripheral
> ile ilgiliydi. `ReadPlanBuilder` yalnızca SVD `addressBlock`'ları içinde
> kalacağından reserved-hole okuması normal akışta oluşmaz; yine de blok-hatası
> toleransı bu ihtimali de kapsayacak.

---

## 4. Test 4 — Çıktı formatı + zamanlama (parser fixture'ı) ✅

**Zamanlama (2 çağrılı temsili snapshot):**

| Çağrı | İçerik | Süre |
|-------|--------|------|
| 1 | RCC `0x100` (gating okuması) | **1397 ms** |
| 2 | GPIOB + GPIOD + USART3 + I2C1 zinciri | **1395 ms** |
| **Toplam** | | **2792 ms** |

Planın hedefi **≤ 3 sn** (Bölüm 3.5) H7'de **karşılandı**. Süreyi connect
domine ediyor (~1.4 sn/çağrı); okunan blok sayısı süreyi neredeyse etkilemiyor.

**Canlı-state çapraz doğrulaması (okuma gerçekten çalışan firmware'i yansıtıyor):**
- `USART3_CR1` (`0x40004800`) = `0x0000002D` → **UE=1**, TE=1, RE=1, RXNEIE=1.
  USART3 NUCLEO-H723ZG'nin ST-Link VCP hattı (PD8/PD9); GPIOD clock'u da açık
  (`AHB4ENR` bit3). Yani § akışının geldiği peripheral canlıda enable görünüyor.
- `I2C1_CR1` (`0x40005400`) = `0x00000001` → **PE=1** (I2C açık; `APB1LENR` bit21).

Bu, plan Faz 2 doğrulama adımını ("firmware'in UART peripheral'ında `CR1.UE=1`
görünmeli") **şimdiden** karşılıyor.

### 4.1 `-r32` çıktı formatı (parser bu şablona yazılacak)

Ham fixture'lar repoda: [`docs/register_fixtures/h7_rcc_r32.txt`](register_fixtures/h7_rcc_r32.txt),
[`docs/register_fixtures/h7_periph_chain_r32.txt`](register_fixtures/h7_periph_chain_r32.txt).

Yapı:
```
<banner: 3 satır>
ST-LINK SN  : ...          <- ST-Link bilgi bloğu (~14 satır)
...
BL Version  : 0x93
                            <- boş satırlar
Reading 32-bit memory content
  Size          : 256 Bytes
  Address:      : 0x58024400
0x58024400 : 0307C025 40000710 00000150 20000089
0x58024410 : 0000001B 00000000 00000048 00000440
...
                            <- her -r32 bloğu arasında boş satırlar
Reading 32-bit memory content   <- sonraki blok...
```

**Veri satırı grameri (parser'ın çapalanacağı tek şey):**
```
^0x[0-9A-Fa-f]{8}\s*:\s*(([0-9A-Fa-f]{8})(\s+[0-9A-Fa-f]{8})*)\s*$
```
- Satır başı: adres (`0x` + 8 hex).
- ` : ` ayracından sonra **1–4 word** (her biri 8 hex, **büyük harf**, `0x`
  öneki YOK), boşlukla ayrılmış.
- Her word, adres/adres+4/... konumundaki 32-bit little-endian değerdir.
- Bir bloğun **son satırı 4'ten az word** içerebilir (ör. 10-word'lük okumada
  son satır 2 word: `0x58020420 : 00000000 00000044`).
- Parser **yalnızca bu regex'e uyan satırları** işler; banner/başlık/boş
  satırların tümü yok sayılır. Bloklar arası eşleme adresle yapılır, sıraya
  güvenilmez.

**Tolerans notları (parser sağlamlığı için):**
- `NVM size  : 1 MBytes` satırında hizalama tutarsız (çift boşluk) — banner
  parse edilmediği için önemsiz, ama format sürümle oynayabilir.
- CLI sürümü çıktıya gömülü (`v2.22.0`); parser sürüm-toleranslı olacak, format
  tanınmazsa anlamlı hata verecek (Risk tablosu, Bölüm 7.2).

---

## 5. Çıkış Kriteri Değerlendirmesi

Plan Faz 0 çıkış kriteri: *"1, 2 (veya bölünmüş-çağrı alternatifi) ve 4 H7'de
doğrulandı; 3 bulgusu plana işlendi."*

| Madde | Durum |
|-------|-------|
| 1 — HOTPLUG reset/halt'sız okuma | ✅ Doğrulandı |
| 2 — Tek çağrıda çoklu `-r32` | ✅ Doğrulandı (bölünmüş-çağrı fallback'i gereksiz) |
| 3 — Clock-off davranışı | ✅ Test edildi; bulgu plana işlendi (Bölüm 3 üstü) |
| 4 — Format + zamanlama | ✅ Kaydedildi (fixture repoda, ≤3 sn) |

**MVP açıldı.** Faz 1 (SVD altyapısı) başlayabilir.

### Plana işlenecek revizyonlar
1. **Bölüm 3.2 / Risk tablosu:** "clock-off okuması H7'de bus-fault + zincir
   düşmesine yol açabilir" ifadesi H7 için **doğrulanmadı** — clock-off okuması
   sessizce `0x00000000`/`0xABFFFFFF` döndürüyor, zincir hayatta kalıyor.
   RCC-first gating'in gerekçesi "fault'tan kaçınma" değil "çöpü gerçek sanmayı
   önleme" olarak güncellenmeli. Blok-hatası toleransı yine de savunma katmanı
   olarak kalır.
2. **ToolDetector:** CubeProgrammer CLI bu makinede yalnızca CubeIDE plugin
   yolunda mevcut; standalone yol ve PATH boş. (Register Inspector'a özel değil,
   mevcut araç-tespit altyapısını ilgilendiren ayrı bir madde.)

---

## 6. ERTELENMİŞ Kontrol Listeleri (kart bağlanınca koşulacak)

### F4 bağlanınca (beklenti: kod değişikliği yok, yalnızca doğrulama)
- [ ] Test 1–4'ün F4 tekrarı. RCC base `0x40023800`.
- [ ] Clock-off okumasının F4'te `0x00000000` mı döndüğü, yoksa farklı pattern
      mı, not edilir (H7'de GPIOA `0xABFFFFFF`, DMA1/LPUART1 `0x00000000` çıktı).
- [ ] Çoklu `-r32` sırası F4'te de güvenilir mi (parser zaten adres-çapalı).

### N6 bağlanınca (deneysel — Bölüm 3.4 fallback'i test edilir)
- [ ] Secured firmware çalışırken `mode=HOTPLUG` **bağlanıyor mu**? (H7'de
      sorunsuz; N6'da TrustZone/RIF nedeniyle "can't get core ID" riski var —
      `Backend.cpp resetN6TargetForCapture` notu.)
- [ ] Bağlanıyorsa RCC/GPIO okuması dönüyor mu, secure register'lar RAZ mı?
- [ ] Başarısızsa `mode=UR` + `-r32` reset+snapshot fallback'i fizibıl mi
      (`sn=` seçimi + 1.2 sn debounce, `resetN6TargetForCapture` deseni).
- [ ] N6 SVD'sindeki secure/non-secure alias (0x5.../0x4...) hangisi kullanılır.
- [ ] Sonuç ne olursa olsun bu dosyaya yazılır; N6 "deneysel" rozetini korur.

---

## 7. Faz 1'e Devredilen Somut Girdiler

- **Parser fixture'ları:** `docs/register_fixtures/*.txt` — hex-dump parser'ı
  bunlara karşı yazılıp test edilecek (Faz 2).
- **Format grameri:** Bölüm 4.1'deki regex.
- **Snapshot mimarisi doğrulandı:** 2 çağrı (RCC-first + peripheral zinciri),
  ~2.8 sn, adres-çapalı eşleme.
- **H7 `boards.json` girdisi için teyit:** device ID `0x483`, board adı
  `NUCLEO-H723ZG`, SVD hedefi `STM32H723`.
- **CLI çağrı deseni:** `-c port=SWD mode=HOTPLUG` + zincirli `-r32 <addr> <byte>`;
  `CliRunner` ile aynı sarmalayıcı (Bölüm 5.1).
