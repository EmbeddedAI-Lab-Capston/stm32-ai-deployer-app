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

## SIRADAKİ İŞ — Faz 10: bellek-öncelikli telemetri

**Tam plan:** [`docs/memory_telemetry_plan.md`](docs/memory_telemetry_plan.md)
— kendi kendine yeterli, adım adım, üç kart için (F4/H7/N6).

Altı alt faz: sensör ham değerlerini bellekten okuma (seqlock ile) → RAM
bütçesi → hız tutarlılık kontrolü → "model sığar mı" ön kontrolü → canlı
peripheral/register izleme → otomatik çok-modelli süpürme.

**Bu fazın çıkış noktası:** F4'ün ST-Link VCP'si hedef USART'a köprülü
değil (harici adaptör kullanılmayacak — kullanıcı kararı), yani F4'te
UART hiç yok. Ama projenin zaten asıl iddiası SWD üzerinden **canlı
bellek okumak**; sensör verisini de oradan okuyunca F4 tam işlevsel hale
geliyor ve UART bir bağımlılık olmaktan çıkıp (H7'de) çapraz doğrulama
aracına dönüşüyor.

### Faz 10.1 — durum (2026-09-09, kısmen tamamlandı)

**Kod tamamlandı, testler yeşil, F4'te canlı doğrulandı; H7 sensör
takılınca ve N6 flash sorunu çözülünce tamamlanacak.**

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
- **H7: kod/ELF eşleşmesi doğrulandı ama sensör canlı testi YAPILAMADI** —
  bu oturumda BME280 fiziksel olarak H7'ye değil F4'e bağlıydı (kullanıcı
  onayı). ELF eşleşmesi ve preset uygulaması doğru çalıştı, `uwTick`/
  `stackWatermark` canlı güncellendi, ama `sensor0/1/2` beklendiği gibi
  `0.000` kaldı (gerçek durum — uydurma değil, o boot'ta hiç başarılı I2C
  okuması olmadı). **Kullanıcı BME280'i H7'ye taktıktan sonra tekrar
  denenmeli** (UART-vs-bellek çapraz doğrulaması, plan §3.11).
- **N6: derleme başarılı** (telemetry.c Cortex-M55'te de sorunsuz derleniyor,
  offset'ler aynı), **flash başarısız**: `STM32_Programmer_CLI karta
  baglanamadi` — bilinen N6 bağlantı kısıtı (§2.5), zaman kutulu bırakıldı.
- **Yol boyunca bulunan, Faz 10.1 kapsamı dışı bir UI hatası:**
  `qml/components/watch/WatchToolbar.qml`'de ELF durum etiketi
  `backend.watchElfPath().length > 0 ? ... : "ELF yüklenmedi"` şeklinde bir
  **metot çağrısına** bağlı — QML'in binding motoru bunu değişiklik için
  izleyemiyor, yani ELF yüklendikten sonra bile etiket "ELF yüklenmedi"
  göstermeye devam ediyor (arka planda gerçek durum doğru, sadece bu etiket
  bayat). Düzeltme `watchElfPath`'i NOTIFY'lı bir `Q_PROPERTY`ye çevirmeyi
  gerektiriyor — bilinçli olarak bu oturumda yapılmadı (kapsam dışı), ama
  gerçek ve düşük riskli bir düzeltme.
- **Kalan (Faz 10.1'i kapatmak için):** H7'de BME280 canlı + UART çapraz
  doğrulama, N6 flash sorununun (gerekirse) çözülmesi veya kalıcı olarak
  "doğrulanamadı" işaretlenmesi, ekran görüntüleri `out/sensor_memory_e2e/`
  altında tamamlanması (`01_h7_*`, `02_h7_*`, `03_h7_uart_vs_bellek_*`).

### Faz 10.2 — RAM bütçesi görselleştirmesi: TAMAMLANDI (2026-09-09)

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
**Bulunan pre-existing hata (kapsam dışı, düzeltilmedi):**
`qml/components/watch/WatchToolbar.qml`'deki "ELF yüklenmedi" etiketi
`backend.watchElfPath()` bir METOD çağrısına bağlı — QML binding motoru
bunu izleyemiyor, ELF yüklendikten sonra bile etiket bayat kalıyor. Düzeltme
`watchElfPath`'i NOTIFY'lı `Q_PROPERTY`ye çevirmek (aynı desen bu fazda
`ramBudget` için zaten uygulandı) — küçük ve düşük riskli ama bu fazın
kapsamı dışında bırakıldı.

### Faz 10.3 — Hız tutarlılık kontrolü: TAMAMLANDI (2026-09-09)

`src/modules/watcher/RateCheck.h/.cpp` (saf, 5 birim testi) +
`Backend::inferenceRateCheck(windowSec)` (`Q_INVOKABLE` — plan gereği canlı
property değil, çağıran pencereyi kendi seçiyor) + `WatchLinkStatus.qml`'de
3 sn'de bir `Timer` ile çağrılan `objectName: "watch.rateCheckBadge"` rozeti.
Rozet **"doğrulandı" DEMİYOR** — "tutarlı"/"TUTARSIZ" diyor (dürüstlük
kuralı, plan §5.1). **F4'te canlı doğrulandı:** "22.7 Hz gözlendi · beyan
125 µs ile tutarlı" (teorik üst sınır 8000 Hz) — gerçek sayılar, gerçek
firmware. Ekran görüntüsü: `out/rate_check_e2e/01_f4_tutarli.png`.
**Tutarsız durum canlı denenmedi** (firmware'in kendi beyanını yalanlaması
gerekirdi, ki mevcut firmware doğru rapor veriyor) — pure fonksiyonun kendisi
birim testiyle (`observedRateExceedingTheoreticalMaxIsInconsistent`) hem
tutarlı hem tutarsız durumu kapsıyor.

### Faz 10.5 — "Bu model bu karta sığar mı" ön kontrolü: TAMAMLANDI (2026-09-09)

`XCubeAIRunner::parseAnalyzeOutput()` (saf, gerçek H7+F4 `stedgeai analyze`
çıktılarıyla test edildi — "Complexity report" bölümündeki eşittir-biçimli
("macc=2,528") yanıltıcı ikinci bir sayıyı ayırt etmek için ":" biçimine
kilitleniyor) + `checkModelFitsBoard()` (`src/modules/flash/ModelFitCheck.h/.cpp`,
saf, 3 birim testi) + `PipelineRunner`'a yeni `warningLine` sinyali
(pipelineLines'a `type:"warn"` düşüyor, `type:"err"`'den ayrı — uyarı asla
pipeline'ı durdurmuyor). **Sığan durum F4'te canlı doğrulandı**
(`anomaly_mlp_int8`: weights=2,696 B, activations=716 B — hiç uyarı
çıkmadı, `pipelineLines` erken-anlık görüntüyle doğrulandı çünkü DebugBridge
listeleri son 20 satıra kırpıyor). **Sığmayan durum canlı DENENEMEDİ:**
depodaki hiçbir gerçek `.tflite` modeli (kws_dnn/kws_dscnn/kws_tcresnet/
wisdm_mlp dahil, hepsi `stedgeai analyze` ile bizzat ölçüldü) F4'ün bile
kapasitesini aşmıyor — plan "sığıyorsa başka bir model dene, uyarıyı
zorlama" diyor, zorlanmadı. Eşik/uyarı mantığının kendisi
`TestModelFitCheck.cpp`'de gerçek model sayılarıyla (kws_dnn'in ölçülen
261.424 B/12.824 B ayak izi) ama sentetik (gerçek olmayan, açıkça
etiketlenmiş) bir "tiny test fixture" kart tanımına karşı doğrulandı.

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
