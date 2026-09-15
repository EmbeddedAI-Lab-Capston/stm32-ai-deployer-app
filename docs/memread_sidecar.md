# stm32aid-memread — Kalıcı Bağlantılı Bellek Okuma Yardımcısı

Oluşturma: **2026-09-15**
İlgili commit'ler: `6e2f278`, `e9951c6`, `30cfad1`, `05df534`

Değişken İzleyici'nin iki okuma yolundan biri. Diğeri `ST-LINK_gdbserver` +
GDB RSP (`DebugLink` + `DebugLinkWorker`), o silinmedi ve çalışıyor.

---

## 1. Neden var

İki ayrı sebep, ikisi de ölçüldü.

**STM32N6'da başka seçenek yok.** `ST-LINK_gdbserver` N6 çekirdeğini
durduramıyor, dolayısıyla hiç oturum açamıyor — `-g`, `-k`, `--halt`, `-m 0`,
`--frequency`, `--pend-halt-timeout`, `-t` ve iki farklı CubeProgrammer `-cp`
denendi, hepsi aynı yerde düştü. Detay: [`n6_kaldigimiz_yer.md`](n6_kaldigimiz_yer.md) §9.

**Her yerde daha güvenilir.** Yüksek hızda fark belirgin:

| H7, 1000 Hz hedef | Gerçekleşen | Kaçırılan |
|---|---|---|
| gdbserver | 990.0 Hz | **92** |
| memread | **1000.0 Hz** | **0** |

Sebep mimari: memread bir örneğin **tüm aralıklarını tek alışverişte** okur,
RSP ise aralık başına bir gidiş-dönüş yapar.

### Denenen ve elenen yol: her okumada CLI çalıştırmak

`STM32_Programmer_CLI` tek atışlık bir araçtır. Birden fazla `-r32` argümanı
tek çağrıda verilebilir, ama çağrının kendisi pahalıdır:

| Yöntem | Örnek başına | Hız |
|---|---|---|
| CLI, her örnekte yeni süreç | 312 ms | **3.1 Hz** |
| CLI + ST-Link Server (`shared`) | 3228 ms | **0.3 Hz** |
| Kalıcı bağlantı (bu doküman) | 1.7–1.9 ms | **520–594 Hz** |

O 312 ms'nin neredeyse tamamı **SWD okuması değil, süreç başlatma**: exe yükle,
DLL'leri çöz, USB'yi tara, bağlan, oku, ayrıl. Gerçek okuma 1 ms'nin altında.

`shared` modunun **daha yavaş** çıkması sezgiye aykırı ama ölçüm net — ST-Link
Server üzerinden gitmek her çağrıya ek maliyet bindiriyor, azaltmıyor.

---

## 2. Neden ayrı bir süreç — EN KRİTİK BÖLÜM

> Bu bölümü okumadan "niye DLL'i doğrudan uygulamaya yüklemiyoruz" diye
> sormayın. Denendi, çalışmıyor, sebebi aşağıda.

ST, CLI'nin içeride kullandığı C API'sini resmî olarak dağıtıyor:
`STM32CubeProgrammer/api/` altında header, DLL, dokümantasyon (`.chm`), ST'nin
kendi örnek programları ve hatta bir **Qt/MinGW örnek projesi** var. Yani bu
tersine mühendislik değil, ST'nin üçüncü taraf araçlar için açtığı kapı.

**Ama süreç içine yüklenemiyor.** `CubeProgrammer_API.dll`'in bağımlılıkları:

```
libgcc_s_seh-1.dll  KERNEL32.dll  msvcrt.dll  SHLWAPI.dll  libstdc++-6.dll
Qt6Core.dll  Qt6Qml.dll  Qt6SerialPort.dll  Qt6Xml.dll
libcrypto-3-x64.dll  libminizip.dll
```

ST'nin taşıdığı Qt **6.10.2**, bu uygulama **6.11.0**. Windows bir süreçte aynı
isimli DLL'den tek tane yükler. Uygulamanın Qt'si önce yüklendiği için ST'nin
DLL'i kendi sürümünün sembollerini bulamıyor:

```
Cannot load library ...CubeProgrammer_API.dll:
The specified procedure could not be found.
```

Üstelik çakışma tam: Core, Qml ve SerialPort'un üçünü de bu uygulama kullanıyor.

