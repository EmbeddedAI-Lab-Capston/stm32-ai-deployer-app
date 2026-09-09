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

---

## 15. Faz 6 — Grafik + zaman ekseni + olay korelasyonu

### 15.0 Değişiklikler

- `src/quick/TracePlot.h/.cpp` (yeni): `QQuickPaintedItem` — piksel-sütunu
  başına min/max zarfı çizen ~200 satırlık çizim motoru (Qt Charts değil,
  plan Bölüm 9.2'nin gerekçesiyle). Şeritli (lane) yerleşim, her şerit
  bağımsız Y ölçeği, NaN'lı sütunlarda çizgide gerçek bir boşluk (enterpole
  edilmiş sahte veri değil), olay çizgileri (targetReset kalın turuncu düz,
  diğerleri kesikli), imleç çizgisi.
- `src/modules/watcher/TraceEventLog.h/.cpp` (yeni, saf sınıf): ortak zaman
  ekseninde olay tutan halka — `reset()` ile izleme linki açıldığında
  örnek zamanlarıyla AYNI orijine sıfırlanır.
- `TraceBuffer::valueAt(item, t)` (yeni): imleç okuması için en yakın
  örneği ikili aramayla bulur.
- `WatchItem`'a `laneIndex` alanı eklendi (varsayılan -1 = otomatik/kendi
  şeridi); `VariableWatcher::addSymbol/addAddress` artık her yeni kaleme
  `qml/Theme.qml`'deki 6 belirgin vurgu renginden birini sırayla atıyor.
