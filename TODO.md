# TODO — STM32 AI Deployer

Bu dosya `CLAUDE.md`'nin klasör yapısı bölümünde adı geçiyordu ama dosyanın
kendisi yoktu — yeni bir bilgisayarda "nerede kaldık" sorusuna hızlı cevap
vermek için 2026-08-09'da oluşturuldu. Güncel tutulması gereken tek yer
burası; ayrıntılı gerekçe/kanıt için ilgili `docs/*.md` dosyalarına bakın.

---

## Build / test komutları

Tam komutlar ve güncel Qt yolu için [`CLAUDE.md`](CLAUDE.md) → "Sık Kullanılan
Komutlar" bölümüne bakın. Kısaca:

```powershell
# <QT_ROOT> kendi Qt kurulum kökünüz (örn. C:\Qt veya D:\Qt)
$env:PATH = "<QT_ROOT>\Tools\CMake_64\bin;<QT_ROOT>\Tools\mingw1310_64\bin;$env:PATH"
cmake -B build -S . -DCMAKE_PREFIX_PATH="<QT_ROOT>/6.11.x/mingw_64" -DCMAKE_BUILD_TYPE=Release -G "MinGW Makefiles"
cmake --build build -j
ctest --test-dir build --output-on-failure
```

**Tuzak:** `STM32AiDeployer.exe` çalışıyorsa link adımı "Permission denied"
verir — önce Görev Yöneticisi'nden kapatın.

---

## Dal durumu (2026-08-09 itibarıyla)

- `main` ve `feature/register-inspector` ikisi de origin'e pushlandı, senkron.
- **`feature/register-inspector` main'e merge EDİLMEDİ** — bilinçli, bkz.
  proje kuralı: tek uzun ömürlü özellik dalı, tüm fazlar birikir, iş bitmeden
  main'e merge yok. Aşağıdaki "Kalan işler" bitmeden merge etmeyin.
- Aktif geliştirme `feature/register-inspector` üzerinde devam etmeli.

---

## Şu an nerede kaldık

**Register Inspector + Değişken İzleyici (Faz 1–9) uygulandı ve bağımsız
denetimden geçti.** Denetim raporu: [`docs/variable_watcher_review.md`](docs/variable_watcher_review.md).

Denetimde bulunan **5 kritik hata** düzeltildi ve gerçek NUCLEO-H723ZG
donanımında doğrulandı:
- `role` alanı sessizce düşürülüyordu → kural motorunun 3/4 kuralı hiç
  eşleşemiyordu, profil karşılaştırma boş dönüyordu.
- Olay ekseni ile örnek ekseni iki ayrı saatti (61 ms kayma) → olay
  korelasyon kapısı hiç açılmıyordu.
- Başarısız okumalar gerçek `0.0` olarak kaydediliyordu → yanlış alarm riski.
- **ST-Link, bir izleme oturumundan sonra kilitleniyordu** (gdbserver `-e`
  persistent modu + Windows'ta zorla öldürme kombinasyonu) — kök nedeni
  bulundu ve düzeltildi, canlı 3 arka arkaya oturumla doğrulandı.
- Uygulamanın kapanış yolunda gerçek bir segfault (`QProcess` null deref).

Ayrıca ~14 regresyon testi eklendi, mutasyon testiyle doğrulandı, 60 sn/
1000 Hz dayanıklılık testi geçti (RSS sabit 12.4 MB).

**Sonuç: özellik teslim edilebilir durumda.** Aşağıdakiler bloklayıcı değil,
ama jüri/demo öncesi yapılması önerilir.

**2026-09-07 — Doğrulama ekosistemi kuruldu (Faz 1-3 tamamlandı).** UI'ı elle
tıklayıp ekran görüntüsü almak yerine artık `--debug-bridge` ile açılan bir
named-pipe kanalı (`DebugBridge`) ve `tools/uiprobe.ps1` sürücüsü var —
`dump`/`props`/`navigate`/`click` ile ekran durumu metin olarak sorgulanabiliyor,
`shot` ile tek komutla ekran görüntüsü alınabiliyor. Watch ekranı ve sekme
çubuğu `objectName` ile isimlendirildi. Kullanım ve tasarım:
[`docs/verification_ecosystem_plan.md`](docs/verification_ecosystem_plan.md).
Faz 4 (QML birim testleri) opsiyonel/düşük öncelik olarak bekliyor.

---

## Faz 10: bellek-öncelikli telemetri — 6 alt fazın 6'sı da uygulandı, F4 VE H7'de canlı doğrulandı (2026-09-14)

**Tam plan:** [`docs/memory_telemetry_plan.md`](docs/memory_telemetry_plan.md)
— kendi kendine yeterli, adım adım, üç kart için (F4/H7/N6).

Altı alt faz: sensör ham değerlerini bellekten okuma (seqlock ile) → RAM
bütçesi → hız tutarlılık kontrolü → "model sığar mı" ön kontrolü → canlı
peripheral/register izleme → otomatik çok-modelli süpürme. **Hepsi
uygulandı, testler yeşil, hem F4'te (2026-09-09) hem H7'de (2026-09-14,
gerçek BME280 ile) canlı doğrulandı — Faz 10.1'in UART-vs-bellek çapraz
doğrulaması dahil.** N6 flash sorunu 2026-09-14'te **çözüldü** — kalıcı bir
ST-Link kısıtı değil, BOOT jumper pozisyonuymuş; bkz. aşağıdaki "N6 çözüldü"
bölümü ve [`docs/n6_kaldigimiz_yer.md`](docs/n6_kaldigimiz_yer.md).

**H7'de BME280 kurulumunda yaşanan gerçek donanım sorunu (2026-09-14,
çözüldü):** BME280 H7'ye ilk takıldığında ne sensör okuması ne de UART
çalıştı (firmware SWD ile canlı doğrulandı — `main.c`'deki sensör-hata
döngüsünde normal şekilde dönüyordu, çökme yoktu; COM3 ham OS seviyesinde
0 bayt veriyordu). Sırasıyla denenip işe yaramayanlar: CSB'yi VCC'ye
bağlamak, SDA/SCL'i yer değiştirmek. **Çözüm: ST-Link USB kablosunun
sökülüp takılması** — sonrasında hem UART hem I2C anında çalışmaya
başladı. Kök neden muhtemelen USB/VCP sürücü tarafında takılı kalmış bir
durumdu, kalıcı bir kablolama sorunu değildi. **Ders:** H7'de UART/I2C
tamamen sessiz kalırsa (ne hata ne veri) ve firmware SWD ile canlı+doğru
çalıştığı doğrulanmışsa, önce USB'yi söküp takmayı dene — kablolamaya
dokunmadan önce.

