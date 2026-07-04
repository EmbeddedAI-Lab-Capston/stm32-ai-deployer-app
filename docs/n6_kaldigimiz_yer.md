# STM32N6 Kaldigimiz Yer

Tarih: 2026-06-02 (asagida 2026-06-06 guncellemesi var)

Bu not, NUCLEO-N657X0-Q uzerinde monitor ve benchmark calismasinda kaldigimiz teknik durumu kaydetmek icin yazildi.

> **Guncel durum ozeti (2026-06-06):** Asagidaki "Su An Takildigimiz Nokta"
> bolumunde anlatilan UART RX/komut sorunu, komut-cevap modelini terk edip
> **LPUART1 @ 209700 baud + reset-sonrasi pasif yakalama** mimarisine
> gecilerek asildi. Detay icin dosyanin sonundaki "Guncelleme 2026-06-06"
> bolumune bakin. Doğrulanmış startup/linker/HAL şablon seti hâlâ `TODO.md`'de
> açık madde — mevcut çözüm çalışıyor ama "tam kanıtlanmış" değil.

## Kart ve Baglanti

- Kart: NUCLEO-N657X0-Q
- ST-LINK SN: `001A00273434511734313937`
- UART VCP: `COM20`
- UART hiz: `115200`
- Programmer: `STM32_Programmer_CLI v2.22.0`
- SigningTool: `STM32_SigningTool_CLI v2.22.0`
- External loader: `MX25UM51245G_STM32N6570-NUCLEO.stldr`
- SDK: `STM32Cube_FW_N6_V1.2.0`
- X-CUBE-AI runtime: `10.2.0`

## Boot / Flash Durumu

N6 external flash boot problemi cozuldu.

Kalici olmasi gereken kritik ayarlar:

- FSBL LRUN destination: `0x34000000`
- FSBL header offset: `0x400`
- FSBL source size: `0x00100000`
- App flash adresi: `0x70100000`
- FSBL flash adresi: `0x70000000`
- App ve FSBL signing argumanlarinda `-align` kullanilmali.

`-align` olmadan SigningTool header v2.3 payload'i `0x400` hizasina koymuyor. Flash verify gecse bile external boot dogru kalkmayabiliyor.

Flash icin calisan mod:

- Yazma modu: `BOOT1 = 2-3`
- Calistirma modu: `BOOT0 = 1-2`, `BOOT1 = 1-2`

## Kanitlanan Calisan Kisim

External boot sonrasi firmware UART boot JSON basabiliyor:

```json
{"t":"boot","card":"NUCLEO-N657X0-Q","sdk":"EdgeAI_v1.0","model":"ucihar_cnn1d_int8","sensor":"BME280","baud":115200,"flash_kb":65536,"ram_kb":4096,"clock_mhz":600}
```

Bu, FSBL + external flash + app entry point zincirinin calistigini gosteriyor.

## Su An Takildigimiz Nokta

Kart UART uzerinden TX yapiyor, yani karttan PC'ye boot mesaji geliyor.

Ancak PC'den karta gonderilen komutlara cevap alinmiyor:

- `INFO?` -> bos
- `BOOT?` -> bos
- `INFER 100 200 300` -> bos
- `BENCH 5 0 1000 42` -> bos
- `BENCH 20 0 1000 42` -> bos

Yani mevcut problem artik flash/boot degil. Problem UART RX yolu veya firmware komut alma tarafinda.

## Yapilan Firmware Degisiklikleri

`templates/base/STM32N6/Src/main.c` ve test projesi `C:\Users\kadirmert\Desktop\n6_test\Src\main.c` icin yapilan denemeler:

- `AI_Runner_Init()` startup'tan kaldirildi, lazy init yapildi.
- `BENCH` ve `INFER` ilk cagrida AI init edecek hale getirildi.
- UART komut polling eklendi.
- `HAL_UART_Receive(..., timeout=0)` yerine dogrudan `USART1->ISR & USART_ISR_RXNE_RXFNE` ve `USART1->RDR` okuma denendi.
- `MX_I2C1_Init()` ve `Sensor_Init()` startup'tan kaldirildi. Amac: BME280/I2C hatasi UART komut yolunu bloke etmesin.
- `PRIMASK`, `FAULTMASK`, `BASEPRI` temizligi eklendi.
- PE5/PE6 icin `HAL_GPIO_ConfigPinAttributes(GPIOE, GPIO_PIN_5 | GPIO_PIN_6, GPIO_PIN_SEC | GPIO_PIN_NPRIV)` denendi.

