#!/bin/sh
set -eu
ENCODER=${1:-./build/install/bin/eac371enc}
TMP=${TMPDIR:-/tmp}/eac371enc-smoke-$$
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
mkdir -p "$TMP"
trap 'rm -rf "$TMP"' EXIT INT TERM

for rate in 48000 44100; do
    python3 "$HERE/generate_test_wav.py" "$TMP/in-${rate}.wav" "$rate" >/dev/null
    "$ENCODER" -q "$TMP/in-${rate}.wav" "$TMP/out-${rate}.eac3"
    python3 "$HERE/inspect_stream.py" "$TMP/out-${rate}.eac3"
done

python3 "$HERE/drain_regression.py" "$ENCODER"
python3 "$HERE/options_regression.py" "$ENCODER"
python3 "$HERE/bitrate_regression.py" "$ENCODER"
