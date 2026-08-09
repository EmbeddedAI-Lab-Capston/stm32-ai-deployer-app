# Değişken İzleyici — Bağımsız Denetim Raporu

**Denetim tarihi:** 2026-08-09
**Kapsam:** `4904040..df9dc68` (Faz 1–9), dal `feature/register-inspector`
**Yöntem:** kod okuma + gerçek derleme + `ctest` + **mutasyon testi (32 mutasyon)**
+ NUCLEO-H723ZG üzerinde canlı ölçüm (özel yazılmış başsız `LiveProbe` koşum aracı)
+ uygulamanın gerçekten çalıştırılması

> Bu rapor `docs/variable_watcher_findings.md`'yi **kanıt değil iddia** kabul eder.
> Her "✅" bağımsız olarak teyit edilmeye çalışılmıştır. Doğrulanamayanlar
> "DOĞRULANAMADI" olarak işaretlidir — "muhtemelen sorun yok" denmemiştir.

---

## 1. Yönetici özeti

**Teslim edilebilir mi? → EVET** (aşağıdaki 2 kalan iş kabul edilerek).

> **Güncelleme (denetim sonu, kart takılı):** raporun ilk hâlinde "ŞARTLI"
> denmişti; iki şartın da kök nedeni bulunup düzeltildi ve **gerçek donanımda
> doğrulandı**. Ayrıntı: Bölüm 10.

Mühendislik kalitesi beklediğimden yüksek: gözlemci ilkesi gerçekten kod düzeyinde
uygulanmış (canlı olarak 20 sn boyunca `S_HALT` hiç görülmedi), gdbserver başlatma
argümanları kanıtlanmış biçimde güvenli (`-g`=attach, `-k`/`--halt` yok), zaman
damgası sözleşmesi koda birebir uygulanmış, saf sınıflar mutasyon testine büyük
ölçüde dayandı. Canlı okuma gerçekten çalışıyor (1000 Hz hedefte **933 Hz**, RTT
0.41 ms, 18675/18675 örnekte değer değişti).

Ama **Faz 8'in tamamı sahada ölüydü** ve bunu hiçbir test yakalamıyordu:
`updateItem()` `role` alanını sessizce düşürdüğü için rol tabanlı 3 kural hiç
eşleşemiyor, profil karşılaştırma tablosu tamamen boş dönüyordu. Ayrıca olay
ekseni ile örnek ekseni **iki ayrı saatti** — canlı ölçtüm, 61 ms sabit kayma;
bu tek başına `watch_rules.json`'daki ±50 ms'lik olay kapısını her zaman
reddettiriyordu. Yani dört kuraldan **dördü de** hiçbir koşulda tetiklenemezdi.
Bunların ikisini düzelttim (ikisi RegionScan'e bağlı, hâlâ ölü).

**Şartlar (ikisi de kapatıldı):** KRİTİK-3'ün kök nedeni bulundu (gdbserver
`-e` persistent modda başlatılıp sonra zorla öldürülüyordu) ve düzeltildi;
arka arkaya 3 oturum + ardından `STM32_Programmer_CLI` bağlantısı ile canlı
doğrulandı. Düzeltme sırasında **uygulamanın kapanış yolunda gerçek bir
segfault** da bulundu (KRİTİK-5).

**Kalan iki iş (bloklayıcı değil):** Faz 5'in gerçek pipeline doğrulaması ve
`AnalysisScreen.qml`'e "İzleme Profilleri" sekmesi. İkincisini bilinçli olarak
**yapmadım**: göremediğim UI'yi göndermek, bu denetimin baştan beri
eleştirdiği hatanın ta kendisi olurdu.

---

## 2. En sivri üç soru — doğrudan cevaplar

### S1. Planın kendi "tamamlandı" tanımı çiğnendi mi?

**Evet, açıkça.** Plan Bölüm 13'ün son cümlesi:

> "Bir faz, birim testleri geçse bile canlı kriterleri karşılamadan
> 'tamamlandı' sayılmaz."

Bölüm 16'daki commit şartları somut canlı ölçütler koyuyor (Faz 4: "1000 Hz'de
≥800 Hz", Faz 5: "`inf_us` ile ±%5 uyuştu", Faz 6: "60 s / 3000 Hz akıcı",
Faz 7: "ST-Link çıkarılmış halde demo tam çalıştı", Faz 8: "sızıntı demosu
kuralı tetikledi"). Bunların çoğu yapılmadı ama faz yine de "tamamlandı" ilan
edildi. Kendi tanımına göre **Faz 5, 6, 7, 8 "tamamlandı" sayılamaz.**

Not: bu, işin değersiz olduğu anlamına gelmiyor — ertelemelerin çoğu gerekçeli.
İhlal olan şey **etiketleme**: findings.md faz sonlarını "Faz N tamamlandı"
diye kapatıyor, oysa planın tanımı gereği bunlar "Faz N kodu hazır, canlı
doğrulama bekliyor" durumunda.

### S2. Testler spec'i mi doğruluyor, yazarın implementasyonunu mu?

**Beklediğimden çok daha iyi — ama önemli kör noktalarla.** 32 mutasyonluk bir
tur çalıştırdım (yöntem notu aşağıda): **23 yakalandı, 9 kaçtı.**

Kaçanlar tam da kritik yerlerdi:

