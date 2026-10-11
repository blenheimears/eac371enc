/*
 * Minimal compatibility support for the FFmpeg-derived AC-3/E-AC-3 encoder.
 * Copyright (C) 2026 the eac371enc contributors
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "standalone_compat.h"
#include "mdct_standalone.h"
#include <stdarg.h>

void av_log(void *ctx, int level, const char *fmt, ...)
{
    va_list ap;
    (void)ctx; (void)level;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

double av_bessel_i0(double x)
{
    /* Power-series I0. The KBD window only calls this for modest |x|. */
    double y = (x * x) * 0.25;
    double term = 1.0, sum = 1.0;
    for (int k = 1; k < 80; k++) {
        term *= y / ((double)k * (double)k);
        sum += term;
        if (fabs(term) <= fabs(sum) * 1e-16)
            break;
    }
    return sum;
}

int ff_get_encode_buffer(AVCodecContext *avctx, AVPacket *pkt, int size, int flags)
{
    (void)avctx; (void)flags;
    uint8_t *p = realloc(pkt->data, (size_t)size);
    if (!p) return AVERROR(ENOMEM);
    pkt->data = p;
    pkt->size = size;
    return 0;
}

void av_packet_unref(AVPacket *pkt)
{
    if (pkt) { free(pkt->data); pkt->data = NULL; pkt->size = 0; pkt->pts = AV_NOPTS_VALUE; }
}

static void vector_fmul_c(float *dst, const float *a, const float *b, int len)
{
    for (int i=0;i<len;i++) dst[i] = a[i] * b[i];
}
static void vector_fmul_reverse_c(float *dst, const float *a, const float *b, int len)
{
    for (int i=0;i<len;i++) dst[i] = a[i] * b[len - 1 - i];
}
AVFloatDSPContext *avpriv_float_dsp_alloc(int bit_exact)
{
    (void)bit_exact;
    AVFloatDSPContext *c = calloc(1, sizeof(*c));
    if (c) { c->vector_fmul = vector_fmul_c; c->vector_fmul_reverse = vector_fmul_reverse_c; }
    return c;
}

static int sad_bytes(void *ctx, const uint8_t *a, const uint8_t *b, ptrdiff_t stride, int h)
{
    (void)ctx;
    int sum = 0;
    /* FFmpeg calls this with width=stride=16, h=16. */
    for (int y=0;y<h;y++) {
        for (int x=0;x<16;x++) sum += abs((int)a[x] - (int)b[x]);
        a += stride; b += stride;
    }
    return sum;
}
void ff_me_cmp_init(MECmpContext *c, AVCodecContext *avctx)
{
    (void)avctx; c->sad[0] = sad_bytes;
}

struct AVTXContext {
    Eac371Mdct *mdct;
};

static void standalone_mdct(AVTXContext *ctx, void *out, void *in,
                            ptrdiff_t stride)
{
    eac371_mdct_forward(ctx->mdct, out, in, stride);
}

int av_tx_init(AVTXContext **ctx, av_tx_fn *fn, int type, int inv, int len,
               const void *scalev, unsigned flags)
{
    AVTXContext *tx;
    float scale;

    (void)flags;
    if (!ctx || !fn || type != AV_TX_FLOAT_MDCT || inv || len <= 0)
        return AVERROR(EINVAL);

    scale = scalev ? *(const float *)scalev : 1.0f;
    tx = calloc(1, sizeof(*tx));
    if (!tx)
        return AVERROR(ENOMEM);
    tx->mdct = eac371_mdct_create(len, scale);
    if (!tx->mdct) {
        free(tx);
        return AVERROR(ENOMEM);
    }

    *ctx = tx;
    *fn = standalone_mdct;
    return 0;
}

void av_tx_uninit(AVTXContext **ctx)
{
    if (ctx && *ctx) {
        eac371_mdct_destroy((*ctx)->mdct);
        free(*ctx);
        *ctx = NULL;
    }
}

void ff_thread_once(AVOnce *control, void (*routine)(void))
{
    if (!*control) { routine(); *control = 1; }
}

static AVCRC crc16_table[257];
static int crc16_ready;
static void crc16_init(void)
{
    if (crc16_ready) return;
    const uint32_t poly = 0x8005;
    for (unsigned i=0;i<256;i++) {
        uint32_t c = i << 24;
        for (unsigned j=0;j<8;j++)
            c = (c << 1) ^ ((poly << 16) & (uint32_t)((int32_t)c >> 31));
        crc16_table[i] = __builtin_bswap32(c);
    }
    crc16_table[256] = 1;
    crc16_ready = 1;
}
const AVCRC *av_crc_get_table(int id)
{
    (void)id; crc16_init(); return crc16_table;
}
uint32_t av_crc(const AVCRC *ctx, uint32_t crc, const uint8_t *buffer, size_t length)
{
    while (length--) crc = ctx[(uint8_t)crc ^ *buffer++] ^ (crc >> 8);
    return crc;
}
