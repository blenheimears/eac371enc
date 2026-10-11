/*
 * Minimal compatibility support for the FFmpeg-derived AC-3/E-AC-3 encoder.
 *
 * Copyright (C) 2026 the eac371enc contributors
 *
 * This file is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 */
#ifndef EAC371_STANDALONE_COMPAT_H
#define EAC371_STANDALONE_COMPAT_H

#include <assert.h>
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846264338327950288
#endif
#ifndef M_SQRT2
#define M_SQRT2 1.41421356237309504880168872420969808
#endif
#ifndef M_SQRT1_2
#define M_SQRT1_2 0.70710678118654752440084436210484904
#endif

#define av_cold
#define av_unused __attribute__((unused))
#if defined(__GNUC__) || defined(__clang__)
#define DECLARE_ALIGNED(n,t,v) t __attribute__((aligned(n))) v
#else
#define DECLARE_ALIGNED(n,t,v) t v
#endif
#define av_uninit(x) x=x
#define FF_ARRAY_ELEMS(a) (sizeof(a) / sizeof((a)[0]))
#define FFMIN(a,b) ((a) > (b) ? (b) : (a))
#define FFMAX(a,b) ((a) > (b) ? (a) : (b))
#define FFABS(a) ((a) >= 0 ? (a) : -(a))
#define av_assert0(x) assert(x)
#define av_assert1(x) assert(x)
#define av_assert2(x) assert(x)
#define AVERROR(e) (-(e))
#define AVERROR_INVALIDDATA AVERROR(EINVAL)
#define AVERROR_UNKNOWN AVERROR(EIO)
#define AVERROR_BUG AVERROR(EFAULT)
#define AV_NOPTS_VALUE INT64_MIN
#define AV_LOG_ERROR 16
#define AV_LOG_WARNING 24
#define AV_LOG_INFO 32
#define AV_CODEC_FLAG_BITEXACT 0x00800000
#define AV_AUDIO_SERVICE_TYPE_MAIN 0
#define AV_AUDIO_SERVICE_TYPE_COMMENTARY 5
#define AV_AUDIO_SERVICE_TYPE_EMERGENCY 6
#define AV_AUDIO_SERVICE_TYPE_VOICE_OVER 7
#define AV_AUDIO_SERVICE_TYPE_KARAOKE 8
#define AV_SAMPLE_FMT_FLTP 8
#define AV_CH_FRONT_LEFT              (UINT64_C(1) << 0)
#define AV_CH_FRONT_RIGHT             (UINT64_C(1) << 1)
#define AV_CH_FRONT_CENTER            (UINT64_C(1) << 2)
#define AV_CH_LOW_FREQUENCY           (UINT64_C(1) << 3)
#define AV_CH_BACK_LEFT               (UINT64_C(1) << 4)
#define AV_CH_BACK_RIGHT              (UINT64_C(1) << 5)
#define AV_CH_FRONT_LEFT_OF_CENTER    (UINT64_C(1) << 6)
#define AV_CH_FRONT_RIGHT_OF_CENTER   (UINT64_C(1) << 7)
#define AV_CH_BACK_CENTER             (UINT64_C(1) << 8)
#define AV_CH_SIDE_LEFT               (UINT64_C(1) << 9)
#define AV_CH_SIDE_RIGHT              (UINT64_C(1) << 10)
#define AV_CH_TOP_CENTER              (UINT64_C(1) << 11)
#define AV_CH_TOP_FRONT_LEFT          (UINT64_C(1) << 12)
#define AV_CH_TOP_FRONT_CENTER        (UINT64_C(1) << 13)
#define AV_CH_TOP_FRONT_RIGHT         (UINT64_C(1) << 14)
#define AV_CH_TOP_BACK_LEFT           (UINT64_C(1) << 15)
#define AV_CH_TOP_BACK_RIGHT          (UINT64_C(1) << 17)
#define AV_CH_SURROUND_DIRECT_LEFT    (UINT64_C(1) << 33)
#define AV_CH_SURROUND_DIRECT_RIGHT   (UINT64_C(1) << 34)
#define AV_CH_WIDE_LEFT               (UINT64_C(1) << 35)
#define AV_CH_WIDE_RIGHT              (UINT64_C(1) << 36)
#define AV_CH_LOW_FREQUENCY_2         (UINT64_C(1) << 37)
#define AV_CH_LAYOUT_MONO            (UINT64_C(1) << 2)
#define AV_CH_LAYOUT_STEREO          ((UINT64_C(1) << 0) | (UINT64_C(1) << 1))
#define AV_CH_LAYOUT_SURROUND        (AV_CH_LAYOUT_STEREO | (UINT64_C(1) << 2))
#define AV_CH_LAYOUT_2_1             (AV_CH_LAYOUT_STEREO | (UINT64_C(1) << 8))
#define AV_CH_LAYOUT_2_2             (AV_CH_LAYOUT_STEREO | (UINT64_C(1) << 9) | (UINT64_C(1) << 10))
#define AV_CH_LAYOUT_QUAD            (AV_CH_LAYOUT_STEREO | (UINT64_C(1) << 4) | (UINT64_C(1) << 5))
#define AV_CH_LAYOUT_4POINT0         (AV_CH_LAYOUT_SURROUND | (UINT64_C(1) << 8))
#define AV_CH_LAYOUT_5POINT0         (AV_CH_LAYOUT_SURROUND | (UINT64_C(1) << 9) | (UINT64_C(1) << 10))
#define AV_CH_LAYOUT_5POINT0_BACK    (AV_CH_LAYOUT_SURROUND | (UINT64_C(1) << 4) | (UINT64_C(1) << 5))
#define AV_CH_LAYOUT_5POINT1         (AV_CH_LAYOUT_5POINT0 | AV_CH_LOW_FREQUENCY)
#define AV_CH_LAYOUT_5POINT1_BACK    (AV_CH_LAYOUT_5POINT0_BACK | AV_CH_LOW_FREQUENCY)