| Mutasyon | Sonuç |
|---|---|
| `WatchSampler` `reply.ok`'u yok say | **KAÇTI** — hiçbir test yoktu |
| `WatchProfile` scale'i düşür (Faz 8 düzeltmesini geri al) | **KAÇTI** |
| Kural motoru `appliesToRole` eşleşmesini yok say | **KAÇTI** |
| Eşik `<` → `<=` (sınır davranışı) | **KAÇTI** |
| Olay kapısında `abs()` kaldır (tek yönlü pencere) | **KAÇTI** |
| `WatchPlanBuilder` `maxReadBytes` sınırını yok say | **KAÇTI** |
| `TraceBuffer::firstTime()` sarmalamayı yok say | **KAÇTI** |
| `GdbRspCodec` checksum maskesi | KAÇTI (düşük değer, RX'te checksum zaten doğrulanmıyor) |
| `decimate()` tahliyeyi yok say | KAÇTI — **eşdeğer mutant**, aşağıda |

Kullanıcının özellikle şüphelendiği `zscoreSpikeIsDetectedWithExpectedZ`
**totoloji değil**: N−1'e çevirme mutasyonunu yakaladı. `TestWatchProfile`
şüphesi ise **haklıydı** — stddev'i yalnızca "≥0 mı" diye kontrol ediyordu ve
Faz 8'in kendi düzeltmesini hiç korumuyordu.

Bu 9 kaçaktan 8'i için regresyon testi yazdım, hepsi artık yakalanıyor.
Kalan 1'i (`decimate` tahliye) **eşdeğer mutanttır**: `decimate()` örnekleri
zaman damgasına göre kovalara attığı için halkayı hangi sırayla gezdiği min/max
sonucunu değiştirmiyor. Test yazılacak bir hata değil.

> **Kendi yöntem hatam — şeffaflık için:** ilk mutasyon turumda dosyaları `mv`
> ile geri yüklüyordum; `mv` yedeğin eski mtime'ını koruduğu için `make` nesne
> dosyasını güncel sanıp yeniden derlemiyordu. Sonuç: ilkinden sonraki her
> mutasyon bir öncekinin üzerine binmişti ve "19/20 yakalandı" gibi **yanlış
> ve fazla iyimser** bir sonuç çıkmıştı. Turu `touch` + her mutasyondan önce
> "temiz baseline yeşil mi" kontrolüyle baştan çalıştırdım; yukarıdaki 23/9
> rakamı düzeltilmiş turdan.

### S3. Hiç regresyon testi yapılmadı — mevcut özellikler kırıldı mı?

**Kısmen doğrulandı, kısmen DOĞRULANAMADI. Dürüst cevap: bilmiyorum.**

Doğrulayabildiklerim:
- Proje tam derleniyor, `ctest` yeşil (düzeltmelerimden sonra da).
- Uygulama açılıyor; `StackLayout` tüm sekmeleri açılışta kurduğu için **her
  ekranın QML'i** örnekleniyor. `app_trace.log`'da izleyiciye ait tek bir
  uyarı yok; tek uyarılar Fabrika Simülasyonu'nun `NodeMarker.qml`'inden
  geliyor ve **bu iş öncesinde de vardı** (bu commit setinde dokunulmamış).
- `m_stlinkOwner` hakemi: `acquireStLink`/`releaseStLink` mantığı basit
  ("boşsa al, sahibi sensen geç, değilse reddet") ve `DebugLink::closed`/
  `failed` yollarının ikisi de sahipliği bırakıyor — kilitlenmeye yol açacak
  bir yol göremedim.
- 4 Hz kural timer'ı boşta CPU yakmıyor: `evaluateWatchRules()` izleme
  çalışmıyorken hemen dönüyor. 22 sn boşta çalıştırmada sorun yok.

**Sonradan doğrulananlar (KRİTİK-3 düzeltildikten sonra):** ST-Link yolu artık
sağlam. Arka arkaya 3 izleme oturumunun **ardından** `STM32_Programmer_CLI`
karta temiz bağlanıyor (`Device ID 0x483`, `STM32H72x/STM32H73x`) — düzeltme
öncesi aynı komut `DEV_USB_COMM_ERR` veriyordu. Bu, Flash / probe / Register
Inspector'ın CLI arka ucunun izlemeden sonra çalıştığının doğrudan kanıtı.

**Hâlâ yapamadıklarım (açıkça eksik):** UART § akışının, Register Inspector
snapshot A→B diff'inin, Analiz CSV/PDF dışa aktarımının ve Fabrika
Simülasyonu'nun **ekran üzerinden fonksiyonel** testi. Sebep: GUI otomasyonum
yok. Bu ekranların QML'i hatasız kuruluyor ve dokundukları C++ yolları
değişmedi, ama "tıklayarak denendi" diyemem.

---

## 3. Faz faz karar tablosu

| Faz | İddia | Gerçek durum | Karar |
|---|---|---|---|
| **1** — debug link | ✅ | Gözlemci beyaz listesi gerçek ve mutasyonla kanıtlandı (`M`/`Z`/`vCont;t` engelli). Canlı: 20 sn sürekli örnekleme, `S_HALT` **hiç** görülmedi. `retain()/release()` dengesi canlı doğrulandı (`refCount`=0). Başlatma argümanları `--help` ile teyit edildi: `-g`=attach, `-e`=persistent, `-k`/`--halt` yok. | **TAMAMLANDI** |
| **2** — GdbServerReader | ✅ | Varsayılan `"cli"` korunmuş; `IRegisterReader` arayüzü duruyor. Hız iddiasını **doğrulayamadım** (ST-Link kilitli). Regresyon riski düşük çünkü varsayılan değişmemiş. | **KOŞULLU** |
| **3** — sembol katmanı | ⚠️✅ | Gerçek H7 ELF'iyle bağımsız doğruladım: `_Min_Stack_Size` `A` tipi (=0x800, **değer**), `__sbrk_heap_end` `b` (static — parser büyük harfe çevirdiği için doğru okunuyor), `_sstack` **yok** → preset'in `_estack-_Min_Stack_Size` fallback'i gerçekten gerekli ve doğru çözüyor. Thumb biti doğru. Canlı "yeşil eşleşme" senaryosu hâlâ yapılmadı. | **TAMAMLANDI** (canlı eşleşme senaryosu hariç) |
| **4** — örnekleyici + UI | ✅ | Canlı ölçtüm: **1000 Hz hedefte 927–941 Hz**, planın "≥800 Hz" şartını geçiyor. **60 sn dayanıklılık testi de yapıldı:** 56188 örnek, 936.5 Hz sürdürüldü, RSS 60 sn boyunca **12.4 MB'da sabit** (sıfır büyüme), `S_HALT`/`S_RESET_ST` hiç görülmedi. Planın Faz 4 şartları artık **karşılanmış durumda**. Örnekleme hızı kuantalama hatası vardı (YÜKSEK-6), düzeltildi. | **TAMAMLANDI** |
| **5** — firmware | ✅ | `stack_paint.c` F4/H7/N6'nın üçünde de Makefile'a ve `main.c`'ye bağlı (kontrol ettim). `g_ai_*` `static` ama bu **sorun değil** — `nm` yerel sembolleri de adresle veriyor ve parser küçük harf tipleri doğru işliyor. Ancak: gerçek pipeline'dan geçirilmedi, `inf_us` ±%5 uyumu ölçülmedi, **ve `stack_paint`'in ürettiği veri şu an hiçbir yerde okunmuyor** (RegionScan decode yok). | **TAMAMLANMADI** |
| **6** — grafik + olay ekseni | ✅ | Fazın **asıl iddiası** olan "olay korelasyonu" iki ayrı saat yüzünden sistematik olarak kaymıştı (KRİTİK-2, 61 ms canlı ölçüm). Düzeltmeden sonra canlı olarak **−0.4 … −1.1 ms** (yalnızca kuyruklu sinyal gecikmesi) — eksen artık gerçekten tek. Grafiğin **görsel** akıcılığı hâlâ denenmedi; 3000 Hz timer yoluyla erişilemiyor (maks mod gerekir). | **KOŞULLU** |
| **7** — kayıt/oynatma | ✅ | Demo CSV gerçek bir kayıt (200 Hz, 5964 örnek, 29.8 s) ve exe'nin yanına kopyalanıyor — kontrol ettim. Oynatma kod yolu sağlam ve testli. Ama "ST-Link fiziksel çıkarılmış demo" yapılmadı; `AnalysisScreen.qml`'e "İzleme Profilleri" sekmesi **hiç eklenmedi** (planda vardı). | **KOŞULLU** |
| **8** — kural motoru + preset | ✅ | **Sahada tamamen ölüydü.** Motorun kendisi matematiksel olarak doğru (eğim, R², z-skoru bağımsız doğruladım) ve iyi testli — ama `role` hiç set edilmediği için 3 kural eşleşemiyor, 4.'sü de saat kayması yüzünden kapıdan geçemiyordu. Profil karşılaştırma tablosu her satırda "karşılığı yok" dönüyordu. | **TAMAMLANMADI** (düzeltmelerimden sonra 4 kuraldan 2'si canlandı) |
| **9** — dokümantasyon | ✅ | ADR bölümü ve PROJECT.md var, büyük ölçüde kodla uyumlu. İki yerde kodun yapmadığını iddia ediyor (Bölüm 5). | **TAMAMLANDI** (düzeltmelerle) |

