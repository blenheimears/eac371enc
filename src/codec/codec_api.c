/*
 * Thin project-local wrapper around the retained FFmpeg-derived AC-3 encoder.
 * Copyright (C) 2026 the eac371enc contributors
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "codec_api.h"
#include "ac3enc.h"
#include "standalone_compat.h"

struct Eac371Codec {
    AVCodecContext avctx;
    AC3EncodeContext enc;
    AVPacket pkt;
    char error[160];
};

static void set_option_defaults(AC3EncodeContext *s)
{
    AC3EncOptions *o = &s->options;
    o->dialogue_level = -31;
    o->bitstream_mode = 0;
    o->center_mix_level = LEVEL_MINUS_4POINT5DB;
    o->surround_mix_level = LEVEL_MINUS_6DB;
    o->dolby_surround_mode = AC3ENC_OPT_NONE;
    o->audio_production_info = 0;
    o->mixing_level = AC3ENC_OPT_NONE;
    o->room_type = AC3ENC_OPT_NONE;
    o->copyright = AC3ENC_OPT_NONE;
    o->original = AC3ENC_OPT_NONE;
    o->extended_bsi_1 = 0;
    o->preferred_stereo_downmix = AC3ENC_OPT_NONE;
    o->ltrt_center_mix_level = -1.0f;
    o->ltrt_surround_mix_level = -1.0f;
    o->loro_center_mix_level = -1.0f;
    o->loro_surround_mix_level = -1.0f;
    o->extended_bsi_2 = 0;
    o->dolby_surround_ex_mode = AC3ENC_OPT_NONE;
    o->dolby_headphone_mode = AC3ENC_OPT_NONE;
    o->ad_converter_type = AC3ENC_OPT_NONE;
    o->eac3_mixing_metadata = 0;
    o->eac3_info_metadata = 0;
    o->dependent_channel_map = 0x1a00;
    o->allow_per_frame_metadata = 0;
    o->stereo_rematrixing = 1;
    o->channel_coupling = AC3ENC_OPT_AUTO;
    o->cpl_start = AC3ENC_OPT_AUTO;
}

void eac371_codec_options_default(Eac371CodecOptions *options, int bitrate)
{
    if (!options)
        return;
    options->bitrate = bitrate;
    options->cutoff_hz = 0;
    options->dialogue_level_db = -31;
    options->coupling = EAC371_COUPLING_AUTO;
    options->coupling_start_band = EAC371_CODEC_OPTION_AUTO;
    options->downmix_mode = EAC371_DOWNMIX_UNSET;
    options->dependent_channel_map = 0;
}

static int apply_options(Eac371Codec *c, const Eac371CodecOptions *options,
                         Eac371CodecLayout layout)
{
    AC3EncOptions *o = &c->enc.options;

    if (!options || options->bitrate <= 0 || options->cutoff_hz < 0 ||
        options->dialogue_level_db < -31 || options->dialogue_level_db > -1 ||
        options->coupling < EAC371_COUPLING_AUTO ||
        options->coupling > EAC371_COUPLING_ON ||
        options->coupling_start_band < EAC371_CODEC_OPTION_AUTO ||
        options->coupling_start_band > 15 ||
        options->downmix_mode < EAC371_DOWNMIX_UNSET ||
        options->downmix_mode > EAC371_DOWNMIX_DPLII) {
        return AVERROR(EINVAL);
    }

    c->avctx.bit_rate = options->bitrate;
    c->avctx.cutoff = options->cutoff_hz;
    o->dialogue_level = options->dialogue_level_db;
    o->channel_coupling = options->coupling;
    o->cpl_start = options->coupling_start_band;

    /* The preferred stereo downmix describes the independent/core program.
     * Do not duplicate that metadata in the dependent surround substream. */
    if (layout != EAC371_CODEC_QUAD_DEPENDENT &&
        layout != EAC371_CODEC_DEPENDENT_3_0)
        o->preferred_stereo_downmix = options->downmix_mode;
    if (layout == EAC371_CODEC_QUAD_DEPENDENT ||
        layout == EAC371_CODEC_DEPENDENT_3_0)
        o->dependent_channel_map = options->dependent_channel_map ?
            options->dependent_channel_map : 0x1a00;

    return 0;
}

