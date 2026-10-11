#!/usr/bin/env python3
"""Lightweight structural inspector for eac371enc raw output.

Parses alternating AC-3 / E-AC-3 syncframes and verifies that every second
frame is an E-AC-3 dependent substream. This is not a decoder/conformance test.
"""
import sys


def bits(data, pos, n):
    v = 0
    for i in range(n):
        p = pos + i
        v = (v << 1) | ((data[p // 8] >> (7 - p % 8)) & 1)
    return v, pos + n


def ac3_size(buf):
    fs = [64,64,80,80,96,96,112,112,128,128,160,160,192,192,224,224,256,256,
          320,320,384,384,448,448,512,512,640,640,768,768,896,896,1024,1024,
          1152,1152,1280,1280]
    if len(buf) < 6 or buf[:2] != b"\x0b\x77":
        raise ValueError("bad sync")
    bsid = (buf[5] >> 3) & 0x1f
    if bsid > 10:
        return None
    fscod = buf[4] >> 6
    code = buf[4] & 0x3f
    if fscod != 0 or code >= len(fs):
        raise ValueError("inspector currently expects 48 kHz AC-3")
    return fs[code] * 2


def eac3_size(buf):
    if len(buf) < 6 or buf[:2] != b"\x0b\x77":
        raise ValueError("bad sync")
    bsid = (buf[5] >> 3) & 0x1f
    if bsid <= 10:
        return None
    return (((buf[2] & 0x07) << 8) | buf[3]) * 2 + 2


def dep_info(frame):
    p = 16
    st, p = bits(frame, p, 2)
    _, p = bits(frame, p, 3)
    _, p = bits(frame, p, 11)
    fscod, p = bits(frame, p, 2)
    _, p = bits(frame, p, 2)
    _, p = bits(frame, p, 3)
    _, p = bits(frame, p, 1)
    _, p = bits(frame, p, 5)
    _, p = bits(frame, p, 5)
    compre, p = bits(frame, p, 1)
    if compre:
        _, p = bits(frame, p, 8)
    chanmape, p = bits(frame, p, 1)
    chanmap = None
    if chanmape:
        chanmap, p = bits(frame, p, 16)
    return st, fscod, chanmap


def main(path):
    data = open(path, "rb").read()
    off = 0
    idx = 0
    while off < len(data):
        if idx % 2 == 0:
            size = ac3_size(data[off:])
            if size is None:
                raise SystemExit(f"frame {idx}: expected AC-3 core")
        else:
            size = eac3_size(data[off:])
            if size is None:
                raise SystemExit(f"frame {idx}: expected E-AC-3 extension")
            st, _, cmap = dep_info(data[off:off+size])
            if st != 1 or cmap != 0x1A00:
                raise SystemExit(f"frame {idx}: bad dependent header stream_type={st}, map={cmap!r}")
        if not size or off + size > len(data):
            raise SystemExit(f"frame {idx}: truncated frame")
        off += size
        idx += 1
    if idx % 2:
        raise SystemExit("odd number of syncframes")
    print(f"OK: {idx//2} AC-3/dependent-E-AC-3 frame pairs")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit(f"usage: {sys.argv[0]} FILE.eac3")
    main(sys.argv[1])
