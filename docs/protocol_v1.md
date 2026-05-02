# STM32 AI Deployer — UART Protocol v1

## Overview

Communication between the STM32 firmware and the desktop application uses a line-based
JSON protocol over UART. Each packet is self-contained and human-readable.

## Packet Format

```
§{JSON object}\r\n
```

| Part     | Value                             | Notes                             |
|----------|-----------------------------------|-----------------------------------|
| `§`      | `0xC2 0xA7` (UTF-8 section sign)  | Start marker, easy to scan for    |
| `{...}`  | Compact JSON object               | No internal newlines              |
| `\r\n`   | CR LF                             | End of packet                     |

- All numeric values are **integers** (no floating-point)
- String values are ASCII only
- Maximum packet length: **256 bytes**

---

## Message Types

### `inf` — Inference Metric

Emitted after each inference run.

```json
{
  "t": "inf",
  "model": "MLP_INT8",
  "inf_us": 8200,
  "ram_b": 3072,
  "acc_pct": 96,
  "label": "walking",
  "card": "STM32F4"
}
```

| Field     | Type    | Unit         | Description                         |
|-----------|---------|--------------|-------------------------------------|
| `t`       | string  | —            | Message type: `"inf"`               |
| `model`   | string  | —            | Model identifier                    |
| `inf_us`  | integer | microseconds | Inference duration                  |
| `ram_b`   | integer | bytes        | RAM used by model                   |
| `acc_pct` | integer | percent      | Top-1 accuracy (0–100)              |
| `label`   | string  | —            | Predicted class label               |
| `card`    | string  | —            | Board identifier                    |

---

### `sys` — System Status

Emitted at approximately 1 Hz.

```json
{
  "t": "sys",
  "uptime_s": 42,
  "temp_c": 38,
  "free_ram_b": 185000,
  "state": "running"
}
```

| Field        | Type    | Unit    | Description                              |
|--------------|---------|---------|------------------------------------------|
| `t`          | string  | —       | Message type: `"sys"`                    |
| `uptime_s`   | integer | seconds | Seconds since last reset                 |
| `temp_c`     | integer | °C      | MCU die temperature                      |
| `free_ram_b` | integer | bytes   | Free heap bytes                          |
| `state`      | string  | —       | `"running"` / `"idle"` / `"error"`       |

---

### `boot` — Boot Message

Emitted once after reset.

```json
{
  "t": "boot",
  "card": "STM32F4",
  "sdk": "EdgeAI_v1.0",
  "model": "MLP_INT8",
  "baud": 115200
}
```

| Field   | Type    | Description                             |
|---------|---------|-----------------------------------------|
| `t`     | string  | Message type: `"boot"`                  |
| `card`  | string  | Board identifier                        |
| `sdk`   | string  | Edge AI SDK version                     |
| `model` | string  | Initially loaded model name             |
| `baud`  | integer | Baud rate in use                        |

---

### `err` — Error

Emitted on firmware errors.

```json
{
  "t": "err",
  "code": 3,
  "msg": "sensor_timeout"
}
```

| Field  | Type    | Description              |
|--------|---------|--------------------------|
| `t`    | string  | Message type: `"err"`    |
| `code` | integer | Error code               |
| `msg`  | string  | Short error description  |

---

## Parser Implementation Notes

1. Buffer incoming bytes until `\r\n` is found
2. Check that the line starts with the `§` byte sequence (`0xC2 0xA7`)
3. Extract the substring after `§` and before `\r\n`
4. Parse as JSON; check the `"t"` field to dispatch to the correct handler
5. Discard lines that do not start with `§` (they may be debug prints)

---

## Error Codes

| Code | Name              | Description                     |
|------|-------------------|---------------------------------|
| 1    | `init_failed`     | Sensor or peripheral init error |
| 2    | `model_load_err`  | Model buffer corrupt            |
| 3    | `sensor_timeout`  | Sensor read timed out           |
| 4    | `inference_ovf`   | Inference time exceeded budget  |
| 5    | `uart_overflow`   | TX buffer overflow              |
