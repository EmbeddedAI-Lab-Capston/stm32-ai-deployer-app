"""Host reference for the N6 firmware's static-input inference.

Rebuilds exactly the tensor FillStaticInput() writes (buf[i] = i & 0xFF,
channel-last 96x96x3, uint8 with the NPU input quantisation scale/zero point
from network.c) and runs the original .tflite through LiteRT, so the board's
g_ai_last_class / output probabilities can be checked against a reference.

Setup (once, in any throwaway venv):
    python -m venv .venv && .venv/Scripts/python -m pip install ai-edge-litert numpy
Run:
    .venv/Scripts/python n6_ai_node/tools/ref_infer.py
Model/ is gitignored - the .tflite has to be present locally.
"""
from pathlib import Path

import numpy as np
from ai_edge_litert.interpreter import Interpreter

MODEL = str(Path(__file__).resolve().parent.parent / "Model" / "mobilenet_v1_0.25_96_tfs_int8.tflite")
NPU_SCALE = 0.00784313771873713   # network.c: Input_0_out_0 quant_scale
NPU_ZERO = 127                    # network.c: Input_0_out_0 quant_offset

raw = (np.arange(96 * 96 * 3) & 0xFF).astype(np.uint8)
real = (raw.astype(np.float32) - NPU_ZERO) * NPU_SCALE
real = real.reshape(1, 96, 96, 3)

# NOTE: model_content, not model_path - LiteRT cannot open a path with
# non-ASCII characters (this repo lives under "D:\Yazılım\...").
itp = Interpreter(model_content=Path(MODEL).read_bytes())
itp.allocate_tensors()
inp = itp.get_input_details()[0]
out = itp.get_output_details()[0]
print("input :", inp["dtype"].__name__, inp["shape"].tolist(), "quant", inp["quantization"])
print("output:", out["dtype"].__name__, out["shape"].tolist(), "quant", out["quantization"])

if inp["dtype"] == np.float32:
    x = real
else:
    s, z = inp["quantization"]
    info = np.iinfo(inp["dtype"])
    x = np.clip(np.round(real / s + z), info.min, info.max).astype(inp["dtype"])
itp.set_tensor(inp["index"], x)
itp.invoke()
y = itp.get_tensor(out["index"])[0]
if out["dtype"] != np.float32:
    s, z = out["quantization"]
    y = (y.astype(np.float32) - z) * s

print("probs :", " ".join(f"{v:.4f}" for v in y))
print("class :", int(np.argmax(y)), f"conf {100 * float(np.max(y)):.1f}%")
# Measured on the board 2026-09-18 (4 boots): 0.2383 0.4727 0.0547 0.0000 0.2383 -> class 1, 47%
print("board :", "0.2383 0.4727 0.0547 0.0000 0.2383  -> class 1, 47%  (2026-09-18)")