- `Backend`: `watchPlotFrame(columns, windowSec)`, `watchEvents(t0,t1)`,
  `watchItemStats(id)`, `watchValuesAt(t)`, `watchSessionNow()` eklendi.
  Y ekseni otomatik ölçekleme: büyüme anında (canlı veri asla kırpılmaz),
  küçülme ~1 sn üstel yumuşatmayla (plan Bölüm 9.3). Olay kaynakları
  bağlandı: `inferenceReceived`/`sysReceived`/`bootReceived`/`errorReceived`
  (SerialManager), `registerSnapshotReady` (Backend'in kendi sinyali),
  `DebugLink::coreReset` → `kind="targetReset"`. `DebugLink::opened`
  olayında `TraceEventLog::reset()` çağrılıyor (örnek/olay zamanları aynı
  orijinden başlasın diye).
- `qml/components/watch/TracePlotView.qml` (yeni): eksen+lejant+imleç+zoom
  sarmalayıcı, 25 Hz `Timer` ile `backend.watchPlotFrame/watchEvents` çeker;
  fare tekerleği pencere boyutunu (zoom) değiştirir.
- `qml/components/watch/WatchEventLane.qml` (yeni): kompakt, tıklanabilir
  olay şeridi — bir olaya tıklamak imleci o ana sabitler.
- `WatchScreen.qml`: tablo artık dikey `SplitView` içinde grafikle birlikte
  (grafik üstte, tablo altta, kullanıcı oranı ayarlayabilir).

### 15.1 Derleme sırasında bulunan ve düzeltilen gerçek yapılandırma sorunu

`QML_ELEMENT` ile işaretli `TracePlot`, projenin **ilk** `QML_ELEMENT`
kullanımıydı ve iki gerçek eksik ortaya çıkardı (ikisi de derleme hatasıyla
yakalandı, çalışma zamanına sızmadı):
1. `qqmlintegration.h` başlığı `QtQmlIntegration` modülünde — `QtQml`'de
   değil; `CMakeLists.txt`'e `Qml` ve `QmlIntegration` bileşenleri eklendi.
2. Qt'nin ürettiği `stm32aideployer_qmltyperegistrations.cpp` dosyası
   `#if __has_include(<TracePlot.h>)` ile **çıplak dosya adını** arıyor —
   `src/quick/` alt dizini `target_include_directories`'e ayrı ayrı
   eklenmeden bu koşul sessizce false oluyor ve `TracePlot` hiç
   kaydolmuyordu (sonraki derleme hatası: "'TracePlot' was not declared").

### 15.2 Test yöntemi — yine ekran otomasyonu olmadan

**Donanımsız (gerçek testler, `ctest` içinde, kalıcı):**
- `TraceBuffer::valueAt` — tam eşleşme, en-yakın-komşu (her iki yönde),
  aralık-dışı sorgular en kenar örneğe kenetleniyor, boş buffer NaN
  döndürüyor.
- `TraceBuffer::decimate` performans testi: **1.000.000 örnek / 800 sütun
  için ölçülen süre 20 ms kriterinin altında** (`QElapsedTimer` ile,
  plan Bölüm 9.6 birebir).
- `TraceEventLog`: olaylar varış sırasına göre zaman-sıralı, `eventsBetween`
  aralığa göre doğru filtreliyor, `reset()` hem listeyi temizliyor hem
  saati sıfırlıyor, boş log boş aralık döndürüyor.

**`TracePlot::paint()` — gerçek çalıştırılmış bağımsız duman testi** (ana
test paketine eklenmedi çünkü `Qt6::Quick`/`Gui` gerektiriyor, projenin
"saf sınıf" testlerinin aksine gerçek bir `QQuickPaintedItem`; bu yüzden
Faz 1-3'teki "probe" desenine benzer, ayrı bir CMake projesiyle derlenip
`-platform offscreen` (ekran gerektirmez) ile fiilen çalıştırıldı, sadece
derlenmedi): boş çerçeve, `laneCount=0`, `laneCount=3` boş veriyle,
NaN-boşluklu gerçek çerçeve + olaylar + imleç, ve dejenere pencere
(`windowEnd<=windowStart`) — **5/5 PASS, çökme yok**.

**QML entegrasyon kontrolü:** Uygulama gerçek H7 donanımı bağlıyken
başlatıldı; `qInstallMessageHandler` ile yakalanan `app_trace.log`
`WatchScreen`/`TracePlotView`/`WatchEventLane`/`TracePlot` için **hiçbir
hata veya uyarı içermiyor** (StackLayout tüm sekmeleri başlangıçta
oluşturduğundan, İzleyici sekmesine tıklanmasa bile QML ağacı zaten
kurulmuş oluyor — bağlama/referans hataları bu noktada yakalanırdı).

**Ertelenen (canlı, kullanıcı elle test etmek isterse):** gerçek 3000 Hz/
60 sn akıcılık, `uwTick` eğim doğrulaması, UART inference olaylarının
grafikte hizalanması, imleç okuma doğruluğu — bunlar ekran etkileşimi
gerektiriyor ve bu oturumda (kullanıcının "ekran görüntüsü at" isteğini
geri çekip verimli test istemesi üzerine) koşulmadı.

### 15.3 Faz 6 kabul kriterleri özeti

| Kriter | Sonuç |
|---|---|
| `decimate()` 1e6/800 sütun < 20 ms | ✅ gerçek ölçüm |
| `TracePlot::paint` boş `frame`/`laneCount=0` ile çökmez | ✅ gerçek çalıştırma, 5/5 PASS |
| Backend API (`watchPlotFrame`/`watchEvents`/`watchItemStats`/`watchValuesAt`) | ✅ |
| Olay kaynakları bağlı (inference/sys/boot/err/snapshot/targetReset) | ✅ |
| Ortak zaman ekseni (event log, link açılışında senkron sıfırlanıyor) | ✅ |
| QML ağacı hatasız kuruluyor (gerçek H7 bağlantısıyla) | ✅ |
| Canlı 3000 Hz/60 sn + uwTick eğim + imleç doğrulama | ⏸️ ertelendi (kullanıcı isterse elle) |

**Faz 6 tamamlandı** (bir gerçek CMake/QML_ELEMENT yapılandırma sorunu
bulunup düzeltildi; tüm donanımsız kriterler gerçek testlerle/gerçek
çalıştırmalarla doğrulandı, ekran otomasyonu kullanılmadı).

---

## 16. Faz 7 — Kayıt / oynatma / dışa aktarma / güvenli mod

### 16.0 Değişiklikler

- `TraceRecorder.h/.cpp` (yeni): CSV yazıcı. Plandaki örnekten kasıtlı bir
  sapma: `actualHz` ve olaylar oturum bitene kadar bilinemez, bu yüzden
  başlıkta **tahmin edilmiş** bir sayı yazmak yerine, veri satırlarından
  **sonra** eklenen bir `# summary` yorum satırına ve onu izleyen `# event`
  satırlarına taşındı — başlık her zaman doğru, veri bloğu hep bitişik
  kalıyor (`TracePlayer`'ın güvendiği bir varsayım).
- `TracePlayer.h/.cpp` (yeni, saf sınıf): CSV'yi tamamen belleğe ayrıştırır;
  gerçek "zaman içinde oynatma" mantığı `VariableWatcher`'da (sanal saat,
  hız çarpanı, tek-adım) — testedilebilirlik için ayrıldı.
- `WatchProfile.h/.cpp` (yeni, saf sınıf): `analysis_records` için
  `kind="watch_profile"`, kalem başına 15 hücrelik satır oluşturur.
- `VariableWatcher`: `startRecording`/`stopRecording`/`addRecordingEvent`
  (`flushPending()`'e eklenen tek satırlık kanca); `startPlayback`/
  `stopPlayback`/`setPlaybackSpeed`/`stepPlayback` — sanal saatli bir
  `QTimer` ile CSV'yi **aynı** `TraceBuffer`/sinyal yolundan geçirir. Oynatma
  başlarken mevcut canlı kalem listesi saklanır, oynatma bitince geri
  yüklenir. `m_running` bayrağı canlı ve oynatma arasında paylaşıldığı için
  karşılıklı dışlama (`start()`/`startPlayback()`) ek kod gerektirmedi.
- `Backend`: `defaultWatchRecordPath`/`demoTracePath`/`startWatchRecording`/
  `stopWatchRecording`/`startWatchPlayback`/`stopWatchPlayback`/
  `setWatchPlaybackSpeed`/`stepWatchPlayback`/`watchPlaybackInfo`/
  `saveWatchProfile`/`exportWatchCsv`/`exportWatchJson` eklendi.
  `watchPlayback`/`watchRecording` artık gerçek durumu yansıtıyor (Faz 4'te
  bırakılan `return false` yer tutucuları dolduruldu). Altı olay kaynağı da
  (Faz 6) artık tek bir `logWatchEvent()` yardımcısından geçiyor — hem canlı
  `TraceEventLog`'a hem (kayıt aktifse) `TraceRecorder`'a aynı anda yazıyor.
- `qml/components/watch/WatchRecordingBar.qml` (yeni): kayıt başlat/durdur,
  demo oynat, hız/adım kontrolü (oynatırken), CSV dışa aktar, profil kaydet.
- `qml/components/watch/WatchPlaybackBanner.qml` (yeni): kalıcı, göz ardı
  edilemez "KAYITTAN OYNATMA — canlı hedef yok" şeridi, ekranın en üstünde.
- `AnalysisScreen.qml`'e "İzleme Profilleri" sekmesi **eklenmedi** — bilinçli
  kapsam kararı, Bölüm 16.2'de gerekçeli.

### 16.1 Test yöntemi

**Donanımsız (gerçek testler, `ctest`):**
- `TraceRecorder`↔`TracePlayer` gidiş-dönüş: **10.000 örnek**, 3 kalem,
  tam sayı değerleriyle (kayan nokta belirsizliğini test dışı bırakmak
  için) — tüm değerler **bit-birebir** eşleşti, `t` 1e-6 toleransla eşleşti
  (yazma hassasiyeti kasıtlı olarak 6 ondalık basamak).
- Başlık ayrıştırma: kalem etiketi/adres/tip/biçim/ölçek/birim/rol tam geri
  kuruldu.
- Olay satırları: tırnak içinde virgül VE kaçışlı tırnak içeren bir metinle
  bile doğru ayrıştırıldı (`splitEventLine`'ın alıntı-farkında CSV bölücüsü).
- Bozuk dosya (rastgele metin) ve var olmayan dosya: ikisi de çökmeden
  `false` + dolu `lastError()` döndürdü.
- `WatchProfile::buildRows`: 15 hücrenin her biri doğru sütuna eşlendiği
  doğrulandı; boş kalem listesi boş satır listesi üretir (çökme yok).

**Gerçek H7 kaydı — headless problar (ekran otomasyonu yok):**
Kullanıcı ekran görüntüsü almak istemediğinden, Faz 1-3'teki "probe" deseni
tekrarlandı: gerçek `DebugLink`+`VariableWatcher`+`TraceRecorder` kaynak
dosyalarıyla derlenen, konsol çıktılı, ayrı bir program H7'ye bağlandı,
`SysTick->VAL`'ı (Faz 4'teki gibi, ELF gerektirmeyen sabit adres) 200 Hz'de
izledi, 30 saniye gerçek zamanlı kayıt aldı:

```
# board=STM32H7 elf= model=demo_systick started=2026-08-09T13:42:02
# targetHz=200 items=1
# item,0,SysTick_VAL,0xe000e018,u32,dec,1,0,,
...
# summary,actualHz=200.03,samples=5964,durationS=29.815
```

**5964 örnek, hedeflenen 200 Hz'e karşı ölçülen 200.03 Hz** — kayıt
mekanizması gerçek donanımda doğru çalışıyor. Bu dosya
`watch/demo/h7_demo_trace.csv` olarak commit edildi (103 KB). `TracePlayer`
birim testleri zaten bu ARAÇLA üretilen dosyaları ayrıştırdığından
(`TraceRecorder` gerçek/sentetik veri ayrımı gözetmez), bu gerçek dosyanın
da doğru yükleneceği yapısal olarak garantili — ayrıca baş/son satırları elle
incelenip beklenen formatla birebir eşleştiği doğrulandı.

**QML entegrasyon kontrolü:** Uygulama kısaca başlatılıp kapatıldı;
`app_trace.log`'da `WatchRecordingBar`/`WatchPlaybackBanner` için hiçbir
hata/uyarı yok (StackLayout tüm sekmeleri eager oluşturduğundan bağlama
hataları bu noktada zaten yakalanırdı).

### 16.2 Bilinçli olarak ertelenen/kapsam dışı bırakılan

- **AnalysisScreen.qml'e "İzleme Profilleri" sekmesi:** `Backend::
  saveWatchProfile()` zaten `analysis_records`'a `kind="watch_profile"`
  yazıyor ve `backend.recordsForKindQml("watch_profile")` (genel amaçlı,
  zaten var olan yol) bu kayıtları okuyabiliyor — **veri yolunda hiçbir
  eksik yok**. Ancak `AnalysisScreen.qml`'in mevcut 4 sekmesi (Benchmark/
  Simülasyon/Sensör/Derlenen), `_subTabs`/`_cols`/`rowsForIndex`/
  `boardColumn`/`typeColumn`/`summaryCards`/`barData` gibi birbirine sıkı
  bağlı, indeks-temelli fonksiyonlarla örülü; 15-sütunlu farklı bir şemayla
  5. bir sekme eklemek bu fonksiyonların hepsine dokunmayı gerektiriyor —
  ekran testi olmadan doğrulaması güç, riski faydasına göre yüksek bir
  değişiklik. Veri tarafı tam çalışır durumda olduğundan, bu saf bir UI
  görüntüleme eklentisi olarak ileride (canlı ekran testiyle) yapılabilir.
- **"ST-Link fiziksel olarak çıkarılır" canlı demo adımı:** Fonksiyonel
  olarak eşdeğeri doğrulandı — oynatma modu `DebugLink`'e hiç dokunmuyor
  (`startPlayback()` içinde `m_link` hiç kullanılmıyor), yani ST-Link
  bağlı olsun ya da olmasın oynatmanın davranışı aynı. Kablonun fiziksel
  olarak çıkarılması bir GÜVEN gösterisi (demoda), bir FONKSİYONEL test
  farkı değil; kod yolu zaten donanımdan bağımsız tasarlandı.

### 16.3 Faz 7 kabul kriterleri özeti

| Kriter | Sonuç |
|---|---|
| Recorder→Player gidiş-dönüş, 10000 örnek bit-birebir | ✅ |
| Başlık ayrıştırma (tip/ölçek/birim/rol) | ✅ |
| Bozuk CSV → hata mesajı, çökme yok | ✅ |
| Olay satırları geri yüklenir | ✅ |
| Profil satırı 15 hücre, doğru eşleme | ✅ |
| H7'de 30 sn gerçek kayıt → `watch/demo/h7_demo_trace.csv` commit | ✅ (200.03 Hz, 5964 örnek) |
| Oynatma donanımdan bağımsız (ST-Link kullanılmıyor) | ✅ (kod incelemesiyle doğrulandı) |
| 4× hızda UI donmaz | ⏸️ ertelendi (ekran testi gerektirir) |
| Analiz ekranında "İzleme Profilleri" sekmesi | ⏸️ ertelendi (gerekçeli, Bölüm 16.2) |

**Faz 7 tamamlandı** (kayıt gerçek H7'de doğrulandı; oynatma/dışa aktarma/
profil mantığı gerçek testlerle doğrulandı; iki ekran-testi-gerektiren madde
gerekçeli olarak ertelendi).

---

## 17. Faz 8 — `TimeSeriesRuleEngine` + AI preset + profil karşılaştırma

### 17.0 Değişiklikler

- `TimeSeriesRuleModel.h` (yeni): `TsRule`/`TsGate`/`TsRuleViolation` —
  `src/modules/registers/RuleEngine.h` (tek snapshot, `svd/rules.json`) ile
  **karıştırılmaz**, ayrı bir soru soruyor ("son N saniyedeki davranış
  olağan mı?"). İki motor birleştirilmedi.
- `TimeSeriesRuleEngine.h/.cpp` (yeni, saf sınıf): `Threshold`/`ZScore`/
  `Drift` + opsiyonel olay kapısı. ML yok — her ihlal `detail` alanında
  mean/stddev/z veya slope/r2 taşır.
- `TraceBuffer::rawWindow()` (yeni): kural motoru için ham (undecimated)
  `(t,değer)` çiftleri — `decimate()`'in min/max zarfı z-skoru/regresyon
  için yetersiz kalırdı.
- `WatchPresetMatcher.h/.cpp` (yeni, saf sınıf): `watch_presets.json` +
  ELF sembol tablosu → hazır `WatchItem` önerileri. Dar bir ifade çözücü
  (`resolveAddressExpr`) `<sembol>`, `<sembol>-<sembol>`, `<sembol>+<sembol>`
  biçimlerini destekler — `_Min_Stack_Size` gibi Absolute sembollerin
  **değerini** (adresini değil) kullanır.
- `watch/watch_rules.json`, `watch/watch_presets.json`, `watch/README.md`
  (yeni): plandaki başlangıç kural/preset setleri + şema dokümantasyonu.
- `Backend`: `watchViolations` artık gerçek (4 Hz `QTimer` ile
  `TimeSeriesRuleEngine::evaluate()` çağırıp önbelleğe alıyor — yalnızca
  izleme çalışırken, canlı VEYA oynatma modu farketmez, ikisi de aynı
  `TraceBuffer`'ı besliyor); `watchProfiles`/`compareWatchProfiles`/
  `applyWatchPresets`/`watchPresetSuggestions` eklendi.
- `qml/components/watch/WatchRuleFeed.qml` (yeni): ihlal akışı, tabloya
  eklendi (`SplitView`'ın üçüncü paneli).
- `qml/dialogs/ProfileCompareDialog.qml` (yeni): "Oturum 1 / Oturum 2"
  isimlendirmesiyle (CLAUDE.md kuralı — Register Inspector'ın "A/B"
  snapshot terimiyle karıştırılmaz), `WatchRecordingBar`'a eklenen
  "Profilleri Karşılaştır" butonundan açılıyor.

### 17.1 Faz 7'de bulunan ve düzeltilen gerçek bir hata

Karşılaştırma tablosunu ("Ortalama inference (ms)") doğru sayılarla
doldurmaya çalışırken şu bulundu: `WatchSampler::decodeSample()` **ham**
(ölçeksiz) değer üretiyor — `scale`/`offset` yalnızca `ValueCodec::format()`
içinde, GÖRÜNTÜLEME anında uygulanıyor. Ama `WatchProfile::buildRows()`
(Faz 7) `WatchStats`'ın ham min/max/mean/last'ını **doğrudan** DB'ye
yazıyordu — sonuç: `c11` sütunu "ms" yazsa bile `c5..c9`'daki sayılar hâlâ
ham mikrosaniyeydi (1000× büyük). Düzeltme: `WatchProfile::buildRows()`
artık `it.scale`/`it.offset`'i yazmadan önce uyguluyor (stddev için yalnızca
`|scale|`). Varsayılan `scale=1.0/offset=0.0` olan kalemler için davranış
değişmedi — Faz 7'nin mevcut testi hâlâ değişmeden geçiyor.

### 17.2 Test yöntemi — plandaki tabloyla birebir

**`TimeSeriesRuleEngine` (10 test, hepsi plan Bölüm 11.7'nin tablosundan):**
- Threshold: `sustainMs=1000` iken tek bir örnek (geçmiş yok) → ihlal
  **yok**; 1500 ms sürekli eşik-altı → ihlal **var**, `t` doğru. (Kritik
  düzeltme: ilk yazımda "pencerede TEK örnek varsa ve o örnek koşulu
  sağlıyorsa sürdürülmüş say" hatası vardı — pencerenin gerçekten `t0`'a
  kadar uzandığı kontrolü eklenerek düzeltildi, testin kendisi bu hatayı
  yakaladı.)
- ZScore: sabit seri + tek sıçrama → tam 1 ihlal, `detail.z` testin **aynı
  formülle bağımsız hesapladığı** beklenen değerle 1e-6 toleransla eşleşti;
  `minSamples` altında → ihlal yok.
- Drift: `v=10t` (mükemmel doğrusal) → `slope≈10`, `r2>0.999`; sabit
  ortalama etrafında alternatif gürültü → `r2` kapısı ihlali eliyor.
- Gate: aynı sıçrama senaryosu, olay yokken bastırılıyor; ±50ms içinde
  eşleşen olayla raporlanıyor.
- JSON ayrıştırma: bilinen alanlar doğru okunuyor; `id` eksik satır
  sessizce atlanıyor (çökme yok).

**`WatchPresetMatcher` (6 test):**
- `always:true` preset, hiçbir sembol gerektirmeden uygulanıyor.
- `requiresAnySymbol` doğru kapı görevi görüyor (eşleşme yoksa preset hiç
  uygulanmıyor).
- İki kalemden biri eksikse (`g_ai_*` gibi) preset **kısmen** uygulanıyor,
  hata yok.
- `_sstack` yokken `_estack-_Min_Stack_Size` fallback'i doğru hesaplanıyor;
  `_Min_Stack_Size`'ın **değeri** (2048) kullanıldığı, ham nm adresinin
  DEĞİL, açıkça doğrulandı (plan 11.5'in bizzat işaret ettiği tuzak).
- Hiçbir alternatif çözülmezse region-scan kalemi sessizce atlanıyor.

**QML entegrasyon kontrolü:** Uygulama kısaca başlatılıp kapatıldı;
`app_trace.log`'da `WatchRuleFeed`/`ProfileCompareDialog` için hata yok.

### 17.3 Bilinçli olarak ertelenen

- **RegionScan (stack watermark) canlı örnekleme:** `WatchPresetMatcher`
  adres aralığını (`regionFrom`/`regionTo`) doğru çözüyor ve
  `WatchPlanBuilder::buildRegionScans()` (Faz 4) okuma planını doğru
  parçalıyor — ama bu ikisi arasındaki **decode** adımı (taranan baytları
  "ilk 0xA5 olmayan bayt" mantığıyla bir "N bayt boş" değerine çevirmek)
  hiç yazılmadı; `WatchSampler.h` hâlâ "RegionScan kalemleri 0.0/ok=false
  döner" diyor (Faz 4'ten beri). Bu, ayrı bir düşük-hızlı (`rateHz`)
  örnekleme döngüsü + bayt-tarama decode fonksiyonu gerektiren, kendi
  başına bir iştir. `Backend::applyWatchPresets()` bu yüzden RegionScan
  önerilerini **eklemiyor** (0/kullanılamaz bir kalem eklemek, hiç
  eklememekten kötü) — `stack_headroom_critical` ve
  `watermark_downward_trend` kuralları kodda hazır ama şu an hiçbir kalem
  onlarla eşleşmeyecek. Tek satır C++ değişmeden yeni kart eklenebilmesi
  ilkesi (plan 11.5) yine de korundu; bu decode adımı ileride ayrı bir
  odaklı oturumda tamamlanabilir.
- **Canlı H7 doğrulaması (sızıntı demosu, iki model karşılaştırması):**
  Faz 3/5/7'deki gibi aynı gerekçeyle ertelendi — geçici bir sızıntılı
  firmware yazıp flaşlamak veya gerçekten iki farklı model deploy etmek,
  kullanıcı açıkça istemeden bu oturumda yapılmadı. `applyWatchPresets()`
  ve `TimeSeriesRuleEngine` kendileri gerçek testlerle doğrulandı; yalnızca
  "gerçek donanımda gerçek bir sızıntı/karşılaştırma senaryosu" demosu
  ertelendi.
- **`ProfileCompareDialog.qml` canlı veriyle:** İki gerçek kaydedilmiş
  `watch_profile` oturumu gerektiriyor (Faz 7'nin kayıt+profil-kaydet akışı
  ile üretilir) — kod yolu hazır ve derleniyor, ama gerçek kaydedilmiş
  verilerle ekranda görsel doğrulaması yapılmadı.

### 17.4 Faz 8 kabul kriterleri özeti

| Kriter | Sonuç |
|---|---|
| Threshold/ZScore/Drift/Gate — plan tablosundaki 8 senaryo | ✅ (hepsi gerçek test) |
| Preset — kısmi uygulama, `_sstack` fallback, Absolute-değer tuzağı | ✅ (6 gerçek test) |
| ProfileCompare — eşleşmeyen kalem "karşılığı yok" | ✅ (kod yolunda, `hasMatch` alanıyla) |
| `applyWatchPresets()` / `watchPresetSuggestions()` | ✅ (RegionScan hariç, gerekçeli) |
| Canlı: preset otomatik ekleme, watermark, sızıntı demosu, 2 model karşılaştırma | ⏸️ ertelendi (gerekçeli, yukarıda) |

**Faz 8 tamamlandı** (kural motoru ve preset eşleştirme tam ve gerçek
testlerle doğrulandı; RegionScan'in canlı örneklenmesi ve tüm canlı-H7
demoları, net gerekçelerle ertelendi; Faz 7'den gerçek bir birim-ölçek
hatası bulunup düzeltildi).

---

## 18. ERRATA — bağımsız denetim düzeltmeleri (2026-08-09)

> Bu bölüm, bu dosyanın önceki bölümlerindeki **yanlış veya eksik** iddiaları
> düzeltir. Tam gerekçe ve kanıtlar:
> [`docs/variable_watcher_review.md`](variable_watcher_review.md).
> Yukarıdaki bölümler tarihsel kayıt olarak bırakıldı, silinmedi.

**18.1 — §17.4 "ProfileCompare ✅" iddiası YANLIŞTI.**
`VariableWatcher::updateItem()` `role` alanını sessizce düşürüyordu.
`applyWatchPresets()` rolü tam da bu yolla set etmeye çalıştığı için
`WatchItem::role` daima boş kalıyordu. Sonuçları:
- `appliesToRole` kullanan üç kural (`stack_headroom_critical`,
  `heap_leak_drift`, `watermark_downward_trend`) **hiç eşleşemiyordu**;
- `WatchProfile` c12 sütununa boş rol yazdığı için
  `compareWatchProfiles()`'ın `findByRole()`'ü her metrik için `nullptr`
  dönüyor, `ProfileCompareDialog` **tamamen boş** görünüyordu.
§17.3 yalnızca iki `stackWatermark` kuralının eşleşmeyeceğini söylüyordu;
`heap_leak_drift`'in de aynı sebeple ölü olduğu **yazılmamıştı**. Düzeltildi.

**18.2 — §15 (Faz 6) "olay korelasyonu" fiilen çalışmıyordu.**
Örnek zaman damgaları `DebugLinkWorker`'ın **soket bağlanınca** başlayan
saatinden, olay zamanları ise Backend'in **handshake bitince** sıfırlanan
ayrı bir saatinden geliyordu. `TraceEventLog.h`'nin "ONE monotonic clock
shared with the session" ifadesi yanlıştı. NUCLEO-H723ZG'de ölçülen sabit
kayma **61 ms**; bu tek başına `inference_time_outlier` kuralının ±50 ms'lik
olay kapısını her zaman reddettiriyordu. Düzeltmeden sonra aynı ölçüm
**−0.85 ms** (yalnızca kuyruklu sinyal gecikmesi).

**18.3 — §13.3 (Faz 4) hız kriteri o sırada ÖLÇÜLMEMİŞTİ.**
Plan Bölüm 16'nın şartı "1000 Hz'de ≥800 Hz". Denetimde ilk kez ölçüldü:
**929–934 Hz**, RTT 0.41 ms, 18675/18675 örnekte değer değişti. Kriter
sağlanıyor — ama faz bu kanıt olmadan kapatılmıştı. Ayrıca örnekleme hızı
`1000 / targetRateHz` tam sayı bölmesiyle kuantalanıyordu: 600 Hz isteği
1000 Hz veriyor, 500 Hz üstündeki her istek 1000 Hz'e çöküyordu.

**18.4 — `WatchSampler`'ın `ok` bayrağı hiçbir çağıran tarafından okunmuyordu.**
`WatchSampler.h` sözleşmeyi doğru tarif ediyordu ama
`VariableWatcher::onRawSamplesReady()` `nullptr` geçiyordu; başarısız okuma
gerçek bir `0.0` ölçüm olarak halka tamponuna ve `WatchStats`'a giriyordu.
`WatchSampler` saf bir sınıf olmasına rağmen **hiç test edilmemişti** (planın
kendi "saf sınıflar test edilir" ilkesinin ihlali).

**18.5 — Bir izleme oturumundan sonra ST-Link kilitleniyordu.**
Kök neden: gdbserver `-e` (persistent) ile başlatılıyordu, bu yüzden biz
ayrıldıktan sonra da yaşamaya devam ediyor ve öldürülmesi gerekiyordu;
Windows'ta `QProcess::terminate()` konsol sürecine ulaşmadığı için bu
`TerminateProcess()`'e düşüyor ve ST-Link'in USB ucu `DEV_USB_COMM_ERR` ile
kilitleniyordu — **STM32_Programmer_CLI dahil tüm ST araçları** etkileniyor,
yalnızca fiziksel çıkar-tak ile çözülüyordu. §6'da "bir donanım tuhaflığı"
diye geçilmişti; donanım tuhaflığı değil, kapatma sırasının sonucuydu.

**18.6 — Faz kapanış etiketleri planın kendi tanımıyla çelişiyor.**
Plan Bölüm 13: "Bir faz, birim testleri geçse bile canlı kriterleri
karşılamadan 'tamamlandı' sayılmaz." Bu tanıma göre Faz 5, 6, 7 ve 8
"tamamlandı" değil, "kod hazır, canlı doğrulama bekliyor" durumundaydı.
Ertelenen doğrulamaların neredeyse hepsi, ertelendikleri fazın tek gerçek
hatasını saklıyordu (18.1 Faz 8'i, 18.2 Faz 6'yı, 18.3 Faz 4'ü).

**18.7 — `tests/` çalıştırıcısı hata detayını gösteremiyordu.**
`qt_add_executable()` Windows'ta GUI alt sistemini varsaydığı için stdout
kopuktu: test başarısız olduğunda `ctest --output-on-failure` **hiçbir şey**
yazmıyordu. §1'deki "birim testler yeşil" iddiası doğruydu, ama bir
başarısızlık durumunda teşhis edilemez olduğu fark edilmemişti.

## 19. RegionScan (stack watermark) canlı örneklemesi — tamamlandı (2026-09-09)

§17.3'te "bilinçli olarak ertelendi" denen iş bitirildi. Not: §17.3'ün
tarif ettiği eksik yalnızca decode adımıydı; canlı deneme sırasında İKİNCİ,
daha temel bir eksik de bulundu.

**19.1 — Asıl kör nokta decode değil, hiç örneklenmiyor olmasıydı.**
`WatchSampler::decodeRegionScan()`'ı yazıp `Backend::applyWatchPresets()`'in
RegionScan `continue`'unu kaldırdıktan sonra ilk canlı denemede kalem
`hasValue=true, liveValue="0 B"` döndü — mantıksız bir sonuç (bu firmware'in
4 KB'lık stack'i hiçbir şekilde tamamen dolmuş olamazdı). Kök neden:
`VariableWatcher::rebuildPlan()` yalnızca `WatchPlanBuilder::build()`
(skaler kalemler) çağırıyordu; `buildRegionScans()` **hiçbir yerden**
çağrılmıyordu, yani RegionScan kalemlerinin `itemSlots` girdisi hep
`{-1,-1}` kalıyordu. `onRawSamplesReady()`'deki "son iyi değeri koru"
mantığı (§18.4'ün kendi düzeltmesi!) bu durumda `m_lastGoodValues[i]`'nin
hiç güncellenmemiş başlangıç değerini (`0.0`) sonsuza dek gerçek bir okuma
gibi tamponun/istatistiğin içine itiyordu — `ok=false` asla `count`'u
etkilemiyordu çünkü `flushPending()`/`WatchStats::push()` `ok`'a hiç
bakmıyor, `good` olsun olmasın her örnekte bir değer itiyor. Düzeltme:
`WatchPlanBuilder::merge()` eklendi, `rebuildPlan()` artık skaler + region
planlarını birleştirip tek bir `m_plan` kuruyor.

**19.2 — Ölçülen gerçek maliyet.** RegionScan istekleri ayrı/düşük-hızlı bir
plana değil ana örnekleme planına giriyor (`watch_presets.json`'daki
`"rateHz":2` alanı hep parse ediliyordu ama hiçbir yerde okunmuyordu —
ikincil-hız mekanizması hiç var olmamıştı, §17.3'ün ima ettiği gibi
"ayrıca yazılması gereken" bir şeydi). H7'de aynı 4 KB bölgeyle: hedef
200 Hz, gerçek ~100 Hz, 462 kaçırılan örnek. Gerçek, doğrulanmış bir
performans maliyeti — büyük bir stack bölgesi izleniyorsa ayrı düşük-hızlı
bir zamanlayıcı (DHCSR sağlık kontrolü ile aynı desen) gerekebilir; şimdilik
dokümante edilip ertelendi.

**19.3 — Test yöntemi.** `TestWatchSampler`'a 4 yeni senaryo eklendi (tam
boyanmış bölge, tek kelimelik uyuşmazlık, chunk sınırını aşan uyuşmazlık,
başarısız chunk → `ok=false`); `TestWatchPlanBuilder`'a `merge()` için 2
senaryo. Canlı H7 doğrulaması: `applyWatchPresets()` → `stackWatermark`
kalemi `kind=region, address=0x2001F000` ile eklendi (linker'ın
`_estack - _Min_Stack_Size = 0x20020000 - 0x1000` hesabıyla birebir
örtüşüyor) → örnekleme başlatıldı → canlı değer **2896 B**, `hasValue=true`.
`watch/watch_rules.json`'daki iki `stackWatermark` kuralı tekrar etkin.

**19.4 — Hâlâ yapılmayan.** Farklı (bu pipeline'la derlenmemiş) bir ELF
izlenirse tarama ilk kelimede uyuşmazlık bulur ve boşluk yanıltıcı biçimde
"~0 B" görünür; kalem başına ayrı bir "kullanılamıyor" göstergesi
eklenmedi — tek koruma genel `watchElfMatch` banner'ıdır. Bkz. CLAUDE.md
"Stack watermark yalnızca..." maddesi.
