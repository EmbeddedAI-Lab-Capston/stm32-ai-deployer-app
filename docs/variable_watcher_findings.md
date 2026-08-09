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

## 11. Faz 3 — Sembol katmanı (`NmSymbolParser`, `ElfSymbolSource`, `ElfTargetMatcher`, `ValueCodec`)

### 11.0 Fixture kaynağı

`tests/fixtures/nm_h7.txt`, planın istediği gibi **gerçek** bir H7 ELF'inden
üretildi — bu depodaki pipeline henüz bir .elf üretmemişti (önceki bir
oturumda üretilmiş çıktı diskte bulunamadı), bu yüzden makinede bulunan
harici bir STM32CubeIDE H7 projesinin (`KWS_Basinc_H7/Debug/KWS_Basinc_H7.elf`,
aynı model adı "anomaly_cnn_int8" ve "BME280" sensörüyle, muhtemelen bu
aracın erken bir deneyi) derlenmiş çıktısı kullanıldı. 561 sembol, gerçek
`arm-none-eabi-nm -S --defined-only` çıktısı, hiç elle düzenlenmedi.

### 11.1 Birim testler (donanımsız) ✅

32 test fonksiyonu (4 suite: `TestNmSymbolParser`, `TestValueCodec`,
`TestWatchModel`, `TestElfTargetMatcher`), hepsi yeşil (`ctest` → 100% passed).
Plan Bölüm 6.3 tablosundaki her satır birebir karşılandı: 4/3 alanlı satır
ayrıştırma, `_Min_Stack_Size` (Absolute, `addressIsValue=true`, değer=2048),
`_estack` (RAM aralığında, Absolute DEĞİL), `Reset_Handler` gerçek fixture'da
**gerçekten `W` (weak) tipinde** çıktı — plandaki "Weak sembol" test senaryosu
uydurma değil, gerçek veriden geldi. Bozuk satır atlama, `ValueCodec::decode`
(U32 LE, I16 negatif, F32, sınır-dışı), `ValueCodec::format` (`"8.200 ms"`
birebir), `WatchStats` (1000 değerde referans two-pass hesaba karşı ≤1e-9
fark), `ElfTargetMatcher`'ın 4 senaryosu (eşleşen/Thumb biti/SP-tutar-vec-tutmaz/sembol-yok).

### 11.2 Yarı-canlı — gerçek ELF'ten sembol yükleme ✅

`ElfSymbolSource` gerçek `arm-none-eabi-nm.exe`'yi çağırarak
`KWS_Basinc_H7.elf`'i yükledi (561 sembol). `_estack`, `_end`, `_ebss`,
`_sbss` bulundu; `_Min_Stack_Size`/`_Min_Heap_Size` `addressIsValue=true`
işaretiyle geldi (izleme listesine adres olarak eklenemez hale gelmiş
olacak — plan Bölüm 6.1'in gerektirdiği tam davranış).

### 11.3 Canlı VTOR okuma + ELF eşleşme testi ⚠️✅ (kısmi — gerekçeli)

H7'de gerçek `DebugLink` üzerinden VTOR (`0xE000ED08`) ve ardından 8 baytlık
vektör tablosu okundu: **`VTOR=0x08000000`, `initialSP=0x20020000`,
`resetVec=0x08005ee5`**.

**Beklenmeyen ama açıklayıcı bulgu:** `initialSP=0x20020000`, makinede bulunan
iki harici H7 ELF'inin (`KWS_Basinc_H7`, `KWS_Ses_H7`, ikisi de
`_estack=0x24050000` — AXI SRAM) **hiçbiriyle eşleşmedi** → her ikisi de
doğru şekilde **Mismatch** olarak işaretlendi. Ama `0x20020000` değeri
**bu aracın kendi** `templates/base/STM32H7/STM32H723ZGTx_FLASH.ld`
dosyasındaki `RAM ORIGIN=0x20000000, LENGTH=128K` → `_estack =
0x20000000+0x20000 = 0x20020000` ile **birebir örtüşüyor**. Yani karttaki
firmware harici CubeIDE projelerinden değil, **bu aracın kendi pipeline'ından**
(muhtemelen önceki bir oturumda) flashlanmış — dolaylı ama net bir kanıt.

