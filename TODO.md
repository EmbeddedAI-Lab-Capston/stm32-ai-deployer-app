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
3. **YÜKSEK — Demo kaydı üretin.** Dağıtılan `watch/demo/h7_demo_trace.csv`
   yalnızca `SysTick_VAL` içeriyor; rolü yok, `inference_us` regex'ine
   uymuyor → demo sırasında kural akışı (`WatchRuleFeed`) boş görünür. Rol
   taşıyan (`heapEnd`, `inferenceUs`) yeni bir kayıt alın.
4. **ORTA — RegionScan (stack watermark) canlı decode'u.** `WatchSampler`'a
   bayt-tarama decode adımı yazılmadı; bu yüzden `watch/watch_rules.json`'daki
   iki `stackWatermark` kuralı `"enabled": false` ile kapalı duruyor. Bağlı:
   `templates/ai_glue/stack_paint.c` şu an ürettiği veriyi okuyan yok.
5. **ORTA — `AnalysisScreen.qml`'e "İzleme Profilleri" sekmesi.** Planda
   vardı, hiç eklenmedi. **Bilinçli olarak yapılmadı** — 5. sekme
   `_subTabs`/`_cols`/`rowsForIndex`/özet kartları/dışa aktarımı birden
   etkiliyor ve görsel doğrulama (GUI otomasyonu yok) gerektiriyor.
6. **DÜŞÜK — `Backend.cpp` 4320 satıra çıktı** (+%30). `WatchFacade` gibi bir
   alt cepheye bölünmesi düşünülebilir — mimari karar, aceleye getirilmemeli.
7. **DÜŞÜK — Register Inspector hız iddiası (Faz 2) hiç canlı ölçülmedi**
   (GDB arka ucu vs CLI). Varsayılan zaten `"cli"` olduğu için regresyon
   riski yok, sadece bir performans iddiası doğrulanmamış durumda.

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
