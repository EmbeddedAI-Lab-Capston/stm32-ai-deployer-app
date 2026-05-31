# STM32 AI Deployer - UART Protocol v1

## Overview

Communication between the STM32 firmware and the desktop application uses a line-based
JSON protocol over UART. Each packet is self-contained and human-readable.

## Packet Format

```text
§{JSON object}\r\n
```

| Part | Value | Notes |
| --- | --- | --- |
| `§` | `0xC2 0xA7` UTF-8 section sign | Start marker, easy to scan for |
| `{...}` | Compact JSON object | No internal newlines |
| `\r\n` | CR LF | End of packet |

- Numeric values are integers.
- String values should be ASCII where possible.
- Maximum packet length is 256 bytes.

## Message Types

### `inf` - Inference Metric

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

### `sensor` - Real Sensor Sample And Result

Emitted when the firmware reads real sensor values and optionally runs inference on
that frame. The desktop app stores these rows under **Gerçek Sensör Veri Analizleri**.

```json
{
  "t": "sensor",
  "sensor": "BME280",
  "seq": 42,
  "values": [25120, 100840, 43600],
  "unit": "milli",
  "model": "anomaly_cnn_int8",
  "inf_us": 956,
  "ram_b": 6594,
  "acc_pct": 90,
  "label": "normal",
  "card": "STM32H7"
}
```

| Field | Type | Description |
| --- | --- | --- |
| `t` | string | Message type: `"sensor"` |
| `sensor` | string | Sensor identifier, for example `BME280`, `MPU6050`, `PDM_MIC` |
| `seq` | integer | Monotonic sample/frame counter |
| `values` | array/string | Sensor readings; integers are recommended |
| `unit` | string | Unit or scale, for example `milli`, `mg`, `raw` |
| `model` | string | Model identifier used for inference |
| `inf_us` | integer | Inference duration in microseconds, or `0` if not run |
| `ram_b` | integer | RAM used by model, or `0` if unavailable |
| `acc_pct` | integer | Confidence/accuracy percentage |
| `label` | string | Predicted class label |
| `card` | string | Board identifier |

### `sys` - System Status

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

### `boot` - Boot Message

Emitted once after reset.

```json
{
  "t": "boot",
  "card": "STM32F4",
  "sdk": "EdgeAI_v1.0",
  "model": "MLP_INT8",
  "sensor": "MPU6050",
  "baud": 115200
}
```

### `bench` - Benchmark Summary

Emitted after a benchmark run.

```json
{
  "t": "bench",
  "model": "MLP_INT8",
  "samples": 100,
  "avg_us": 8200,
  "min_us": 7900,
  "max_us": 8500,
  "ram_b": 3072,
  "free_ram_b": 185000,
  "label": "walking",
  "card": "STM32F4"
}
```

### `err` - Error

Emitted on firmware errors.

```json
{
  "t": "err",
  "code": 3,
  "msg": "sensor_timeout"
}
```

## Parser Implementation Notes

1. Buffer incoming bytes until `\r\n` is found.
2. Check that the line starts with `§` (`0xC2 0xA7`).
3. Extract the substring after `§` and before `\r\n`.
4. Parse as JSON and dispatch using the `"t"` field.
5. Discard or display lines that do not start with `§`; they may be debug prints.

## Error Codes

| Code | Name | Description |
| --- | --- | --- |
| 1 | `init_failed` | Sensor or peripheral init error |
| 2 | `model_load_err` | Model buffer corrupt |
| 3 | `sensor_timeout` | Sensor read timed out |
| 4 | `inference_ovf` | Inference time exceeded budget |
| 5 | `uart_overflow` | TX buffer overflow |