**Bu fazın çıkış noktası:** F4'ün ST-Link VCP'si hedef USART'a köprülü
değil (harici adaptör kullanılmayacak — kullanıcı kararı), yani F4'te
UART hiç yok. Ama projenin zaten asıl iddiası SWD üzerinden **canlı
bellek okumak**; sensör verisini de oradan okuyunca F4 tam işlevsel hale
geliyor ve UART bir bağımlılık olmaktan çıkıp (H7'de) çapraz doğrulama
aracına dönüşüyor.

### Faz 10.1 — durum: TAMAMLANDI (2026-09-09 F4, 2026-09-14 H7)

**Kod tamamlandı, testler yeşil, hem F4 hem H7'de canlı doğrulandı. N6'nın
flash sorunu 2026-09-14'te çözüldü (BOOT jumper).**

- `templates/ai_glue/telemetry.h/.c` (seqlock korumalı `TelemetryBlock`)
  eklendi; üç kartın da `main.c`+`Makefile`'ı güncellendi (N6'daki sentetik-
  veri yedek yoluna da `sensor_ok` doğru yansıtılıyor).
- Host: `WatchItem::guardBeginAddr/guardEndAddr`, `WatchPlanBuilder` guard'ı
  aynı bloğa dahil edip `guardSlots` üretiyor, `WatchSampler::decodeSample()`
  seqlock kapısını uyguluyor, `WatchPresetMatcher` `offset_bytes` +
  `guardBegin`/`guardEnd` JSON alanlarını çözüyor, `Backend::applyWatchPresets()`
  guard adreslerini yeni kaleme aktarıyor (`VariableWatcher::updateItem`'a
  `guardBeginAddr`/`guardEndAddr` prop desteği eklendi — bu olmadan preset'ten
  gelen guard bilgisi sessizce kayboluyordu, canlı testte yakalandı).
  6 yeni birim testi (`TestWatchSampler` x3, `TestWatchPlanBuilder` x1,
  `TestWatchPresetMatcher` x2), hepsi + mevcut tüm testler yeşil.
- `watch/watch_presets.json`'a `sensor_memory` preseti eklendi
  (`sensor0/1/2`, `memInferUs`, `memInferCount`) — offset'ler (4/8/12/44/48,
  guard 0/88) **tahmin edilmedi**, H7/F4/N6 ELF'lerinin üçünde de
  `gdb ptype /o TelemetryBlock` ile ayrı ayrı doğrulandı (üçünde de aynı,
  padding yok).
- **F4: tam canlı doğrulandı** (UART yok, sadece SWD). Gerçek BME280 değerleri
  okundu (sıcaklık/nem/basınç fiziksel olarak makul, küçük doğal jitter
  dışında kararlı) ve `memInferCount`/`memInferUs` mevcut `g_ai_infer_count`/
  `g_ai_last_inference_us` ile **birebir eşleşti** — iki bağımsız SWD okuma
  yolunun aynı sonucu vermesi, seqlock decode'unun doğruluğunun kanıtı.
  Ekran görüntüsü + ham JSON: `out/sensor_memory_e2e/04_f4_*`.
- **H7: tam canlı doğrulandı (2026-09-14), UART çapraz doğrulaması dahil.**
  BME280 H7'ye takıldıktan ve donanım sorunu (yukarı bakın) çözüldükten
  sonra: ELF eşleşti, presetler uygulandı, `sensor0/1/2` gerçek değerler
  verdi (sıcaklık ~24.3-24.5°C, nem ~%36-37, basınç ~1005.0-1005.2 hPa) VE
  **UART'taki `§ sensor` paketiyle aynı anda karşılaştırıldı** — ikisi de
  aynı büyüklükte (UART: `values=[25000, 35638, 1004971]` milli-birim ↔
  İzleyici: `24.4 / 36.0 / 1005.06`). `memInferCount`/`memInferUs`
  `g_ai_infer_count`/`g_ai_last_inference_us` ile birebir eşleşti (ör.
  `9619` / `0.944 ms` iki yoldan da aynı). Ekran görüntüleri:
  `out/sensor_memory_e2e/01_h7_presetler_uygulandi.png`,
  `02_h7_canli_sensor_degerleri.png`,
  `03_h7_uart_vs_bellek_capraz_dogrulama.png`.
