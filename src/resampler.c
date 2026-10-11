/*
 * Small streaming polyphase windowed-sinc resampler for eac371enc.
 * Copyright (C) 2026 the eac371enc contributors
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "resampler.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846264338327950288
#endif
#define RS_TAPS 32
#define RS_HALF (RS_TAPS/2)
#define RS_PHASES 1024

struct Eac371Resampler {
    int in_rate, out_rate, channels;
    double step;
    double pos;
    int64_t base;
    float *buf;
    size_t frames, cap;
    float *kernel;
    int eof;
    int64_t total_in;
    int64_t produced;
    int64_t target_out;
    char error[128];
};

static double sinc_pi(double x) {
    if (fabs(x) < 1e-12) return 1.0;
    return sin(M_PI*x)/(M_PI*x);
}

static int build_kernel(Eac371Resampler *r)
{
    r->kernel = malloc((size_t)RS_PHASES * RS_TAPS * sizeof(float));
    if (!r->kernel) return -1;
    /* Keep transition band below the lower Nyquist frequency. */
    double cutoff = r->out_rate < r->in_rate ? 0.94 * (double)r->out_rate / r->in_rate : 0.94;
    for (int p=0;p<RS_PHASES;p++) {
        double frac = (double)p / RS_PHASES;
        double sum = 0.0;
        for (int t=0;t<RS_TAPS;t++) {
            int off = t - (RS_HALF - 1);
            double x = off - frac;
            double ax = fabs(x) / RS_HALF;
            double w = ax < 1.0 ? (0.42 + 0.5*cos(M_PI*ax) + 0.08*cos(2*M_PI*ax)) : 0.0;
            double h = cutoff * sinc_pi(cutoff * x) * w;
            r->kernel[(size_t)p*RS_TAPS+t] = (float)h;
            sum += h;
        }
        if (fabs(sum) < 1e-20) return -1;
        for (int t=0;t<RS_TAPS;t++) r->kernel[(size_t)p*RS_TAPS+t] /= (float)sum;
    }
    return 0;
}

Eac371Resampler *eac371_resampler_create(int in_rate, int out_rate, int channels)
{
    if (in_rate <= 0 || out_rate <= 0 || channels <= 0) return NULL;
    Eac371Resampler *r = calloc(1,sizeof(*r));
    if (!r) return NULL;
    r->in_rate=in_rate; r->out_rate=out_rate; r->channels=channels;
    r->step=(double)in_rate/out_rate;
    r->pos=0.0;
    if (build_kernel(r)<0) { eac371_resampler_destroy(r); return NULL; }
    return r;
}

int eac371_resampler_push(Eac371Resampler *r, const float *in, size_t n)
{
    if (!r || !in || r->eof) return -1;
    if (!n) return 0;
    if (r->frames + n > r->cap) {
        size_t nc = r->cap ? r->cap : 8192;
        while (nc < r->frames+n) nc *= 2;
        float *p = realloc(r->buf, nc*(size_t)r->channels*sizeof(float));
        if (!p) { snprintf(r->error,sizeof(r->error),"out of memory"); return -1; }
        r->buf=p; r->cap=nc;
    }
    memcpy(r->buf+r->frames*(size_t)r->channels,in,n*(size_t)r->channels*sizeof(float));
    r->frames += n;
    r->total_in += (int64_t)n;
    return 0;
}

static void drop_old(Eac371Resampler *r)
{
    int64_t keep_from = (int64_t)floor(r->pos) - RS_HALF - 2;
    if (keep_from <= r->base) return;
    int64_t d64=keep_from-r->base;
    if (d64 > (int64_t)r->frames) d64=(int64_t)r->frames;
    size_t d=(size_t)d64;
    if (d) {
        memmove(r->buf,r->buf+d*(size_t)r->channels,(r->frames-d)*(size_t)r->channels*sizeof(float));
        r->frames-=d; r->base+=d64;
    }
}

int eac371_resampler_pull(Eac371Resampler *r, float *out)
{
    if (!r || !out) return -1;
    if (r->eof && r->produced >= r->target_out) return 0;
    int64_t center=(int64_t)floor(r->pos);
    int64_t latest=center + RS_HALF;
    int64_t available_end=r->base+(int64_t)r->frames;
    if (!r->eof && latest >= available_end) return 0;
    double frac=r->pos-center;
    int ph=(int)lrint(frac*(RS_PHASES-1));
    if (ph < 0)
        ph = 0;
    if (ph >= RS_PHASES)
        ph = RS_PHASES - 1;
    const float *k=r->kernel+(size_t)ph*RS_TAPS;
    for(int ch=0;ch<r->channels;ch++) {
        double sum=0.0;
        for(int t=0;t<RS_TAPS;t++) {
            int64_t idx=center + t - (RS_HALF-1);
            float v=0.0f;
            if(idx>=r->base && idx<available_end)
                v=r->buf[(size_t)(idx-r->base)*(size_t)r->channels+ch];
            /* Outside the known signal at start/end is zero padding. */
            sum += (double)v*k[t];
        }
        out[ch]=(float)sum;
    }
    r->pos += r->step;
    r->produced++;
    drop_old(r);
    return 1;
}

void eac371_resampler_finish(Eac371Resampler *r)
{
    if (!r || r->eof) return;
    r->eof=1;
    r->target_out=(int64_t)llround((double)r->total_in*r->out_rate/r->in_rate);
    if (r->target_out < 0) r->target_out=0;
}
const char *eac371_resampler_error(const Eac371Resampler *r) { return r&&r->error[0]?r->error:"resampler error"; }
void eac371_resampler_destroy(Eac371Resampler *r) { if(r){free(r->buf);free(r->kernel);free(r);} }