**Sonuç:** İki farklı gerçek ELF, canlı okunan hedef vektör tablosuna karşı
**doğru şekilde Mismatch** verdi (`spMatches=false`, `resetMatches=false`
ikisinde de) — planın "en kötü hata modu" senaryosuna karşı asıl korumanın
(sessizce yanlış gösterme yerine görünür uyarı) çalıştığının canlı kanıtı.
**Live "Match" senaryosu gösterilmedi:** gerçekten eşleşen ELF'i üretmek bu
aracın kendi pipeline'ını (tflite→stedgeai→gcc→flash) yeniden çalıştırıp
kartı yeniden flaşlamayı gerektiriyordu; kullanıcıyla onaylanıp **bilinçli
olarak ertelendi** — Match dal mantığı zaten `TestElfTargetMatcher::
matchingSpAndResetVectorYieldsMatch` ile gerçekçi değerlerle birim test
edilmiş durumda, ve Mismatch dalı (aynı karşılaştırma kodu, aynı canlı okuma
yolu) iki bağımsız gerçek ELF ile doğrulandı. Faz 5/7 (pipeline'ın kendi
`.elf`'ini üretip commit edeceği `watch/demo/` çalışması) sırasında gerçek
bir Match örneği doğal olarak ortaya çıkacak.

### 11.4 Faz 3 kabul kriterleri özeti

| Kriter | Sonuç |
|---|---|
| Birim testler (gerçek fixture) yeşil | ✅ 32/32 |
| `_Min_Stack_Size`/`_Min_Heap_Size` izlenemez (adres değil değer) | ✅ |
| Yarı-canlı: gerçek ELF'ten `_estack`/`_end`/`_ebss`/`_sbss` bulundu | ✅ |
| Canlı: doğru ELF → yeşil (Match) | ⏸️ ertelendi (gerekçeli, yukarıda) |
| Canlı: kasten farklı ELF → sarı (Mismatch) | ✅ (iki farklı ELF ile) |
| Canlı: link kapalıyken → gri (Unknown) | ⏸️ Faz 4 UI'sı olmadan gösterilemez; "eksik sembol → Unknown" birim testiyle "uydurma karşılaştırma yok" ilkesi zaten kanıtlı |

**Faz 3 tamamlandı** (iki UI-bağımlı canlı senaryo, gerekçesiyle birlikte
Faz 4/9'a not düşülerek ertelendi — kod tarafında hiçbir açık yok).

---

## 12. Ertelenmiş doğrulamalar — "kart gelince" listesi

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

---

## 13. Faz 4 — İzleme UI'sı (sampler, ring buffer, ekran)

### 13.1 QML layout hatası — bulundu ve düzeltildi

İlk derlemede `WatchItemTable` (tablo) İzleyici sekmesinde tamamen boş
görünüyordu — başlık satırı, satırlar veya boş-durum metni hiç render
olmuyordu, üstteki araç çubuğu ise anormal derecede uzun görünüyordu.

**Kök neden:** `WatchToolbar` (kök: `RowLayout`) ve `SectionHeader` (kök:
`ColumnLayout`) kendileri de birer Layout türü; QtQuick.Layouts'ta bir Layout,
başka bir Layout içine iç içe konduğunda `Layout.fillHeight` **varsayılan
olarak `true`** olur (düz bir `Item`/`Rectangle`'ın aksine, ki onlarda
varsayılan `false`'tur). Sonuç: dış `ColumnLayout` içindeki araç çubuğu,
tabloya ayrılması gereken dikey alanın neredeyse tamamını sessizce yutuyordu;
tablo yalnızca birkaç pikselik bir alana sıkışıyordu.

**Düzeltme:** `qml/screens/WatchScreen.qml`'de `hdr` (SectionHeader) ve `tb`
(WatchToolbar) üzerine açıkça `Layout.fillHeight: false` eklendi. Ekran
görüntüsüyle doğrulandı: araç çubuğu, tablo başlığı (Etkin/Etiket/Adres/
Tip/Biçim/Ölçek/Birim/Canlı Değer/Min/Max/Ort), boş-durum mesajı ve alt durum
şeridi hepsi doğru boyut ve konumda render oluyor.

### 13.2 Canlı H7 testi — kullanıcı tarafından, manuel adres ile ✅

ELF eşleştirme/sembol çözümleme gerektirmeyen, donanım-bağımsız sabit bir
hedef seçildi: **`SysTick->VAL`** (`0xE000E018`, her Cortex-M çekirdeğinde
aynı adreste duran ARM çekirdek register'ı — hangi kullanıcı firmware'inin
flashlı olduğuna bağlı değil, bu yüzden Faz 3'teki ELF-eşleşme belirsizliği
hiç devreye girmedi).

Kullanıcı uygulamayı bizzat çalıştırıp şu adımları izledi: İzleyici sekmesi →
**Bağlan** (`STM32H7 bağlı` durumuna geçti) → **Adres Ekle** (`0xe000e018`,
`u32`, `dec`) → **Başlat** (200 Hz).

**Gözlem:** Canlı Değer sütunu sürekli değişti; **Min=242, Max=274968,
Ort=138923** — SysTick'in LOAD'dan 0'a aşağı sayıp yeniden yüklenen
"testere dişi" davranışıyla tutarlı, gerçek donanımdan okunan, sabit/donmuş
olmayan bir değer. Bu, uçtan uca boru hattının (bağlan → adres ekle → canlı
okuma döngüsü → decode → tablo/istatistik güncelleme) gerçek H7 üzerinde
çalıştığının doğrudan kanıtıdır.

**Not — plan Bölüm 7.8'in kalan maddeleri ertelendi:** 1000 Hz hedefte 60 sn
sürdürülebilirlik/UI donmazlık testi, azami hız (`0`=maks) ölçümü ve
60 sn/1000 Hz/8 değişkende RSS bellek artışı (<100 MB) testleri, ekran
otomasyonunun (fare tıklama/ekran görüntüsü) hem yavaş hem token-maliyetli
olduğu görülüp kullanıcının kendi elle testine geçilmesi kararıyla bu oturumda
koşulmadı. Temel boru hattı (bağlantı, canlı okuma, decode, UI güncelleme)
gerçek donanımda kanıtlanmış durumda; yüksek-hız/uzun-süre sağlamlık testi
ileride (Faz 6 grafik ekranıyla birlikte, gerçek kullanım sırasında) doğal
olarak ortaya çıkacak.

### 13.3 Faz 4 kabul kriterleri özeti

| Kriter | Sonuç |
|---|---|
| C++ tarafı (WatchPlanBuilder/TraceBuffer/WatchSampler/VariableWatcher) derlenir, birim testleri yeşil | ✅ |
| Backend API (properties/invokables/signals) + ST-Link hakemi (`m_stlinkOwner`) | ✅ |
| QML ekranları (toolbar/tablo/durum şeridi/dialoglar) doğru render olur | ✅ (Bölüm 13.1'deki hata düzeltildikten sonra) |
| Canlı H7: bağlan → adres ekle → başlat → değişen değer görünür | ✅ (Bölüm 13.2) |
| 60 sn/1000 Hz sürdürülebilirlik + bellek testi | ⏸️ ertelendi (gerekçeli, yukarıda) |

**Faz 4 tamamlandı** (bir gerçek UI hatası bulunup düzeltildi; temel canlı
boru hattı H7'de doğrulandı; yüksek-hız sürdürülebilirlik testi gerekçeli
olarak ertelendi).

---

## 14. Faz 5 — Firmware: static terfi + stack boyama

### 14.0 Değişiklikler

- `templates/ai_glue/ai_runner.c`: `q_input`/`output_data` yerel (stack)
  dizileri, dosya-kapsamlı `static ai_i8 g_ai_input[...]` /
  `g_ai_output[...]`'a taşındı. `AI_Runner_Infer()` sonunda
  `g_ai_last_inference_us`, `g_ai_infer_count` (++), `g_ai_last_class`,
  `g_ai_last_confidence` dolduruluyor — hepsi `static volatile`.
- `templates/ai_glue/stack_paint.c/.h` (yeni): `StackPaint_Init()`,
  `_sstack`'ten mevcut `SP - STACK_PAINT_MARGIN`'e kadar `0xA5A5A5A5`
  deseniyle boyuyor; boş/ters aralıkta (`end <= start`) sessizce hiçbir şey
  yapmıyor.
- Üç `templates/base/STM32*/STM32*_FLASH.ld`: F4 ve H7'ye `_sstack = _estack
  - _Min_Stack_Size;` eklendi (N6'da zaten vardı) — üç kartta da artık aynı
    sembol seti, `stack_paint.c` şartlı/yedek mantık gerektirmiyor.
- Üç `templates/base/STM32*/Src/main.c`: `StackPaint_Init()`, `HAL_Init()`
  hemen sonrasında (ilk derin çağrıdan önce) çağrılıyor.
- Üç `templates/base/STM32*/Makefile`: `Src/stack_paint.c` derleme listesine
  eklendi. (`templates/ai_glue/*.c/*.h` zaten `PipelineRunner::stepPrepare()`
  tarafından dizin taraması ile kopyalanıyor — C++ tarafında ayrıca bir dosya
  listesi güncellemesi gerekmedi.)

### 14.1 Test yöntemi — gerçek donanım/pipeline'sız, derleyici-temelli doğrulama

Kullanıcı bilgisayardan uzaktaydı (ekran görüntüsü / canlı test yok isteği).
Bu yüzden doğrulama tamamen **gerçek `arm-none-eabi-gcc` ile, gerçek H7
CMSIS/HAL başlıklarına karşı, hedef derleyici bayraklarıyla** (Makefile'daki
`-mcpu=cortex-m7 -mthumb -mfpu=fpv5-d16 -mfloat-abi=hard -DSTM32H723xx`
birebir) yapıldı — ekran/tıklama otomasyonu hiç kullanılmadı:

1. **`stack_paint.c` — gerçek derleme:** Şablonun kendi `Inc/main.h`,
   `ai_config.h`, `stm32h7xx_hal_conf.h` dosyaları + yerel diskteki gerçek
   `STM32Cube_FW_H7_V1.12.1` CMSIS/HAL başlıklarıyla, **sıfır hata/uyarı**
   (`-Wall`) derlendi. `nm` çıktısı: `_sstack` **`U`** (tanımsız — linker'dan
   beklendiği gibi), `StackPaint_Init`/`StackPaint_Pattern` **`T`** (doğru
   tanımlı). GCC, döngüyü otomatik olarak `memset`'e optimize etti (desen
   4 baytın tekrarı olduğu için) — beklenen, zararsız bir derleyici
   optimizasyonu.
2. **`ai_runner.c` diff'i — izole derleme (sahte X-CUBE-AI başlığı ile):**
   Gerçek bir eğitilmiş model olmadan `network.h` üretilemediği için, test
   mühendisliğinde yaygın bir teknikle (üçüncü taraf bağımlılığı sahte/stub
   ile izole etme) minimal bir `network.h` stub'ı yazıldı (`ai_i8`,
   `ai_buffer`, `ai_handle`, `ai_network_*` fonksiyonları, ilgili makrolar).
   Gerçek dosya bu stub'a karşı **`-Wall -Wextra` ile sıfır hata/uyarı**
   derlendi. `nm -S --defined-only` çıktısı 6 yeni sembolün tümünü doğru
   boyut ve bölümde (`.bss`, `b`) gösterdi:
   `g_ai_last_inference_us`(4B), `g_ai_infer_count`(4B),
   `g_ai_last_class`(1B), `g_ai_last_confidence`(1B), `g_ai_input`(16B-stub),
   `g_ai_output`(4B-stub) — plan Bölüm 8.3 madde 2'nin ("nm çıktısında
   görünür ve RAM adreslerinde") yapısal kanıtı, gerçek boyutlar yalnızca
   gerçek modelin `AI_NETWORK_IN/OUT_1_SIZE` değerlerine bağlı olduğundan
   stub boyutlarıyla.
3. Mevcut Qt birim test paketi (`ctest --test-dir build`) bu değişikliklerden
   sonra da **yeşil** (Faz 5 hiçbir C++ dosyasına dokunmadı, regresyon
   beklenmiyordu — doğrulandı).

### 14.2 Ertelenen doğrulamalar (gerekçeli)

Plan Bölüm 8.3'ün 1, 3, 4, 5, 6 numaralı maddeleri (pipeline'ı gerçek bir
modelle yeniden çalıştırıp H7'yi reflaş etmeyi, ardından İzleyici'de canlı
`g_ai_infer_count` artışını ve UART `inf_us` ile ±%5 uyumu izlemeyi
gerektiriyor) bu oturumda **çalıştırılmadı** — kullanıcı bilgisayardan
uzaktaydı ve Faz 3'te de aynı gerekçeyle ("Gerek yok, mevcut kanıt yeterli")
benzer bir canlı-reflaş adımı ertelenmişti. Statik/derleyici kanıtı (Bölüm
14.1) kodun doğruluğu için yeterli; canlı sayı artışı ve stack boyama
deseni okuması, kullanıcı bir sonraki gerçek model pipeline'ı çalıştırıp
flaşladığında doğal olarak doğrulanabilir.

### 14.3 Faz 5 kabul kriterleri özeti

| Kriter | Sonuç |
|---|---|
| `g_ai_*` statikleri dosya-kapsamlı, `nm`'de görünür boyut/bölümde | ✅ (stub ile yapısal olarak kanıtlandı) |
| `stack_paint.c` gerçek H7 başlıklarına karşı sıfır hata/uyarı derlenir | ✅ |
| Üç kartta da `_sstack` linker sembolü tutarlı şekilde var | ✅ (F4/H7'ye eklendi, N6'da zaten vardı) |
| `StackPaint_Init()` üç `main.c`'de de `HAL_Init()` sonrası çağrılıyor | ✅ |
| Üç `Makefile`'da `Src/stack_paint.c` derleme listesinde | ✅ |
| Mevcut Qt birim testleri regresyonsuz | ✅ |
| Canlı H7: `g_ai_infer_count` artışı + `inf_us` ±%5 UART uyumu + boyama deseni okuma | ⏸️ ertelendi (gerekçeli, yukarıda) |
| Firmware boyut artışı < 1 KB | ⏸️ gerçek model olmadan ölçülemez |

**Faz 5 tamamlandı** (firmware kodu yazıldı, gerçek çapraz-derleyiciyle
doğrulandı; canlı reflaş gerektiren adımlar gerekçeli olarak ertelendi).
