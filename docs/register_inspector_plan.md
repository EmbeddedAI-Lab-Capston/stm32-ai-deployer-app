# Register Inspector — Planlama Dokümanı

> **Durum:** Planlama tamamlandı, açık kararlar kullanıcı tarafından
> onaylandı (2026-07-05, bkz. Bölüm 7.1) — implementasyon başlamadı.
> **Amaç:** Bu doküman, özelliği implemente edecek modelin sırayla takip
> edeceği tek referanstır. Kod içermez; kararları ve gerekçelerini içerir.
> **Okuma sırası:** Önce bu doküman, sonra `CLAUDE.md` + `docs/PROJECT.md`
> (mimari kalıplar), N6 için `docs/n6_kaldigimiz_yer.md`.
> **Git:** Trunk-based, faz başına kısa ömürlü feature branch; commit'lere
> AI co-author/attribution satırı EKLENMEZ — bağlayıcı kurallar Bölüm 6
> sonunda ("Git iş akışı").
> **Donanım:** Şu an elde yalnızca H7 var; F4/N6 ileride bağlanacak
> (Bölüm 3.8 ve Faz 0'daki ertelenmiş kontrol listeleri).

---

## 1. Özet ve Ürün Değeri

### 1.1 Ne yapıyoruz?

Kart **çalışırken** (runtime), firmware'e tek satır kod eklemeden, STM32'nin
memory-mapped peripheral register'larını ST-Link/SWD üzerinden okuyup
**insan-okunur bir register tablosu (snapshot)** üreten bir debug aracı.
x86'daki register/flag table analojisi: "şu an DMA1 Stream0 EN=1 mi, USART1
RXNE takılı mı, RCC hangi clock'ları açmış, GPIO'lar hangi modda" — tek bakışta.

### 1.2 Neden değerli?

Gömülü geliştirmede bugünkü akış: bir DMA/SAI/UART sorununu anlamak için
firmware'e elle `printf("DMA_SxCR=%08lx")` benzeri kod ekleniyor, derleniyor,
flash'lanıyor, tek tek izleniyor. Bu döngü dakikalar sürüyor ve **kod eklemek
davranışı değiştirebiliyor** (timing, RAM). Register Inspector bu döngüyü
ortadan kaldırır:

- **Firmware'e dokunmadan** dışarıdan canlı görünürlük (STM32_Programmer_CLI
  zaten projede flash/probe/N6-reset için kullanılıyor — yeni araç bağımlılığı yok).
