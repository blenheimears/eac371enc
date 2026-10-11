#!/usr/bin/env python3
"""Verify aggregate --bitrate quality allocation and option interactions."""
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ENCODER = sys.argv[1] if len(sys.argv) > 1 else "./build/install/bin/eac371enc"


def encode(inp, out, *args):
    subprocess.run([ENCODER, "-q", *args, inp, out], check=True)
    with open(out, "rb") as f:
        return f.read()


def main():
    with tempfile.TemporaryDirectory(prefix="eac371-bitrate-") as td:
        wav = os.path.join(td, "input.wav")
        subprocess.run(
            [sys.executable, os.path.join(HERE, "generate_test_wav.py"), wav, "48000"],
            check=True,
            stdout=subprocess.DEVNULL,
        )

        default = encode(wav, os.path.join(td, "default.eac3"))
        aggregate_1024 = encode(
            wav, os.path.join(td, "aggregate-1024.eac3"), "--bitrate", "1024k"
        )
        if aggregate_1024 != default:
            raise SystemExit("--bitrate 1024k did not select the default 640k+384k split")

        aggregate_768 = encode(
            wav, os.path.join(td, "aggregate-768.eac3"), "--bitrate", "768k"
        )
        explicit_768 = encode(
            wav,
            os.path.join(td, "explicit-768.eac3"),
            "--core-bitrate",
            "384k",
            "--extension-bitrate",
            "384k",
        )
        if aggregate_768 != explicit_768:
            raise SystemExit("--bitrate 768k did not select the expected 384k+384k split")

        aggregate_448 = encode(
            wav, os.path.join(td, "aggregate-448.eac3"), "--bitrate", "448k"
        )
        explicit_448 = encode(
            wav,
            os.path.join(td, "explicit-448.eac3"),
            "--core-bitrate",
            "224k",
            "--extension-bitrate",
            "224k",
        )
        if aggregate_448 != explicit_448:
            raise SystemExit("--bitrate 448k did not select the expected 224k+224k split")

        mixed = subprocess.run(
            [
                ENCODER,
                "-q",
                "--bitrate",
                "768k",
                "--core-bitrate",
                "512k",
                wav,
                os.path.join(td, "mixed.eac3"),
            ],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        if mixed.returncode == 0:
            raise SystemExit("aggregate and per-stream bitrate options were accepted together")

        too_low = subprocess.run(
            [ENCODER, "-q", "--bitrate", "80k", wav, os.path.join(td, "low.eac3")],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        if too_low.returncode == 0:
            raise SystemExit("invalid too-low aggregate bitrate was accepted")

    print("OK: aggregate bitrate allocation regression")


if __name__ == "__main__":
    main()
