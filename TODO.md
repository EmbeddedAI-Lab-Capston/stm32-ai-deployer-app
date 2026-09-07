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

---

## Kalan işler (öncelik sıralı)

Tam gerekçe ve efor tahmini için `docs/variable_watcher_review.md` Bölüm 8'e
bakın — burası yalnızca özet, oradan senkron tutun:

1. **YÜKSEK — Faz 5 doğrulaması.** Gerçek bir pipeline (.tflite → derle →
   flash) koşturup `nm | grep g_ai_` ile sembollerin göründüğünü, UART
   `inf_us` ile `g_ai_last_inference_us`'ın ±%5 uyuştuğunu, firmware boyut
   artışının <1 KB olduğunu doğrulayın. Faz 5'in hiç yapılmamış **tek**
   kabul kriteri.
2. **YÜKSEK — Uçtan uca senaryo elle koşulmalı.** ELF yükle → sembol ekle →
   başlat → grafik → kaydet → durdur → oynat → profil kaydet → karşılaştır.
   Katmanlar tek tek kanıtlandı, tam zincir hiç çalıştırılmadı.
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