**Plan Bölüm 17 ihlali:** Plan "Faz 6 düşürülmez, yarısını teslim etmek yanlıştır"
diyor. Hiçbir faz resmen düşürülmedi ama Faz 6'nın ayırt edici özelliği (olay
korelasyonu) fiilen çalışmıyordu. Bu, planın uyardığı hatanın tam olarak
gerçekleşmiş hâlidir — sadece "düşürme" değil "yarım teslim" biçiminde.

---

## 4. Bulgu listesi

### KRİTİK

**K-1 · `role` alanı sessizce düşürülüyor → Faz 8'in tamamı ölü** ✅ DÜZELTİLDİ
`src/modules/watcher/VariableWatcher.cpp:145` (`updateItem`), çağıran `src/bridge/Backend.cpp:4176`
`updateItem()` `label/address/type/format/scale/offset/unit/enabled/color/laneIndex`
işliyordu ama **`role`'ü değil**. `applyWatchPresets()` tam da `role`'ü bu yolla
set etmeye çalışıyordu. Sonuç zinciri: `WatchItem::role` daima boş →
`TimeSeriesRuleEngine::itemMatchesRule()` `appliesToRole` kurallarında hiç
eşleşmiyor → `heap_leak_drift`, `stack_headroom_critical`,
`watermark_downward_trend` **asla tetiklenemez**; ayrıca `WatchProfile` c12
sütununa boş rol yazıyor → `compareWatchProfiles()`'ın `findByRole()`'ü her
metrik için `nullptr` → `ProfileCompareDialog` **tamamen boş**.
*Kanıt:* kod okuma + mutasyon T7 ("rol eşleşmesini yok say") hiçbir testi
bozmadı, yani bu davranış test edilmiyordu.
*Düzeltme:* `role` (ve `regionBytes`) eklendi; `applyWatchPresets()` artık
`format`/`offset`'i de aktarıyor ve reddedilen `addAddress()`'i kontrol ediyor.
Regresyon testi: `TestTimeSeriesRuleEngine::ruleDoesNotMatchAnItemWithADifferentOrEmptyRole`.

**K-2 · Olay ekseni ile örnek ekseni iki ayrı saat (61 ms kayma)** ✅ DÜZELTİLDİ
`src/modules/debug/DebugLinkWorker.cpp:65` (`m_sessionClock.start()`) ↔ `src/bridge/Backend.cpp:3450` (`m_eventLog.reset()`)
Örnek zaman damgaları worker'ın **soket bağlanınca** başlayan saatinden;
olay zamanları ise Backend'in **handshake bitince** sıfırlanan ayrı bir
`QElapsedTimer`'ından geliyordu. `TraceEventLog.h`'nin başlığı "ONE monotonic
clock shared with the session" diyordu — **yanlıştı**.
*Kanıt (canlı ölçüm, NUCLEO-H723ZG):*
```
first sample t (worker m_sessionClock) = 0.062382 s
same instant on eventLog clock         = 0.001298 s
=> sabit eksen kayması                 = 0.061085 s
```
*Etkisi:* (a) `watch_rules.json`'daki `inference_time_outlier` kuralının kapısı
`withinMs: 50` — kayma 61 ms > 50 ms olduğu için **eşzamanlı olan her olay
reddediliyordu**; (b) `watchPlotFrame()` pencere sonunu olay saatinden alıp
tampondaki örnekleri örnek saatinden okuduğu için canlı grafik en yeni 61 ms'i
hiç göstermiyordu; (c) kural motoruna `now` yanlış eksenden gidiyordu.
*Düzeltme:* `DebugLinkWorker::handshakeSucceeded()` artık örnek saatinin o
andaki değerini de bildiriyor; `DebugLink::sessionElapsedAtOpen()` bunu
sunuyor; `TraceEventLog::reset(originS)` olay eksenini aynı orijine oturtuyor.
Kalan hata = kuyruklu sinyal gecikmesi (< 1 ms).
Regresyon testi: `TestTraceEventLog::resetWithOriginPutsEventsOnTheSampleClockAxis`
(mutasyon V1 ile doğrulandı).

**K-3 · Bir izleme oturumundan sonra ST-Link kilitleniyordu — KÖK NEDEN BULUNDU** ✅ DÜZELTİLDİ
`src/modules/debug/GdbServerProcess.cpp` (`spawn`, `stop`, `stopBlocking`, hazır-olma tespiti)

*Belirti:* temiz biten bir oturumdan sonra **sonraki her** bağlantı denemesi
`Target USB comms error` ile başarısız oluyordu; 10/20/30 sn beklemek
düzeltmiyordu; `STM32_Programmer_CLI` de `DEV_USB_COMM_ERR` veriyordu — yani
prob USB seviyesinde kilitliydi ve **tüm** ST araçlarını etkiliyordu. Artık
gdbserver süreci kalmadığını da doğruladım, yani süreç sızıntısı değildi.