Eac371Codec *eac371_codec_create(Eac371CodecLayout layout,
                                 const Eac371CodecOptions *options)
{
    Eac371Codec *c = calloc(1, sizeof(*c));
    int ret;
    if (!c)
        return NULL;

    c->avctx.priv_data = &c->enc;
    c->avctx.sample_rate = 48000;
    c->avctx.audio_service_type = AV_AUDIO_SERVICE_TYPE_MAIN;
    c->avctx.flags = AV_CODEC_FLAG_BITEXACT;
    c->avctx.sample_fmt = AV_SAMPLE_FMT_FLTP;
    c->pkt.pts = AV_NOPTS_VALUE;
    set_option_defaults(&c->enc);

    ret = apply_options(c, options, layout);
    if (ret < 0) {
        snprintf(c->error, sizeof(c->error), "invalid encoder options");
        free(c);
        return NULL;
    }

    /* The retained FFmpeg encoder supports all standard AC-3 acmod variants.
     * The dependent quad variant remains the original 7.1 path unchanged. */
    switch (layout) {
    case EAC371_CODEC_5_1:
        c->avctx.ch_layout.mask = AV_CH_LAYOUT_5POINT1; break;
    case EAC371_CODEC_AC3_5_0:
        c->avctx.ch_layout.mask = AV_CH_LAYOUT_5POINT0; break;
    case EAC371_CODEC_AC3_4_1:
        c->avctx.ch_layout.mask = AV_CH_LAYOUT_4POINT0 | AV_CH_LOW_FREQUENCY; break;
    case EAC371_CODEC_AC3_4_0:
        c->avctx.ch_layout.mask = AV_CH_LAYOUT_4POINT0; break;
    case EAC371_CODEC_AC3_3_1:
        c->avctx.ch_layout.mask = AV_CH_LAYOUT_SURROUND | AV_CH_LOW_FREQUENCY; break;
    case EAC371_CODEC_AC3_3_0:
        c->avctx.ch_layout.mask = AV_CH_LAYOUT_SURROUND; break;
    case EAC371_CODEC_AC3_2_1:
        c->avctx.ch_layout.mask = AV_CH_LAYOUT_STEREO | AV_CH_LOW_FREQUENCY; break;
    case EAC371_CODEC_AC3_STEREO:
        c->avctx.ch_layout.mask = AV_CH_LAYOUT_STEREO; break;
    case EAC371_CODEC_AC3_MONO:
        c->avctx.ch_layout.mask = AV_CH_LAYOUT_MONO; break;
    case EAC371_CODEC_QUAD_DEPENDENT:
        c->avctx.ch_layout.mask = AV_CH_LAYOUT_QUAD; break;
    case EAC371_CODEC_DEPENDENT_3_0:
        c->avctx.ch_layout.mask = AV_CH_LAYOUT_SURROUND; break;
    default:
        eac371_codec_destroy(c);
        return NULL;
    }
    c->avctx.ch_layout.nb_channels = 0;
    for (uint64_t mask = c->avctx.ch_layout.mask; mask; mask >>= 1)
        c->avctx.ch_layout.nb_channels += mask & 1u;
    if (layout == EAC371_CODEC_QUAD_DEPENDENT ||
        layout == EAC371_CODEC_DEPENDENT_3_0)
        ret = ff_eac3_float_encode_init(&c->avctx);
    else
        ret = ff_ac3_float_encode_init(&c->avctx);
    if (ret < 0) {
        snprintf(c->error, sizeof(c->error),
                 "encoder initialization failed (%d)", ret);
        eac371_codec_destroy(c);
        return NULL;
    }
    return c;
}

int eac371_codec_encode(Eac371Codec *c, float *const *samples,
                        const uint8_t **packet, size_t *packet_size)
{
    AVFrame f = {0};
    int got = 0, ret;
    if (!c || !samples || !packet || !packet_size)
        return -1;
    f.extended_data = (uint8_t **)samples;
    f.pts = AV_NOPTS_VALUE;
    ret = ff_ac3_encode_frame(&c->avctx, &c->pkt, &f, &got);
    if (ret < 0 || !got) {
        snprintf(c->error, sizeof(c->error), "encoding failed (%d)", ret);
        return -1;
    }
    *packet = c->pkt.data;
    *packet_size = (size_t)c->pkt.size;
    return 0;
}

int eac371_codec_frame_samples(const Eac371Codec *c)
{
    return c ? c->avctx.frame_size : 0;
}

const char *eac371_codec_error(const Eac371Codec *c)
{
    return c && c->error[0] ? c->error : "unknown codec error";
}

void eac371_codec_destroy(Eac371Codec *c)
{
    if (!c)
        return;
    if (c->avctx.priv_data)
        ff_ac3_encode_close(&c->avctx);
    av_packet_unref(&c->pkt);
    free(c);
}