- Ham hex değil, **bit-field düzeyinde decode** (`DMA1_S0CR.EN=1`,
  `USART1_CR1.RXNEIE=0`) ve **reset değeriyle karşılaştırma** ("bu register
  reset'ten beri değişmiş → biri konfigüre etmiş").
- **İki snapshot arası diff**: "hata öncesi / hata sonrası ne değişti?" —
  debug'ın asıl sorusu.
- Projenin kendi geçmişinden somut örnek: N6'daki UART RX sorununda
  (`docs/n6_kaldigimiz_yer.md`) haftalarca firmware'e ISR/RDR yazdıran kod
  eklendi. Bu araç olsaydı `USART1->ISR` dışarıdan tek tıkla okunurdu.

### 1.3 Ürün konumu

Ana uygulamaya yeni bir üst sekme ("Register") olarak eklenir. Bitirme
projesi sunumunda "AI deploy aracı" hikâyesini "uçtan uca gömülü AI geliştirme
istasyonu" hikâyesine güçlendirir: deploy → izle → **debug et** → analiz et.

---

## 2. MVP Kapsamı ve Kapsam-Dışı

### 2.1 MVP'de VAR

| # | Özellik | Not |
|---|---------|-----|
| 1 | Tek tıkla register snapshot (kullanıcı tetikler) | `mode=HOTPLUG` — core **durmaz**, reset atılmaz |
| 2 | CMSIS-SVD tabanlı register tanımları | F4 + H7 + N6 SVD dosyaları uygulamayla dağıtılır |
| 3 | Peripheral seçimi (checkbox listesi + arama) | Varsayılan preset: RCC, GPIO, DMA, USART/UART/LPUART, I2C, SPI, SAI, EXTI |
| 4 | Bit-field decode + enum isimleri | SVD `fields` + `enumeratedValues` |
| 5 | Reset değeriyle karşılaştırma | "Reset'ten farklı" satırları vurgula |
| 6 | Snapshot A/B **diff** | Bellekte iki snapshot, değişen register/field vurgusu |
| 7 | Snapshot'ı JSON'a dışa aktarma | İleride LLM katmanının girdisi (Bölüm 9) |
| 8 | Yan-etkili register'ları otomatik atlama | SVD `readAction` + veri register'ı kara listesi |
| 9 | Clock'u kapalı peripheral'ı tespit edip atlama | Önce RCC okunur, enable bit'leri decode edilir |
| 10 | H7'de doğrulanmış çalışma (şu an eldeki tek kart) | F4/N6 tanımları 1. günden hazır; kartlar bağlanınca Faz 0 kontrol listeleriyle doğrulanır — N6 "deneysel" etiketiyle (Bölüm 3.4) |

### 2.2 MVP'de YOK (bilinçli kapsam-dışı)

| Özellik | Neden dışarıda | Mimari kapıyı kapatıyor mu? |
|---------|----------------|------------------------------|
| Sürekli canlı izleme / periyodik polling | Her snapshot 1 process + SWD connect (~1-3 sn); CLI ile canlı polling verimsiz. Kalıcı bağlantı (CubeProg C++ API / GDB server) ayrı iş | Hayır — `RegisterReader` arayüzü tek-atımlık; altına kalıcı-bağlantılı ikinci implementasyon eklenebilir |
| Register'a **yazma** | Tehlikeli + MVP değeri düşük; salt-okunur araç olarak konumlanıyor | Hayır — CLI `-w32` ile ileride eklenebilir |
| Snapshot'ları SQLite'a kaydetme | Aşama 5 (DB) henüz başlamadı; JSON export yeterli | Hayır — `AnalysisManager`'ın esnek `kind` modeline `"register"` kind'ı sonradan eklenebilir |
| LLM yorumlama katmanı | Açıkça gelecek işi (kullanıcı talebi) | Hayır — JSON export formatı LLM promptu olarak tasarlandı (Bölüm 9) |
| Halt → oku → devam ("atomik" snapshot) | HOTPLUG canlı okuma MVP için yeterli; halt'lı mod ayrı semantik | Hayır — `ReadPlan`'a mod parametresi eklenebilir |
| Core register'ları (PC, LR, xPSR, NVIC...) | CLI ile sistem-alanı okuması ayrı doğrulama ister; MVP peripheral odaklı | Kısmen — NVIC/SCB SVD'de var, aynı mekanizmayla ileride açılabilir |
| Custom (kullanıcı tanımlı) kartlar için SVD | Kullanıcı SVD dosyası göstererek ekleyebilsin — MVP'de sadece dosya-bazlı altyapı hazır, UI'sı yok | Hayır — `boards.json` + SVD klasörü zaten kullanıcı dosyası kabul eder |

---

## 3. Zorunlu Soruların Cevapları (Karar + Gerekçe)

### 3.1 Register tanım kaynağı → **CMSIS-SVD** ✅ KARAR

**Karar:** ARM'ın standart makine-okunur formatı **CMSIS-SVD** (System View
Description, XML) kullanılacak. ST her STM32 cihazı için resmi SVD yayınlar;
içinde her peripheral'ın base address'i, her register'ın offset/size/access/
**resetValue**/**readAction**'ı, her bit-field'ın offset/width'i ve
**enumeratedValues** (sembolik değer isimleri) vardır. Elle tanım yazmak
sürdürülemez; SVD tam olarak bu iş için var (debugger'ların "peripheral view"
ekranları da bunu kullanır).

**Kaynak:** [modm-io/cmsis-svd-stm32](https://github.com/modm-io/cmsis-svd-stm32)
deposu — ST sitesinden indirilen SVD'lerin **Apache-2.0 lisanslı**, N6 dahil
28 STM32 ailesini kapsayan aynası. Gereken dosyalar (kesin dosya adları
indirme sırasında teyit edilecek; hedef cihazlar template'lerdeki cihazlarla
eşleşmeli):

| Kart preset'i | Hedef cihaz (templates/) | SVD dosyası |
|---------------|--------------------------|-------------|
| STM32F4 | STM32F407VGTx | `STM32F407.svd` |
| STM32H7 | STM32H723ZGTx | `STM32H723.svd` |
| STM32N6 / NUCLEO-N657X0-Q | STM32N657X0 | `STM32N657.svd` (N6 klasöründeki varyant) |

**Dağıtım:** SVD dosyaları qrc'ye **gömülmez** (dosya başına 1-4 MB, exe ve
bellek şişer). `templates/` kalıbı aynen kopyalanır: repo kökünde `svd/`
klasörü + CMake post-build `copy_directory` ile exe yanına kopya;
çalışma zamanında `applicationDirPath() + "/svd"` (PipelineRunner emsali).

**Parser gereksinimleri (SVD spesifikasyonundan):**
- `<peripheral derivedFrom="...">` çözümü (ör. USART2, USART1'den türer —
  çözülmezse peripheral'ların çoğu boş kalır).
- `<dim>/<dimIncrement>/<dimIndex>` register dizileri (ör. DMA stream
  register'ları `S%sCR` kalıbıyla tanımlı olabilir).
- `readAction` özniteliği (`clear`/`set`/`modify`/`modifyExternal`) —
  yan-etkili register tespiti (Bölüm 3.2).
- `access` (`read-only`/`write-only`/...) — write-only register'lar okunmaz.
- `resetValue` / `resetMask` — karşılaştırma tabanı.

**KARARLAŞTIRILDI (kullanıcı onayladı, 2026-07-05):** SVD dosyaları **repoya
commit edilir** ve uygulamayla dağıtılır — Apache-2.0 modm aynasından alınacak,
`svd/LICENSE` ve kaynak URL notu eklenecek. Gerekçe: bitirme demosu internet
bağımsız olmalı. (ST'nin orijinal dağıtım koşulları execution sırasında hızlıca
teyit edilmeli; pratik risk düşük, modm aynası yıllardır herkese açık.)

### 3.2 Okuma mekaniği + halt problemi → **HOTPLUG: core DURMAZ** ✅ KARAR (Faz 0'da donanımda doğrulanacak)

**En kritik soru buydu; cevap aracın lehine:**

- `STM32_Programmer_CLI -c port=SWD mode=HOTPLUG [sn=<SN>] -r32 <addr> <byte-sayısı>`
  komutu hedefe **reset atmadan ve core'u durdurmadan (halt yok)** bağlanır ve
  belleği okur. ST dokümantasyonu HOTPLUG modunu tam olarak "uygulama
  çalışırken RAM/IP register'larını okumak" için tanımlar (UM2237). Okumalar
  ST-Link'in AHB-AP erişimiyle, CPU'ya paralel bus üzerinden yapılır — DMA
  transferi, UART akışı, timer'lar **çalışmaya devam eder**.
- **DBGMCU freeze bit'leri devreye girmez:** `DBGMCU_APBxFZR` bit'leri yalnızca
  core debug-halt'tayken peripheral'ları dondurur. Biz halt etmediğimiz için
  ilgisizdir; DBGMCU'ya dokunulmayacak.
- **Mevcut monitör akışıyla çakışmaz:** ST-Link'in VCP (UART) köprüsü ile SWD
  debug portu bağımsızdır; snapshot alınırken UART Monitor akmaya devam eder.
  (Projede zaten emsal var: N6'da port açıkken `-rst` gönderiliyor.)

**Bilinmesi gereken iki semantik sınır (planın parçası, UI'da da belirtilecek):**

1. **Snapshot atomik değildir.** Okumalar sıralı yapılır (~1-3 sn'ye yayılır);
   hızlı değişen bir sayaç register'ının iki okuması aynı ana ait olmaz.
   Debug değeri için bu kabul edilebilir (konfigürasyon/flag register'ları
   yavaş değişir); UI'da snapshot'a zaman damgası ve "canlı sistemden sıralı
   okuma" notu düşülür. Gerçek atomiklik isteyen "halt→oku→resume" modu
   gelecek işi (CLI'da `-halt` komutunun varlığı/davranışı **araştırılmalı**).
2. **Okumanın kendisi yan etki yaratabilir.** Bazı register'ları OKUMAK durum
   değiştirir (ör. `USART_RDR` okumak RXNE'yi düşürür, SPI/SAI veri
   register'ı FIFO'dan eleman çeker). Çözüm: okuma planı (**ReadPlan**)
   şunları **asla okumaz** ve aralıkları bunların etrafından bölerek üretir:
   - SVD'de `readAction` özniteliği olan register'lar,
   - `access=write-only` register'lar,
   - isim kara listesi (savunma katmanı — SVD eksik işaretlemiş olabilir):
     `DR`, `RDR`, `TXDR`, `RXDR`, `DATAR`, `FIFO` içeren veri register'ları.
   Atlanan register UI'da "yan etki nedeniyle atlandı" olarak gösterilir —
   sessizce kaybolmaz.

**Clock'u kapalı peripheral problemi:** Clock'u enable edilmemiş bir
peripheral'ın adresini okumak F4'te genelde 0 döner, H7/N6'da **bus fault /
CLI hatası** üretebilir ve zincirdeki sonraki okumaları düşürebilir. Çözüm
iki aşamalı snapshot: (1) önce yalnızca RCC bloğu okunur, enable register'ları
(`AHBxENR`/`APBxENR`...) decode edilir; (2) yalnızca clock'u açık
peripheral'lar okunur. Kapalı olanlar UI'da "clock kapalı" rozetiyle listelenir
— bu bilginin kendisi de değerli debug çıktısıdır ("SAI'yi açtım sanıyordum,
clock'u kapalıymış").

### 3.3 Kart farkları → **%100 veri-tabanlı, board-agnostik kod** ✅ KARAR

C++ tarafında **tek bir register adresi bile hardcode edilmez**. Kart başına
her şey iki veri dosyasından gelir:

1. **SVD dosyası** — tüm adres/offset/field bilgisi.
2. **`svd/boards.json`** — kart → SVD eşlemesi + karta özel meta:

   - eşleme anahtarları: board preset adı (`BoardInfo.name`) ve/veya probe
     `deviceId` (ör. `0x413`=F407, `0x483`=H72x/73x) — probe zaten
     `applyDetectedStLinkBoard()` ile deviceId topluyor;
   - RCC enable-bit decode ipucu: hangi peripheral'ın hangi RCC register/bit'ine
     bağlı olduğu SVD'den **türetilir** (RCC `xxENR` field adları peripheral
     adlarıyla eşleştirilir, ör. `GPIOAEN` → `GPIOA`); eşleşemeyen istisnalar
     için `boards.json`'da elle düzeltme alanı bulunur;
   - varsayılan peripheral preset listesi (kart bazında farklılaşabilir);
   - N6 için özel bayraklar (`"access": "experimental"`, secure alias notu).

**Yeni kart eklemek = `svd/` klasörüne SVD dosyası koy + `boards.json`'a bir
kayıt ekle.** Kullanıcının üst düzey genişletilebilirlik şartı birebir karşılanır.

Bu mekanizma F4/H7/N6'ya özel DEĞİLDİR: modm aynasında 28 STM32 ailesinin
tamamının SVD'si mevcut; yarın bambaşka bir aile (L4, G4, U5, WB...) aynı iki
dosyayla eklenir. C++ tarafında aile-özel hiçbir varsayım (RCC yerleşimi,
adres haritası, peripheral listesi) bulunmadığı için "çoğu STM32 kartına
uyumluluk" tasarımın doğal sonucudur. Execution sırasında uyulacak turnusol
testi: **"bu kod yeni bir aile için değişmek zorunda mı?" sorusunun cevabı
her sınıf için 'hayır' olmalı** — cevap 'evet' ise o bilgi SVD/boards.json'a
taşınmalıdır. (Custom kart UI'sı MVP dışı; dosya-bazlı yol 1. günden çalışır.)

### 3.4 N6 / TrustZone → **deneysel; canlı okuma garanti değil, fallback tanımlı** ✅ KARAR

Projenin kendi bulgusu (`Backend.cpp` içindeki NOT + `n6_kaldigimiz_yer.md`):
N6'da secure firmware çalışmaya başladıktan sonra **normal SWD connect "can't
get core ID" ile başarısız oluyor**; yalnızca `mode=UR` (under reset) bağlanıyor
— o da kartı **resetliyor**. HOTPLUG'ın secured-durumda çalışıp çalışmayacağı
belirsiz; RIF/TrustZone ve debug authentication secure bölge okumalarını
engelleyebilir (RAZ veya hata döner).

**Plan:**
- N6'da önce **HOTPLUG denenir** (Faz 0 spike'ında donanımda test edilecek —
  sonuç ne olursa olsun `docs/register_inspector_findings.md`'ye yazılacak).
- HOTPLUG başarısızsa UI, N6 için **"Reset + Snapshot" fallback**'i sunar:
  `mode=UR` ile bağlan, reset sonrası (firmware yeniden başlarken/başladıktan
  hemen sonra) register'ları oku. Bu **canlı state değildir** — DMA-ortası
  hatayı göstermez; ama boot/clock/pin konfigürasyon hatalarını (N6'nın asıl
  çile alanı) yakalamak için hâlâ değerlidir. UI'da açık uyarıyla verilir.
- Okunamayan (secure/erişim reddi) bloklar snapshot'ı düşürmez; register
  başına "okunamadı" durumu veri modelinde birinci sınıf vatandaştır.
- N6 SVD'sinde secure/non-secure alias adresleri varsa (0x5... / 0x4...),
  hangisinin kullanılacağı Faz 0'da netleştirilir (**araştırılmalı**).

**KARARLAŞTIRILDI (kullanıcı onayladı, 2026-07-05):** N6'da HOTPLUG
çalışmazsa "Reset + Snapshot" fallback'i **kabul edildi** — N6 bu özellikte
"deneysel" rozetiyle ve UI'da açık uyarıyla yer alır. Gerekçe: motivasyon
N6 debug acısından geliyor, kısmi görünürlük bile kazanç.

### 3.5 Performans → **tek process, toplu aralık okuması** ✅ KARAR

- Register başına process açmak yasak. Hedef: **snapshot başına 2 CLI
  çağrısı** — (1) RCC, (2) seçili + clock'u açık peripheral blokları.
- Bir çağrıda birden çok `-r32 <addr> <size>` komutu zincirlenir (UM2237
  komut zincirlemeyi destekliyor; **birden çok `-r32`'nin tek çağrıda
  davranışı Faz 0'da fiilen doğrulanacak** — çalışmazsa peripheral-grubu
  başına çağrıya bölünür, ör. 3-4 çağrı/snapshot; hâlâ kabul edilebilir).
- Okuma birimi SVD `addressBlock`'larıdır (peripheral başına tipik 0x400
  byte), yan-etkili register'ların etrafından bölünmüş **güvenli alt
  aralıklar** halinde. Varsayılan preset ~15-25 blok, toplam birkaç KB —
  SWD'de milisaniyeler; süreyi connect (~0.5-1.5 sn) domine eder. Hedef
  toplam: **≤ 3 sn** (F4'te ölçülecek).
- Çıktı ayrıştırma: `-r32` çıktısındaki her satır `0xADRES : W0 W1 ...`
  formatındadır; parser **satırdaki adres alanına çapalanır**, satır sırasına
  güvenmez (ST topluluğunda çoklu-adres okumada sıra karışması raporu var).
- CLI erişimi uygulama genelinde **serileştirilir**: snapshot, flash, probe ve
  N6-reset aynı anda ST-Link'e bağlanamaz — Backend'de ortak "programmer
  meşgul" kilidi (Bölüm 5.3).
- Gelecek (MVP dışı): sürekli izleme gerekirse kalıcı bağlantı —
  STM32CubeProgrammer C++ API (MSVC DLL'leri; MinGW ile linklemek sorunlu,
  bu yüzden MVP'de reddedildi) veya ST-LINK GDB server istemcisi.

### 3.6 Ham hex yetmez → **decode birinci sınıf gereksinim** ✅ KARAR

Her register satırı üç katmanda gösterilir:
1. **Ham:** adres + 32-bit hex değer.
2. **Field decode:** SVD field'larına göre `EN=1`, `DIR=0b01`, enum varsa
   sembolik ad (`DIR=MEMORY_TO_PERIPH`).
3. **Reset karşılaştırma:** `resetValue`(+`resetMask`) ile fark varsa satır
   vurgulanır; field düzeyinde "reset'te 0'dı, şimdi 1" gösterilir.

Bu decode mantığı **UI'dan bağımsız** bir sınıfta yaşar (Bölüm 5) — hem QML
hem JSON export hem gelecek LLM katmanı aynı decode'u kullanır.

### 3.7 Snapshot diff → **MVP'DE VAR** ✅ KARAR

Gerekçe: aracın asıl debug değeri tam da burada ("hata öncesi A, hata sonrası
B — ne değişti?"); veri modeli kurulduktan sonra diff, iki `QHash` karşılaştırması
kadar ucuz. Kapsam: bellekte **A/B iki slot** (kalıcılık yok), UI'da "yalnızca
değişenleri göster" filtresi, field düzeyinde eski→yeni gösterimi, diff'in
JSON export'a dahil edilmesi. Çok-snapshot'lı zaman çizelgesi MVP dışı.

### 3.8 MVP sınırı → **mekanik H7'de kanıtlanır (eldeki kart); F4/N6 bağlanınca doğrulanır** ✅ KARAR (2026-07-05 revize)

Motivasyon N6'dan geliyor ama N6 en riskli hedef (TrustZone). **Şu an elde
fiziksel olarak yalnızca H7 var; F4 ve N6 ileride bağlanacak.** Sıralama:
1. **H7** — birincil doğrulama kartı. Eldeki tek kart olması yanında teknik
   olarak da doğru seçim: clock-gating bus-fault davranışı ve çok-domain RCC
   gibi en katı durumlar H7'de — H7'de çalışan mekanik F4'te büyük olasılıkla
   doğrudan çalışır, tersi garanti değil.
2. **F4** — SVD + `boards.json` tanımları 1. günden repoda hazır; kart
   bağlandığında yalnızca Faz 0'ın F4 kontrol listesi koşulur, kod değişikliği
   beklenmez.
3. **N6** — kart bağlandığında **"deneysel"** rozetiyle (Bölüm 3.4 fallback).

**Donanım eksikliği geliştirmeyi bloklamaz:** SVD parser, okuma planı, decode,
Backend ve UI katmanları donanımsız geliştirilip Faz 0'da kaydedilen gerçek
CLI çıktısı üzerinden test edilir; yalnızca uçtan uca okuma doğrulaması H7'ye
(ve ileride F4/N6'ya) ihtiyaç duyar.

Peripheral kapsamı: SVD sayesinde *tüm* peripheral'lar teknik olarak
mevcuttur; MVP **varsayılan preset**'i motivasyondaki acı noktalara göre
seçer: `RCC, GPIO, DMA(+H7'de DMAMUX/MDMA), USART/UART/LPUART, I2C, SPI,
SAI, EXTI`. Kullanıcı listeden herhangi bir peripheral'ı ekleyip çıkarabilir.

### 3.9 UI → **yeni üst sekme "Register"** ✅ KARAR

Monitor'e gömmek yerine yeni sekme; gerekçe: Monitor UART-eksenli ve canlı
akış ekranı, Register Inspector SWD-eksenli ve tablo-yoğun — ekran alanı ve
zihinsel model ayrı. `Main.qml`'de `TopTabBar.tabs`'a 7. eleman + `StackLayout`'a
`RegisterScreen {}` eklemek mevcut kalıbın rutin uygulaması. Detay Bölüm 5.5.

---

## 4. Veri Modeli

### 4.1 SVD tarafı (tanımlar — salt okunur, kart seçilince yüklenir)

```
SvdDevice                     (bir SVD dosyasının tamamı)
 ├─ name, description, cpu bilgisi
 └─ peripherals: [SvdPeripheral]
     ├─ name ("DMA1"), groupName ("DMA"), description
     ├─ baseAddress (quint64), addressBlocks [(offset, size)]
     ├─ derivedFrom çözülmüş halde (parser sonrası self-contained)
     └─ registers: [SvdRegister]
         ├─ name ("S0CR"), description, addressOffset, size
         ├─ access (RW/RO/WO), readAction (varsa), resetValue, resetMask
         └─ fields: [SvdField]
             ├─ name ("EN"), bitOffset, bitWidth, access, description
             └─ enumeratedValues: [{value, name, description}]  (opsiyonel)
```

Tümü plain struct (`SvdModel.h`, QObject değil). Parser `derivedFrom` ve
`dim` çözümünü yükleme anında yapar; modelin geri kalanı bu detayları bilmez.

### 4.2 Okuma tarafı (çalışma zamanı değerleri)

```
ReadPlan                      (snapshot öncesi üretilir)
 └─ items: [{peripheralName, startAddr, byteCount}]   // güvenli alt aralıklar

RegisterSnapshot              (bir snapshot'ın ham sonucu)
 ├─ takenAt (QDateTime), boardName, deviceName, svdFile, connectMode
 ├─ values:  QHash<quint64 /*addr*/, quint32 /*value*/>
 ├─ skipped: QHash<quint64, SkipReason>   // SideEffect | WriteOnly | ClockOff
 └─ errors:  [{peripheralName, message}]  // blok bazlı okuma hataları

