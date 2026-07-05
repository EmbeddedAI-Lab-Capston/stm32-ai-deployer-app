# SVD Files — Register Inspector

Bu klasör, Register Inspector'ın register tanımlarını okuduğu **CMSIS-SVD**
(System View Description) dosyalarını ve kart eşleme dosyasını (`boards.json`)
içerir. Uygulama derleme sonrası bu klasörü exe yanına kopyalar; çalışma
zamanında `applicationDirPath()/svd` üzerinden okunur.

## İçerik

| Dosya | Cihaz | Kart preset'i | Boyut |
|-------|-------|---------------|-------|
| `STM32F407.svd` | STM32F407 | STM32F4 | ~2 MB |
| `STM32H723.svd` | STM32H723 | STM32H7 (NUCLEO-H723ZG) | ~3.9 MB |
| `STM32N657.svd` | STM32N657 | STM32N6 (deneysel) | ~23 MB |
| `boards.json` | — | kart → SVD eşlemesi + meta | — |

## Kaynak ve Lisans

SVD dosyaları **modm-io/cmsis-svd-stm32** aynasından alınmıştır:
<https://github.com/modm-io/cmsis-svd-stm32> (main dalı, indirme: 2026-07-05).

Bu ayna, STMicroelectronics'in resmi SVD'lerinin bir kopyasıdır ve
**Apache-2.0** lisanslıdır. Her `.svd` dosyası kendi başlığında SPDX
tanımlayıcısını ve ST telif hakkı bildirimini taşır:

```
Copyright (c) 2024 STMicroelectronics.
SPDX-License-Identifier: Apache-2.0
```

Dosyalar değiştirilmeden dağıtılır. Apache-2.0 tam metni:
<https://www.apache.org/licenses/LICENSE-2.0>.

> **Neden repoya commit edildi:** Bitirme demosu internet bağımsız olmalı
> (plan kararı A1). Dosyalar büyük (özellikle N6 ~23 MB); N6 deneysel hedef
> olduğundan ileride ayrı indirmeye alınması değerlendirilebilir.

## Yeni kart ekleme

1. İlgili `.svd` dosyasını bu klasöre koy (modm aynasından veya ST'den).
2. `boards.json`'a bir kayıt ekle: `match` (kart adı ve/veya probe deviceId),
   `svd` (dosya adı), `access`, `defaultPeripherals`.

C++ tarafında hiçbir değişiklik gerekmez — kod tamamen veri-tabanlıdır
(plan Bölüm 3.3).
