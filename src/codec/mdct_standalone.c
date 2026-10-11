/*
 * Copyright (c) Lynne
 *
 * Power of two FFT concepts:
 * Copyright (c) Lynne
 * Copyright (c) 2008 Loren Merritt
 * Copyright (c) 2002 Fabrice Bellard
 * Partly based on libdjbfft by D. J. Bernstein
 *
 * This file contains an adaptation of the forward-MDCT folding and rotation
 * used by FFmpeg's libavutil/tx_template.c. The transform dispatcher and
 * architecture-specific code have been replaced by a small project-local
 * radix-2 FFT because eac371enc only needs the 256-point forward float MDCT.
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
#include "mdct_standalone.h"

#include <math.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846264338327950288
#endif
#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923132169163975144
#endif

typedef struct ComplexFloat {
    float re;
    float im;
} ComplexFloat;

struct Eac371Mdct {
    int len;
    ComplexFloat *exp;
    ComplexFloat *scratch;
};

static int is_power_of_two(unsigned value)
{
    return value && !(value & (value - 1));
}

static void fft_forward(ComplexFloat *data, int n)
{
    /* Natural-order, unscaled, forward complex FFT (negative exponent). */
    for (unsigned i = 1, j = 0; i < (unsigned)n; i++) {
        unsigned bit = (unsigned)n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j) {
            ComplexFloat tmp = data[i];
            data[i] = data[j];
            data[j] = tmp;
        }
    }

    for (int size = 2; size <= n; size <<= 1) {
        double angle = -2.0 * M_PI / size;
        float step_re = (float)cos(angle);
        float step_im = (float)sin(angle);
        int half = size >> 1;

        for (int base = 0; base < n; base += size) {
            float w_re = 1.0f;
            float w_im = 0.0f;
            for (int j = 0; j < half; j++) {
                ComplexFloat u = data[base + j];
                ComplexFloat v0 = data[base + j + half];
                ComplexFloat v = {
                    v0.re * w_re - v0.im * w_im,
                    v0.re * w_im + v0.im * w_re
                };
                float next_re;

                data[base + j].re = u.re + v.re;
                data[base + j].im = u.im + v.im;
                data[base + j + half].re = u.re - v.re;
                data[base + j + half].im = u.im - v.im;

                next_re = w_re * step_re - w_im * step_im;
                w_im = w_re * step_im + w_im * step_re;
                w_re = next_re;
            }
        }
    }
}

Eac371Mdct *eac371_mdct_create(int len, float scale)
{
    Eac371Mdct *ctx;
    int half;
    double theta;
    double magnitude;

    if (len < 2 || !is_power_of_two((unsigned)len) ||
        !is_power_of_two((unsigned)(len >> 1))) {
        return NULL;
    }

    ctx = calloc(1, sizeof(*ctx));
    if (!ctx)
        return NULL;
    ctx->len = len;
    half = len >> 1;
    ctx->exp = malloc((size_t)half * sizeof(*ctx->exp));
    ctx->scratch = malloc((size_t)half * sizeof(*ctx->scratch));
    if (!ctx->exp || !ctx->scratch) {
        eac371_mdct_destroy(ctx);
        return NULL;
    }

    /* This is FFmpeg's forward-MDCT exponent generation specialized for float. */
    theta = (scale < 0.0f ? half : 0) + 1.0 / 8.0;
    magnitude = sqrt(fabs((double)scale));
    for (int i = 0; i < half; i++) {
        double alpha = M_PI_2 * (i + theta) / half;
        ctx->exp[i].re = (float)(cos(alpha) * magnitude);
        ctx->exp[i].im = (float)(sin(alpha) * magnitude);
    }
    return ctx;
}

void eac371_mdct_forward(Eac371Mdct *ctx, float *dst, const float *src,
                         ptrdiff_t stride)
{
    const int len = ctx->len;
    const int len2 = len >> 1;
    const int len4 = len >> 2;
    const int len3 = len2 * 3;
    ComplexFloat *z = ctx->scratch;
    int out_stride = (int)(stride / (ptrdiff_t)sizeof(*dst));

    if (out_stride < 1)
        out_stride = 1;

    /* Folding + pre-rotation adapted from FFmpeg ff_tx_mdct_fwd(). */
    for (int i = 0; i < len2; i++) {
        int k = 2 * i;
        float tmp_re;
        float tmp_im;
        float exp_re = ctx->exp[i].re;
        float exp_im = ctx->exp[i].im;

        if (k < len2) {
            tmp_re = -src[len2 + k] + src[len2 - 1 - k];
            tmp_im = -src[len3 + k] - src[len3 - 1 - k];
        } else {
            tmp_re = -src[len2 + k] - src[5 * len2 - 1 - k];
            tmp_im = src[-len2 + k] - src[len3 - 1 - k];
        }

        /* FFmpeg stores CMUL outputs as (im,re) at this stage. */
        z[i].im = tmp_re * exp_re - tmp_im * exp_im;
        z[i].re = tmp_re * exp_im + tmp_im * exp_re;
    }

    fft_forward(z, len2);

    /* Post-rotation/reordering adapted from FFmpeg ff_tx_mdct_fwd(). */
    for (int i = 0; i < len4; i++) {
        int i0 = len4 + i;
        int i1 = len4 - i - 1;
        ComplexFloat src0 = z[i0];
        ComplexFloat src1 = z[i1];
        ComplexFloat exp0 = ctx->exp[i0];
        ComplexFloat exp1 = ctx->exp[i1];

        dst[(2 * i1 + 1) * out_stride] =
            src0.re * exp0.im - src0.im * exp0.re;
        dst[(2 * i0) * out_stride] =
            src0.re * exp0.re + src0.im * exp0.im;

        dst[(2 * i0 + 1) * out_stride] =
            src1.re * exp1.im - src1.im * exp1.re;
        dst[(2 * i1) * out_stride] =
            src1.re * exp1.re + src1.im * exp1.im;
    }
}

void eac371_mdct_destroy(Eac371Mdct *ctx)
{
    if (!ctx)
        return;
    free(ctx->exp);
    free(ctx->scratch);
    free(ctx);
}
