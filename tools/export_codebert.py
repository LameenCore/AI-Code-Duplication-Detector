"""
One-time script: downloads microsoft/codebert-base from Hugging Face and
exports it to ONNX format, plus saves the tokenizer's vocabulary files.

After this runs once, the C++ project never needs Python again — it just
loads model/codebert.onnx directly via ONNX Runtime.

Usage (from the project root):
    pip install transformers torch
    python tools/export_codebert.py
"""

import os
import json
import torch
from transformers import AutoTokenizer, AutoModel

MODEL_NAME = "microsoft/codebert-base"
OUTPUT_DIR = "model"

os.makedirs(OUTPUT_DIR, exist_ok=True)

print(f"Downloading tokenizer and model: {MODEL_NAME}")
print("(first run only — this pulls ~500MB and may take a few minutes)")

tokenizer = AutoTokenizer.from_pretrained(MODEL_NAME)
model = AutoModel.from_pretrained(MODEL_NAME)
model.eval()

# Save the tokenizer's vocabulary files (vocab.json, merges.txt, etc.) into
# model/ — Day 22's C++ tokenizer will read these directly so it uses the
# exact same vocabulary as the model.
tokenizer.save_pretrained(OUTPUT_DIR)
print(f"Tokenizer files saved to {OUTPUT_DIR}/")

# A short, fixed test sentence just to trace the model's forward pass for
# export, and to give us real input_ids/attention_mask numbers to hardcode
# into today's C++ smoke test (we're not writing a real tokenizer yet).
dummy_text = "int add(int a, int b) { return a + b; }"
inputs = tokenizer(dummy_text, return_tensors="pt")

onnx_path = os.path.join(OUTPUT_DIR, "codebert.onnx")

print("Exporting model to ONNX (this can take a minute)...")
torch.onnx.export(
    model,
    (inputs["input_ids"], inputs["attention_mask"]),
    onnx_path,
    input_names=["input_ids", "attention_mask"],
    output_names=["last_hidden_state", "pooler_output"],
    dynamic_axes={
        "input_ids": {0: "batch", 1: "sequence"},
        "attention_mask": {0: "batch", 1: "sequence"},
        "last_hidden_state": {0: "batch", 1: "sequence"},
    },
    opset_version=14,
)

print(f"Model exported to {onnx_path}")

# Save the sample input alongside the model so we have a record of exactly
# which numbers correspond to this test sentence.
sample = {
    "text": dummy_text,
    "input_ids": inputs["input_ids"].tolist()[0],
    "attention_mask": inputs["attention_mask"].tolist()[0],
}
with open(os.path.join(OUTPUT_DIR, "sample_input.json"), "w") as f:
    json.dump(sample, f, indent=2)

print("\n--- COPY THESE FOR THE C++ TEST ---")
print("input_ids:", sample["input_ids"])
print("attention_mask:", sample["attention_mask"])
print("------------------------------------")
print("\nDone. Send these printed arrays back so we can wire them into the C++ test.")