#define ARCH_X86 0
#define ARCH_X86_64 0
#define ARCH_AARCH64 0
#define ARCH_ARM 0
#define ARCH_MIPS 0
#define ARCH_PPC 0
#define ARCH_RISCV 0
#define HAVE_FAST_UNALIGNED 1
#define CONFIG_SMALL 1
#define CONFIG_EAC3_ENCODER 1
#define emms_c() ((void)0)
#define LOCAL_ALIGNED_16(t,n,a,b) t n a b __attribute__((aligned(16)))
#define LOCAL_ALIGNED_32(t,n,a,b) t n a b __attribute__((aligned(32)))
#define FFALIGN(x,a) (((x)+(a)-1) & ~((a)-1))
#define FFSWAP(t,a,b) do { t _swap_tmp=(a); (a)=(b); (b)=_swap_tmp; } while (0)

static inline int av_clip(int a, int amin, int amax) { return a < amin ? amin : a > amax ? amax : a; }
static inline float av_clipf(float a, float amin, float amax) { return a < amin ? amin : a > amax ? amax : a; }
static inline unsigned av_clip_uintp2_c(int a, int p) { unsigned m = (1u << p) - 1u; return a < 0 ? 0 : (unsigned)a > m ? m : (unsigned)a; }
#define av_clip_uintp2(a,p) av_clip_uintp2_c((a),(p))
static inline int av_log2(unsigned v) { return v ? 31 - __builtin_clz(v) : 0; }
static inline uint16_t av_bswap16(uint16_t x) { return (uint16_t)((x << 8) | (x >> 8)); }
static inline uint32_t av_bswap32(uint32_t x) { return __builtin_bswap32(x); }
static inline uint16_t AV_RB16(const void *p) { const uint8_t *q=p; return (uint16_t)((q[0]<<8)|q[1]); }
static inline void AV_WB16(void *p, unsigned v) { uint8_t *q=p; q[0]=(uint8_t)(v>>8); q[1]=(uint8_t)v; }
static inline unsigned av_zero_extend(unsigned v, unsigned p) { return p >= 32 ? v : v & ((1u << p) - 1u); }
#define MAC64(d,a,b) ((d) += (int64_t)(a) * (int64_t)(b))

