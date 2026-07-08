# Register Inspector — JSON Export Şeması

> **Durum:** Stabil sözleşme (contract). `schemaVersion` alanı olmadan
> geriye dönük uyumsuz bir değişiklik yapılmaz — yeni alanlar eklenebilir,
> mevcut alanlar silinmez/anlamı değiştirilmez. LLM entegrasyonuyla
> çalışan taraflar bu şemaya karşı mock veriyle geliştirme yapabilir.

Üreten kod: `Backend::exportRegisterSnapshotJson(path)`
(`src/bridge/Backend.cpp`). "JSON Dışa Aktar" butonuyla tetiklenir.

## Kapsam

Dışa aktarım her zaman **o an ekranda görüntülenen slot**'u (A veya B)
içerir. Her iki slot da doluysa (`registerDiffAvailable() == true`),
**diff** ve **kural motoru ihlalleri** otomatik olarak eklenir — ayrı bir
"export modu" seçimi yok, mevcut en zengin veri tek dosyada.

## Şema

```json
{
  "schemaVersion": 1,
  "exportedAt": "2026-07-06T21:15:00",

  "board": {
    "name": "STM32H7",
    "device": "STM32H723",
    "svdFile": "STM32H723.svd",
    "supportLevel": "stable"
  },

  "snapshot": {
    "slot": "A",
    "takenAt": "2026-07-06T21:10:34",
    "connectMode": "HOTPLUG",
    "peripherals": [
      {
        "name": "USART3",
        "description": "Universal synchronous asynchronous receiver transmitter",
        "clock": "on",
        "registers": [
          {
            "name": "CR1",
            "addr": "0x40004800",
            "description": "Control register 1",
            "raw": "0x0000002D",
            "reset": "0x00000000",
            "changed": true,
            "status": "ok",
            "fields": [
              {
                "name": "UE",
                "description": "USART enable",
                "bitOffset": 0,
                "bitWidth": 1,
                "value": 1,
                "resetValue": 0,
                "enumName": "",
                "changed": true
              }
            ]
          }
        ]
      }
    ]
  },

  "diff": {
    "present": true,
    "takenAtA": "2026-07-06T21:10:34",
    "takenAtB": "2026-07-06T21:12:01",
    "changedRegisters": [
      {
        "peripheral": "USART3",
        "register": "CR1",
        "addr": "0x40004800",
        "statusA": "ok",
        "statusB": "ok",
        "changedFields": [
          {
            "name": "UE",
            "description": "USART enable",
            "before": 0,
            "after": 1
          }
        ]
      }
    ]
  },

  "ruleViolations": [
    {
      "ruleId": "usart_enabled_no_baud",
      "severity": "warning",
      "peripheral": "USART3",
      "register": "CR1",
      "field": "UE",
      "message": "USART3: UE=1 ama BRR=0 (baud rate ayarlanmamış olabilir)"
    }
  ]
}
```

## Alan notları

- **Hiçbir zaman sadece ham hex verilmez** — her register'ın `raw`/`reset`
  değerleri hex string olarak verilir (`"0x...."`, kolay okunur/loglanır),
  ama asıl anlam her zaman `fields[]` altında **field/enum seviyesinde**
  taşınır. Bu, LLM promptlarının hex yorumlamak zorunda kalmaması için
  bilinçli bir tasarım (plan Bölüm 9).
- `diff.present == false` ise `diff.changedRegisters` boş dizi olur (slot B
  henüz alınmamışsa).
- `status` alanı `"ok" | "clock-off" | "side-effect" | "write-only" |
  "unreadable"` değerlerinden biridir — `"ok"` olmayan register'ların
  `fields[]` dizisi boştur (decode edilmemiştir).
- `ruleViolations`, kural motorunun (`svd/rules.json`, deterministik,
  LLM'siz) o an görüntülenen slot üzerinde ürettiği ihlal listesidir.
