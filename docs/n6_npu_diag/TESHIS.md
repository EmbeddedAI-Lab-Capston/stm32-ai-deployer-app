# STM32N6 NPU Asılma Teşhisi — STM32 AI Deployer ile

**Tarih:** 2026-09-16 · **Kart:** NUCLEO-N657X0-Q (ST-Link `001A0027…`)
**Firmware:** `n6_ai_node` — `Template_FSBL_LRUN` + Neural-ART `mobilenet_v1_0.25_96`
**Araç:** STM32 AI Deployer, Değişken İzleyici (memread yolu, 200 Hz)

> Bu teşhis, projenin kendi aracıyla yapıldı. Ekran görüntüleri numaralı
> sırayla bu klasörde. Semptom: inference başlıyor ama hiç bitmiyor
> (`g_ai_status = 1`, `g_ai_infer_count = 0`), CPU `LL_Streng_Wait` içinde
> dönüyor.

---

## 1. Aracın kendi doğrulaması

| Ölçüm | Sonuç |
|---|---|
| ST-Link bağlantısı (memread) | `watchLinkState = open` |
| Örnekleme | **200.0 Hz, 0 kaçırılan, 0 okuma hatası** |
| Zaman kayması (skew) | 3.49 ms (11 ayrı blok okunuyor) |
| **ELF ↔ hedef eşleşmesi** | **`match`** |

ELF eşleşme detayı — **bu, kontrolün bizim üretmediğimiz bir firmware'de ilk
gerçek sınavıydı ve geçti:**

```
vtor             = 0x34000400
targetInitialSp  = 0x34200000   elfEstack        = 0x34200000   ✓
targetResetVec   = 0x34009da1   elfResetHandler  = 0x34009da0   ✓
```

---

## 2. NPU register okumaları (canlı, asılı hâldeyken)

ATON taban adresi `0x480E0000`, offsetler `ATON.h`'dan türetildi.

| Register | Adres | Değer |
|---|---|---|
| CLKCTRL.CTRL | `0x480E0000` | `0x00000001` |
| CLKCTRL.BGATES | `0x480E0010` | `0x00180201` |
| INTCTRL.INTREG | `0x480E1008` | `0x00000004` |
| EPOCHCTRL.CTRL | `0x480FE000` | `0x00000000` |

10 stream engine'in CTRL register'ı tarandı — **üçü RUNNING (bit 31):**

| Engine | CTRL | Çözümlenen bitler | ADDR | Nerede | POS |
|---|---|---|---|---|---|
| STRENG0 | `0x8008C101` | EN, LSBMODE, **RUNNING** | `0x342E6C00` | AXISRAM5 — aktivasyonlar | **36** |
| STRENG7 | `0x8008C185` | EN, SINGLE, CONT, LSBMODE, **RUNNING** | `0x71037160` | **harici flash — ağırlıklar** | **36** |
| STRENG9 | `0x80080009` | EN, DIR, **RUNNING** | `0x342E0000` | AXISRAM5 — aktivasyonlar | **36** |

Diğer yedi engine boşta (`CTRL = 0`).

### Kritik gözlem

**POS üç motorda da 36'da donmuş** ve 200 Hz'de ~1200 örnek boyunca hiç
değişmiyor (min = max = 36). Motorlar başlamış, 36 bayt ilerlemiş ve durmuş.

---

## 3. Yasadışı erişim var mı — RISAF12 (harici flash filtresi)

`RISAF12_BASE_S = 0x58011000` (CMSIS'ten hesaplandı).

| Register | Adres | Değer |
|---|---|---|
| CR | `0x58011000` | `0x00000000` |
| IASR (yasadışı erişim durumu) | `0x58011008` | `0x00000000` |
| IAESR | `0x58011020` | `0x00000000` |
| IADDR (yasadışı adres) | `0x58011024` | `0x00000000` |
| REG0.CFGR / START / END | `0x580110 40/44/48` | hepsi `0x00000000` |

**Hiçbir yasadışı erişim bayrağı yok.** Yani NPU'nun okuması *reddedilmiyor*.

> Not: RISAF12'nin tüm register'larının sıfır okunması iki şey anlamına
> gelebilir — filtre hiç yapılandırılmamış (ST'nin varsayılanı: yalnızca
> secure+privileged+CID=1 geçer, ki NPU tam öyle etiketli) **veya** register
> bloğu o an okunabilir değil. Ayırt edilmedi.

---

## 4. Değerlendirme

Elenen açıklamalar (hepsi ölçümle):

- ❌ Ağırlıklar erişilemiyor → firmware `0x71000000`'dan `0xE6DEE804` okuyor, `.raw` ile birebir
- ❌ NPU ölü/saatsiz → ATON register alanı okunuyor, CLKCTRL.CTRL = 1
- ❌ Yasadışı erişim reddi → RISAF12'de hiçbir bayrak yok
- ❌ NPU cache sorunu → cache kapalıyken de aynı
- ❌ RISAF açılmamış → açmak çözmüyor, üstüne debug erişimini öldürüyor

**En güçlü kalan işaret:** üç motorun da **aynı** noktada (POS=36) donması.
Farklı belleklere (AXISRAM5 ve harici flash) giden motorların tam aynı sayıda
bayt sonra durması, tek tek bellek erişim sorunlarıyla açıklanamaz; ortak bir
duruş sebebine (NPU fonksiyonel saati / stream switch yapılandırması /
bekleyen bir handshake) işaret eder.

`INTCTRL.INTREG = 0x4` (bit 2 set) temizlenmemiş bir kesme bayrağı gösteriyor
— henüz çözümlenmedi, sıradaki bakılacak yer.

---

## 5. Ekran görüntüleri

| Dosya | Ne gösteriyor |
|---|---|
| `01`, `02` | İzleme listesine NPU register'larının ve firmware sembollerinin eklenmesi |
| `03` | ST-Link bağlantısı açık, ELF eşleşmesi `match` |
| `04` | 200 Hz örnekleme çalışıyor, 0 hata |
| `05` | 10 stream engine'in CTRL taraması — üçü RUNNING |
| `06` | STRENG ADDR/POS — donmuş pozisyonlar |
| `07` | RISAF12 yasadışı erişim register'ları — temiz |
| `08`, `09` | Register Inspector (N6'da `error` döndü — firmware asılıyken CLI arka ucu bağlanamadı) |
| `10` | İzleyici son durum |

## 6. Aracın kendisi hakkında çıkan bulgular

1. **ELF↔hedef eşleşme kontrolü yabancı firmware'de çalışıyor** — ilk gerçek kanıt.
2. **memread yolu N6'da asılı firmware üzerinde de 200 Hz'de kusursuz okuyor.**
3. **Register Inspector N6'da bu senaryoda çalışmıyor** (`registerStage: error`).
   Firmware asılıyken CLI arka ucunun bağlanamaması — açık iş.
4. **DebugBridge `props` yanıtı uzun listeleri kırpıyor** — 26 kalemin yalnızca
   son 21'i döndü, ilk eklenenler kaybolmuş gibi göründü. Teşhis sırasında
   yanlış yola sürükledi; düzeltilmeli.
5. Değişken İzleyici **ham adresle peripheral izlemeyi** gerçek bir teşhiste
   taşıdı: NPU'nun SVD'si olmamasına rağmen 10 stream engine taranabildi.
