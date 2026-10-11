#!/usr/bin/env python3
"""Verify exposed standalone AC-3/E-AC-3 encoder controls reach the codec core."""
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ENCODER = sys.argv[1] if len(sys.argv) > 1 else "./build/install/bin/eac371enc"


def bits(data, pos, n):
    value = 0
    for i in range(n):
        p = pos + i
        value = (value << 1) | ((data[p // 8] >> (7 - p % 8)) & 1)
    return value, pos + n


def ac3_size(buf):
    fs = [64,64,80,80,96,96,112,112,128,128,160,160,192,192,224,224,
          256,256,320,320,384,384,448,448,512,512,640,640,768,768,896,896,
          1024,1024,1152,1152,1280,1280]
    code = buf[4] & 0x3f
    return fs[code] * 2


def parse_core(frame):
    p = 32
    _, p = bits(frame, p, 2)  # fscod
    _, p = bits(frame, p, 6)  # frmsizecod
    bsid, p = bits(frame, p, 5)
    _, p = bits(frame, p, 3)  # bsmod
    acmod, p = bits(frame, p, 3)
    if (acmod & 1) and acmod != 1:
        _, p = bits(frame, p, 2)
    if acmod & 4:
        _, p = bits(frame, p, 2)
    if acmod == 2:
        _, p = bits(frame, p, 2)
    _, p = bits(frame, p, 1)  # lfe
    dialnorm, p = bits(frame, p, 5)
    compre, p = bits(frame, p, 1)
    if compre:
        _, p = bits(frame, p, 8)
    langcode, p = bits(frame, p, 1)
    if langcode:
        _, p = bits(frame, p, 8)
    audprodie, p = bits(frame, p, 1)
    if audprodie:
        _, p = bits(frame, p, 5)
        _, p = bits(frame, p, 2)
    _, p = bits(frame, p, 1)  # copyright
    _, p = bits(frame, p, 1)  # original
    dmix = None
    if bsid == 6:
        xbsi1e, p = bits(frame, p, 1)
        if xbsi1e:
            dmix, p = bits(frame, p, 2)
    return bsid, dialnorm, dmix


def parse_dep_dialnorm(frame):
    p = 16
    _, p = bits(frame, p, 2)   # strmtyp
    _, p = bits(frame, p, 3)   # substreamid
    _, p = bits(frame, p, 11)  # frmsiz
    _, p = bits(frame, p, 2)   # fscod
    _, p = bits(frame, p, 2)   # numblkscod
    _, p = bits(frame, p, 3)   # acmod
    _, p = bits(frame, p, 1)   # lfeon
    _, p = bits(frame, p, 5)   # bsid
    dialnorm, p = bits(frame, p, 5)
    return dialnorm


def encode(inp, out, *args):
    subprocess.run([ENCODER, "-q", *args, inp, out], check=True)
    return open(out, "rb").read()


def main():
    with tempfile.TemporaryDirectory(prefix="eac371-options-") as td:
        wav = os.path.join(td, "input.wav")
        subprocess.run([sys.executable, os.path.join(HERE, "generate_test_wav.py"), wav, "48000"],
                       check=True, stdout=subprocess.DEVNULL)

        default = encode(wav, os.path.join(td, "default.eac3"))
        tuned = encode(wav, os.path.join(td, "tuned.eac3"),
                       "--dialnorm", "-27", "--downmix-mode", "loro")
        cut = encode(wav, os.path.join(td, "cut.eac3"), "--cutoff", "8000")
        cpl_off = encode(wav, os.path.join(td, "cpl-off.eac3"), "--coupling", "off")
        cpl_on = encode(wav, os.path.join(td, "cpl-on.eac3"),
                        "--coupling", "on", "--coupling-start-band", "8")

        core_size = ac3_size(tuned)
        bsid, core_dialnorm, dmix = parse_core(tuned[:core_size])
        dep_dialnorm = parse_dep_dialnorm(tuned[core_size:])
        if core_dialnorm != 27 or dep_dialnorm != 27:
            raise SystemExit("--dialnorm was not written to both core and dependent stream")
        if bsid != 6 or dmix != 2:
            raise SystemExit("--downmix-mode loro was not written to AC-3 extended BSI metadata")

        if cut == default:
            raise SystemExit("--cutoff did not affect encoded output")
        if cpl_off == default:
            raise SystemExit("--coupling off did not affect encoded output")
        if cpl_on == cpl_off:
            raise SystemExit("coupling controls did not affect encoded output")

        bad = subprocess.run([ENCODER, "-q", "--dialnorm", "-32", wav,
                              os.path.join(td, "bad.eac3")],
                             stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        if bad.returncode == 0:
            raise SystemExit("invalid --dialnorm was accepted")

    print("OK: codec-control option regression")


if __name__ == "__main__":
    main()