static inline void *av_mallocz(size_t n) { return calloc(1, n); }
static inline void *av_malloc(size_t n) { return malloc(n); }
static inline void av_free(void *p) { free(p); }
static inline void av_freep(void *pp) { void **p=(void**)pp; if (*p) free(*p); *p=NULL; }
#define FF_ALLOC_TYPED_ARRAY(p,n) ((p) = malloc(sizeof(*(p)) * (size_t)(n)))
#define FF_ALLOCZ_TYPED_ARRAY(p,n) ((p) = calloc((size_t)(n), sizeof(*(p))))

void av_log(void *ctx, int level, const char *fmt, ...);
double av_bessel_i0(double x);

/* Minimal channel-layout wrapper used by the retained initialization code. */
typedef struct AVChannelLayout {
    int nb_channels;
    uint64_t mask;
} AVChannelLayout;
static inline uint64_t av_channel_layout_subset(const AVChannelLayout *l, uint64_t mask) { return l->mask & mask; }
static inline int av_channel_layout_describe(const AVChannelLayout *l, char *buf, size_t n) {
    return snprintf(buf, n, "%dch", l->nb_channels);
}

/* Tiny AV* structs: these are project-private shims, not FFmpeg ABI. */
typedef struct AVClass { int unused; } AVClass;
typedef struct AVCodecContext {
    void *priv_data;
    int sample_rate;
    int64_t bit_rate;
    int cutoff;
    AVChannelLayout ch_layout;
    int frame_size;
    int initial_padding;
    int audio_service_type;
    int flags;
    int sample_fmt;
} AVCodecContext;
typedef struct AVFrame {
    uint8_t **extended_data;
    int64_t pts;
} AVFrame;
typedef struct AVPacket {
    uint8_t *data;
    int size;
    int64_t pts;
} AVPacket;

int ff_get_encode_buffer(AVCodecContext *avctx, AVPacket *pkt, int size, int flags);
static inline int64_t ff_samples_to_time_base(AVCodecContext *avctx, int samples) { (void)avctx; return samples; }
void av_packet_unref(AVPacket *pkt);

/* Float DSP used by ac3enc_template.c. */
typedef struct AVFixedDSPContext { int unused; } AVFixedDSPContext;

typedef struct AVFloatDSPContext {
    void (*vector_fmul)(float *dst, const float *src0, const float *src1, int len);
    void (*vector_fmul_reverse)(float *dst, const float *src0, const float *src1, int len);
} AVFloatDSPContext;
AVFloatDSPContext *avpriv_float_dsp_alloc(int bit_exact);

/* Only SAD[0] is required by the AC-3 exponent strategy heuristic. */
typedef int (*me_cmp_func)(void *, const uint8_t *, const uint8_t *, ptrdiff_t, int);
typedef struct MECmpContext { me_cmp_func sad[1]; } MECmpContext;
void ff_me_cmp_init(MECmpContext *c, AVCodecContext *avctx);

/* Minimal MDCT API replacing libavutil/tx. */
typedef struct AVTXContext AVTXContext;
typedef void (*av_tx_fn)(AVTXContext *, void *out, void *in, ptrdiff_t stride);
#define AV_TX_FLOAT_MDCT 1
int av_tx_init(AVTXContext **ctx, av_tx_fn *fn, int type, int inv, int len, const void *scale, unsigned flags);
void av_tx_uninit(AVTXContext **ctx);

/* One-shot once primitive is enough: the standalone encoder is initialized serially. */
typedef int AVOnce;
#define AV_ONCE_INIT 0
void ff_thread_once(AVOnce *control, void (*routine)(void));

/* CRC16-ANSI API used by FFmpeg's AC-3 frame finalizer. */
typedef uint32_t AVCRC;
#define AV_CRC_16_ANSI 0
const AVCRC *av_crc_get_table(int id);
uint32_t av_crc(const AVCRC *ctx, uint32_t crc, const uint8_t *buffer, size_t length);

/* Debug helpers retained as no-ops in non-debug builds. */
#define ff_dlog(...) ((void)0)
static inline const char *av_get_sample_fmt_name(int fmt) { (void)fmt; return "fltp"; }

#endif
