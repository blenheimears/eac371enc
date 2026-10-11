#!/usr/bin/env python3
"""Generate a tiny 8-channel PCM WAV for smoke testing."""
import math
import struct
import sys
import wave

path = sys.argv[1] if len(sys.argv) > 1 else "test-7.1.wav"
rate = int(sys.argv[2]) if len(sys.argv) > 2 else 48000
seconds = 0.20
frames = int(rate * seconds)
freqs = [220, 260, 300, 80, 340, 380, 420, 460]

with wave.open(path, "wb") as wav:
    wav.setnchannels(8)
    wav.setsampwidth(2)
    wav.setframerate(rate)
    block = bytearray()
    for n in range(frames):
        t = n / rate
        for frequency in freqs:
            sample = int(0.06 * 32767 * math.sin(2 * math.pi * frequency * t))
            block += struct.pack("<h", sample)
    wav.writeframes(block)
print(path)
