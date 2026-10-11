#!/usr/bin/env python3
"""Regression tests for the 256-sample AC-3/E-AC-3 encoder drain tail."""
import math
import os
import struct
import subprocess
import sys
import tempfile
import wave

ENCODER = sys.argv[1] if len(sys.argv) > 1 else "./build/install/bin/eac371enc"
RATE = 48000
CHANNELS = 8
FRAME = 1536
DELAY = 256


def write_wav(path, frames, tail_signal=False):
    with wave.open(path, "wb") as w:
        w.setnchannels(CHANNELS)
        w.setsampwidth(2)
        w.setframerate(RATE)
        out = bytearray()
        for n in range(frames):
            for ch in range(CHANNELS):
                value = 0.0
                if tail_signal and n >= max(0, frames - DELAY):
                    pos = n - max(0, frames - DELAY)
                    value = 0.18 * math.sin(2 * math.pi * (280 + ch * 45) * pos / RATE)
                sample = max(-32768, min(32767, round(value * 32767)))
                out += struct.pack("<h", sample)
        w.writeframes(out)


def ac3_size(buf):
    fs = [64,64,80,80,96,96,112,112,128,128,160,160,192,192,224,224,
          256,256,320,320,384,384,448,448,512,512,640,640,768,768,896,896,
          1024,1024,1152,1152,1280,1280]
    if len(buf) < 6 or buf[:2] != b"\x0b\x77":
        raise ValueError("bad AC-3 sync")
    if ((buf[5] >> 3) & 0x1f) > 10:
        return None
    fscod = buf[4] >> 6
    code = buf[4] & 0x3f
    if fscod != 0 or code >= len(fs):
        raise ValueError("unexpected AC-3 frame format")
    return fs[code] * 2


def eac3_size(buf):
    if len(buf) < 6 or buf[:2] != b"\x0b\x77":
        raise ValueError("bad E-AC-3 sync")
    if ((buf[5] >> 3) & 0x1f) <= 10:
        return None
    return (((buf[2] & 7) << 8) | buf[3]) * 2 + 2


def split_pairs(data):
    frames = []
    off = 0
    idx = 0
    while off < len(data):
        size = ac3_size(data[off:]) if idx % 2 == 0 else eac3_size(data[off:])
        if size is None or off + size > len(data):
            raise ValueError(f"bad frame {idx}")
        frames.append(data[off:off + size])
        off += size
        idx += 1
    if idx % 2:
        raise ValueError("odd syncframe count")
    return [frames[i] + frames[i + 1] for i in range(0, len(frames), 2)]


def expected_pairs(samples):
    if samples <= 0:
        return 0
    full, rem = divmod(samples, FRAME)
    pairs = full + (1 if rem else 0)
    if rem == 0 or rem > FRAME - DELAY:
        pairs += 1
    return pairs


def encode(inp, out):
    subprocess.run([ENCODER, "-q", inp, out], check=True)
    return split_pairs(open(out, "rb").read())


def main():
    with tempfile.TemporaryDirectory(prefix="eac371-drain-") as td:
        for samples in (1, 1280, 1281, 1536, 3072, 4608, 4609):
            wav = os.path.join(td, f"n{samples}.wav")
            out = os.path.join(td, f"n{samples}.eac3")
            write_wav(wav, samples)
            pairs = encode(wav, out)
            want = expected_pairs(samples)
            if len(pairs) != want:
                raise SystemExit(
                    f"drain regression: {samples} samples produced {len(pairs)} pairs, expected {want}"
                )

        zero_wav = os.path.join(td, "zero-1536.wav")
        tail_wav = os.path.join(td, "tail-1536.wav")
        zero_out = os.path.join(td, "zero-1536.eac3")
        tail_out = os.path.join(td, "tail-1536.eac3")
        write_wav(zero_wav, 1536, False)
        write_wav(tail_wav, 1536, True)
        zero_pairs = encode(zero_wav, zero_out)
        tail_pairs = encode(tail_wav, tail_out)
        if len(zero_pairs) != 2 or len(tail_pairs) != 2:
            raise SystemExit("1536-sample drain case did not produce two frame pairs")
        if zero_pairs[-1] == tail_pairs[-1]:
            raise SystemExit("drain frame does not contain the buffered final 256 samples")

    print("OK: AC-3/E-AC-3 256-sample drain regression")


if __name__ == "__main__":
    main()
