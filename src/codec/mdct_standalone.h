/*
 * Project-local MDCT wrapper used by the FFmpeg-derived AC-3 encoder.
 * Copyright (C) 2026 the eac371enc contributors
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef EAC371_MDCT_STANDALONE_H
#define EAC371_MDCT_STANDALONE_H

#include <stddef.h>

typedef struct Eac371Mdct Eac371Mdct;

Eac371Mdct *eac371_mdct_create(int len, float scale);
void eac371_mdct_forward(Eac371Mdct *ctx, float *dst, const float *src,
                         ptrdiff_t stride);
void eac371_mdct_destroy(Eac371Mdct *ctx);

#endif