*Kök neden:* gdbserver `-e` (`--persistent`) ile başlatılıyordu. Persistent
modda sunucu biz detach ettikten sonra da dinlemeye devam eder, dolayısıyla
**onu bizim öldürmemiz gerekiyordu**. Windows'ta `QProcess::terminate()` bir
konsol sürecine ulaşamaz (pencere yok, WM_CLOSE gidecek yer yok), bu yüzden
2 sn sonra devreye giren `kill()` = `TerminateProcess()` çalışıyordu. Sunucu
USB tanıtıcısını kapatmaya fırsat bulamadan öldürülünce ST-Link'in ucu askıda
kalıyordu.

*Düzeltme (üç parça):*
1. `-e` kaldırıldı — sunucu biz detach edince kendiliğinden ve temiz kapanıyor
   (logda `Shutting down... Exit.` görülüyor).
2. Hazır-olma tespiti TCP yoklamasından **sunucunun kendi
   "Waiting for debugger connection" satırına** çevrildi. Bu şart: persistent
   olmayan bir sunucu, yoklama bağlantısı kapanınca kendini kapatırdı.
3. `stop()`/`stopBlocking()` önce doğal çıkışı bekliyor (`waitForFinished`,
   3 sn), `terminate()`/`kill()` yalnızca son çare; zorla öldürme durumunda
   kullanıcı log'dan uyarılıyor.
Ayrıca `DEV_USB_COMM_ERR` / `Target USB comms error` artık hata işaretleri
listesinde ve kullanıcıya ham sunucu metni yerine **"kabloyu çıkarıp takın,
bu durumdan yazılımla çıkılamaz"** deniyor.

*Kanıt (canlı, kart takılı):* düzeltme öncesi 2. oturum **her seferinde**
başarısızdı. Düzeltmeden sonra arka arkaya **3 oturum** sorunsuz açıldı
(939/929/937 Hz), artık gdbserver süreci kalmadı, ve hemen ardından
`STM32_Programmer_CLI -c port=SWD mode=HotPlug` temiz bağlandı:
`Device ID 0x483 / STM32H72x-STM32H73x`.

**K-5 · Kapanış yolunda segfault (`QProcess` null dereference)** ✅ DÜZELTİLDİ
`src/modules/debug/GdbServerProcess.cpp` — `stop()` / `stopBlocking()`
`waitForFinished()` `QProcess::finished` sinyalini **eşzamanlı** dağıtır;
`onProcessFinished()` bu sırada `m_process`'i `nullptr` yapıp `deleteLater()`
çağırıyor. Dönüşte kod `m_process->deleteLater()` demeye devam ediyordu →
**null dereference**. `stopBlocking()` `qApp::aboutToQuit`'e bağlı olduğu için
bu, uygulamanın **normal kapanış yolunda** bir çökmeydi ve bu denetimden önce
de mevcuttu (`stopBlocking` düzeltmemden bağımsız olarak aynı deseni
kullanıyordu); çıkış kodu kimse tarafından okunmadığı için görünmemişti.
*Kanıt:* `LiveProbe` her koşuda `exit=139` (SIGSEGV) veriyordu — sonuçları
yazdırdıktan sonra çöktüğü için fark edilmesi zordu. Düzeltmeden sonra 3/3
koşuda `exit=0`.
*Düzeltme:* tek bir null-güvenli `releaseProcess()` yardımcısı; her
`waitForFinished()` sonrası üye yeniden kontrol ediliyor.

**K-4 · Başarısız okumalar gerçek `0.0` olarak kaydediliyordu** ✅ DÜZELTİLDİ
`src/modules/watcher/VariableWatcher.cpp:270` (`onRawSamplesReady`)
`WatchSampler::decodeSample(..., nullptr)` — `ok` bayrakları **atılıyordu**.
Başarısız okuma `0.0` döndürdüğü için bu değer halka tamponuna ve `WatchStats`'a
gerçek bir ölçümmüş gibi giriyordu. `stack_headroom_critical` gibi bir kural
için sonuç "0 B stack boşluk kaldı" **yanlış alarmı** olurdu (kuralın hiç
tetiklenmemesinden daha kötü).
*Kanıt:* mutasyon P3 (`reply.ok` kontrolünü kaldır) hiçbir testi bozmadı —
`WatchSampler` **hiç test edilmemişti** (oysa saf bir sınıf, planın kendi
"saf sınıflar test edilir" ilkesinin ihlali).
*Düzeltme:* `ok` artık okunuyor; başarısız okuma **son geçerli değeri**
koruyor, sayaç `rateInfo()["readErrors"]` ile UI'ya açılıyor. Yeni test dosyası
`tests/TestWatchSampler.cpp` (4 test).

### YÜKSEK

**Y-5 · Kablo çekilince izleyici bunu hiç öğrenmiyordu** ✅ DÜZELTİLDİ
`src/modules/watcher/VariableWatcher.cpp:50` — `DebugLink::closed`'a **hiçbir şey
bağlı değildi**. USB çekilince/gdbserver çökünce `m_running` `true` kalıyor, UI
"örnekleme sürüyor" demeye devam ediyor, kullanıcıya hata gösterilmiyordu.
*Düzeltme:* `onLinkClosed()` eklendi — örneklemeyi durduruyor, kaydı düzgün
kapatıyor (özet/olay kuyruğu yazılıyor), kullanıcıyı uyarıyor ve takılı kalan
ELF-eşleşme kontrolünü sıfırlıyor.