DecodedRegister               (SVD + snapshot birleşimi — UI/JSON/LLM girdisi)
 ├─ peripheral, register adı, addr, rawValue, resetValue, changedFromReset
 ├─ status: Ok | Skipped(reason) | Unreadable
 └─ fields: [{name, value, enumName?, resetFieldValue, changed}]

SnapshotDiff                  (A/B karşılaştırma)
 └─ entries: [{addr, register, oldValue, newValue, changedFields[...]}]
```

QML'e geçiş: Backend bu yapıları `QVariantList/QVariantMap` ağacına çevirir
(mevcut `records → QVariantList` kalıbı). Ham struct'lar QML'e sızmaz.

### 4.3 `svd/boards.json` (kart eşleme dosyası)

```json
{
  "boards": [
    {
      "match": { "names": ["STM32F4"], "deviceIds": ["0x413"] },
      "svd": "STM32F407.svd",
      "access": "stable",
      "defaultPeripherals": ["RCC","GPIOA","GPIOB","GPIOC","DMA1","DMA2",
                              "USART1","USART2","I2C1","SPI1","EXTI"],
      "rccOverrides": {}
    },
    { "match": { "names": ["STM32H7"], "deviceIds": ["0x483"] },
      "svd": "STM32H723.svd", "access": "stable", "...": "..." },
    { "match": { "names": ["STM32N6","NUCLEO-N657X0-Q"] },
      "svd": "STM32N657.svd", "access": "experimental",
      "notes": "TrustZone: HOTPLUG başarısızsa mode=UR fallback" }
  ]
}
```

(Şema örnektir; execution sırasında alan adları netleştirilir. Kritik ilke:
**karta özel her bilgi bu dosyada + SVD'de**, C++'ta değil.)

---

## 5. Bileşenler / Sınıflar ve Mevcut Mimariye Oturma

Yeni modül: `src/modules/registers/` — mevcut modül düzeninin (board/flash/
serial/analysis/simulation) kardeşi.

### 5.1 Yeni C++ sınıfları

| Sınıf | Dosya | Sorumluluk | Kullandığı mevcut kalıp |
|-------|-------|------------|-------------------------|
| `SvdModel` (struct'lar) | `SvdModel.h` | Bölüm 4.1 veri yapıları | `PipelineConfig.h` gibi salt-header struct |
| `SvdParser` | `SvdParser.h/.cpp` | SVD XML → `SvdDevice`; `derivedFrom`/`dim`/`readAction` çözümü | `QXmlStreamReader` (Qt-native, yeni bağımlılık yok) |
| `SvdCatalog` | `SvdCatalog.h/.cpp` | `svd/` klasörü + `boards.json` okuma; `BoardInfo` → doğru `SvdDevice`; parse edilmiş modelleri cache'ler; parse'ı **arka planda** çalıştırır | `applicationDirPath()/svd` (PipelineRunner'ın templates emsali); asenkron parse **`QtConcurrent::run` ile** (KARARLAŞTIRILDI — CMake'e `Qt6::Concurrent` eklenir; sonuç ana thread'e sinyalle döner) |
| `ReadPlanBuilder` | `ReadPlanBuilder.h/.cpp` | Seçili peripheral'lar + SVD → güvenli okuma aralıkları; RCC-gating filtresi; N6 fallback modu | Saf mantık, bağımlılığı yok — birim test edilebilir |
| `RegisterReader` | `RegisterReader.h/.cpp` | `ReadPlan` → CLI argümanları; çıktı hex-dump parser'ı; blok hatası toleransı | **`CliRunner` yeniden kullanılır** (`setCliPath` + `run(args)` + `outputLine`/`finished` sinyalleri — FlashManager'ın kullandığı aynı sarmalayıcı) |
| `RegisterInspector` | `RegisterInspector.h/.cpp` | Orkestratör (manager): SVD yükle → RCC oku → plan kur → oku → decode → snapshot A/B tut → diff üret → JSON export. Sinyaller: `busyChanged`, `progressChanged(stage)`, `snapshotReady`, `errorOccurred` | `FlashManager`/`PipelineRunner` manager kalıbı; `main.cpp`'de oluşturulup Backend'e verilir |

### 5.2 Mevcut dosyalarda değişiklik

| Dosya | Değişiklik |
|-------|-----------|
| `src/bridge/Backend.h/.cpp` | Yeni bölüm: register Q_PROPERTY'leri (`registerBusy`, `registerStage`, `registerModel`, `registerDiffModel`, `registerSupportLevel`) + Q_INVOKABLE'lar (`registerPeripheralList()`, `takeRegisterSnapshot(slot, peripherals)`, `computeRegisterDiff()`, `exportRegisterSnapshotJson(path)`, `clearRegisterSnapshots()`); `wireRegisters()` ile sinyal bağlama — QML **yalnızca** Backend'i görür (facade kuralı) |
| `src/main.cpp` | `RegisterInspector` oluştur (`&app` parent), Backend ctor'una geçir |
| `src/core/AppSettings.h/.cpp` | Yeni anahtarlar: `registers/last_peripherals` (kart adı → seçim, JSON), `registers/svd_dir` (opsiyonel override; boşsa exe yanı `svd/`) |
| `CMakeLists.txt` | Yeni kaynaklar; `Qt6::Concurrent` (tercihe göre); `svd/` için templates-benzeri post-build copy; `RegisterScreen.qml` + yeni component'ler `qt_add_qml_module`'a |
| `qml/Main.qml` | `TopTabBar.tabs`'a `{ label: "Register" }` + `StackLayout`'a `RegisterScreen {}` |
| `CLAUDE.md`, `docs/PROJECT.md` | Klasör yapısı, modül listesi, AppSettings anahtarları güncellemesi (CLAUDE.md'nin kendi güncelleme kuralı gereği) |

**Dokunulmayanlar:** `PacketParser` (UART protokolüyle sıfır ilişki),
`SerialManager/Worker`, `src/ui/*` ölü kod, `AnalysisManager` (MVP'de DB yok).

### 5.3 Eşzamanlılık ve kilitler

- ST-Link'e aynı anda tek istemci: `RegisterInspector` snapshot'a başlamadan
  Backend üzerinden `flashBusy || pipelineBusy || probeBusy` kontrolü yapılır;
  tersi yönde flash/probe başlarken `registerBusy` kontrol edilir. Basit ortak
  guard yeterli (ayrı mutex sınıfı icat edilmez); N6 `resetN6TargetForCapture`
  da bu guard'a dahil edilir.
- UART Monitor bağlantısı snapshot sırasında **kapatılmaz** (VCP bağımsız).
- Tüm CLI işlemleri zaten asenkron (`CliRunner` hiç `waitForFinished`
  çağırmaz); UI bloklanmaz. SVD parse arka planda; sonuç ana thread'e sinyalle
  döner (UI güncellemesi yalnızca ana thread kuralı).

### 5.4 Araç yolu

`STM32_Programmer_CLI` yolu mevcut zincirle bulunur: `AppSettings::programmerCliPath()`
→ boşsa `FlashManager::detectCliPath()` (Backend'deki emsal akış). Yeni araç
tespiti gerekmiyor.

### 5.5 QML tarafı

Yeni dosyalar:
- `qml/screens/RegisterScreen.qml` — düzen:
  - Üst şerit: `SectionHeader` ("Register Inspector" + kart adı),
    `StatusPill` (destek düzeyi: stable/experimental/desteklenmiyor),
    `AppButton`'lar: "Snapshot A", "Snapshot B", "Diff", "JSON Dışa Aktar".
  - Sol panel (`Card`): peripheral checkbox listesi + arama kutusu + preset
    ("Varsayılan", "Tümü", "Haberleşme/DMA"); seçim `AppSettings`'e yazılır.
  - Sağ panel (`Card`): register ağacı — peripheral başlığı (genişler/daralır)
    → register satırı (ad, adres, hex değer, reset-fark vurgusu, durum rozeti:
    atlandı/clock kapalı/okunamadı) → genişleyince field satırları (ad, değer,
    enum adı, değişim vurgusu). Diff modunda eski→yeni sütunu ve "yalnızca
    değişenler" filtresi.
  - Alt şerit: ilerleme/aşama metni (`backend.registerStage`), zaman damgaları.
- `qml/components/RegisterTree.qml` (yeni component) — genişleyebilir liste;
  mevcut `DataTable` düz tablo olduğu için ağaç görünümüne uymuyor; ama stil
  dili (satır yüksekliği, renkler) `DataTable`/`Theme`'den aynen alınır.

Kurallar: tüm renk/spacing `Theme.qml`'den; hex/monospace için
`Theme.monoFamily`; `backend`/`appState` erişimi mevcut ekranlardaki
`typeof backend !== "undefined"` koruma kalıbıyla; `MockData`'ya register
mock'u eklemek opsiyonel (tasarım önizlemesi için faydalı, zorunlu değil).

---

## 6. Aşamalı Uygulama Adımları (Execution Sırası)

> Her fazın sonunda derleme + elle doğrulama yapılır; faz atlanmaz.
> Faz 0 kod yazmadan önce ZORUNLUDUR — tüm plan onun bulgularına dayanır.

### Faz 0 — Donanım doğrulama spike'ı (kod yok, ~yarım gün)

> **Donanım durumu (2026-07-05):** Elde şu an yalnızca **H7** var; F4 ve N6
> ileride bağlanacak. Faz 0'ın zorunlu kısmı H7 üzerinde koşulur; F4/N6
> maddeleri "kart bağlanınca" kontrol listesi olarak ertelenir ve MVP'yi
> bloklamaz.

**H7 üzerinde ŞİMDİ koşulacak (zorunlu):** PowerShell'den elle testler,
sonuçlar `docs/register_inspector_findings.md`'ye yazılır:
1. Firmware UART akarken `-c port=SWD mode=HOTPLUG -r32 <RCC_base> 0x100`
   (H7 RCC base adresi SVD'den alınır) → UART akışı kesintisiz mi? Reset
   olmadı mı? (uptime sıfırlanmamalı)
2. Tek çağrıda çoklu okuma: `... -r32 <RCC> ... -r32 <GPIOA> 0x28
   -r32 <DMA1> 0xD0` → hepsi çıktıda mı, format nasıl, sıra güvenilir mi?
3. Clock'u KAPALI bir peripheral adresini oku → 0 mı dönüyor, hata mı?
   Hata ise zincirin devamı çalışıyor mu? (H7'de bus-fault riski en yüksek —
   tam da bu yüzden birincil test kartı.) D3 domain'den bir peripheral örneği
   de dahil edilsin.
4. `-r32` çıktı formatını birebir kaydet (parser bu fixture üstüne yazılacak);
   süre ölç (connect + N blok okuma).
5. CLI sürümünü kaydet (`v2.22.0` mevcut); çıktı formatı sürüme bağımlı
   olabilir — parser toleranslı yazılacak.

**Çıkış kriteri (MVP'yi açan):** 1, 2 (veya bölünmüş-çağrı alternatifi) ve 4
H7'de doğrulandı; 3 bulgusu plana işlendi (gerekirse Bölüm 3.2 revize).

**ERTELENMİŞ kontrol listeleri (kart bağlanınca koşulacak, findings'e eklenecek):**
- **F4 bağlanınca:** H7 madde 1-4'ün tekrarı (RCC base 0x40023800; kapalı-clock
  okumasının F4'te 0 dönüp dönmediği not edilir). Beklenti: kod değişikliği yok,
  yalnızca doğrulama.
- **N6 bağlanınca:** secured firmware çalışırken `mode=HOTPLUG` dene →
  bağlanıyor mu? Bağlanıyorsa RCC/GPIO okuması dönüyor mu? Başarısızsa
  `mode=UR` ile `-r32` (reset+snapshot fallback fizibilitesi). Sonuca göre
  Bölüm 3.4'teki fallback devreye girer (Faz 6).

### Faz 1 — SVD altyapısı
1. `svd/` klasörü: 3 SVD dosyası (modm-io aynasından) + `boards.json` +
   `LICENSE`/kaynak notu; CMake post-build copy.
2. `SvdModel.h`, `SvdParser`, `SvdCatalog` (asenkron yükleme + cache).
3. Doğrulama: uygulama açılışında (geçici debug logu) H7 SVD'si parse edilip
   peripheral/register sayıları ve bilinen değerler referans manuel (RM0468)
   ile karşılaştırılır (ör. GPIOA/RCC base adresleri, MODER offset 0x00,
   resetValue; DMA1 stream register'larının `dim` çözümü; USART'ların
   `derivedFrom` çözümü). F4/N6 SVD'leri de parse edilip yalnızca
   hatasız-yükleme + peripheral sayısı kontrol edilir (donanım gerekmez).

### Faz 2 — Okuma katmanı
1. `ReadPlanBuilder`: seçim + SvdDevice → güvenli aralıklar (readAction/WO/
   kara liste bölmesi); RCC-first akışı için "yalnızca RCC" planı.
2. `RegisterReader`: `CliRunner` ile çalıştırma; Faz 0'da kaydedilen gerçek
   çıktı üzerinde hex-dump parser'ı; adres-çapalı eşleme; blok hatası → 
   `errors`'a ekle, devam et; çoklu-`-r32` desteklenmiyorsa çağrı bölme.
3. RCC decode: SVD `xxENR` field adı → peripheral eşlemesi + `boards.json`
   override'ları.
4. Doğrulama: H7'de gerçek snapshot alınıp bilinen durumla karşılaştırma
   (ör. firmware'in kullandığı UART peripheral'ında `CR1.UE=1` görünmeli;
   kullanılmayan bir peripheral "clock kapalı" çıkmalı).

### Faz 3 — Orkestrasyon + Backend
1. `RegisterInspector`: durum makinesi (idle → svd-load → rcc-read →
   block-read → decode → ready), A/B slotları, hata yolları.
2. `Backend` genişletmesi (Bölüm 5.2), `main.cpp` kurulumu, AppSettings
   anahtarları, meşguliyet guard'ları (flash/probe/pipeline/N6-reset ile).
3. Doğrulama: QML olmadan Backend Q_INVOKABLE'ları geçici bir debug
   tetikleyicisiyle uçtan uca çalıştırılır.

### Faz 4 — QML ekranı
1. `RegisterScreen.qml` + `RegisterTree.qml` (Bölüm 5.5); `Main.qml` sekmesi.
2. Peripheral seçimi kalıcılığı; busy/progress; hata ve "atlandı/clock kapalı/
   okunamadı" rozetleri; reset-fark vurgusu.
3. Doğrulama: H7'de elle senaryo: firmware çalışırken snapshot →
   beklenen konfigürasyon görünüyor; UART Monitor kesintisiz.

### Faz 5 — Diff + JSON export
1. `SnapshotDiff` üretimi + UI diff modu ("yalnızca değişenler" filtresi).
2. JSON export (Bölüm 9 formatı); dosya diyaloğu mevcut ekranlardaki
   `QtQuick.Dialogs` kalıbıyla.
3. Doğrulama senaryosu: snapshot A → firmware'de bir aksiyon tetikle (ör.
   UART gönderimi başlat) → snapshot B → diff'te ilgili DMA/USART bit
   değişimleri görünmeli.

### Faz 6 — Dokümantasyon + ertelenmiş kart doğrulamaları
1. `CLAUDE.md` (klasör yapısı, AppSettings listesi, aşama tablosu) ve
   `docs/PROJECT.md` güncellemesi; `docs/register_inspector_findings.md`
   sonuçlarının plana geri işlenmesi. MVP burada biter (H7-doğrulanmış).
2. **F4 bağlandığında:** Faz 0'daki ertelenmiş F4 kontrol listesi koşulur;
   beklenmedik fark çıkarsa (olası değil) düzeltme yapılır.
3. **N6 bağlandığında:** ertelenmiş N6 spike'ı koşulur. HOTPLUG çalışıyorsa
   etkinleştir; değilse "Reset + Snapshot" fallback (`mode=UR`, `sn=` seçimi
   ve 1.2 sn debounce dahil `resetN6TargetForCapture` emsalindeki desenle) +
   UI uyarısı. Her iki sonuçta da N6 "deneysel" rozetini korur.

### Git iş akışı — trunk-based (TÜM fazlar için bağlayıcı)

- **Trunk-based geliştirme:** `main` her zaman derlenir ve çalışır durumda
  tutulur. Her faz için `main`'den **kısa ömürlü bir feature branch** açılır,
  fazın doğrulama adımı geçince `main`'e merge edilir ve branch silinir.
  Uzun ömürlü geliştirme dalı açılmaz; fazlar arası bekleyen iş biriktirilmez.
- **Branch adlandırma (öneri):**
  `feature/register-findings` (Faz 0 dokümanı), `feature/register-svd-infra`
  (Faz 1), `feature/register-reader` (Faz 2), `feature/register-backend`
  (Faz 3), `feature/register-ui` (Faz 4), `feature/register-diff-export`
  (Faz 5), `feature/register-docs` + kart-doğrulama işleri (Faz 6).
- Erken fazların merge'i kullanıcıya görünmez (UI sekmesi ancak Faz 4'te
  gelir) — bu, feature-flag ihtiyacı olmadan trunk-based çalışmayı mümkün kılar.
- **Commit mesajları** repo geleneğine uyar (Conventional Commits, mevcut
  geçmişteki gibi): `feat(registers): ...`, `docs(registers): ...`,
  `refactor(registers): ...`. Küçük, faz-içi mantıklı adımlar halinde commit.
- **KESİN KURAL — AI attribution yasağı:** Commit mesajlarına, PR
  açıklamalarına veya koda **hiçbir AI co-author / üretim notu EKLENMEZ**:
  `Co-Authored-By: Claude ...` satırı, "Generated with Claude Code" ve
  benzerleri **yasaktır**. Execution yapan model varsayılan olarak bu tür
  satırlar ekliyorsa bilinçli olarak çıkarmak zorundadır. Commit author'ı
  yalnızca repo'nun yapılandırılmış git kullanıcısıdır.

---

## 7. AÇIK KARARLAR ve RİSKLER

### 7.1 KARARLAŞTIRILAN AÇIK KARARLAR (kullanıcı onayı alındı — 2026-07-05)

Aşağıdaki dört karar planlama sırasında "açık" bırakılmış, kullanıcıya
sorulmuş ve **onaylanmıştır**. Execution bunları verili kabul etmelidir:

| # | Karar | Sonuç |
|---|-------|-------|
| A1 | SVD dağıtımı | ✅ **Repoya commit et** — `svd/` klasörü, modm aynasından (Apache-2.0), LICENSE + kaynak notu ile; demo internet bağımsız |
| A2 | N6 fallback | ✅ **Kabul** — HOTPLUG çalışmazsa "Reset + Snapshot" (`mode=UR`), "deneysel" rozeti + UI uyarısıyla |
| A3 | Sekme adı | ✅ **"Register"** — mevcut UI'daki İngilizce teknik sekme adlarıyla (Dashboard/Flash/Benchmark) tutarlı |
| A4 | Asenkron SVD parse | ✅ **`QtConcurrent::run`** — CMake'e `Qt6::Concurrent` eklenir; QThread+Worker kalıbı bu tek-atımlık iş için kullanılmaz |

### 7.2 RİSKLER ve azaltmaları

| Risk | Etki | Azaltma |
|------|------|---------|
| Çoklu `-r32` tek çağrıda desteklenmiyor/hatalı (toplulukta sıra-karışması raporu var) | Snapshot yavaşlar | Faz 0'da doğrula; adres-çapalı parser; gerekirse grup-başına çağrı (3-4 process/snapshot hâlâ kabul edilebilir) |
| Kapalı-clock peripheral okuması H7/N6'da bus fault ile zinciri düşürür | Eksik snapshot | RCC-first gating (tasarımda); blok hatası toleransı; Faz 0 testi |
| N6 TrustZone/RIF: HOTPLUG bağlanamaz veya secure register'lar RAZ | N6'da canlı okuma yok | A2 fallback'i; register-başına "okunamadı" durumu; beklenti yönetimi (deneysel rozet) |
| Yan-etkili register'ın SVD'de `readAction` ile işaretlenmemiş olması | Okuma canlı sistemi bozar (ör. FIFO boşalır) | İsim kara listesi savunma katmanı; varsayılan preset veri-register'ı içermeyen konfig/flag odaklı; dokümantasyonda açık uyarı |
| ST SVD'lerinde bilinen alan hataları/eksikler | Yanlış decode | Decode "advisory" konumlandırılır (ham hex her zaman görünür); hata bulunursa SVD dosyası yerinde düzeltilebilir (veri-tabanlı tasarımın avantajı) |
| Snapshot atomik değil (zamana yayılı) | Hızlı değişen register'larda tutarsız okuma algısı | UI'da açık semantik notu; konfig/flag register'ları odaklı kullanım; gelecekte halt'lı mod |
| CLI çıktı formatı sürümle değişebilir | Parser kırılır | Faz 0'da mevcut sürüm (2.22.0) çıktısı fixture olarak kaydedilir; parser toleranslı + format tanınmazsa anlamlı hata |
| ST-Link'e eşzamanlı erişim (flash/probe/N6-reset ile çakışma) | CLI hataları | Backend'de ortak meşguliyet guard'ı (Bölüm 5.3) |
| F4 ve N6 donanımı şu an elde yok (yalnızca H7) | F4/N6 uçtan uca doğrulaması gecikir | H7-öncelikli doğrulama (Bölüm 3.8); F4/N6 tanım dosyaları 1. günden repoda; ertelenmiş Faz 0 kontrol listeleri kartlar bağlanınca koşulur (Faz 6) — MVP'yi bloklamaz |
| Büyük SVD parse süresi (H7 birkaç MB) | UI donması | Arka planda parse + cache (SvdCatalog); kart seçiminde ön-yükleme |

---

## 8. Değerlendirilen ve Reddedilen Alternatifler (kayıt için)

| Alternatif | Neden reddedildi |
|------------|------------------|
| Register tanımlarını elle C++/JSON yazmak | Yüzlerce register × 3 kart; sürdürülemez; SVD zaten standart |
| Firmware'e register-dump komutu eklemek (UART üzerinden) | Tam da kaldırmak istediğimiz yöntem; firmware'e kod ekletiyor; N6'da komut-RX zaten çalışmıyor |
| STM32CubeProgrammer C++ API (DLL) ile kalıcı bağlantı | DLL'ler MSVC derlemeli, proje MinGW — linklenmesi sorunlu; CLI+QProcess mevcut kanıtlanmış kalıp | 
| OpenOCD / pyOCD / ST-LINK GDB server | Yeni araç bağımlılığı + dağıtım yükü; CLI zaten kurulu ve ToolDetector'da; gelecekte canlı-izleme gerekirse yeniden değerlendirilebilir |
| Okumayı `-r <addr> <size> dump.bin` ile dosyaya almak | stdout `-r32` daha basit ve mevcut CliRunner satır-akışına uyuyor; gerekirse Faz 0 bulgusuna göre değişebilir |

---

## 9. Gelecek LLM Katmanı İçin Bırakılan Kanca

MVP'de LLM yok; ama mimari şu üç kapıyı açık bırakır:

1. **JSON export formatı LLM-hazır tasarlanır** (self-describing): kart/cihaz
   kimliği, zaman damgası, her register için ad + açıklama (SVD `description`
   alanları dahil edilir!), ham değer, reset değeri, field bazında
   ad/değer/enum-adı/değişim bayrağı, atlanma/okunamama sebepleri ve varsa
   A/B diff bölümü. Bu dosya, "bu snapshot'ı yorumla: DMA neden akmıyor?"
   promptuna ek bağlam gerekmeden yapıştırılabilir olmalıdır.
2. **Decode mantığı UI'dan ayrı** (`RegisterInspector` + model sınıfları):
   ileride bir `RegisterAdvisor` sınıfı aynı `DecodedRegister`/`SnapshotDiff`
   yapılarını tüketip LLM API'sine gönderebilir; UI ve okuma katmanı değişmez.
3. **SVD `description` alanları parse'ta atılmaz** (bellek maliyeti kabul) —
   LLM'e "bu register ne işe yarar" bağlamını vermek için gereklidir; UI'da
   da tooltip olarak kullanılabilir.

LLM katmanı geldiğinde muhtemel akış: snapshot/diff JSON → sistem promptu
("STM32 uzmanısın; beklenen/reset değerleri ve field açıklamaları ekte") →
"olası kök nedenler + kontrol önerileri" çıktısı → Register ekranında panel.
Bu akış için bugünden yapılması gereken TEK şey 1-3'e uymaktır.

---

## 10. Kaynaklar

- CMSIS-SVD spesifikasyonu: https://arm-software.github.io/CMSIS_5/SVD/html/index.html
- STM32 SVD aynası (Apache-2.0, N6 dahil): https://github.com/modm-io/cmsis-svd-stm32
- STM32CubeProgrammer kullanım kılavuzu (UM2237, bağlantı modları + CLI komutları):
  https://www.st.com/resource/en/user_manual/um2237-stm32cubeprogrammer-software-description-stmicroelectronics.pdf
- HOTPLUG ile reset'siz/halt'sız okuma (ST topluluğu):
  https://community.st.com/t5/stm32cubeprogrammer-mcus/how-can-i-use-the-stm32cubeprogrammer-windows-executable-to-read/td-p/143253
  ve https://community.st.com/t5/stm32cubeprogrammer-mcus/stm32-programmer-cli-read-without-reconnecting/td-p/140442
- Proje içi: `docs/PROJECT.md` (mimari), `docs/n6_kaldigimiz_yer.md` (N6 debug
  erişim kısıtları), `src/bridge/Backend.cpp` (`resetN6TargetForCapture` —
  `mode=UR` gerekçe notu).