Bu degisikliklerden sonra boot JSON gelmeye devam ediyor, fakat komut cevaplari hala bos.

## N6 UART Ayarlari

Su an kullanilan UART:

- Instance: `USART1`
- TX: `PE5`
- RX: `PE6`
- AF: `GPIO_AF7_USART1`
- Clock source: `RCC_USART1CLKSOURCE_HSI`
- Baud: `115200`

ST'nin NUCLEO-N657X0-Q orneklerinde de PE5/PE6 USART1 kullanimi goruldu. Arkadas raporunda gecen `LPUART1 @ 209700` bizim mevcut calisan boot TX yoluyla birebir uyumlu degil; bu yuzden dogrudan ona gecilmedi.

## Bir Sonraki Adimlar

1. Host tarafinda COM20 testini DTR/RTS acik sekilde tekrar dene.
2. Firmware'e minimal RX debug ekle:
   - Gelen her byte'i aninda echo et.
   - `USART1->ISR` degerini periyodik raporla.
   - RXNE goruluyor mu netlestir.
3. ST `UART_HyperTerminal_IT` ornegindeki LL RX modelini template'e kucuk olcekli uygula:
   - `LL_USART_EnableIT_RXNE(USART1)`
   - `LL_USART_ReceiveData8(USART1)`
   - `USART1_IRQHandler` icinde byte besleme.
4. Gerekirse USART1 clock source'u ST tracer ornegindeki gibi `RCC_USART1CLKSOURCE_IC9 + PLL4` ile dene.
5. BME280/I2C gercek sensor monitor icin ayrica ele alinacak:
   - Su an synthetic `INFER/BENCH` sensor bagimsiz calismali.
   - N6 icin `PB6/PB7` I2C pinleri supheli; raporda `PH9/PC1` veya `PB10/PB11` alternatifleri geciyor.

## Son Flashlanan Test Binary

Son yazilan test binary:

```text
C:\Users\kadirmert\Desktop\n6_test\build\ucihar_cnn1d_int8_NUCLEO-N657X0-Q-trusted-align.bin
```

Son build/sign bilgisi:

- Entry point: `0x3400906d`
- App address: `0x70100000`
- Verify: basarili

Son test sonucu:

- Boot JSON: geldi
- `INFO?`, `BOOT?`, `INFER`, `BENCH`: cevap yok

## Guncelleme 2026-06-06 — Komut yolu terk edildi, pasif yakalamaya gecildi

Yukarida anlatilan RX/komut sorunu (host'tan giden `INFO?`/`BENCH` gibi
komutlara firmware'in cevap vermemesi) cozulmedi — bunun yerine mimari
degistirildi ve soruna ihtiyac kalmadi:

- Firmware UART instance'i **USART1 → LPUART1** olarak degisti, baud
  **115200 → 209700**'e cikarildi (`templates/base/STM32N6/Src/main.c`,
  `MX_LPUART1_UART_Init`).
- N6 icin artik host'tan komut gonderilmiyor. Bunun yerine `Backend`
  (`src/bridge/Backend.cpp`) N6 karti/portu tespit edince
  (`boardOrPortLooksLikeN6`) baglanti anında **STM32_Programmer_CLI ile karti
  resetler** (`resetN6TargetForCapture()`) ve firmware'in reset sonrasi
  kendiliginden urettigi boot/inference JSON akisini **pasif olarak dinler**.
  Komut-cevap protokolu (INFO?/BENCH) N6 icin kullanilmiyor; F4/H7'de
  degismedi.
- `AppState::setActiveBoard` N6 secildiginde baud'u otomatik `209700`'e
  ayarlar (`kN6DefaultBaud`), kullanicinin elle baud secmesine gerek yok.
- Sonuc: N6 uzerinde boot mesaji + reset sonrasi otonom inference/sensor
  akisi guvenilir sekilde calisiyor. Interaktif komut-cevap (canli `BENCH N`
  parametreleri gibi) N6'da hala desteklenmiyor — F4/H7'de oldugu gibi
  kullanici tetikli benchmark N6'da reset+capture akisina indirgenmis
  durumda.
- Acik kalan is: `TODO.md`'de "STM32N6 icin dogrulanmis startup/linker/HAL
  template seti ekleme" hala isaretlenmemis — mevcut LPUART1 cozumu pratikte
  calisiyor ama sablon seti tam anlamiyla "kanitlanmis/genellenmis" degil.