- **N6: derleme başarılı** (telemetry.c Cortex-M55'te de sorunsuz derleniyor,
  offset'ler aynı). O gün "flash başarısız" (`STM32_Programmer_CLI karta
  baglanamadi`) diye kapatılmıştı; **2026-09-14'te sebebi bulundu ve aşıldı** —
  kart flash-boot jumper pozisyonundaydı, o modda ROM'un güvenli boot'u debug
  bellek erişimini kapatıyor. Geliştirme boot pozisyonunda hem yükleme hem canlı
  okuma çalışıyor.
- **Yol boyunca bulunan, Faz 10.1 kapsamı dışı bir UI hatası:**
  `qml/components/watch/WatchToolbar.qml`'de ELF durum etiketi
  `backend.watchElfPath().length > 0 ? ... : "ELF yüklenmedi"` şeklinde bir
  **metot çağrısına** bağlı — QML'in binding motoru bunu değişiklik için
  izleyemiyor, yani ELF yüklendikten sonra bile etiket "ELF yüklenmedi"
  göstermeye devam ediyor (arka planda gerçek durum doğru, sadece bu etiket
  bayat). Düzeltme `watchElfPath`'i NOTIFY'lı bir `Q_PROPERTY`ye çevirmeyi
  gerektiriyor — bilinçli olarak bu oturumda yapılmadı (kapsam dışı), ama
  gerçek ve düşük riskli bir düzeltme.
- **Kalan:** N6 tarafı artık bloklu değil; kalan iş RAM-boot yolunu uygulamaya
  bağlamak (aşağıdaki "N6 çözüldü" bölümü).

### Faz 10.2 — RAM bütçesi görselleştirmesi: TAMAMLANDI (2026-09-09 F4, 2026-09-14 H7)

`src/modules/watcher/RamBudget.h/.cpp` (saf, 6 birim testi) + `Backend::ramBudget`
(canlı `Q_PROPERTY`, `NOTIFY ramBudgetChanged` — `watchItems`/`watchRateInfo`
gibi diğer canlı property'lerle aynı desen) + `qml/components/watch/RamBudgetBar.qml`
(`objectName: "watch.ramBudget"`, `WatchLinkStatus`'ın üstünde). Sembol
ADRESİ (`_end`/`_estack`) ile izlenen kalemin DEĞERİ (`heapEnd`/
`stackWatermark`) birbirine karıştırılmıyor — plan §4.1'in asıl endişesi.
**F4'te canlı doğrulandı:** gerçek "%35.8 (68.7 KB / 192.0 KB)" gösterdi,
sayılar elle çapraz kontrol edildi (staticUsed+heapUsed+stackUsed+freeBytes
== ramTotal). `heapEnd` bu firmware'de hiç yok (malloc kullanılmıyor) —
`heapTop=0` doğru davranış, hata değil. Ekran görüntüsü:
`out/ram_budget_e2e/01_f4_bar.png`.
**H7'de de canlı doğrulandı (2026-09-14):** gerçek "%88.7 (908.3 KB /
1024.0 KB)" — sayılar yine self-consistent (928856+0+1208+118512=1048576
== ramTotal tam). H7'nin çok daha büyük statik ayak izi (~907 KB, X-CUBE-AI
1D CNN ağırlıkları+aktivasyonları F4'ün MLP'sinden çok daha büyük) doğru
şekilde yansıdı. Ekran görüntüsü: `out/ram_budget_e2e/02_h7_bar.png`.
**Bulunan pre-existing hata (kapsam dışı, düzeltilmedi):**
`qml/components/watch/WatchToolbar.qml`'deki "ELF yüklenmedi" etiketi
`backend.watchElfPath()` bir METOD çağrısına bağlı — QML binding motoru
bunu izleyemiyor, ELF yüklendikten sonra bile etiket bayat kalıyor. Düzeltme
`watchElfPath`'i NOTIFY'lı `Q_PROPERTY`ye çevirmek (aynı desen bu fazda
`ramBudget` için zaten uygulandı) — küçük ve düşük riskli ama bu fazın
kapsamı dışında bırakıldı.

### Faz 10.3 — Hız tutarlılık kontrolü: TAMAMLANDI (2026-09-09 F4, 2026-09-14 H7)

`src/modules/watcher/RateCheck.h/.cpp` (saf, 5 birim testi) +
`Backend::inferenceRateCheck(windowSec)` (`Q_INVOKABLE` — plan gereği canlı
property değil, çağıran pencereyi kendi seçiyor) + `WatchLinkStatus.qml`'de
3 sn'de bir `Timer` ile çağrılan `objectName: "watch.rateCheckBadge"` rozeti.
Rozet **"doğrulandı" DEMİYOR** — "tutarlı"/"TUTARSIZ" diyor (dürüstlük
kuralı, plan §5.1). **F4'te canlı doğrulandı:** "22.7 Hz gözlendi · beyan
125 µs ile tutarlı" (teorik üst sınır 8000 Hz) — gerçek sayılar, gerçek
firmware. Ekran görüntüsü: `out/rate_check_e2e/01_f4_tutarli.png`.
**H7'de de canlı doğrulandı (2026-09-14):** "21.1 Hz gözlendi · beyan 943 µs
ile tutarlı" (teorik üst sınır ~1060 Hz — H7'nin 1D CNN modeli F4'ün
MLP'sinden ~7x daha yavaş inference veriyor, doğru şekilde daha düşük bir
üst sınıra yansıdı). Ekran görüntüsü: `out/rate_check_e2e/02_h7_tutarli.png`.
**Tutarsız durum canlı denenmedi** (firmware'in kendi beyanını yalanlaması
gerekirdi, ki mevcut firmware doğru rapor veriyor) — pure fonksiyonun kendisi
birim testiyle (`observedRateExceedingTheoreticalMaxIsInconsistent`) hem
tutarlı hem tutarsız durumu kapsıyor.

### Faz 10.5 — "Bu model bu karta sığar mı" ön kontrolü: TAMAMLANDI (2026-09-09 F4, 2026-09-14 H7)

`XCubeAIRunner::parseAnalyzeOutput()` (saf, gerçek H7+F4 `stedgeai analyze`
çıktılarıyla test edildi — "Complexity report" bölümündeki eşittir-biçimli
("macc=2,528") yanıltıcı ikinci bir sayıyı ayırt etmek için ":" biçimine
kilitleniyor) + `checkModelFitsBoard()` (`src/modules/flash/ModelFitCheck.h/.cpp`,
saf, 3 birim testi) + `PipelineRunner`'a yeni `warningLine` sinyali
(pipelineLines'a `type:"warn"` düşüyor, `type:"err"`'den ayrı — uyarı asla
pipeline'ı durdurmuyor). **Sığan durum F4'te canlı doğrulandı**
(`anomaly_mlp_int8`: weights=2,696 B, activations=716 B — hiç uyarı
çıkmadı, `pipelineLines` erken-anlık görüntüyle doğrulandı çünkü DebugBridge
listeleri son 20 satıra kırpıyor). **H7'de de canlı doğrulandı
(2026-09-14):** `anomaly_cnn_int8` (weights=6,856 B, activations=6,592 B,
H7'nin 1024 KB RAM/2048 KB Flash'ına karşı) — yine hiç uyarı çıkmadı, aynı
erken-anlık görüntü yöntemiyle doğrulandı. Ekran görüntüsü:
`out/fit_check_e2e/02_h7_uyari_yok.png`. **Sığmayan durum canlı DENENEMEDİ:**
depodaki hiçbir gerçek `.tflite` modeli (kws_dnn/kws_dscnn/kws_tcresnet/
wisdm_mlp dahil, hepsi `stedgeai analyze` ile bizzat ölçüldü) F4'ün bile
kapasitesini aşmıyor — plan "sığıyorsa başka bir model dene, uyarıyı
zorlama" diyor, zorlanmadı. Eşik/uyarı mantığının kendisi
`TestModelFitCheck.cpp`'de gerçek model sayılarıyla (kws_dnn'in ölçülen
261.424 B/12.824 B ayak izi) ama sentetik (gerçek olmayan, açıkça
etiketlenmiş) bir "tiny test fixture" kart tanımına karşı doğrulandı.

### Faz 10.4 — Canlı peripheral/register izleme: TAMAMLANDI (2026-09-09 F4/H7 kısmi, 2026-09-14 H7 tam)

`RegisterInspector::registersOfPeripheral()` (SVD'den register listesi —
adres, access, `readAction`, `hasReadSideEffect`, açıklama, best-effort
`clockKnown`/`clockEnabled`) + üç yeni `Backend` metodu
(`watchPeripheralList`/`watchRegistersOf`/`addWatchRegister`) +
`qml/dialogs/RegisterPickerDialog.qml` (`watch.registerPickerDialog`,
peripheral arama + register listesi, `⚠` ikonu + "Yine de Ekle" onay
bandı) + `WatchToolbar.qml`'e `watch.addRegisterButton`. **Gözlemci
ilkesi kod düzeyinde zorunlu:** `hasReadSideEffect` (register-level
`readAction` VEYA herhangi bir field'ın `readAction`'ı VEYA `write-only`
access) true olan bir register, `acknowledgeReadAction=false` iken
`Backend::addWatchRegister()` tarafından **eklenmeden reddediliyor** —
canlı doğrulandı (`addWatchRegister("I2C1","ICR",false)` → `""` döndü,
`watchItems` boş kaldı; `true` ile → gerçekten eklendi, doğru adres
`0x4000541c`).

**Canlı doğrulama (H7, sensör bağlı değilken, 2026-09-09):** `I2C1.CR1` =
gerçek `0x00000001` (PE biti seti — firmware I2C1'i gerçekten açmış),
`DMA1.S0NDTR` = gerçek `0x00000000` (DMA hiç kullanılmıyor — aşağıya bkz.),
200.4 Hz / 0 kaçırılan. Ekran görüntüsü:
`out/live_register_e2e/03_h7_canli_register_degerleri.png`.

**Canlı doğrulama (H7, BME280 gerçekten okurken, 2026-09-14):** `I2C1.ISR`
bu sefer gerçek bus aktivitesi yakaladı — min=`0x00000001` (boşta),
max=`0x00008001` (BUSY biti anlık set), grafikte periyodik "sivri uçlar"
olarak görünür oldu (her I2C transaction'ında bir tepe). `I2C1.ICR`
(write-only, readAction) yine onaysız reddedildi, onaylı doğru adrese
(`0x4000541c`) eklendi. Ekran görüntüsü:
`out/live_register_e2e/05_h7_canli_register_ve_sensor.png`.

**Canlı doğrulama (F4, gerçek BME280 aktifken):** `I2C1.SR1` min/max
`0x00/0x40` (durum bitleri gerçekten değişiyor), **`I2C1.DR` min=`0x2f`
max=`0xed` ort=`0x7a`** — gerçek I2C veri hattındaki BME280 baytları canlı
görüldü, plandaki DMA örneğinden çok daha ikna edici bir kanıt. Ekran
görüntüsü: `out/live_register_e2e/04_f4_canli_grafik.png`.

**N6:** SVD tarama/register listeleme çalışıyor (canlı bağlantı
gerektirmiyor), ama Watch bağlantısı "Error in initializing ST-LINK
device" ile başarısız oldu. **2026-09-14'te gerçek sebebi bulundu:**
genel bir ST-Link kısıtı değil — `ST-LINK_gdbserver` N6 çekirdeğini
durduramıyor (aynı gdbserver H7'de çalışıyor, STM32_Programmer_CLI ise
aynı N6 çekirdeğini durdurabiliyor). Register Inspector'ın CLI arka ucu
N6'da çalışır durumda; bloklu olan yalnızca gdbserver gerektiren
Değişken İzleyici. Detay: `docs/n6_kaldigimiz_yer.md` §9.

**Gerçek bulgu — plan'ın varsayımı yanlış çıktı, doğrulanmadan
yazılmadı:** F4'ün SVD'sinde I2C register'larının HİÇBİRİNDE
`readAction` yok (I2C3'ten `derivedFrom` — ham XML'de doğrulandı), yani
plan'ın "I2C1.ISR reddedilmeli" örneği F4'te ÇALIŞMIYOR. H7'de de `ISR`
değil `ICR` (write-only) reddediliyor. Board-özel farklılık gerçekten var
— plan'ın kendi uyarısı ("tahmin etme, doğrula") haklı çıktı.

**Kapsam sınırlaması (bilinçli, plan §6.3.5'in kendi izniyle):**
`watch_presets.json`'a "AI veri yolu" register hazır-grubu **eklenmedi** —
bu, `WatchPresetMatcher`'ın ELF sembol çözümleme modelini SVD register
çözümlemesine de genişletmeyi gerektirir (ayrı bir alt-sistem). Manuel
seçim (`RegisterPickerDialog`) zaten tam işlevsel ve bu fazın güvenlik
kritik gereksinimini karşılıyor; preset kolaylığı ileride ayrı bir iş
olarak eklenebilir.

**Yeni kural (devre dışı, dürüstlük gereği):** `watch_rules.json`'a
`dma_stall_during_inference` eklendi ama `"enabled": false` — çünkü
`bme280.c` DMA değil `HAL_I2C_Mem_Read` (bloklayan) kullanıyor, yani
`DMA1.S0NDTR` zaten hep `0` (kullanılmadığı için, takıldığı için değil);
kural etkinleştirilirse yanlış alarm üretir. `TimeSeriesRuleEngine`'in
`"op":"=="` desteği koddan doğrulandı (zaten vardı, `qFuzzyCompare` ile).

### Faz 10.6 — Otomatik çok-modelli süpürme: TAMAMLANDI (2026-09-09 F4, 2026-09-14 H7)

`src/modules/flash/ModelSweepRunner.h/.cpp` (yeni `QObject`, durum makinesi
`Idle→Compiling→Connecting→Watching→Saving→(sonraki)`) + `Backend`'e
`startModelSweep`/`cancelModelSweep`/`sweepStatus` (canlı `Q_PROPERTY`) +
`qml/dialogs/ModelSweepDialog.qml` (`sweep.startButton`/`sweep.cancelButton`/
`sweep.progressList`) + Benchmark ekranına `benchmark.sweepButton`. **Bilinçli
tasarım kararı:** `ModelSweepRunner` `PipelineRunner`/`VariableWatcher`'a
DOĞRUDAN dokunmuyor, sadece `Backend`'in KENDİ public invokable/property
yüzeyini kullanıyor — bu sayede paylaşılan tek ST-Link kilidini
(`m_stlinkOwner`) atlamak yerine otomatik olarak ona uyuyor: her modelin
derle+flash adımından ÖNCE İzleyici bağlantısı kapatılıyor (flash'ın
ST-Link'i boş bulması gerekiyor), örnekleme bitince tekrar kapatılıp
sıradaki modele geçiliyor — elle UI kullanımıyla birebir aynı akış.

**Canlı doğrulama (F4, gerçek BME280, 2 model — `anomaly_mlp_int8` +
`weather_mlp_int8`, 20 sn/model):** İkinci koşuda ikisi de "ok", her ikisi
de `İzleme Profilleri` sekmesinde doğru veriyle (gerçek BME280 basıncı
`~998.8 hPa` dahil) göründü. Toplam süre ~80 saniye (bu küçük MLP
modelleri için — plan'ın "1.5-3 dk/model" tahmini büyük/yavaş-derlenen
modelleri varsayıyordu, küçük modellerde daha hızlı olması beklenen ve
dürüst bir sonuç). Ekran görüntüleri: `out/model_sweep_e2e/`
(`01_dialog.png`, `02_ilerleme.png` — gerçek "[1/1] anomaly_mlp_int8 —
Derleniyor/Flash..." canlı metniyle, `03_sonuc_profilleri.png`).

**Canlı testte yakalanan VE düzeltilen 2 gerçek hata (ilk koşu
kasıtlı olarak "olduğu gibi" bırakılıp gözlemlendi, ikinci koşuda
düzeltmeler doğrulandı):**
1. `loadWatchElf()` sonrası `applyWatchPresets()` HİÇ BEKLEMEDEN
   çağrılıyordu — ama sembol yükleme `ElfSymbolSource` üzerinden ASENKRON
   (bir `arm-none-eabi-nm` alt-süreci). İlk koşuda süpürmenin İLK modeli
   (`anomaly_mlp_int8`) bu yüzden BOŞ sembol tablosuna karşı preset
   çözümledi (0 kalem eklendi) ve `saveWatchProfile()` haklı olarak
   "profil kaydedilemedi" ile reddetti; ikinci model ise BİR ÖNCEKİ
   modelin (hâlâ önbellekte duran) sembol tablosuna karşı çözümleyip
   şans eseri "başarılı" görünmüştü (roller/adresler aynı şablon
   kod tabanından geldiği için isimler örtüşüyordu, ama bu YANLIŞ
   davranıştı). Düzeltme: `Backend::watchSymbolsLoaded(int)` sinyali
   beklenmeden `applyWatchPresets()` çağrılmıyor artık (+ 15 sn'lik bir
   zaman aşımı koruması, ELF yükleme hiç bitmezse süpürmeyi sonsuza kadar
   asılı bırakmasın diye).
2. Modeller arası `clearWatchItems()` hiç çağrılmıyordu —
   `applyWatchPresets()` yalnızca EKLER, hiç silmez, yani ikinci modelin
   profili İLK modelin kalemlerini de taşıyordu (canlı yakalandı: 12
   yerine 24 kalem). Düzeltme: her modelin ELF'i yüklenmeden hemen önce
   `clearWatchItems()` çağrılıyor.

**Canlı doğrulama (H7, gerçek BME280, 2 model — `anomaly_cnn_int8` +
`weather_cnn_int8`, 2026-09-14):** İlk H7 koşusunda ikisi de "ok" durumuna
geçti AMA kaydedilen profillerin ikisinde de inference/sensör değerleri
sıfırdı (`g_ai_infer_count` vb. tüm 2000 örnekte 0) — kod hatası değil,
**gerçek bir donanım zamanlama bulgusu**: flash+reset sonrası İzleyici'nin
bağlan→ELF yükle→preset uygula→başlat zinciri saniyeler içinde tamamlanıyor,
ama H7'nin BME280 bağlantısı reset sonrası birkaç saniye toparlanma
istiyor olabilir (bugünkü oturumda zaten kararsız çıkmıştı — bkz. yukarıdaki
USB kablo notu); 20 sn'lik pencere bu toparlanma süresine denk gelmiş
olabilir. `saveWatchProfile()`'ın "başarı" ölçütü yalnızca "kalemler boş
değil" — "değerler anlamlı" değil, yani süpürme bunu YAKALAMAZ. Süre 25 sn'e
çıkarılıp hemen tekrar koşulduğunda (sensör artık kararlı) ikisi de gerçek
veriyle geldi: `anomaly_cnn_int8` ort. inference 0.944 ms, `weather_cnn_int8`
ort. inference 3.180 ms — az önceki elle ölçümle birebir eşleşti. Ekran
görüntüleri: `out/model_sweep_e2e/04_h7_sonuc_profilleri.png` (sıfır veri
durumu, dürüstlük için silinmedi), `05_h7_sonuc_profilleri_gercek_veri.png`
(düzeltilmiş/gerçek veri). **Not:** bu, `ModelSweepRunner`'da düzeltilecek
bir kod hatası değil — flash sonrası sensörün ısınma/toparlanma süresi
firmware'in kendi sorumluluğu dışında, host'un bilemeyeceği bir gerçek
donanım gecikmesi; ileride istenirse süpürmeye "ilk N saniyeyi say sayma"
gibi bir marj eklenebilir ama bu oturumda yapılmadı (kapsam dışı, gerçek
sorun donanım tarafında zaten çözüldü).

**Bitti sayılır ki (plan §8.4) — hepsi karşılandı:**
- [x] Bir modelin başarısızlığı süpürmeyi durdurmuyor (ilk koşuda gerçek
  bir hatayla CANLI kanıtlandı: model 1 başarısız oldu, süpürme model
  2'ye geçti ve onu bitirdi)
- [x] İptal düğmesi gerçekten iptal ediyor — canlı test: derleme
  ortasında `cancelModelSweep()` çağrıldı, sonrasında `pipelineBusy=false`,
  `stlinkOwner=""`, `watchLinkOpen=false` — hiçbir kilit/yarım durum kalmadı
- [x] F4'te 2 modelle uçtan uca koştu (iki kez — biri hatayı bulmak,
  biri düzeltmeyi doğrulamak için)
- [x] H7'de de 2 modelle uçtan uca koştu (iki kez — biri donanım
  zamanlama bulgusunu yakalamak, biri gerçek veriyle doğrulamak için)
- [x] Profiller Analiz ekranında (İzleme Profilleri) doğru veriyle görünüyor
- [x] Testler yeşil, commit atıldı

---

## N6 çözüldü (2026-09-14) — RAM-boot yolu açıldı

**Tam anlatım:** [`docs/n6_kaldigimiz_yer.md`](docs/n6_kaldigimiz_yer.md)
(2026-09-14'te baştan yazıldı; eski sürüm ciddi biçimde yanlıştı).

Aylardır "kalıcı ST-Link kısıtı" sanılan N6 bağlantı sorunu **BOOT jumper
pozisyonuymuş.** Flash-boot modunda ROM'un güvenli boot'u debug biriminin bellek
erişimini kapatıyor (tasarım gereği — sahadaki imzalı ürün korunsun diye).
Geliştirme boot pozisyonunda her şey açılıyor. Cihaz **OPEN mode**'da, Debug
Authentication kilidi yok.

Ayrıca **dış flash ve imzalama gereksizmiş**: linker script zaten programı
AXISRAM `0x34000400`'e bağlıyor, ST-Link doğrudan oraya yazıp başlatabiliyor
(0.13 sn). LRUN yolunda FSBL'in yaptığı iş de zaten buydu.

**Bulunan ve düzeltilen gerçek firmware hatası:** N6 startup'ı CP10/CP11'i
(FPU/Helium) hiç açmıyordu — CubeN6 SDK'sının `SystemInit`'i bunu açıkça
"secure application" (FSBL) yapar diye bırakıyor. FSBL olmayan RAM boot'ta
hard-float build ilk float komutunda NOCP UsageFault → HardFault veriyordu
(`CFSR = 0x00080000` ile kanıtlandı). F4'teki `SystemInit` hatasının kardeşi.
`templates/base/STM32N6/startup_stm32n6xx.s`'e idempotent düzeltme eklendi.

**Canlı doğrulandı:** 600 MHz, inference **42 µs** (F4 125 µs, H7 943 µs),
`uwTick` doğru artıyor, `g_telemetry` seqlock'u geçerli, `DHCSR = 0x01110000`
(`S_HALT=0`, `S_RETIRE_ST=1`), `GPIOE_IDR = 0x60` (PE5/PE6 — USART1 pinleri).
Non-secure ve secure peripheral alias'ları aynı değeri veriyor, yani SVD
adresleri olduğu gibi kullanılabilir.

**Hâlâ kapalı olan tek şey:** `ST-LINK_gdbserver` N6 çekirdeğini durduramıyor
(`-g`, `-k`, `--halt`, `-m 0`, `--frequency`, `--pend-halt-timeout`, `-t` ve iki
farklı CubeProgrammer `-cp` — hepsi denendi). Aynı gdbserver H7'de çalışıyor,
STM32_Programmer_CLI aynı N6 çekirdeğini durdurabiliyor → gdbserver sınırı,
donanım değil. Ölçülen alternatif: CLI tek çağrıda çok aralık okuyor,
**312 ms → ~3.2 Hz**.

**Sıradaki işler:**
1. **RAM-boot'u uygulamaya bağla.** `PipelineRunner`/`Backend` N6 için flash
   yerine: `stedgeai → derle → AXISRAM'e yaz → VTOR/CPACR/MSP/PC kur →
   çalıştır`. `MSP`/`PC` imajın ilk 8 baytından okunmalı, sabitlenmemeli.
   Öncesinde daima `mode=UR -run` ile gerçek reset (bayat `HARDFAULTACT`
   SysTick'i maskeliyor).
2. **Register Inspector'ı N6'da canlı doğrula.** `CliRegisterReader` zaten elle
   doğrulanan komutun aynısını üretiyor; muhtemelen kod değişikliği gerekmiyor.
3. **İzleyici için CLI arka ucu** (~3.2 Hz) — isteğe bağlı.
4. **NPU register okuma.** SVD'de NPU bloğu yok; tam harita
   `C:\ST\STEdgeAI\4.0\Middlewares\ST\AI\Npu\Devices\STM32N6xx\ATON.h`'de
   (ST'nin iç adı **ATON**, "NPU" diye aramak sonuç vermez).
   `NPU_BASE_NS = 0x480E0000`. Önce NPU'yu fiilen kullanan bir model gerekli —
   mevcut MLP'de `RCC_AHB5ENR.NPUEN = 0`.
5. **Pasif yakalama mantığını gözden geçir.** `resetN6TargetForCapture()`
   flash-boot'u varsayıyor (kart kendi açılır). RAM boot'ta başlatan biz
   olduğumuz için bu akış yeniden düşünülmeli.

---

## Kalan işler (öncelik sıralı)

Tam gerekçe ve efor tahmini için `docs/variable_watcher_review.md` Bölüm 8'e
bakın — burası yalnızca özet, oradan senkron tutun:

1. ~~**YÜKSEK — Faz 5 doğrulaması.**~~ **2026-09-07 tamamlandı.** Gerçek
   pipeline (H7 + `Models/environmental/anomaly_cnn_int8.tflite` + BME280)
   `Backend.runPipeline()` üzerinden koşturuldu, 5 adımın hepsi (analiz →
   C kodu üretimi → proje hazırlama → derleme → flash) başarıyla bitti.
   `arm-none-eabi-nm` ile `g_ai_last_inference_us` dahil 6 `g_ai_*` sembolü
   ELF'te doğrulandı; kart gerçek donanımda (NUCLEO-H723ZG + GY-BME280) UART
   üzerinden canlı `inf_us` (~926-941 µs) akıttı, uygulamanın Monitör
   ekranında da görüldü. Ekran görüntüleri: `out/faz5_screenshots/`
   (gitignored, silinmedi). **Not:** "±%5 uyuşma" kriteri yapı gereği
   otomatik sağlanıyor — `AI_Runner_Infer()` `g_ai_last_inference_us`'ı ve
   UART `inf_us`'ı aynı `elapsed_us` değerinden dolduruyor
   (`templates/ai_glue/ai_runner.c`). "Firmware boyut artışı <1 KB" kriteri
   bu ilk temiz derlemede kıyaslanacak bir önceki build olmadığı için test
   edilmedi (mutlak boyut: text+data+bss = 88.016 bayt). Yol boyunca iki
   gerçek eksik bulunup giderildi: `STM32Cube_FW_H7` SDK'sı hiçbir ST
   kurulumuyla gelmiyordu (bkz. `docs/dev_machine_setup.md` §7) ve
   `DebugBridge`'in liste kırpması baş yerine kuyruktan yapılacak şekilde
   düzeltildi (asıl pipeline hatasını gizliyordu).
2. ~~**YÜKSEK — Uçtan uca senaryo elle koşulmalı.**~~ **2026-09-09 tamamlandı.**
   Gerçek H7 + `anomaly_cnn_int8` ELF'i ile tam zincir koşuldu: bağlan → ELF
   yükle (eşleşme doğrulandı: SP+reset vektörü tutuyor) → `AI Presetlerini
   Uygula` (6 rol-etiketli kalem: `inferenceUs`/`inferCount`/`lastClass`/
   `confidence`/`hwTick`/`bssEnd`) → başlat (200 Hz gerçek, 0 kaçırılan/hata,
   canlı grafik) → kayıt başlat/durdur (gerçek 200 Hz CSV, `watch_rules.json`
   formatına uygun) → oynat (kayıttan güvenli mod, banner görünür, değerler
   birebir eşleşti) → profil kaydet (x2) → **karşılaştır**: rol-etiketli iki
   profil arasında `Ortalama/Tepe inference (ms)` gerçek delta=0 ile eşleşti,
   heap/stack (izlenmedikleri için) doğru şekilde "karşılığı yok" gösterdi →
   CSV dışa aktarım (görünür pencere, birim etiketli). Ekran görüntüleri:
   `out/watch_e2e/` (gitignored, silinmedi).
   **Yol boyunca bulunanlar:** (a) `DebugBridge.cmdInvoke` 4 argümanla
   sınırlıydı ve dönüş değerini hiç yakalamıyordu — `flashFirmware` (5 arg)
   ve `watchProfiles()`/`compareWatchProfiles()` gibi `QVariantList`/
   `QVariantMap` dönen metotlar test edilemiyordu; kalıcı olarak 6 arguman +
   `QGenericReturnArgument` ile dönüş değeri yakalama eklendi. (b)
   `SymbolPickerDialog`/`WatchRecordingBar`/`ProfileCompareDialog`'daki
   birkaç butona (`watch.symbolDialogCloseButton`,
   `watch.compareProfilesButton`, `watch.compareDialogCloseButton`)
   `objectName` eklendi — önceden yalnızca fare tıklamasıyla kapatılabiliyorlardı.
   (c) Elle eklenen semboller (`Sembol Ekle`) rol taşımıyor — bu yüzden
   `Profilleri Karşılaştır` rol gerektiren metrikleri boş gösteriyordu; bu bir
   hata değil, `AI Presetlerini Uygula` akışının amaçlanan kullanım şekli
   olduğunun doğrulanması (madde 3'teki demo kaydı eksikliğiyle aynı kök
   neden — rol etiketleme).
3. ~~**YÜKSEK — Demo kaydı üretin.**~~ **2026-09-09 tamamlandı.** Eski
   `watch/demo/h7_demo_trace.csv` yalnızca `SysTick_VAL` içeriyordu (rolsüz).
   Yeni kayıt gerçek H7'de alındı: Watch (GDB) ve UART aynı anda bağlıyken,
   `AI Presetlerini Uygula` ile 6 rol-etiketli kalem (`inferenceUs` dahil,
   `g_ai_last_inference_us` etiketi zaten `inference_us` regex'ine uyuyor) ~33 s
   / 200 Hz / 6562 örnek kaydedildi — UART'tan gelen gerçek `inf` paketleri
   sayesinde kayda **704 gerçek `inference` olayı** gömüldü, yani
   `inference_time_outlier` kuralının `gate: {eventKind:"inference"}` şartı
   artık demo oynatımında da sağlanabiliyor (bu çalışmada donanım çok kararlı
   olduğu için z-skoru eşiğini aşan bir aykırılık çıkmadı — bu beklenen/dürüst
   bir sonuç, zorlanmadı). Oynatma canlı UI'da doğrulandı: banner görünüyor,
   birim/rol etiketleri (`0.926 ms` vb.) doğru taşınıyor. Header'daki `elf=`
   alanı commit'e gitmeden kişisel makine yolundan temizlendi.
4. ~~**ORTA — RegionScan (stack watermark) canlı decode'u.**~~ **2026-09-09
   tamamlandı.** `WatchSampler::decodeRegionScan()` eklendi: `_sstack`'ten
   `_estack`'e taranan bloğu 32-bit kelime kelime `regionPattern`
   (`0xA5A5A5A5`) ile kıyaslar, ilk uyuşmayan kelimenin bayt ofsetini "kalan
   stack boşluğu" olarak döner; çok parçalı (chunk'lı) bölgeleri de doğru
   takip eder, bir parça başarısız olursa tüm taramayı `ok=false` yapar
   (sahte kısmi değer üretmez). **Asıl kör nokta decode değil, hiç
   çağrılmıyor olmasıydı:** `Backend::applyWatchPresets()` RegionScan
   önerilerini açıkça `continue` ile atlıyordu, VE
   `VariableWatcher::rebuildPlan()` yalnızca `WatchPlanBuilder::build()`
   (skaler) çağırıyordu — `buildRegionScans()` hiçbir yerden çağrılmıyordu.
   İkisi de düzeltildi (`WatchPlanBuilder::merge()` eklendi, skaler+region
   planlarını birleştiriyor). Canlı H7'de doğrulandı: `stackWatermark`
   kalemi gerçek "2896 B" değeri verdi (`hasValue=true`). `watch_rules.json`
   içindeki iki `stackWatermark` kuralı tekrar etkin. 6 yeni birim testi
   eklendi (`TestWatchSampler`: 4 RegionScan senaryosu, `TestWatchPlanBuilder`:
   2 `merge()` senaryosu), hepsi + mevcut tüm testler geçti.
   **Bilinen maliyet (ölçüldü, aynı H7 oturumunda):** RegionScan istekleri
   ayrı/düşük hızlı bir plana değil, ANA örnekleme planına giriyor (öyle bir
   ikincil-hız mekanizması hiç yoktu — presetteki `"rateHz":2` alanı hiçbir
   yerde okunmuyordu, sadece parse ediliyordu). Sonuç: 4 KB'lık bu bölge
   200 Hz hedefini gerçek ~100 Hz'e düşürdü (462 kaçırılan örnek). Büyük bir
   stack bölgesi için gerçek bir maliyet — ayrı düşük-hızlı zamanlayıcı
   (DHCSR sağlık kontrolü gibi) ileride eklenebilir, şimdilik dokümante
   edildi (bkz. `WatchPlanBuilder.h`, `CLAUDE.md`).
5. ~~**ORTA — `AnalysisScreen.qml`'e "İzleme Profilleri" sekmesi.**~~
   **2026-09-09 tamamlandı.** Eskiden "GUI otomasyonu yok" gerekçesiyle
   ertelenmişti — artık [[project-verification-ecosystem|DebugBridge/uiprobe]]
   var, gerekçe geçersizleşti. `Backend::watchProfileRecords()` eklendi
   (diğer dört `*Records()` erişimcisiyle birebir aynı desen —
   `recordsForKind("watch_profile")`); `AnalysisScreen.qml`'e 5. sekme
   eklendi, mevcut genel `_cols`/`rowsForIndex`/`colsForIndex`/
   `boardColumn`/`typeColumn`/`summaryCards`/`barData`/CSV-PDF dışa aktarım
   makinesi **hiç özel kod yazmadan** yeniden kullanıldı (`WatchProfile::
   buildRows()`'un zaten ürettiği c0..c14 hücre biçimi, diğer 4 sekmenin
   kullandığı `{id,kind,cells}` şekliyle bire bir örtüşüyordu). Canlı H7
   verisiyle doğrulandı: 24 kalem satırı, doğru özet kartları (Toplam
   Kalem/Kart Sayısı/Kalem Türü/Ort. Hz), gerçek kalem bazlı ortalama
   grafiği. Ekran görüntüsü: `out/regionscan_e2e/02_izleme_profilleri_tab.png`.
   **Bilinen sınırlama:** depolama satır-başına-KALEM (item) granülerliğinde
   — bir oturumda izlenen 6 kalem 6 ayrı satır olarak görünür, aynı oturuma
   ait olduklarını gösteren ortak bir "oturum" sütunu/kimliği yok (board+not
   ile elle ilişkilendirilebilir). "Sil" butonu bu yüzden tek bir kalemi
   siler, tüm oturumu değil — kapsam dışı bırakıldı.
6. **DÜŞÜK — `Backend.cpp` 4320 satıra çıktı** (+%30). `WatchFacade` gibi bir
   alt cepheye bölünmesi düşünülebilir — mimari karar, aceleye getirilmemeli.
7. ~~**DÜŞÜK — Register Inspector hız iddiası (Faz 2) hiç canlı ölçülmedi**~~
   **2026-09-09 ölçüldü.** H7'de aynı 16 peripheral'lık plan için 5'er koşum:
   **CLI ortalama 273 ms, GDB ortalama 389 ms** — GDB arka ucu CLI'dan
   **daha hızlı değil, aksine ~%40 daha yavaş.** Sebep: `GdbServerReader`
   her okuma için `DebugLink::retain()`/`release()` çağırıyor (paylaşılan
   ST-Link'in doğruluk/güvenlik sözleşmesi — bkz. CLAUDE.md "DebugLink
   referans sayımlıdır"), yani her snapshot'ta gdbserver süreci baştan
   başlatılıp durduruluyor; kalıcı bağlantı yeniden kullanımı YOK. Plan
   dosyasındaki "hız kazancı" varsayımı, mevcut retain/release mimarisiyle
   gerçekleşmiyor — bu bir hata değil (mimari doğruluk için kasıtlı), ama
   performans iddiası artık yanlışlanmış durumda. Varsayılan `"cli"` kalmalı;
   "gdb" seçeneğinin tek potansiyel faydası hız değil, farklı bir bağlantı
   yolu olması (CLAUDE.md'deki not güncellenmeli: "hız kazancı konfordur"
   ifadesi artık "konfor bile değil, sadece alternatif yol" olarak
   okunmalı).
8. ~~**Register Inspector'ın geniş kapsamlı canlı testi.**~~ **2026-09-09
   tamamlandı.** Önceki oturum yalnızca tek bir RCC snapshot'ı doğrulamıştı;
   bu oturumda H7 + F4 + N6 (üçü de bağlıyken) üzerinde: Snapshot A/B → **A→B
   farkı** (gerçek GPIOD/USART3.ISR TXE-TC değişimleri, doğru alan/adres/Δ),
   kural motoru (`registerRuleViolations`, 0 ihlal — sağlıklı donanımda
   beklenen/doğru sonuç, USART/I2C/TIM hepsi doğru yapılandırılmış), JSON
   dışa aktarım (1.1 MB, board+diff+violations şeması doğru), **GDB okuma
   arka ucu** (`setRegisterReadBackend("gdb")`, `mode: GDB-ATTACH`, aynı 53
   değişiklik/0 hata CLI ile — commit 599c2fb'nin ST-Link hedefleme
   düzeltmesi GDB yolunda da doğrulandı, sonra varsayılan `"cli"`'ye geri
   alındı). LLM danışman yolu yalnızca "API key yokken doğru şekilde
   devre dışı" olarak doğrulandı — gerçek bir LLM çağrısı kimlik bilgisi
   gerektirdiği için denenmedi.
   **Yol boyunca 2 gerçek hata bulunup düzeltildi:** (a) SVD `<description>`
   metinleri kaynak XML'in kendi satır sonlarını/girintisini olduğu gibi
   taşıyordu — `RegisterDiffView`'in sabit yükseklikli satırları bu yüzden
   bir sonraki satırın üzerine biniyordu (H7'nin diff popup'ında canlı
   yakalandı); tek noktadan (`SvdParser::normalizeDescription`,
   `QString::simplified()`) düzeltildi, tüm tüketiciler (diff/ağaç/tooltip)
   otomatik faydalanıyor. (b) `RegisterInspector::finishWithError()` başarısız
   bir deneme sonrası hedef slotu hiç geçersiz kılmıyordu — N6'nın (bilinen/
   dokümante TrustZone kısıtı yüzünden) RCC okuması başarısız olduğunda,
   Register Tablosu bir önceki **F407 snapshot'ını** N6 başlığı altında
   sessizce "geçerli" göstermeye devam ediyordu; slot artık hata anında
   temizleniyor ve `registerModelChanged()` de tetikleniyor ki QML ağacı
   aynı anda güncellensin.

---

## Daha geniş yol haritası (CLAUDE.md'nin kendi tablosundan)

`CLAUDE.md` → "Geliştirme Aşamaları" bölümünde ⏳ bekliyor olarak işaretli,
Değişken İzleyici'nin dışında kalan aşamalar:

| Aşama | Başlık | Not |
|---|---|---|
| 5 | Veritabanı ve kayıt | `analysis_records` tek-tablo şeması hâlâ geçerli; normalize 4-tablo hedefi (`docs/PROJECT.md`'de bahsi geçen) henüz implemente edilmedi |
| 6 | Canlı dashboard | Başlanmadı |
| 7 | Model karşılaştırma | Değişken İzleyici'nin profil karşılaştırma özelliği (Faz 8) bunun bir parçası sayılabilir — ama ekranda erişilemiyor (bkz. madde 5 yukarıda) |
| 8 | Bitirme demo hazırlığı | Başlanmadı |

---

## Bu dosyayı güncelleme kuralı

Bir maddeyi bitirdiğinizde buradan silin/işaretleyin. Yeni bir denetim/bulgu
raporu yazarsanız (bir önceki örnek: `docs/variable_watcher_review.md`),
onun "kalan işler" bölümünü buraya özetleyin ki tek bir yerden bakılabilsin.
