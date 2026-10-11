# Codec source provenance

The files in this directory are the minimal AC-3/E-AC-3 encoding subset used by
`eac371enc`. They are compiled directly into the program; this directory is not
an FFmpeg source tree and the program does not link to FFmpeg/libavcodec,
libavutil, libswresample, or any other libav* library.

## FFmpeg-derived files

The retained codec implementation is derived primarily from the FFmpeg 7.1.1
sources that were included with the supplied `ffmpeg_eac371` project. The original FFmpeg
copyright and LGPL notices remain at the top of each retained source/header
file. Where the standalone extraction changes includes, registration glue, or
calls into generic FFmpeg infrastructure, those copyright/license headers are
left intact.

Retained FFmpeg-derived modules cover only what this encoder needs:

- AC-3 common tables and bit allocation (`ac3*`, `ac3tab*`)
- AC-3 encoder and float path (`ac3enc*`)
- E-AC-3 syntax/data and encoder (`eac3*`)
- generic C audio/AC-3 DSP used by the encoder (`audiodsp*`, `ac3dsp*`)
- KBD window generation (`kbdwin*`)
- forward-MDCT folding/rotation adapted from FFmpeg `libavutil/tx_template.c`
  (`mdct_standalone.c`), paired with a small project-local radix-2 FFT

`ac3enc.c` and `eac3enc.c` descend from the versions modified by HellGauss in
the supplied project. In particular, the dependent E-AC-3 stream/header changes
are retained. Their HellGauss modification notices and the underlying FFmpeg
notices are preserved.


### Selected post-7.1.1 backports

The project intentionally remains a small standalone extraction rather than a
snapshot of current FFmpeg. 0.1.2 selectively adapts two newer behaviors:

- FFmpeg commit `b5bfb7716c4914efdccbc3b507a77d676616be51`,
  `avcodec/ac3enc: drain the buffered samples at the end of encoding`. The
  FFmpeg audio-frame queue/packet side-data implementation is **not** copied;
  `eac371enc` implements the equivalent drain decision in its own streaming
  front end because raw elementary output has no generic discard-padding side
  data.
- The newer `put_bits_assume_flushed()` usage in FFmpeg's AC-3 bitstream writer
  is adapted to the project-local writer as a byte-alignment assertion/compiler
  hint. The project-local writer remains its own minimal implementation.

No architecture-specific FFmpeg SIMD source is included in 0.1.4. That is
planned as a separate, reviewable backport after hardware compatibility testing.

## Project-local compatibility layer

`standalone_compat.[ch]`, `put_bits.h`, `codec_api.[ch]`, and the FFT portion of
`mdct_standalone.c` are project-local
LGPL-2.1-or-later code. They provide only the small buffer, DSP, transform,
bit-writer, CRC, and context API surface required by the retained codec sources.
They are not FFmpeg ABI compatibility shims and must not be treated as libav*
objects outside this encoder.

See `../../FFMPEG-LICENSE.md`, `../../COPYING.LGPLv2.1`,
`../../LICENSE-HellGauss.txt`, and the individual source headers for licensing
and attribution.