**Kanıt zinciri:** Qt'ye hiç bağlanmayan bir prototip çalıştı; aynı kod Qt'li bir
programa konunca yukarıdaki hatayı verdi.

Bu yüzden `stm32aid-memread` **Qt'ye hiç bağlanmaz**. Doğrulanabilir:

```
$ objdump -p build/stm32aid-memread.exe | grep "DLL Name:"
libgcc_s_seh-1.dll   KERNEL32.dll   msvcrt.dll   libstdc++-6.dll
```

Sıfır Qt. ST'nin kopyaları onun yanında sorunsuz yükleniyor.

---

## 3. Mimari

```
STM32AiDeployer (Qt 6.11)
  VariableWatcher ──► DebugLink ──┬─ Backend::Gdb     ─► GdbServerProcess + DebugLinkWorker ─► TCP ─► gdbserver
                                  └─ Backend::MemRead ─► MemReadWorker + MemReadClient
                                                              │ QProcess, stdin/stdout
                                                              ▼
                                                     stm32aid-memread (Qt YOK)
                                                              │ CubeProgrammer_API.dll
                                                              ▼
                                                          ST-Link ─► hedef
```

**`DebugLink`'in public yüzeyi değişmedi.** İki worker aynı sinyalleri aynı
anlamlarla yayar, bu yüzden `VariableWatcher` ve `Backend` **tek satır bile
değişmedi**.

| Dosya | Rol |
|---|---|
| `src/modules/debug/MemReadProtocol.h/.cpp` | Tel protokolü. **Saf C++**, iki taraf da derler. 17 birim testi. |
| `src/modules/debug/MemReadClient.h/.cpp` | Sidecar süreç yönetimi + protokol köprüsü. **Bloklar** — sadece worker thread'inde. |
| `src/modules/debug/MemReadWorker.h/.cpp` | QThread worker. Hız ayarı, DHCSR sağlığı, istatistik. |
| `tools/memread/CubeProgReader.h/.cpp` | DLL sarmalayıcı. **Saf C++**, Qt yok. |
| `tools/memread/main.cpp` | Sidecar giriş noktası. |

### Sidecar "aptal"dır — bilinçli

Hız ayarı, DHCSR sağlık kontrolü, toplu yayın, skew ölçümü gibi akıllı kısımlar
**Qt tarafındaki `MemReadWorker`'da** kaldı. Sidecar sadece "şu aralıkları oku"
der. Gerekçe: o mantık `DebugLinkWorker`'da zaten test edilmiş ve CLAUDE.md'de
kalıcı karar olarak yazılı; Qt'siz bir binary'de kopyalamak ikisinin birbirinden
ayrılmasına davetiye olurdu.

### Yaşam döngüsü pipe'a bağlı

Sidecar stdin'i EOF görene kadar okur ve orada çıkar. Kapatma protokolü budur:
`MemReadClient::stop()` sadece `closeWriteChannel()` çağırır. Öldürme yalnızca
takılmış süreç için yedek yol.

**Bu bilinçli olarak gdbserver'ın tersi.** Onun persistent modu biz ayrıldıktan
sonra dinlemeye devam ediyor, dolayısıyla onu öldürmemiz gerekiyor; Windows'ta
bu fiilen `TerminateProcess()` olup ST-Link'in USB ucunu **fiziksel çıkar-tak
gerektirecek** şekilde kilitliyordu (CLAUDE.md, review K-3). Burada port yok,
dinlemede kalan sunucu yok, sızdırılacak tutamaç yok.

---

## 4. Gözlemci ilkesi — yapısal, disiplinle değil

`CubeProgReader` DLL'den **yalnızca `allowedSymbols()` listesindeki sembolleri**
çözümler:

```
setDisplayCallbacks  setVerbosityLevel  setLoadersPath  getStLinkList
connectStLink        readMemory         freeLibraryMemory  disconnect
```

`writeMemory`, `massErase`, `sectorErase`, `sendResetCommand` için
`GetProcAddress` **hiç çağrılmaz**. Yani hedefe yazmak ya da onu resetlemek
sonraki bir hatayla bile mümkün değil — imkânsız.

