# STM32N6 Kaldigimiz Yer

Tarih: 2026-06-02

Bu not, NUCLEO-N657X0-Q uzerinde monitor ve benchmark calismasinda kaldigimiz teknik durumu kaydetmek icin yazildi.

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