**Y-6 · Örnekleme hızı tam sayı bölmesiyle kuantalanıyordu** ✅ DÜZELTİLDİ
`src/modules/debug/DebugLinkWorker.cpp:392` — `1000 / targetRateHz`.
600 Hz istendiğinde `1000/600 = 1` ms → **1000 Hz**; 500 Hz üstündeki **her**
istek 1000 Hz'e çöküyordu. 3000 Hz (planın Faz 6 hedefi) bu yoldan
**erişilemez** — sadece `targetRateHz=0` (maks) modu ulaşabilir.
*Düzeltme:* en yakın milisaniyeye yuvarlama (600 Hz → 2 ms → 500 Hz, istenene
1000 Hz'den daha yakın). Kuantalama fiziksel bir QTimer sınırı; kalıcı çözüm
maks moddur, bu yüzden davranış yorumda açıklandı.

**Y-7 · Birim ölçek dışa aktarımlarda tutarsızdı** ✅ DÜZELTİLDİ
`src/bridge/Backend.cpp` `exportWatchCsv()` / `exportWatchJson()`
Faz 8'de `WatchProfile` için düzeltilen hatanın **aynısı** iki dışa aktarımda
duruyordu: `unit` alanı "ms" derken sayılar ham mikrosaniyeydi.
*Düzeltme:* ikisi de artık `raw * scale + offset` uyguluyor (stddev yalnızca
`|scale|`), CSV başlığına birim yazılıyor, JSON'a `scale`/`offset` eklendi.
Regresyon testi: `TestWatchProfile::scaleAndOffsetAreAppliedToStoredNumbers`
(mutasyon F3/F3b ile doğrulandı).
*Bilinçli bırakılan:* `watchPlotFrame()` ve kural motoru **ham** değerle
çalışmaya devam ediyor. Kural eşikleri için bu doğru olan: z-skoru zaten
ölçekten bağımsız, mevcut eşiklerin hepsi `scale=1` kalemlere ait. Grafiğin Y
ekseni ise ölçek uygulanan bir birim etiketiyle ham sayı gösteriyor — ORTA
seviye bir tutarsızlık, "Kalan işler"e alındı (görsel doğrulama gerektiriyor).

**Y-8 · Test çalıştırıcısı hata detayını gösteremiyordu** ✅ DÜZELTİLDİ
`tests/CMakeLists.txt` — `qt_add_executable()` Windows'ta GUI alt sistemini
varsayıyor, stdout kopuk. Sonuç: test **başarısız olduğunda**
`ctest --output-on-failure` **hiçbir şey yazmıyordu**; hangi assertion'ın
patladığını öğrenmek imkânsızdı (bunu bizzat yaşadım, bisect etmek zorunda
kaldım). *Düzeltme:* `WIN32_EXECUTABLE FALSE`.

**Y-9 · `TracePlayer`: boş `scale` sütunu 0.0'a düşüyordu** ✅ DÜZELTİLDİ
`src/modules/watcher/TracePlayer.cpp:137` — `toDouble()` başarısızlıkta 0.0
döndürüyor; elle düzenlenmiş/yabancı bir CSV'de tüm seriyi **düzleştirirdi**.
`VariableWatcher::loadItems()` aynı alanı `toDouble(1.0)` ile okuyor — tutarsız.
*Düzeltme:* 1.0 fallback. Test: `blankScaleColumnFallsBackToOneNotZero`.

**Y-10 · `TraceEventLog` sınırsız büyüyordu** ✅ DÜZELTİLDİ
`src/modules/watcher/TraceEventLog.cpp` — her inference paketi bir olay ekliyor,
liste hiç kırpılmıyordu; üstelik `eventsBetween()` bu listeyi grafik hızında
(25 Hz) doğrusal tarıyor. Uzun oturumda hem bellek hem CPU büyür.
*Düzeltme:* `kMaxEvents = 20000` tavanı, en eskiler düşüyor. Test:
`eventListIsCappedAndKeepsNewest`.

### ORTA

| # | Yer | Sorun | Durum |
|---|---|---|---|
| O-11 | `GdbServerProcess.cpp` | `cubeprogrammer_bin_dir` boşken `-cp ""` gönderiliyordu; gdbserver `Couldn't locate STM32CubeProgrammer` yazıp çıkıyor ve bu metin hiçbir hata işaretine uymuyordu. | ✅ `-cp` artık koşullu; marker listesine eklendi |
| O-12 | `GdbServerProcess.cpp` | `"Cannot"` işareti çok genişti (zararsız bir "Cannot enable SWO" tüm açılışı düşürebilirdi). | ✅ `"Cannot connect"` / `"Cannot open"` olarak daraltıldı |
| O-13 | `TraceBuffer.cpp` | `rawWindow()` yorumu "O(range)" diyordu, gerçekte tüm tamponu tarıyordu (4 Hz'de 120k slot × kalem). | ✅ İkili arama + ileri yürüyüş |
| O-14 | `Backend.cpp` | `exportWatchCsv()` boş tamponda 800 satır boş hücre yazıp `true` dönüyordu. | ✅ Veri yoksa reddediyor ve açıklıyor |
| O-15 | `TracePlayer.cpp` | Bozuk satır sayısı `m_lastError`'a yazılıyordu ama `load()` `true` döndüğü için kimse okumuyordu (sessiz veri kaybı). | ✅ Ayrı `loadWarning()`, kullanıcıya bildiriliyor |
| O-16 | `DebugLink.cpp:222` | Soket beklenmedik kapanınca `m_refCount` zorla 0'a çekiliyor; sahipler hâlâ referans tuttuğunu sanıyor, sonraki `release()` Debug build'de `Q_ASSERT` ile uygulamayı düşürür. | ❌ Açık — bkz. Kalan işler |
| O-17 | `Backend.cpp` | 3332 → 4320 satır (+%30); cephe sınıfı tanrı sınıfa dönüyor. | ❌ Açık — mimari karar sizin |
| O-18 | `VariableWatcher.cpp` | `maybeCheckElfMatch()` cevap gelmezse takılı kalıyordu. | ✅ `onLinkClosed()` sıfırlıyor (zaman aşımı hâlâ yok) |
| O-19 | `watch_rules.json` | Tetiklenemeyecek iki `stackWatermark` kuralı etkin görünüyordu. | ✅ `"enabled": false` + gerekçe; motor bu bayrağı uyguluyor (testli) |
| O-20 | `Backend.cpp` | Grafik Y ekseni ham sayıyı ölçekli birim etiketiyle gösteriyordu. | ✅ Ölçek uygulanıyor; imleçte `scaledValue` ayrıca veriliyor |

### TEMİZ ÇIKAN ALANLAR

Aşağıdakileri kırmaya çalıştım, kıramadım:

- **Gözlemci ilkesi (beyaz liste).** `M`, `Z/z`, `G/P`, `vCont;t` engelli;
  `isAllowedOutgoing`'i devre dışı bırakan ve `M`'yi beyaz listeye ekleyen iki
  ayrı mutasyonun **ikisi de** test tarafından yakalandı. `\x03` gibi ham
  bayt gönderen tek yol `sendRaw()` ve o da beyaz listeden geçiyor.
- **gdbserver başlatma argümanları.** `--help` çıktısıyla teyit: `-g`=attach,
  `-e`=persistent, `-d`=SWD. `-k` (`--initialize-reset`) ve `--halt`
  **kullanılmıyor** ve `Q_ASSERT` ile korunuyor. Güvenli.
- **"Hedef hiç durmadı" iddiası.** Tek bir DHCSR okuması değil: 4 Hz sağlık
  kontrolü 20 sn boyunca (≈80 okuma) çalıştı, `S_HALT` hiç görülmedi.
- **Zaman damgası sözleşmesi.** `t` gerçekten ilk `m` paketi yazılmadan hemen
  önce (`pumpNextRequest`, `nextIndex==0`), `skewUs` gerçekten son cevap
  ayrıştırılana kadar. Kodda birebir uygulanmış. Canlı `skewUs` = 304 µs.
- **`ElfTargetMatcher`.** Thumb biti doğru, eksik sembol `Unknown` (ne match
  ne mismatch), VTOR'un işaret ettiği yeri okuduğu için RAM'e taşınmış vektör
  tablosu yanlış alarm üretmiyor. İki mutasyon da yakalandı.
- **`WatchStats` (Welford).** Bağımsız referansla karşılaştırdım, doğru; iki
  ayrı bozma mutasyonu yakalandı.
- **Kural motoru matematiği.** Eğim `sxy/sxx`, R² `sxy²/(sxx·syy)` — ders
  kitabı doğrusu. Yanlış formüle çeviren üç mutasyon da yakalandı.
- **`nm` parser.** Gerçek H7 ELF'iyle doğruladım: `A` tipi değer olarak
  işaretleniyor, küçük harf (static) tipler doğru okunuyor.
- **gdbserver süreç temizliği.** Uygulama/koşum aracı kapandıktan sonra
  Task Manager'da artık süreç yok.

---

## 5. Dokümanın yanılttığı yerler

1. **`TraceEventLog.h` başlığı (Faz 6):** "All timestamped in seconds against
   **ONE monotonic clock shared with the session**... so event markers line up
   with TraceBuffer sample times on the same axis." → **Yanlıştı.** İki ayrı
   saat vardı, canlı ölçülen kayma 61 ms. `Backend.cpp:3448`'deki yorum da
   ("both starting fresh at link-open") aynı yanlışı tekrar ediyordu. İkisi de
   düzeltildi.

2. **`findings.md` §17.3:** RegionScan'in eksikliği dürüstçe anlatılmış ve
   `stack_headroom_critical` + `watermark_downward_trend`'in eşleşmeyeceği
   **açıkça** yazılmış — bu iyi. Ama **`heap_leak_drift`'in de aynı şekilde
   ölü olduğu söylenmiyor**; oysa `role` hiç set edilmediği için o da hiç
   tetiklenemiyordu. Yani "4 kuraldan 2'si hazır" izlenimi veriliyor, gerçek
   "4'ünden 0'ı" idi.

3. **`findings.md` §17.4 tablosu:** "ProfileCompare — eşleşmeyen kalem
   'karşılığı yok' ✅ (kod yolunda, `hasMatch` alanıyla)". Kod yolu vardı ama
   `role` boş olduğu için **her** satır "karşılığı yok" dönüyordu; yani
   özellik ✅ değil, fiilen çalışmıyordu.

4. **`WatchSampler.h:20`:** "(disabled/RegionScan/failed chunk) come back as
   value 0.0, ok=false" — teknik olarak doğru, ama **`ok` hiçbir çağıran
   tarafından okunmuyordu**. Doküman doğru bir sözleşme tarif edip
   uygulanmadığını söylemiyordu.

5. **`TraceBuffer.h:55`:** `rawWindow()` için "Cost is O(samples in range)" —
   gerçekte tüm tamponu tarıyor.

6. **Faz kapanış cümleleri genel olarak:** "Faz N tamamlandı" ifadesi, planın
   Bölüm 13'teki kendi tanımıyla çelişiyor (S1'e bakınız). "Kod hazır, canlı
   doğrulama bekliyor" daha dürüst bir etiket olurdu.

7. **`CLAUDE.md` ADR bölümü** genel olarak kodla uyumlu — özellikle
   `retain()/release()`, beyaz liste, "aile-özel hiçbir şey hardcode edilmez"
   maddelerini kodda doğruladım. Tek istisna, zaman damgası maddesinin
   "tek eksen" ima etmesi (yukarıdaki 1. madde).

---

## 6. Ertelenen maddeler hakkında karar

| Ertelenen | Karar | Gerekçe |
|---|---|---|
| **RegionScan canlı decode'u** | **KABUL EDİLEMEZ (kısmen)** | Erteleme kararı **makul** — ayrı bir iş, `applyWatchPresets()`'ın 0/kullanılamaz kalem eklememesi doğru mühendislik. Kabul edilemez olan, **çalışmayacağı bilinen 2 kuralı `watch_rules.json`'da tutmak**: dosya ürünle birlikte dağıtılıyor, şema dokümante edilmiş, ama o kurallar hiçbir koşulda tetiklenemez. Ya kurallar `"enabled": false` benzeri bir işaretle kapatılmalı ya da JSON'da açık bir `_status` notu olmalı. Şu hâliyle dosya çalışan bir şey vaat ediyor. Faz 5'in `stack_paint.c`'si de bu yüzden şu an **hiçbir işe yaramıyor** — veriyi üretiyor, okuyan yok. |
| Faz 4: 1000 Hz / 60 sn dayanıklılık + bellek | **KABUL EDİLEBİLİR ama artık gereksiz** | Ben 1000 Hz'de 20 sn ölçtüm: **933 Hz**, plan şartı (≥800 Hz) sağlanıyor. 60 sn/bellek kısmı hâlâ açık ama risk düşük (halka tamponu sabit boyutlu ve 64 MB tavanı kodda). |
| Faz 5: gerçek pipeline + `inf_us` ±%5 | **KABUL EDİLEMEZ** | Bu, Faz 5'in **tek** doğrulama kriteriydi. Firmware tarafı hiç uçtan uca çalıştırılmadı; `g_ai_*` sembollerinin gerçek bir pipeline çıktısında göründüğü **hiç görülmedi**. Elimdeki gerçek H7 ELF'lerinde bu semboller yok (eski derlemeler). Ucuz bir test: bir pipeline koş, `nm | grep g_ai_`. |
| Faz 6: 3000 Hz akıcılık | **KABUL EDİLEMEZ (teknik olarak imkânsızdı)** | Sadece ertelenmemiş; Y-6 yüzünden timer yoluyla 1000 Hz üstü **erişilebilir değildi**. Yani kriter ertelenmiş değil, karşılanamaz durumdaydı ve bu fark edilmemişti. |
| Faz 6: imleç okuma / olay hizalaması | **KABUL EDİLEMEZ** | Tam da K-2'nin gizlendiği yer. Bir kez canlı bakılsa 61 ms'lik kayma görülürdü. |
| Faz 7: 4× hızda UI + ST-Link'siz demo | **KABUL EDİLEBİLİR** | Kod yolu testli, demo CSV gerçek ve dağıtılıyor. Yine de jüri öncesi **mutlaka** bir kez elle denenmeli — bu, planın "demo güvenlik ağı" dediği şey. |
| Faz 7: `AnalysisScreen.qml`'e "İzleme Profilleri" sekmesi | **KABUL EDİLEMEZ** | Bu "ertelendi" bile denmemiş, sessizce atlanmış. Profiller DB'ye yazılıyor ama uygulamada **listelenebilecekleri tek yer** `ProfileCompareDialog`; Analiz ekranından görünmüyorlar. |
| Faz 8: sızıntı demosu / 2 model karşılaştırma | **KABUL EDİLEMEZ** | Yapılsaydı K-1 anında ortaya çıkardı (karşılaştırma tablosu boş dönerdi). Ertelenen doğrulama, gerçek bir hatayı sakladı — "ertelemek ucuzdur" varsayımının somut karşı örneği. |
| Faz 3: canlı "ELF Match" yeşil senaryosu | **KABUL EDİLEBİLİR** | Mantık testli, thumb biti doğru, gerçek ELF'te semboller doğrulandı. Risk düşük. |

**Genel değerlendirme:** ertelemelerin bir kısmı gerçek mühendislik kararı
(donanım yokluğu, kapsam). Ama bir örüntü var: **ertelenen doğrulamaların
neredeyse hepsi, ertelendikleri fazın tek gerçek hatasını saklıyordu.** Faz 8'in
canlı demosu K-1'i, Faz 6'nın olay hizalaması K-2'yi, Faz 4'ün canlı ölçümü
Y-6'yı gösterirdi. Birim testler bu hataların **hiçbirini** yakalayamazdı çünkü
hepsi katmanlar *arasındaki* bağlantıda yaşıyordu.

---

## 7. Fiilen düzeltilenler

Tümü `feature/register-inspector` dalında, derleme + `ctest` yeşil.

| # | Düzeltme | Dosya | Regresyon testi |
|---|---|---|---|
| 1 | `role`/`regionBytes` artık `updateItem()` ile set edilebiliyor | `VariableWatcher.cpp` | `ruleDoesNotMatchAnItemWithADifferentOrEmptyRole` |
| 2 | `applyWatchPresets()` `format`/`offset` aktarıyor, reddi kontrol ediyor | `Backend.cpp` | — |
| 3 | Olay ekseni örnek eksenine oturtuldu (61 ms kayma giderildi) | `DebugLinkWorker.*`, `DebugLink.*`, `TraceEventLog.*`, `Backend.cpp` | `resetWithOriginPutsEventsOnTheSampleClockAxis` |
| 4 | Başarısız okuma artık son geçerli değeri koruyor + `readErrors` sayacı | `VariableWatcher.*` | `TestWatchSampler` (4 test, yeni dosya) |
| 5 | `DebugLink::closed` işleniyor (kablo çekilmesi/çökme) | `VariableWatcher.*` | — (QObject, birim testlenemiyor) |
| 6 | Örnekleme hızı en yakın ms'ye yuvarlanıyor | `DebugLinkWorker.cpp` | — |
| 7 | CSV/JSON dışa aktarımı `scale`/`offset` uyguluyor | `Backend.cpp` | `scaleAndOffsetAreAppliedToStoredNumbers` |
| 8 | `TracePlayer` boş `scale` → 1.0 | `TracePlayer.cpp` | `blankScaleColumnFallsBackToOneNotZero` |
| 9 | `TraceEventLog` 20000 olay tavanı | `TraceEventLog.*` | `eventListIsCappedAndKeepsNewest` |
| 10 | Test exe'si konsol alt sistemine alındı | `tests/CMakeLists.txt` | — |
| 11 | Halka sarmalama içeriği test edildi | — | `wraparoundKeepsNewestSamplesAndTimes` |
| 12 | `maxReadBytes` bölme davranışı test edildi | — | `maxReadBytesLimitSplitsIntoSeparateRequests` |
| 13 | Eşik sınırı + çift yönlü olay kapısı test edildi | — | 2 yeni test |

**Test sayısı:** +14 test (1 yeni suite dahil). Kaçan 9 mutasyondan 8'i artık
yakalanıyor; 9.'su eşdeğer mutant olarak gerekçelendirildi.

**Mimari değişiklik yapmadım.** K-3 (ST-Link kilitlenmesi) ve `Backend`'in
bölünmesi (O-17) sizin kararınız — talimatınız gereği önce raporluyorum.

---

## 8. Kalan işler (öncelik sıralı)

| # | İş | Öncelik | Efor |
|---|---|---|---|
| 1 | **Faz 5 doğrulaması:** bir pipeline koş, `nm \| grep g_ai_` ile sembolleri gör, `g_ai_last_inference_us` ile UART `inf_us`'u ±%5 karşılaştır, boyut artışı <1 KB mi bak. Faz 5'in **tek** kabul kriteriydi ve hiç yapılmadı. | YÜKSEK | 2–3 saat |
| 2 | **Uçtan uca senaryo bir kez elle koşulmalı:** ELF → sembol → başlat → grafik → kaydet → durdur → oynat → profil kaydet → karşılaştır. K-1/K-2 sonrası artık anlamlı sonuç vermeli; grafiğin görsel akıcılığı da burada görülür. | YÜKSEK | 1 saat |
| 3 | **Demo kaydı üretin.** Dağıtılan `h7_demo_trace.csv`'nin tek kalemi `SysTick_VAL`: ne rolü var ne `inference_us` regex'ine uyuyor, dolayısıyla demo sırasında kural akışı **boş kalır**. Rol taşıyan (`heapEnd`, `inferenceUs`) bir kayıt alın. | YÜKSEK | 30 dk |
| 4 | RegionScan decode'u (`WatchSampler`'a bayt tarama + ayrı düşük hızlı döngü), ardından iki `stackWatermark` kuralını `"enabled": true` yapın. Faz 5'in `stack_paint.c`'si buna bağlı. | ORTA | 1 gün |
| 5 | `AnalysisScreen.qml`'e "İzleme Profilleri" sekmesi (planda vardı, sessizce atlanmıştı). **Bilinçli olarak yapmadım:** 5. sekme `_subTabs`/`_cols`/`rowsForIndex`/`boardColumn`/özet kartları/grafik/dışa aktarım adlandırmasını birden etkiliyor ve görsel doğrulama yapamıyorum. Göremediğim UI'yi göndermek bu raporun eleştirdiği hatanın aynısı olurdu. | ORTA | 2–4 saat |
| 6 | O-16: `refCount` zorla sıfırlama yerine sahiplere "link öldü" bildirimi; Debug build'de `Q_ASSERT` düşmesin. | ORTA | 2 saat |
| 7 | O-17: `Backend`'den `WatchFacade` ayrıştırılsın (4320 satır). | DÜŞÜK | 1 gün |
| 8 | `maybeCheckElfMatch()` için zaman aşımı. | DÜŞÜK | 30 dk |