Bu, gdbserver yolundaki `GdbRspCodec::isAllowedOutgoing()` beyaz listesinin
birebir karşılığıdır. `isMutatingSymbol()` liste büyüdükçe kuralı korur ve
`TestMemReadProtocol` yanındaki testler bunu doğrular.

**Ek fayda:** gdbserver `DHCSR.C_DEBUGEN`'i set eder, memread etmez. Ölçüldü —
aynı kartta gdb `0x01010001`, memread `0x01010000`. Yani yeni varsayılan aynı
zamanda daha az müdahaleci olan.

---

## 5. Protokol

Her mesaj: **4 baytlık little-endian uzunluk öneki + gövde**. Uzunluk sadece
gövdeyi kapsar, böylece mesaj sınırı taşımayan bir akıştan okuyan taraf ne kadar
bekleyeceğini her zaman bilir.

| Komut | Gövde | Yanıt |
|---|---|---|
| `Ping` (1) | — | sürüm metni |
| `Connect` (2) | seri numarası | kart adı, ya da hata |
| `Read` (3) | N × (id, adres, uzunluk) | N × (id, adres, ok, veri, hata) |
| `Disconnect` (4) | — | onay |

Tasarım notları:

- **Veri uzunluk önekli**, sonlandırıcılı değil — hedef belleği `0x00` ve `0x0A`
  baytlarıyla dolu, herhangi bir sonlandırıcı veriyi bozardı.
- Sidecar stdin/stdout'u **ikili moda** alır (`_setmode(..., _O_BINARY)`).
  Alınmazsa Windows `0x0A` baytlarını yeniden yazar ve içinde satır sonu geçen
  her okuma sessizce bozulur.
- İnanılmaz uzunluk (`kMaxPayloadBytes` üstü) **oturumu bitirir**, atlanmaz —
  senkronu kaybetmiş bir akışı komut diye çözmek daha kötü olurdu.
- Hedefteki okuma hatası farklıdır: `ok=false` olarak geri gelir, böylece kısmen
  başarılı bir örnek yine kullanılabilir.

17 birim testi bunu kapsıyor: gidiş-dönüş, içinde NUL geçen ikili veri, parça
parça gelen çerçeveler, ve geçerli bir mesajın **her kesme noktası**.

---

## 6. Ölçümler (2026-09-15, canlı donanım)

### Üç kart, 200 Hz hedefi — hepsinde 0 kaçırılan, 0 okuma hatası

| Kart | Gerçekleşen | rtt | skew | `SysTick_LOAD` → saat |
|---|---|---|---|---|
| F4 | 200.0 Hz | 0.92 ms | 884 µs | 167999 → **168 MHz** |
| H7 | 200.4 Hz | 0.79 ms | 707 µs | 274999 → **275 MHz** |
| N6 | 200.0 Hz | 0.70 ms | 690 µs | 599999 → **600 MHz** |

Son sütun sadece "veri geliyor" demiyor: üç kartın da **bilinen gerçek saatiyle
birebir tutuyor**, yani okunan değerler doğru.

### Yüksek hız

- N6, 500 Hz hedefi: **501.0 Hz**, 0 kaçırılan, rtt 0.87 ms
- H7, 1000 Hz hedefi: **1000.0 Hz / 0 kaçırılan** (gdb: 990.0 / 92)

### Dayanıklılık (40 sn, maksimum hız, N6)

```
21.226 örnek / 106.130 okuma
530.6 Hz   (hiç düşmedi: 531.6 → 531.9 → 531.0 → 530.6)
0 hata
RSS 16.128 KB → 16.144 KB   (+16 KB — sızıntı yok)
firmware tick 39.724 ms ilerledi, ölçüm 40.001 ms sürdü
```

Son satır önemli: hedefin kendi zamanı ölçüm süresiyle örtüşüyor, yani okumalar
gerçek ve **hedef hiç rahatsız edilmedi**.

---

## 7. Tuzaklar — hepsi gerçekten vakit kaybettirdi

**`connectStLink` `-545` döndürüyor.** Sebep `setLoadersPath()` çağrılmaması;
API cihaz veritabanını o yoldan çözüyor. Hata kodu hiçbir enum'da yok.

**`QLibrary` DLL'i bulamıyor (hata 126).** Dosyayı mutlak yolla vermek yetmez —
API kendi klasöründen bir düzine kardeş DLL çekiyor ve onlar arama yolundan
çözülüyor. Yüklemeden önce `SetDllDirectoryW(binDir)`, sonra geri al.

