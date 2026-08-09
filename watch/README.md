# watch/ — Değişken İzleyici veri dosyaları

Bu klasör, Değişken İzleyici (Variable Watcher) özelliğinin kod-dışı veri
dosyalarını barındırır. Tam tasarım: `docs/variable_watcher_plan.md`;
canlı doğrulama sonuçları: `docs/variable_watcher_findings.md`.

## `watch_rules.json` — `TimeSeriesRuleEngine` kuralları

`src/modules/registers/RuleEngine.h`'in okuduğu `svd/rules.json` ile
**karıştırılmaz** — o TEK bir register snapshot'ının o anki tutarlılığını
sorar, bu dosya SON N SANİYEDEKİ zaman serisi davranışını sorar. İki motor
asla birleştirilmez (bkz. `CLAUDE.md`).

```json
{
  "id": "stack_headroom_critical",
  "severity": "error",              // "info" | "warning" | "error"
  "appliesToRole": "stackWatermark", // VEYA appliesToLabelRegex — ikisi birden değil
  "type": "threshold",               // "threshold" | "zscore" | "drift"
  "op": "<", "value": 512, "sustainMs": 1000,
  "message": "{label}: yalnizca {value} B stack bosluk kaldi (esik {threshold} B)"
}
```

- **threshold**: `value op sabit`. `sustainMs=0` ise yalnızca en son örnek
  kontrol edilir; `sustainMs>0` ise o süre boyunca **her** örnek koşulu
  sağlamalı (tek bir eski/yetersiz örnek yanlış pozitif üretmez).
- **zscore**: `windowMs` penceresinde ortalama/std sapma hesaplanır,
  `minSamples`'tan az örnek varsa değerlendirilmez; en son örneğin
  `|x-ort|/std > k` olması gerekir.
- **drift**: pencere üzerinde doğrusal regresyon; eğim işaretine göre
  (`minSlopePerSec` pozitifse artış, negatifse azalış) eşik + `R² >= minR2`
  kapısı — gürültülü bir seride tesadüfi eğimi eler.
- **gate** (opsiyonel): ihlal, yalnızca `eventKind` tipinde bir olay
  `withinMs` içinde varsa raporlanır.
- Mesaj şablonundaki `{label}` `{value}` `{threshold}` `{mean}` `{z}`
  `{slope}` `{r2}` `{windowSec}` yer tutucuları koşul tipine göre doldurulur.

Motor **ML tabanlı değildir** — her ihlal `detail` alanında hangi sayının
hangi eşiği nasıl aştığını taşır (aritmetik gösterilir, "model öyle dedi"
denmez).

## `watch_presets.json` — AI-farkındalıklı otomatik izleme listesi

Semboller ELF'ten gelir; bu dosyada **hiçbir kart adı geçmez** — yeni bir
kart eklemek `.svd` + `boards.json` + linker template işidir, bu dosyayı
etkilemez.

- `always: true` → sembol tablosundan bağımsız her zaman denenir (`hal_timebase`,
  `core_memory`).
- `requiresAnySymbol` → listelenen sembollerden **en az biri** yüklü ELF'te
  varsa preset uygulanır (`xcubeai_runtime` — X-CUBE-AI kullanmayan bir
  firmware'de gösterilmez).
- Bulunamayan tek tek semboller **sessizce atlanır** — preset kısmen
  uygulanır, hata üretilmez.
- `kind: "regionScan"` (stack watermark): `regionFrom` bir **alternatif
  listesi** — ilk çözülen kazanır (`_sstack` yoksa `_estack-_Min_Stack_Size`
  ifadesi hesaplanır). `_Min_Stack_Size` bir `A` tipi (Absolute) sembol
  olduğundan **değeri** kullanılır, adresi değil. İfade çözücü yalnızca
  `<sembol>` / `<sembol>-<sembol>` / `<sembol>+<sembol>` biçimlerini
  destekler — genel bir ifade motoru değildir.

> **Bilinen sınır (Faz 8 itibarıyla):** `regionScan` kalemleri için adres
> aralığı doğru çözülür (`WatchPresetMatcher`), ancak bu aralığın canlı
> olarak taranıp bir "N bayt boş" watermark değerine dönüştürülmesi henüz
> örnekleme katmanına bağlanmadı — `WatchPlanBuilder::buildRegionScans()`
> okuma planını doğru parçalıyor, ama `WatchSampler` şu an RegionScan
> kalemlerini `0.0/ok=false` olarak döndürüyor (bkz. `WatchSampler.h`
> yorumu). Bu, ayrı bir düşük-hızlı örnekleme döngüsü + bayt-tarama
> decode mantığı gerektiren, bilinçli olarak ertelenmiş bir iş — detay
> `docs/variable_watcher_findings.md` Bölüm 17'de.

## `demo/h7_demo_trace.csv`

Gerçek NUCLEO-H723ZG'den kaydedilmiş 30 saniyelik bir izleme kaydı (Faz 7).
`Backend::demoTracePath()` bunu bulur; "Demo Kaydını Oynat" butonu ST-Link
takılı olmasa bile ekranın tam çalışmasını sağlar (bitirme sunumu güvenlik
ağı). CSV biçiminin tek doğruluk kaynağı: `TraceRecorder.cpp`.
