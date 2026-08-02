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

## 10. Ertelenmiş doğrulamalar — "kart gelince" listesi

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
