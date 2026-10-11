/* Copyright (C) 2026 the eac371enc contributors
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef EAC371_RESAMPLER_H
#define EAC371_RESAMPLER_H
#include <stddef.h>
typedef struct Eac371Resampler Eac371Resampler;
Eac371Resampler *eac371_resampler_create(int input_rate, int output_rate, int channels);
int eac371_resampler_push(Eac371Resampler *r, const float *interleaved, size_t frames);
/* Returns 1 and fills one output frame, 0 if more input is required, -1 on error. */
int eac371_resampler_pull(Eac371Resampler *r, float *interleaved_out);
void eac371_resampler_finish(Eac371Resampler *r);
const char *eac371_resampler_error(const Eac371Resampler *r);
void eac371_resampler_destroy(Eac371Resampler *r);
#endif