## 9. Jüri demosu risk değerlendirmesi

Soru: *"bitirme jürisi önünde gerçekten gösterilebilir mi, hangi adımda patlama
riski var?"*

- **K-3 çözüldü — eski en yüksek risk kalktı.** İzleyiciden sonra flash/
  register/probe artık çalışıyor (canlı kanıtlandı), demo sırası serbest.
- **İkinci risk:** kural akışı (`WatchRuleFeed`) demo sırasında büyük olasılıkla
  **boş** kalacak. Dağıtılan `h7_demo_trace.csv`'nin tek kalemi `SysTick_VAL`;
  ne bir rolü var ne de `inference_us` regex'ine uyuyor. Yani "AI-farkındalıklı
  anomali tespiti" ekranda hiçbir şey göstermez. Demo öncesi ya rol taşıyan bir
  kayıt üretin ya da bu paneli demo dışında bırakın.
- **Düşük risk:** kayıttan oynatma yolu sağlam ve ST-Link'siz çalışıyor —
  planın "demo güvenlik ağı" fikri doğru kurulmuş. Bir kez elle denenmesi şart.
- **Kalan en yüksek risk artık "hiç uçtan uca denenmemiş olması".** Tek tek
  katmanlar kanıtlandı (canlı okuma, hız, dayanıklılık, saat ekseni, kural
  motoru, oynatma), ama tam zincir bir kez bile baştan sona çalıştırılmadı.
  Kalan işler #2 bunu kapatır ve jüri öncesi **mutlaka** yapılmalı.
