/*
 * Standalone AC-3 / dependent E-AC-3 encoder API.
 * Copyright (C) 2026 the eac371enc contributors
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef EAC371_CODEC_API_H
#define EAC371_CODEC_API_H
#include <stddef.h>
#include <stdint.h>

#define EAC371_CODEC_FRAME_SAMPLES 1536
#define EAC371_CODEC_DELAY_SAMPLES 256
#define EAC371_CODEC_OPTION_AUTO (-1)
#define EAC371_CODEC_OPTION_NONE (-1)

typedef enum Eac371CodecLayout {
    EAC371_CODEC_5_1 = 0,
    EAC371_CODEC_QUAD_DEPENDENT = 1,
    EAC371_CODEC_AC3_MONO,
    EAC371_CODEC_AC3_STEREO,
    EAC371_CODEC_AC3_2_1,
    EAC371_CODEC_AC3_3_0,
    EAC371_CODEC_AC3_3_1,
    EAC371_CODEC_AC3_4_0,
    EAC371_CODEC_AC3_4_1,
    EAC371_CODEC_AC3_5_0,
    EAC371_CODEC_DEPENDENT_3_0
} Eac371CodecLayout;

typedef enum Eac371CouplingMode {
    EAC371_COUPLING_AUTO = -1,
    EAC371_COUPLING_OFF = 0,
    EAC371_COUPLING_ON = 1
} Eac371CouplingMode;

typedef enum Eac371DownmixMode {
    EAC371_DOWNMIX_UNSET = -1,
    EAC371_DOWNMIX_NOT_INDICATED = 0,
    EAC371_DOWNMIX_LTRT = 1,
    EAC371_DOWNMIX_LORO = 2,
    EAC371_DOWNMIX_DPLII = 3
} Eac371DownmixMode;

typedef struct Eac371CodecOptions {
    int bitrate;
    int cutoff_hz;                 /* 0 = encoder default */
    int dialogue_level_db;         /* -31 .. -1 */
    Eac371CouplingMode coupling;
    int coupling_start_band;       /* -1 = auto, otherwise 0 .. 15 */
    Eac371DownmixMode downmix_mode; /* normally used by the AC-3 core */
    uint16_t dependent_channel_map; /* custom E-AC-3 channel map; 0 = legacy quad */
} Eac371CodecOptions;

typedef struct Eac371Codec Eac371Codec;

void eac371_codec_options_default(Eac371CodecOptions *options, int bitrate);
Eac371Codec *eac371_codec_create(Eac371CodecLayout layout,
                                 const Eac371CodecOptions *options);
int eac371_codec_encode(Eac371Codec *c, float *const *samples,
                        const uint8_t **packet, size_t *packet_size);
int eac371_codec_frame_samples(const Eac371Codec *c);
const char *eac371_codec_error(const Eac371Codec *c);
void eac371_codec_destroy(Eac371Codec *c);
#endif
