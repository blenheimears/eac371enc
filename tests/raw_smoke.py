#!/usr/bin/env python3
"""Raw-only build smoke test, including conditional help and low-rate allocation."""
import math
import os
import struct
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ENCODER = sys.argv[1] if len(sys.argv) > 1 else "./build/eac371enc"

help_text = subprocess.check_output([ENCODER, "--help"], text=True)
if "libsndfile" in help_text.lower():
    raise SystemExit("raw-only --help unexpectedly mentions libsndfile")
if "raw PCM input only" not in help_text:
    raise SystemExit("raw-only --help does not identify the input mode")

with tempfile.TemporaryDirectory(prefix="eac371-raw-") as td:
    raw = os.path.join(td, "input.f32le")
    out = os.path.join(td, "output.eac3")
    with open(raw, "wb") as f:
        for i in range(48000):
            values = [0.08 * math.sin(2.0 * math.pi * (220 + 55 * c) * i / 48000.0) for c in range(8)]
            f.write(struct.pack("<8f", *values))
    proc = subprocess.run([
        ENCODER, "--raw-pcm", "--sample-rate", "48000", "--channels", "8",
        "--sample-format", "f32le", "--channel-layout", "7.1", "--bitrate", "448k",
        raw, out,
    ], text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True)
    if "Core: AC-3 5.1 @ 224 kbit/s; extension: dependent E-AC-3 4 channels @ 224 kbit/s" not in proc.stderr:
        raise SystemExit("448k aggregate rate did not select the expected 224k+224k split")
    subprocess.run([sys.executable, os.path.join(HERE, "inspect_stream.py"), out], check=True)
print("OK: raw-only frontend smoke test")