- **Amaca hizmet:** özellik projenin asıl amacına (model dağıtımı/karşılaştırma)
  gerçekten hizmet ediyor — `inferenceUs`/`heapEnd` presetleri ve profil
  karşılaştırma tam olarak "iki modeli kıyasla" sorusuna cevap veriyor. K-1
  düzeltilmeden bu vaadin **hiçbiri** çalışmıyordu.

---

## 10. Canlı donanım doğrulama kaydı (NUCLEO-H723ZG, kart takılı)

Aşağıdakiler `LiveProbe` (gerçek `DebugLink`/`DebugLinkWorker`/
`GdbServerProcess` sınıflarını kullanan başsız koşum aracı) ile ölçüldü.
İzlenen adres `SysTick->VAL` (`0xE000E018`) — ELF/sembol gerektirmez, her
Cortex-M'de sabittir ve sürekli değişir.

### 10.1 Örnekleme başarımı

| Ölçüm | Sonuç | Plan şartı |
|---|---|---|
| 1000 Hz hedefte gerçekleşen | **927 – 941 Hz** (5 ayrı koşu) | ≥800 Hz ✅ |
| 60 sn sürekli | 56188 örnek, **936.5 Hz** | — ✅ |
| Ortalama RTT | 0.41 ms | — |
| `skewUs` (tek bloklu plan) | 304 – 383 µs | — |
| Kaçırılan deadline (60 sn) | 476 / 56188 (%0.85) | — |
| Değer gerçekten değişti | 56186 / 56188 | mantıklı ✅ |