**STM32CubeIDE'nin paketlediği CubeProgrammer'da API DLL'i YOK.** Sadece
`STM32_Programmer_CLI.exe` var. gdbserver için gayet iyi bir `-cp`, bunun için
işe yaramaz. Bu yüzden API dizini `-cp` dizininden **ayrı çözümlenir**
(`main.cpp`): önce ayardaki dizin, sonra CLI'nin dizini, sonra `ToolDetector`.

**`freeLibraryMemory()` her `readMemory`'den sonra çağrılmalı.** ST her çağrıda
yeni tampon ayırıyor. 500 Hz × 5 aralık = saniyede 2500 ayırma; çağrılmazsa
dakikalar içinde ciddi sızıntı.

**`DebugLink` null dereference'ı.** Kapanış ve hata yollarında
`m_gdbProcess->stop()` koşulsuz çağrılıyordu; gdbserver'ı olmayan bir arka uçta
bu segfault. Canlı koşuda yakalandı, düzeltildi.

---

## 8. Yapılandırma

```
watch/link_backend = "memread"   (varsayılan)  | "gdb"
```

Transport **ilk `retain()`'den önce** seçilir — canlı sahipler varken
değiştirmek onları yok olacak bir worker'da bırakırdı (`Q_ASSERT` ile korunuyor).

**Otomatik geri düşüş:** API DLL'i veya sidecar bulunamazsa ayar ne derse desin
`"gdb"` kullanılır ve sebebi `qWarning` ile yazılır. Sadece CubeIDE'nin
CubeProgrammer'ına sahip bir makinede bu fark yaratır.

---

## 9. Bilinen sınırlar / açık işler

- **Dağıtım düşünülmedi.** `stm32aid-memread.exe` şu an `build/` köküne çıkıyor
  ve uygulama kendi yanında arıyor. Geliştirmede çalışır; kurulum paketi
  yapılacaksa ayrıca ele alınmalı. `libgcc_s_seh-1.dll` ve `libstdc++-6.dll`
  yanında bulunmalı (uygulamanın kendisi de zaten onlara bağlı).
- **Register Inspector bu yolu kullanmıyor.** Hâlâ `CliRegisterReader`
  (varsayılan) veya `GdbServerReader`. memread'i üçüncü bir `IRegisterReader`
  gerçeklemesi yapmak mantıklı bir sonraki adım — snapshot başına ~273 ms olan
  CLI maliyetini düşürebilir.
- **`kMaxReadBytes` bilerek 4096.** API çok daha büyük okuma yapabilir, ama bu
  değer gdbserver yolunun ulaştığı tavanla aynı tutuldu ki `WatchPlanBuilder`
  planı iki transport'ta da **aynı bloklara** bölsün. Yükseltmek kanıtlanmış plan
  şekillerini ölçülmemiş bir kazanç için değiştirirdi.
- **Kurtarma yolları sınırlı test edildi.** Sidecar çökmesi ve pipe kopması
  işleniyor (`handleTransportLoss`), ama USB kablosu çekilmesi gibi senaryolar
  canlı denenmedi.
- **N6'da tek başına yetmez:** kart geliştirme boot pozisyonunda olmalı, yoksa
  hiçbir transport bellek okuyamaz. Bkz. [`n6_kaldigimiz_yer.md`](n6_kaldigimiz_yer.md) §3.

---

## 10. Sorun giderme

| Belirti | Sebep |
|---|---|
| `The specified procedure could not be found` | Qt sürüm çakışması — DLL süreç içine yüklenmeye çalışılmış (§2) |
| `CubeProgrammer_API.dll yuklenemedi (126)` | `SetDllDirectoryW` yapılmamış, ya da dizinde API DLL'i yok (CubeIDE kopyası) |
| `ST-Link baglantisi basarisiz (kod -545)` | `setLoadersPath()` çağrılmamış |
| `stm32aid-memread bulunamadi` | Sidecar uygulamanın yanında değil; `cmake --build` ile üretiliyor |
| Link açılmıyor, N6 | BOOT jumper'ı flash-boot konumunda olabilir |
| Yavaş yavaş artan bellek | `freeLibraryMemory()` atlanmış |
