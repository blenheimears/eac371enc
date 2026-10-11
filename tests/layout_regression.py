#!/usr/bin/env python3
"""Cross-layout AC-3/dependent-E-AC-3 stream checks (header level, not hardware tests)."""
import os
import struct
import subprocess
import sys
import tempfile

ENCODER = sys.argv[1]
FRAME = 1536

# Input layout, number of channels, core acmod, LFE, dependent custom map, dep acmod
CASES = [
    ('mono', 1, 1, 0, None, None),
    ('stereo', 2, 2, 0, None, None),
    ('2.1', 3, 2, 1, None, None),
    ('3.0', 3, 3, 0, None, None),
    ('3.1', 4, 3, 1, None, None),
    ('4.0', 4, 5, 0, None, None),
    ('4.1', 5, 5, 1, None, None),
    ('5.0', 5, 7, 0, None, None),
    ('5.0(back)', 5, 7, 0, None, None),
    ('5.1', 6, 7, 1, None, None),
    ('5.1(back)', 6, 7, 1, None, None),
    ('6.0', 6, 7, 0, 0x1900, 3),
    ('6.1', 7, 7, 1, 0x1900, 3),
    ('6.1(back)', 7, 7, 1, 0x1900, 3),
    ('7.0', 7, 7, 0, 0x1a00, 6),
    ('7.0(front)', 7, 7, 0, 0x1c00, 6),
    ('7.1', 8, 7, 1, 0x1a00, 6),
    ('7.1(wide)', 8, 7, 1, 0x1c00, 6),
]

AC3_SIZE = [64,64,80,80,96,96,112,112,128,128,160,160,192,192,224,224,
            256,256,320,320,384,384,448,448,512,512,640,640,768,768,896,896,
            1024,1024,1152,1152,1280,1280]


def read_bits(data, pos, n):
    val = 0
    for i in range(n):
        val = (val << 1) | ((data[(pos + i)//8] >> (7-(pos+i)%8)) & 1)
    return val, pos+n


def core_info(data):
    assert data[:2] == b'\x0b\x77'
    assert (data[5] >> 3) <= 10
    p = 32
    _,p=read_bits(data,p,2)   # fscod
    sc,p=read_bits(data,p,6)  # frame size code
    _,p=read_bits(data,p,5)   # bsid
    _,p=read_bits(data,p,3)   # bsmod
    mode,p=read_bits(data,p,3)
    if mode & 1 and mode != 1: _,p=read_bits(data,p,2)
    if mode & 4: _,p=read_bits(data,p,2)
    if mode == 2: _,p=read_bits(data,p,2)
    lfe,p=read_bits(data,p,1)
    return AC3_SIZE[sc]*2, mode, lfe


def dep_info(data):
    assert data[:2] == b'\x0b\x77'
    assert (data[5] >> 3) > 10
    length = (((data[2]&7)<<8)|data[3])*2+2
    pos=16
    stream,pos=read_bits(data,pos,2)
    _,pos=read_bits(data,pos,3)  # substreamid
    _,pos=read_bits(data,pos,11) # size
    _,pos=read_bits(data,pos,2)  # fscod
    _,pos=read_bits(data,pos,2)  # blks
    mode,pos=read_bits(data,pos,3)
    lfe,pos=read_bits(data,pos,1)
    _,pos=read_bits(data,pos,5) # bsid
    _,pos=read_bits(data,pos,5) # dialnorm
    comp,pos=read_bits(data,pos,1)
    if comp: _,pos=read_bits(data,pos,8)
    has_map,pos=read_bits(data,pos,1)
    mask,pos=read_bits(data,pos,16)
    assert has_map == 1
    return length, stream, mode, lfe, mask


def fail(*args):
    r = subprocess.run(args, capture_output=True, text=True)
    assert r.returncode != 0, f'expected rejection: {args!r}'


with tempfile.TemporaryDirectory(prefix='eac371-layout-test-') as td:
    for layout, channels, mode, lfe, dep_mask, dep_mode in CASES:
        inp = os.path.join(td, f'in-{layout}.raw')
        out = os.path.join(td, f'out-{layout}.ac3' if dep_mask is None else f'out-{layout}.eac3')
        # Each channel has different signal and samples at a frame boundary to test draining.
        with open(inp, 'wb') as f:
            frame = struct.pack('<' + 'f'*channels, *[0.03 * (c+1) for c in range(channels)])
            f.write(frame * FRAME)
        command = (ENCODER, '--quiet', '--raw-pcm', '--sample-rate', '48000',
                   '--sample-format', 'f32le', '--channels', str(channels),
                   '--channel-layout', layout, '--bitrate', '448k', inp, out)
        subprocess.run(command, check=True, capture_output=True)
        b = open(out, 'rb').read()
        p = 0
        pairs = 0
        while p < len(b):
            length, acmod, core_lfe = core_info(b[p:])
            assert (acmod, core_lfe) == (mode, lfe), (layout, acmod, core_lfe)
            p += length
            if dep_mask is not None:
                length, stream, ext_mode, ext_lfe, mask = dep_info(b[p:])
                assert (stream, ext_mode, ext_lfe, mask) == (1, dep_mode, 0, dep_mask), (layout, stream, ext_mode, ext_lfe, hex(mask))
                p += length
            pairs += 1
        assert pairs == 2, (layout, pairs)
        if dep_mask is None:
            fail(ENCODER, '--quiet', '--raw-pcm', '--sample-rate', '48000',
                 '--channel-layout', layout, '--extension-bitrate', '128k', inp, out)
    fail(ENCODER, '--quiet', '--raw-pcm', '--sample-rate', '48000',
         '--channel-layout', '6.0', '--channels', '7', inp, out)
    fail(ENCODER, '--quiet', '--raw-pcm', '--sample-rate', '48000',
         '--channel-layout', '7.2', inp, out)
print(f'OK: {len(CASES)} layouts, core/extension signaling, and invalid option rejection')
