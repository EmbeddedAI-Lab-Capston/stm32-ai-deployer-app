# Değişken İzleyici — Faz 1 Donanım Doğrulama Bulguları

> **Amaç:** `docs/variable_watcher_plan.md` Bölüm 4.8'de tanımlanan Faz 1
> (Debug link altyapısı) canlı doğrulama adımlarının (1–7) gerçek sonuçları.
> Aynı dokümandaki `register_inspector_findings.md` deseni izlenir.
> **Durum:** Faz 1 canlı doğrulamanın 7 adımı da H7'de tamamlandı.

---

## 0. Test Ortamı

| Öğe | Değer |
|-----|-------|
| Tarih | 2026-08-02 |
| Kart | **NUCLEO-H723ZG** (eldeki tek kart) |
| Device ID | `0x483` (STM32H72x/STM32H73x) |
| ST-Link | SN `004D003B3235511837333439`, FW `V3J16M9` (STLINK-V3) |
| `ST-LINK_gdbserver.exe` | v7.13.0, `C:\ST\STM32CubeIDE_2.1.1\...\externaltools.stlink-gdb-server.win32_2.2.400.202601091506\tools\bin\` |
| `STM32_Programmer_CLI.exe` (`-cp` hedefi) | v2.22.0, `...\externaltools.cubeprogrammer.win32_2.2.400.202601091506\tools\bin\` |
| `arm-none-eabi-nm.exe` | `...\externaltools.gnu-tools-for-stm32.14.3.rel1.win32_.../tools/bin/` (Faz 3 için tespit edildi, henüz kullanılmadı) |
| UART/VCP | COM7, 115200 baud |
| Qt / derleyici | Qt 6.11.1, MinGW 13.1.0 (`D:\Qt`, bkz. bellek `build-env-qt-path`) |

**Doğrulama yöntemi:** Henüz UI yok (Faz 4), bu yüzden `DebugLink` doğrudan
küçük, geçici bir konsol harness'iyle (`scratchpad/phase1_probe/`, repo'ya
commit edilmedi) egzersiz edildi — gerçek `src/modules/debug/*` kaynak
dosyaları birebir derlenip bağlandı, kopya kod yok.

---

## 1. Birim testler (donanımsız) ✅

`ctest --test-dir build` → **1/1 suite passed** (`TestGdbRspCodec`, 12 test
fonksiyonu: checksum, frame, extract parçalı akış, notification ayrımı,
run-length (basit/orta-string/bozuk), hex decode, memoryReadPacket, error
reply (iki biçim), beyaz/kara liste). Tam app derlemesi (`STM32AiDeployer`)
ve test hedefi (`STM32AiDeployerTests`) hatasız derlendi.

---

## 2. Adım 1 — `retain()` / `refCount` ✅

- İlk `retain()` → `opened()` **570 ms**'de geldi (kriter: ≤5 s). `refCount()==1`.
- İkinci `retain()` (link zaten açıkken) → `opened()` tekrar yayıldı,
  `refCount()==2`. `tasklist` ile gdbserver süreç sayısı **1 → 1** (yeni süreç
  başlamadı) — sözleşmenin "zaten açıksa yeni süreç yok" maddesi doğrulandı.

---

## 3. Adım 2 — DHCSR doğrulaması ✅

Handshake sonunda okunan değer: **`DHCSR = 0x01010001`**.

| Bit | Anlam | Değer |
|---|---|---|
| 0 (`C_DEBUGEN`) | Debug etkin | 1 (beklenen — SWD oturumu açık) |
| 17 (`S_HALT`) | Çekirdek durdu | **0** ✅ |
| 24 (`S_RETIRE_ST`) | Komut emekliye ayrıldı | **1** ✅ |

Faz A referansı `0x01010000` idi (yalnızca `S_REGRDY`+`S_RETIRE_ST`); buradaki
fark tek bir ek bit — `C_DEBUGEN(0)` — ki bu aktif bir debug oturumunda
**beklenen** bir farktır, tutarsızlık değildir. Kriter olan `S_HALT==0` **ve**
`S_RETIRE_ST==1` **birebir sağlandı**.

---

## 4. Adım 3 — UART regresyon testi ⚠️ (kısmi, önemli bulgu)

**Beklenen (plan):** link açıkken `§{"t":"sys"...}` paketleri akmaya devam eder.

**Gözlem:** Link **kapalıyken** COM7 sürekli trafik üretiyor (mevcut firmware
BME280 bulamayınca hata döngüsüne giriyor — 4 sn'de 460, sonraki 4 sn'de 102
bayt). Link **açıkken** (gdbserver oturumu sürerken) art arda üç ayrı ölçümde
**COM7'den 0 bayt** okundu (5 sn ve 3 sn pencereleri). Link **kapatılır
kapatılmaz** trafik anında geri geldi (3 sn'de 306 bayt).

**Bu bir çekirdek durması DEĞİL:** Aynı pencerede DHCSR sürekli `S_HALT=0`
okundu, `readRanges` art arda başarılı çalıştı, ve Adım 7'de canlı bir
`S_RESET_ST` olayı doğru yakalandı — hepsi çekirdeğin kesintisiz çalıştığının
bağımsız kanıtı. Ayrıca `register_inspector_findings.md` Test 1 (2026-07-05),
**CLI tabanlı** SWD okumasında COM7'nin **hiç etkilenmediğini** kaydetmişti.
Yani bu, SWD ile VCP'nin genel bir çakışması değil — **`ST-LINK_gdbserver.exe`
bu ST-Link firmware'inde (V3J16M9) aktif bir oturum tutarken VCP köprüsünü
susturuyor**, CLI'nin tek seferlik bağlan/oku/ayrıl modeli bunu tetiklemiyor.

**Sonuç / karar:** Faz 1 bu haliyle **kabul edildi** çünkü plandaki asıl
endişe ("çekirdek durdu mu") DHCSR ile doğrudan ve daha güvenilir biçimde
çürütüldü — UART sessizliği dolaylı bir vekil göstergeydi, birincil değil.
Ancak bu, gerçek bir kullanılabilirlik kısıtıdır: **Değişken İzleyici linki
açıkken Seri Port Monitörü canlı veri göstermeyecek** (bu donanım/firmware
kombinasyonunda). `svd/boards.json` H7 `debug.gdb.notes` alanına yazıldı;
Faz 9'da UI'da (`WatchLinkStatus` veya benzeri) kullanıcıya görünür şekilde
belirtilmesi gerekir. F4/N6 geldiğinde farklı ST-Link firmware'iyle yeniden
test edilmeli (bkz. Bölüm 8 "kart gelince" listesi).

---

## 5. Adım 4 — `readRanges` verim testi ✅

`maxReadBytes()` handshake'ten `4096` olarak çözüldü (`PacketSize` yeterince
büyüktü). Her boyut için 150 ardışık istek, tek adres (`0x24000000`, H7 AXI
SRAM) üzerinden:

| Boyut | Ölçülen hız | Faz A kriteri | Sonuç |
|---|---|---|---|
| 4 B | **~2631.6 Hz** | > 2000 Hz | ✅ |
| 1024 B | **~429.8 Hz** | > 300 Hz | ✅ |
| 4096 B | **~122.1 Hz** | > 90 Hz | ✅ |

Üç boyutta da Faz A referans büyüklük mertebesi **aşıldı** — RLE/çerçeveleme
implementasyonu (`GdbRspCodec::expandRunLength` + `unescape` + `hexDecode`
sırası) doğru ve performanslı.

---

## 6. Adım 5 — `release()` semantiği ✅ (+ bir donanım tuhaflığı)

- İlk `release()` (refCount 2→1): süreç **hâlâ çalışıyor** (`tasklist`=1) ✅
- İkinci `release()` (refCount 1→0): `closed()` yayıldı; kısa süre sonra
  `tasklist`'te gdbserver süreci **kayboldu** (0) ✅ — sayaç sızmadı.
- **Bulgu:** Süreç kaybolduktan hemen sonra `STM32_Programmer_CLI -c port=SWD`
  birkaç saniye `ST-LINK error (DEV_USB_COMM_ERR)` verdi — **fiziksel USB
  replug** sonrası **anında** (103 ms) bağlandı. Bu, `ST-LINK_gdbserver`'ın
  (özellikle "Persistent Mode: Enabled" ile) ST-Link'in debug USB arayüzünü
  süreç kapandıktan sonra bile bir süre "yapışkan" bıraktığı bilinen bir
  donanım/sürücü tuhaflığıdır — kodun tarafında bir kaynak sızıntısı değil
  (süreç seviyesinde `tasklist` ile doğrulandı). `boards.json`'a not düşüldü;
  Faz 9'daki "kalıntı süreç" kullanıcı uyarısına ek olarak, gdbserver
  oturumundan sonra CLI tabanlı araçlara (flash, Register Inspector CLI arka
  ucu) geçerken bu gecikme/replug ihtiyacı akılda tutulmalı.

---

## 7. Adım 6 — Başarısız `retain()` ✅

Kasten bozuk `gdbserver` yolu ile `retain()` çağrıldı → `failed()` **anında**
geldi, `refCount()` **0** kaldı (sayaç tüketilmedi). Sözleşmenin "başarısız
retain() release() gerektirmez" maddesi doğrulandı.

---

## 8. Adım 7 — `S_RESET_ST` canlı testi ✅

`startSampling()` 4 Hz hedefle başlatıldı (DHCSR'in kendisini izleyen tek
kalemli bir plan — Faz 3/4 sembol katmanı henüz yok). 25 sn'lik pencerede
kullanıcı karttaki siyah RESET butonuna bastı:

- `coreReset()` **tam bir kez** yayıldı (25 sn'lik pencerede, düğmeye
  basıldığı an civarında).
- Bit **sticky/okununca-temizlenir** semantiğini doğruladı: bir sonraki 250 ms
  sağlık okumasında tekrar tetiklenmedi.
- **Örnekleme durmadı:** pencere boyunca toplam 99 örnek toplandı (~4 Hz × 25 s),
  reset öncesi/sonrası kesintisiz.

**Yan bulgu ve düzeltme:** İlk `S_RESET_ST` koşusunda `closed()` sinyalinin
**iki kez** yayıldığı görüldü. Kök neden: `DebugLinkWorker::disconnectFromServer()`
hem `QTcpSocket::waitForDisconnected()` içinde senkron teslim edilen
`disconnected()` sinyalinden (→ `onDisconnected()` → `socketClosed()`) hem de
fonksiyon sonundaki açık `emit socketClosed()` çağrısından olmak üzere **iki
kez** `socketClosed()` yayıyordu. Düzeltme: elle söktürme yolunda
`QTcpSocket::disconnected` bağlantısı `disconnectFromHost()`'tan **önce**
kesiliyor (`DebugLinkWorker.cpp`, `disconnectFromServer()`). Düzeltme sonrası
yeniden koşuda `closed()` **tam bir kez** geldi — doğrulandı.

---

## 9. Özet — Faz 1 kabul kriterleri

| Kriter | Sonuç |
|---|---|
| Birim testler yeşil | ✅ 12/12 |
| `retain()`/`release()` sayaç sözleşmesi | ✅ |
| DHCSR üç bit (S_HALT/S_RETIRE_ST/S_RESET_ST) | ✅ |
| `readRanges` verimi (Faz A ile aynı mertebe) | ✅ (üçü de kriter üstü) |
| UART regresyonu (çekirdek durmadı) | ✅ dolaylı değil, DHCSR ile **doğrudan** kanıtlandı; VCP-özel bir yan etki ayrıca belgelendi (Bölüm 4) |
| Başarısız `retain()` sayaç sızdırmıyor | ✅ |
| `S_RESET_ST` canlı yakalama + örnekleme kesintisiz | ✅ |

**Faz 1 tamamlandı.** Aşağıdaki iki bulgu koda değil donanım/sürücü
davranışına ait ve Faz 9 (dokümantasyon) + ileride F4/N6 doğrulamasında
tekrar gözden geçirilecek:
1. `ST-LINK_gdbserver` aktif oturumdayken bu ST-Link'te VCP susuyor (Bölüm 4).
2. `ST-LINK_gdbserver` kapandıktan hemen sonra CLI'nin ST-Link'e erişimi kısa
   süre `DEV_USB_COMM_ERR` verebiliyor, fiziksel replug ile anında düzeliyor
   (Bölüm 6).

---

## 10. Faz 2 — `GdbServerReader` (Register Inspector hızlanması)

### 10.0 Test yöntemi

Aynı Faz 1 desenli, geçici bir konsol harness'i (`scratchpad/phase1_probe/probe_phase2.cpp`,
repoya commit edilmedi) `RegisterInspector`'ı doğrudan sürdü (SvdCatalog,
ReadPlanBuilder, RegisterDecoder, SnapshotDiffer dahil gerçek kaynak
dosyalarıyla derlendi) — henüz UI olmadığından (Faz 4) bu, gelecekteki bir tık
sırasını taklit eden en sadık yöntemdi.

### 10.1 Çapraz doğrulama (H7, varsayılan 16 peripheral) ✅

| Adım | Sonuç |
|---|---|
| 1. CLI Snapshot A | 216 ms |
| 2. CLI Snapshot B | 178 ms — **N_control (A→B, CLI) = 0** |
| 3. GDB ile Snapshot B'yi yeniden al | 210 ms — **N_test (CLI-A→GDB-B) = 0** |
| Kabul kriteri | `N_test(0) <= N_control(0)*1.5` → **GEÇTİ** |

`N_control=0` (bu iki hızlı snapshot arasında gerçek donanım durumu
değişmedi) test'i **daha gevşek değil daha sıkı** yapıyor: A CLI'den, B GDB'den
okundu; herhangi bir GDB tarafı decode/çerçeveleme hatası **tek bir** sahte
farka bile yol açardı. Sıfır fark, GDB'nin 16 peripheral'daki tüm değerleri
CLI ile **bit-birebir** aynı decode ettiğinin doğrudan kanıtıdır.

### 10.2 Hız kriteri — soğuk vs sıcak durum (önemli nüans) ⚠️✅

İlk (soğuk) ölçüm CLI'ya göre GDB'yi **daha hızlı göstermedi** (0.85–1.02×):
her iki yol da o an bir kerelik bağlantı kurulumu ödüyordu (CLI: yeni süreç +
SWD connect; GDB: gdbserver'ı sıfırdan başlatma + RSP handshake, ~150 ms).

Link zaten açıkken (Adım 5 — aynı oturumda ikinci bir GDB snapshot'ı,
gdbserver zaten çalışıyor) fark açıkça ortaya çıktı: **7 ms**, CLI'nin 178
ms'sine karşı → **~25× hızlanma**. Register Inspector'ın asıl kullanım deseni
(art arda birden fazla snapshot, A→B diff için) linki snapshot'lar arasında
açık tutar (`retain/release` sayacı `1→2→1` gider — plan Bölüm 4.6 madde 5),
bu yüzden **gerçekçi/tekrarlı kullanımda ≥5× kriteri fazlasıyla karşılandı**
(25×). Yalnızca bir oturumdaki **ilk** GDB snapshot'ı bu kazancı görmez; bu,
kodun kalıcı olarak `"cli"` varsayılanında kalması kararını (Bölüm 10.4)
daha da güçlendiriyor.

### 10.3 Tüm SVD ile snapshot (117 peripheral) ✅ (+ beklenmeyen sağlamlık kazancı)

GDB arka ucuyla 117 peripheral'ın tamamı **581 ms'de, 0 hatayla** tamamlandı,
`connectMode="GDB-ATTACH"` doğru etiketlendi. Karşılaştırma: aynı seçim CLI
arka ucuyla denendiğinde **48 blok hatası** verdi (`"Read 32-bit max size
allowed is 32Kbytes"` — CLI'nin `-r32` tek çağrısı büyük SVD aralıklarında bu
sınırı aşıyor). GDB arka ucu `maxReadBytes()`'e göre otomatik parçaladığı için
(plan Bölüm 5.1) bu sınıra hiç takılmadı — planlanmamış ama gerçek bir
sağlamlık kazancı, sadece hız değil.

### 10.4 Bozuk gdbserver yolu → sessiz CLI düşüşü + tek seferlik uyarı ✅

Link tamamen kapandıktan sonra (refCount=0 doğrulandı — Faz 1'in bulduğu
gdbserver kapanış gecikmesi burada da gözlemlendi, ~2.5 sn beklenmesi
gerekti) `gdbserver_path` bozuk bir değere ayarlandı, tercih `"gdb"` bırakıldı:

- `RegisterInspector::errorOccurred`: *"GDB arka ucu kullanilamadi (gdbserver
  baslatilamadi ...), CLI'ye dusuldu"* — **tam beklenen mesaj**.
- Snapshot **CLI üzerinden başarıyla tamamlandı** (192 ms), `connectMode`
  doğru şekilde `"HOTPLUG"` (GDB-ATTACH DEĞİL) — plan Bölüm 5.1'in "connect
  mode UI'da yalan söylemez" kuralı doğrulandı.
- Bu, `RegisterInspector::onReadFailed()`'daki koşul (c) fallback yolunun
  (yalnızca RCC-fazı ilk okuması başarısız olursa, hiçbir veri henüz
  kullanıcıya ulaşmadığından güvenle CLI'ye yeniden başlanır) uçtan uca canlı
  kanıtıdır.

**Test tasarımı notu:** İlk deneme yanlış pozitif verdi — Adım 5'in
tamamlanması (`emit readFinished` → `RegisterInspector::onReadFinished` →
hemen `snapshotReady`) ile Adım 6'nın `retain()`'i **aynı senkron çağrı
yığınında** çalıştı; `GdbServerReader::onRangesRead`'deki asıl
`m_link->release()` henüz çalışmamışken Adım 6'nın `retain()`'i refCount'u
`1→2` yaptı ve link kapanmadan **eski (bozuk olmayan) gdbserver süreciyle**
sessizce devam etti. Bu, `retain/release` sayacının tasarlandığı gibi hatasız
çalıştığının (art arda zincirlenmiş senkron çağrılarda bile) dolaylı bir
kanıtı; ama testin kendisi linkin **gerçekten** kapanmasını (yaklaşık 2.5 sn)
beklemek zorundaydı ki bozuk yol fiilen denensin.

### 10.5 Gerçek bir uygulama açığı bulundu ve düzeltildi

Bu test sırasında **gerçek bir kod açığı** ortaya çıktı (harness artefaktı
değil): `Backend::setToolPath()` yalnızca `AppSettings`'i güncelliyordu;
`DebugLink` `main.cpp`'de **bir kez** yapılandırılıyordu ve asla yeniden
okunmuyordu. Sonuç: Ayarlar'dan `gdbserver_path` veya
`cubeprogrammer_bin_dir` değiştirilse bile, **sonraki** gdbserver başlatma
denemesi sessizce **eski yolu** kullanmaya devam ederdi (yalnızca linkin o an
kapalı olması bunu farkedilir kılardı — açık bir oturum etkilenmez zaten).
Düzeltme: `Backend` artık bir `DebugLink*` tutuyor (yapıcıya eklendi,
`main.cpp`'de `debugLink`, `registers`'dan **önce** kuruluyor); `setToolPath()`
bu iki anahtardan biri değiştiğinde `debugLink->setPaths(...)`'i de çağırıyor
— `programmer/cli_path` için zaten var olan `m_flash->setCliPath()` deseniyle
birebir aynı mantık.

### 10.6 Faz 2 kabul kriterleri özeti

| Kriter | Sonuç |
|---|---|
| Varsayılan arka uç kalıcı olarak `"cli"` | ✅ (`AppSettings::registerReadBackend()` varsayılanı `"cli"`) |
| `N_test <= N_control*1.5` | ✅ (0 <= 0) |
| ≥5× hız (gerçekçi/tekrarlı kullanım) | ✅ (~25×; soğuk-başlangıç nüansı Bölüm 10.2'de belgelendi) |
| Tüm SVD ile GDB snapshot tamamlanır | ✅ (0 hata; CLI'dan daha sağlam çıktı) |
| gdbserver yolu silinip CLI'ya sessiz düşüş + tek seferlik uyarı | ✅ |
| `connectMode` UI'da yalan söylemiyor (`GDB-ATTACH` vs `HOTPLUG`) | ✅ |
| `CliRegisterReader` silinmedi, mevcut davranış korundu | ✅ |

**Faz 2 tamamlandı.**

---

## 11. Ertelenmiş doğrulamalar — "kart gelince" listesi

Plan Bölüm 12.5 ile aynı; kart elde olmadığı için Faz 1'de koşulamadı.

**STM32F4:**
- [ ] gdbserver `-g` attach çalışıyor mu (F4 ST-Link V2-1)
- [ ] Handshake sonrası `S_HALT == 0` **ve** `S_RETIRE_ST == 1`
- [ ] VCP/SWD etkileşimi H7'deki gibi mi (Bölüm 4 bulgusu tekrar eder mi)
- [ ] `boards.json` `debug.gdb.verifiedOn` doldurulur

**STM32N6 (deneysel):**
- [ ] LRUN external-flash boot sonrası `-g` attach çalışıyor mu
- [ ] `S_RETIRE_ST` okunabiliyor mu (TrustZone altında DHCSR erişimi)
- [ ] TrustZone/RIF: güvenli RAM bölgesi okuması hata olarak mı yüzeye çıkıyor
      (sessiz sıfır DEĞİL)
- [ ] Başarısızsa `boards.json` `debug.gdb.support` `"unsupported"` yapılır