### 10.2 Bellek (Faz 4'ün ertelenen kriteri)

60 sn boyunca 5 sn aralıkla ölçülen RSS: **12.4 MB — hiç değişmedi.**
Halka arabelleği sabit boyutlu ayrıldığı için büyüme yok. Planın "<100 MB
artış" şartı fazlasıyla sağlanıyor.

```
t+8s  12.4 MB    t+28s 12.4 MB    t+48s 12.4 MB
t+13s 12.4 MB    t+33s 12.4 MB    t+53s 12.4 MB
t+18s 12.4 MB    t+38s 12.4 MB
t+23s 12.4 MB    t+43s 12.4 MB
```

### 10.3 Gözlemci ilkesi

60 sn kesintisiz örnekleme + 4 Hz DHCSR sağlık kontrolü (≈240 okuma) boyunca
`S_HALT` **hiç** görülmedi, `S_RESET_ST` **hiç** görülmedi. Bu, "hedef hiç
durmadı" iddiasının tek bir anlık okumaya değil, oturum boyu sürekli izlemeye
dayandığı anlamına gelir.

### 10.4 Zaman ekseni (K-2 düzeltmesinin kanıtı)

Aynı ölçüm, aynı kod yolu, düzeltme öncesi ve sonrası:

| | Eksenler arası sabit kayma |
|---|---|
| Düzeltme **öncesi** | **+61.1 ms** |
| Düzeltme **sonrası** (5 koşu) | **−0.4 … −1.1 ms** |

Kalan sub-milisaniye fark, worker'ın `handshakeSucceeded` sinyalinin ana
thread'e kuyruklu teslim süresidir — yapısal değil, gürültü.

### 10.5 ST-Link sağlığı (K-3 düzeltmesinin kanıtı)

| Senaryo | Düzeltme öncesi | Düzeltme sonrası |
|---|---|---|
| 1. oturum | ✅ açılıyor | ✅ açılıyor |
| 2. oturum (arka arkaya) | ❌ `Target USB comms error` | ✅ açılıyor |
| 3. oturum | ❌ (10/20/30 sn beklemek de çözmüyor) | ✅ açılıyor |
| Sonra `STM32_Programmer_CLI` | ❌ `DEV_USB_COMM_ERR` | ✅ `Device ID 0x483 / STM32H72x-H73x` |
| Artık gdbserver süreci | yok (temiz) | yok (temiz) |
| Süreç çıkış kodu | 139 (SIGSEGV, K-5) | 0 |

### 10.6 Doğrulanamayanlar

Dürüstlük için: aşağıdakiler **hâlâ ölçülmedi**, "muhtemelen iyidir"
denmiyor — GUI otomasyonum olmadığı için ekran üzerinden yapılması gerekiyor:

- Grafiğin görsel akıcılığı, zoom/kaydırma, imleç okuma doğruluğu.
- Kayıttan oynatmanın 4× hızda UI'yi dondurup dondurmadığı.
- UART monitörü ile izleyicinin **aynı anda** çalışması (Faz 1'in "gdbserver
  aktifken VCP susuyor" bulgusu hâlâ geçerli mi).
- Register Inspector A→B diff'i, Analiz CSV/PDF, Fabrika Simülasyonu'nun
  ekran üzerinden fonksiyonel testi.
