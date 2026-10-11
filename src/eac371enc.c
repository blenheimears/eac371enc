/*
 * eac371enc - Blu-ray compatible AC-3 core + dependent E-AC-3 7.1 encoder
 *
 * Copyright (C) 2026 the eac371enc contributors
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * The AC-3/E-AC-3 coding engine in src/codec is derived from FFmpeg and keeps
 * the original FFmpeg and HellGauss notices in each retained source file.
 */
#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef EAC371_HAVE_SNDFILE
#define EAC371_HAVE_SNDFILE 1
#endif
#if EAC371_HAVE_SNDFILE
#include <sndfile.h>
#endif

#include "codec/codec_api.h"
#include "resampler.h"

#define PROGRAM_NAME "eac371enc"
#define PROGRAM_VERSION "0.1.6-TEST"
#define OUTPUT_SAMPLE_RATE 48000
#define INPUT_CHANNELS 8 /* maximum number of interleaved source channels */
#define CANON_CHANNELS 11
#define CORE_CHANNELS 6
#define DEP_CHANNELS 4
#define FRAME_SAMPLES EAC371_CODEC_FRAME_SAMPLES
#define READ_FRAMES 4096

enum ChannelIndex {
    C_FL = 0,
    C_FR,
    C_FC,
    C_LFE,
    C_BL,
    C_BR,
    C_SL,
    C_SR,
    C_BC,
    C_FLC,
    C_FRC
};

/* Each raw layout has an explicit interleaved speaker order.  A file with an
 * SFC_GET_CHANNEL_MAP_INFO uses its speaker metadata instead.  Negative core
 * selectors represent actual, headroom-preserving downmixes, never silence. */
typedef struct InputLayout {
    const char *name;
    int count;
    int input[INPUT_CHANNELS];
    int core_count;
    int core_src[CORE_CHANNELS];
    Eac371CodecLayout core_codec;
    int dep_count;
    int dep_src[DEP_CHANNELS];
    unsigned dep_map;
    Eac371CodecLayout dep_codec;
} InputLayout;

#define MIX_L (-1)
#define MIX_R (-2)
static const InputLayout layouts[] = {
    {"mono", 1, {C_FC}, 1, {C_FC}, EAC371_CODEC_AC3_MONO, 0, {0}, 0x0000, EAC371_CODEC_QUAD_DEPENDENT},
    {"stereo", 2, {C_FL, C_FR}, 2, {C_FL, C_FR}, EAC371_CODEC_AC3_STEREO, 0, {0}, 0x0000, EAC371_CODEC_QUAD_DEPENDENT},
    {"2.1", 3, {C_FL, C_FR, C_LFE}, 3, {C_FL, C_FR, C_LFE}, EAC371_CODEC_AC3_2_1, 0, {0}, 0x0000, EAC371_CODEC_QUAD_DEPENDENT},
    {"3.0", 3, {C_FL, C_FR, C_FC}, 3, {C_FL, C_FR, C_FC}, EAC371_CODEC_AC3_3_0, 0, {0}, 0x0000, EAC371_CODEC_QUAD_DEPENDENT},
    {"3.1", 4, {C_FL, C_FR, C_FC, C_LFE}, 4, {C_FL, C_FR, C_FC, C_LFE}, EAC371_CODEC_AC3_3_1, 0, {0}, 0x0000, EAC371_CODEC_QUAD_DEPENDENT},
    {"4.0", 4, {C_FL, C_FR, C_FC, C_BC}, 4, {C_FL, C_FR, C_FC, C_BC}, EAC371_CODEC_AC3_4_0, 0, {0}, 0x0000, EAC371_CODEC_QUAD_DEPENDENT},
    {"4.1", 5, {C_FL, C_FR, C_FC, C_LFE, C_BC}, 5, {C_FL, C_FR, C_FC, C_LFE, C_BC}, EAC371_CODEC_AC3_4_1, 0, {0}, 0x0000, EAC371_CODEC_QUAD_DEPENDENT},
    {"5.0", 5, {C_FL, C_FR, C_FC, C_SL, C_SR}, 5, {C_FL, C_FR, C_FC, C_SL, C_SR}, EAC371_CODEC_AC3_5_0, 0, {0}, 0x0000, EAC371_CODEC_QUAD_DEPENDENT},
    {"5.0(back)", 5, {C_FL, C_FR, C_FC, C_BL, C_BR}, 5, {C_FL, C_FR, C_FC, C_BL, C_BR}, EAC371_CODEC_AC3_5_0, 0, {0}, 0x0000, EAC371_CODEC_QUAD_DEPENDENT},
    {"5.1", 6, {C_FL, C_FR, C_FC, C_LFE, C_SL, C_SR}, 6, {C_FL, C_FR, C_FC, C_LFE, C_SL, C_SR}, EAC371_CODEC_5_1, 0, {0}, 0x0000, EAC371_CODEC_QUAD_DEPENDENT},
    {"5.1(back)", 6, {C_FL, C_FR, C_FC, C_LFE, C_BL, C_BR}, 6, {C_FL, C_FR, C_FC, C_LFE, C_BL, C_BR}, EAC371_CODEC_5_1, 0, {0}, 0x0000, EAC371_CODEC_QUAD_DEPENDENT},
    {"6.0", 6, {C_FL, C_FR, C_FC, C_BC, C_SL, C_SR}, 5, {C_FL, C_FR, C_FC, MIX_L, MIX_R}, EAC371_CODEC_AC3_5_0, 3, {C_SL, C_BC, C_SR}, 0x1900, EAC371_CODEC_DEPENDENT_3_0},
    {"6.1", 7, {C_FL, C_FR, C_FC, C_LFE, C_BC, C_SL, C_SR}, 6, {C_FL, C_FR, C_FC, C_LFE, MIX_L, MIX_R}, EAC371_CODEC_5_1, 3, {C_SL, C_BC, C_SR}, 0x1900, EAC371_CODEC_DEPENDENT_3_0},
    {"6.1(back)", 7, {C_FL, C_FR, C_FC, C_LFE, C_BL, C_BR, C_BC}, 6, {C_FL, C_FR, C_FC, C_LFE, MIX_L, MIX_R}, EAC371_CODEC_5_1, 3, {C_BL, C_BC, C_BR}, 0x1900, EAC371_CODEC_DEPENDENT_3_0},
    {"7.0", 7, {C_FL, C_FR, C_FC, C_BL, C_BR, C_SL, C_SR}, 5, {C_FL, C_FR, C_FC, MIX_L, MIX_R}, EAC371_CODEC_AC3_5_0, 4, {C_SL, C_SR, C_BL, C_BR}, 0x1a00, EAC371_CODEC_QUAD_DEPENDENT},
    {"7.1", 8, {C_FL, C_FR, C_FC, C_LFE, C_BL, C_BR, C_SL, C_SR}, 6, {C_FL, C_FR, C_FC, C_LFE, MIX_L, MIX_R}, EAC371_CODEC_5_1, 4, {C_SL, C_SR, C_BL, C_BR}, 0x1a00, EAC371_CODEC_QUAD_DEPENDENT},
    {"7.0(front)", 7, {C_FL, C_FR, C_FC, C_FLC, C_FRC, C_SL, C_SR}, 5, {C_FL, C_FR, C_FC, C_SL, C_SR}, EAC371_CODEC_AC3_5_0, 4, {C_SL, C_SR, C_FLC, C_FRC}, 0x1c00, EAC371_CODEC_QUAD_DEPENDENT},
    {"7.1(wide)", 8, {C_FL, C_FR, C_FC, C_LFE, C_FLC, C_FRC, C_SL, C_SR}, 6, {C_FL, C_FR, C_FC, C_LFE, C_SL, C_SR}, EAC371_CODEC_5_1, 4, {C_SL, C_SR, C_FLC, C_FRC}, 0x1c00, EAC371_CODEC_QUAD_DEPENDENT},
};

static const char *channel_names[CANON_CHANNELS] = {
    "FL", "FR", "FC", "LFE", "BL", "BR", "SL", "SR", "BC", "FLC", "FRC"
};

static int equals_ascii(const char *a, const char *b)
{
    while (*a && *b) {
        char ac = *a++, bc = *b++;
        if (ac >= 'A' && ac <= 'Z') ac += 'a' - 'A';
        if (bc >= 'A' && bc <= 'Z') bc += 'a' - 'A';
        if (ac != bc) return 0;
    }
    return !*a && !*b;
}

static const InputLayout *find_layout(const char *name)
{
    for (size_t i = 0; i < sizeof(layouts) / sizeof(layouts[0]); i++) {
        char explicit_names[96] = {0};
        size_t pos = 0;
        const InputLayout *l = &layouts[i];
        if (equals_ascii(name, l->name) ||
            (equals_ascii(name, "1.0") && equals_ascii(l->name, "mono")) ||
            (equals_ascii(name, "2.0") && equals_ascii(l->name, "stereo")))
            return l;
        for (int c = 0; c < l->count; c++) {
            int n = snprintf(explicit_names + pos, sizeof(explicit_names) - pos,
                             "%s%s", c ? "+" : "", channel_names[l->input[c]]);
            if (n < 0 || (size_t)n >= sizeof(explicit_names) - pos) break;
            pos += (size_t)n;
        }
        if (equals_ascii(name, explicit_names)) return l;
    }
    return NULL;
}

static const InputLayout *default_layout(int channels)
{
    static const char *names[] = {
        NULL, "mono", "stereo", "3.0", "4.0", "5.0", "5.1", "6.1", "7.1"
    };
    return channels >= 1 && channels <= INPUT_CHANNELS ? find_layout(names[channels]) : NULL;
}
typedef struct Options {
    int core_bitrate;
    int extension_bitrate;
    int aggregate_bitrate;
    int aggregate_bitrate_set;
    int core_bitrate_set;
    int extension_bitrate_set;
    int compand_surrounds;
    int cutoff_hz;
    int dialnorm_db;
    Eac371CouplingMode coupling;
    int coupling_start_band;
    Eac371DownmixMode downmix_mode;
    int quiet;
    int raw_pcm;
    int raw_sample_rate;
    int raw_channels;
    int raw_sample_format; /* 1=s16le, 2=s24le, 3=s32le, 4=f32le */
    const InputLayout *layout;
    const char *input_path;
    const char *output_path;
} Options;

typedef struct BitReader {
    const uint8_t *data;
    size_t size;
    size_t bitpos;
} BitReader;

typedef struct EncodePipe {
    Eac371Codec *core;
    Eac371Codec *dep;
    FILE *out;
    float *core_storage;
    float *dep_storage;
    float *core_ch[CORE_CHANNELS];
    float *dep_ch[DEP_CHANNELS];
    const InputLayout *layout;
    int fill;
    int compand;
    int64_t pairs; /* AC-3 frames, with a dependent partner when present */
    int64_t samples_in;
} EncodePipe;

static void usage(FILE *out)
{
    fprintf(out,
            "%s %s\nEncode plain AC-3 (up to 5.1), or AC-3 compatibility core "
            "+ dependent E-AC-3 for 6.0 through 7.1.\n\n"
            "Usage: %s [options] INPUT OUTPUT.ac3|OUTPUT.eac3\n\n"
#if EAC371_HAVE_SNDFILE
            "INPUT is a libsndfile-supported audio file or raw PCM.\n"
#else
            "INPUT must be raw PCM (raw PCM input only in this build).\n"
#endif
            "Input is resampled to 48000 Hz internally when necessary.\n\n"
            "  --bitrate RATE            AC-3 rate for <=5.1; aggregate for >5.1\n"
            "  --core-bitrate RATE       AC-3 rate (default 640k)\n"
            "  --extension-bitrate RATE  dependent E-AC-3 rate (default 384k)\n"
            "  --cutoff HZ               low-pass cutoff, 0=auto\n"
            "  --dialnorm DB             dialogue normalization -31..-1 dB\n"
            "  --coupling MODE           auto, on, off\n"
            "  --coupling-start-band N   auto or 0..15\n"
            "  --downmix-mode MODE       preferred core-to-stereo downmix metadata:\n"
            "                            unset, notindicated, ltrt, loro, dplii\n"
            "  --no-compand              disable legacy core surround-fold compander\n"
            "  --raw-pcm                 input is headerless little-endian PCM\n"
            "  --sample-rate HZ          raw PCM sample rate\n"
            "  --channels N              raw PCM channel count 1..8\n"
            "  --sample-format FMT       s16le, s24le, s32le, f32le (default f32le)\n"
            "  --bit-depth N             integer PCM shorthand 16, 24, 32\n"
            "  --channel-layout LAYOUT   explicit interleaved order for raw/file input\n"
            "                            (required when channel count is ambiguous)\n"
            "                            mono, stereo, 2.1, 3.0, 3.1, 4.0, 4.1,\n"
            "                            5.0, 5.1, 6.0, 6.1, 7.0, 7.1,\n"
            "                            5.0(back), 5.1(back), 6.1(back),\n"
            "                            7.0(front), 7.1(wide),\n"
            "                            or the corresponding FL+FR+... order\n"
            "  -q, --quiet               suppress output\n"
            "  -h, --help                help\n"
            "  --version                 version\n"
            "Default raw layouts follow FFmpeg channel order. Use --channel-layout\n"
            "for 6.0/7.0 and other count-ambiguous variants.\n",
            PROGRAM_NAME, PROGRAM_VERSION, PROGRAM_NAME);
}

static int parse_rate(const char *s)
{
    char *end = NULL;
    double value;
    double multiplier = 1.0;

    errno = 0;
    value = strtod(s, &end);
    if (errno || end == s || value <= 0.0)
        return -1;

    if (*end == 'k' || *end == 'K') {
        multiplier = 1000.0;
        end++;
    } else if (*end == 'm' || *end == 'M') {
        multiplier = 1000000.0;
        end++;
    }

    if (*end || value * multiplier > 100000000.0)
        return -1;

    return (int)llround(value * multiplier);
}

/*
 * Aggregate bitrate quality model.
 *
 * At constrained total rates the dependent stream needs a larger share than
 * the earlier allocator gave it.  It carries four discrete surround channels,
 * and listening tests showed that starving those channels is more objectionable
 * than modestly reducing the 5.1 compatibility core.  Use an even core/extension
 * target through 768 kbit/s, then taper toward the established 640k+384k split
 * at 1024 kbit/s. Above that point the AC-3 core remains capped at 640 kbit/s
 * and the additional rate goes to the E-AC-3 extension.
 *
 * The core must use one of AC-3's legal nominal rates, so choose the legal rate
 * nearest the target and give the exact remainder to the dependent E-AC-3 stream.
 */
#define MIN_DEP_BITRATE 40000
#define MAX_DEP_BITRATE 1024000

static double target_core_fraction(int total)
{
    if (total <= 768000)
        return 0.50;
    if (total >= 1024000)
        return 0.625;
    return 0.50 + 0.125 * ((double)(total - 768000) / 256000.0);
}

static int allocate_aggregate_bitrate(int total, int *core, int *extension)
{
    static const int ac3_rates[] = {
        48000, 56000, 64000, 80000, 96000, 112000, 128000,
        160000, 192000, 224000, 256000, 320000, 384000,
        448000, 512000, 576000, 640000
    };
    const double target_core = total * target_core_fraction(total);
    double best_distance = HUGE_VAL;
    int best_core = 0;
    int best_ext = 0;

    if (!core || !extension)
        return -1;

    for (size_t i = 0; i < sizeof(ac3_rates) / sizeof(ac3_rates[0]); i++) {
        const int c = ac3_rates[i];
        const int e = total - c;
        const double distance = fabs((double)c - target_core);

        if (e < MIN_DEP_BITRATE || e > MAX_DEP_BITRATE)
            continue;

        /* On exact ties prefer the extension, because the compatibility core
         * already contains a downmixed representation of the surround field. */
        if (distance < best_distance - 1e-12 ||
            (fabs(distance - best_distance) <= 1e-12 && c < best_core)) {
            best_distance = distance;
            best_core = c;
            best_ext = e;
        }
    }

    if (!best_core)
        return -1;
    *core = best_core;
    *extension = best_ext;
    return 0;
}

static int parse_raw_sample_format(const char *s)
{
    if (!strcmp(s, "s16le")) return 1;
    if (!strcmp(s, "s24le")) return 2;
    if (!strcmp(s, "s32le")) return 3;
    if (!strcmp(s, "f32le")) return 4;
    return 0;
}


static int parse_int(const char *s, int minimum, int maximum, int *out)
{
    char *end = NULL;
    long value;

    errno = 0;
    value = strtol(s, &end, 10);
    if (errno || end == s || *end || value < minimum || value > maximum)
        return -1;
    *out = (int)value;
    return 0;
}

static int parse_coupling(const char *s, Eac371CouplingMode *mode)
{
    if (!strcmp(s, "auto"))
        *mode = EAC371_COUPLING_AUTO;
    else if (!strcmp(s, "on"))
        *mode = EAC371_COUPLING_ON;
    else if (!strcmp(s, "off"))
        *mode = EAC371_COUPLING_OFF;
    else
        return -1;
    return 0;
}

static int parse_downmix_mode(const char *s, Eac371DownmixMode *mode)
{
    if (!strcmp(s, "unset"))
        *mode = EAC371_DOWNMIX_UNSET;
    else if (!strcmp(s, "notindicated") || !strcmp(s, "not-indicated"))
        *mode = EAC371_DOWNMIX_NOT_INDICATED;
    else if (!strcmp(s, "ltrt"))
        *mode = EAC371_DOWNMIX_LTRT;
    else if (!strcmp(s, "loro"))
        *mode = EAC371_DOWNMIX_LORO;
    else if (!strcmp(s, "dplii"))
        *mode = EAC371_DOWNMIX_DPLII;
    else
        return -1;
    return 0;
}

static int parse_options(int argc, char **argv, Options *opt)
{
    memset(opt, 0, sizeof(*opt));
    opt->core_bitrate = 640000;
    opt->extension_bitrate = 384000;
    opt->compand_surrounds = 1;
    opt->dialnorm_db = -31;
    opt->coupling = EAC371_COUPLING_AUTO;
    opt->coupling_start_band = EAC371_CODEC_OPTION_AUTO;
    opt->downmix_mode = EAC371_DOWNMIX_UNSET;
    opt->raw_channels = 0;
    opt->raw_sample_format = 4;

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];

        if (!strcmp(arg, "-h") || !strcmp(arg, "--help")) {
            usage(stdout);
            exit(0);
        } else if (!strcmp(arg, "--version")) {
            printf("%s %s\n", PROGRAM_NAME, PROGRAM_VERSION);
            exit(0);
        } else if (!strcmp(arg, "-q") || !strcmp(arg, "--quiet")) {
            opt->quiet = 1;
        } else if (!strcmp(arg, "--no-compand")) {
            opt->compand_surrounds = 0;
        } else if (!strcmp(arg, "--raw-pcm")) {
            opt->raw_pcm = 1;
        } else if (!strcmp(arg, "--sample-rate")) {
            if (++i >= argc || parse_int(argv[i], 1, 768000, &opt->raw_sample_rate) < 0) {
                fprintf(stderr, "%s: --sample-rate requires a positive rate\n", PROGRAM_NAME);
                return -1;
            }
        } else if (!strcmp(arg, "--channels")) {
            if (++i >= argc || parse_int(argv[i], 1, 64, &opt->raw_channels) < 0) {
                fprintf(stderr, "%s: --channels requires a positive count\n", PROGRAM_NAME);
                return -1;
            }
        } else if (!strcmp(arg, "--sample-format")) {
            if (++i >= argc || !(opt->raw_sample_format = parse_raw_sample_format(argv[i]))) {
                fprintf(stderr, "%s: --sample-format requires s16le, s24le, s32le, or f32le\n", PROGRAM_NAME);
                return -1;
            }
        } else if (!strcmp(arg, "--bit-depth")) {
            int depth = 0;
            if (++i >= argc || parse_int(argv[i], 16, 32, &depth) < 0 || (depth != 16 && depth != 24 && depth != 32)) {
                fprintf(stderr, "%s: --bit-depth requires 16, 24, or 32\n", PROGRAM_NAME);
                return -1;
            }
            opt->raw_sample_format = depth == 16 ? 1 : depth == 24 ? 2 : 3;
        } else if (!strcmp(arg, "--channel-layout")) {
            if (++i >= argc || !(opt->layout = find_layout(argv[i]))) {
                fprintf(stderr, "%s: unsupported --channel-layout (see --help)\n", PROGRAM_NAME);
                return -1;
            }
        } else if (!strcmp(arg, "--bitrate")) {
            int rate;
            if (++i >= argc) {
                fprintf(stderr, "%s: --bitrate requires a value\n", PROGRAM_NAME);
                return -1;
            }
            rate = parse_rate(argv[i]);
            if (rate < 0) {
                fprintf(stderr, "%s: invalid bitrate: %s\n", PROGRAM_NAME,
                        argv[i]);
                return -1;
            }
            opt->aggregate_bitrate = rate;
            opt->aggregate_bitrate_set = 1;
        } else if (!strcmp(arg, "--core-bitrate") ||
                   !strcmp(arg, "--extension-bitrate")) {
            int is_core = !strcmp(arg, "--core-bitrate");
            int rate;

            if (++i >= argc) {
                fprintf(stderr, "%s: %s requires a value\n", PROGRAM_NAME, arg);
                return -1;
            }
            rate = parse_rate(argv[i]);
            if (rate < 0) {
                fprintf(stderr, "%s: invalid bitrate: %s\n", PROGRAM_NAME,
                        argv[i]);
                return -1;
            }
            if (is_core) {
                opt->core_bitrate = rate;
                opt->core_bitrate_set = 1;
            } else {
                opt->extension_bitrate = rate;
                opt->extension_bitrate_set = 1;
            }
        } else if (!strcmp(arg, "--cutoff")) {
            if (++i >= argc || parse_int(argv[i], 0, OUTPUT_SAMPLE_RATE / 2,
                                         &opt->cutoff_hz) < 0) {
                fprintf(stderr, "%s: --cutoff requires 0..24000 Hz\n",
                        PROGRAM_NAME);
                return -1;
            }
        } else if (!strcmp(arg, "--dialnorm")) {
            if (++i >= argc || parse_int(argv[i], -31, -1,
                                         &opt->dialnorm_db) < 0) {
                fprintf(stderr, "%s: --dialnorm requires -31..-1 dB\n",
                        PROGRAM_NAME);
                return -1;
            }
        } else if (!strcmp(arg, "--coupling")) {
            if (++i >= argc || parse_coupling(argv[i], &opt->coupling) < 0) {
                fprintf(stderr, "%s: --coupling requires auto, on, or off\n",
                        PROGRAM_NAME);
                return -1;
            }
        } else if (!strcmp(arg, "--coupling-start-band") ||
                   !strcmp(arg, "--cpl-start-band")) {
            if (++i >= argc) {
                fprintf(stderr, "%s: %s requires auto or 0..15\n",
                        PROGRAM_NAME, arg);
                return -1;
            }
            if (!strcmp(argv[i], "auto")) {
                opt->coupling_start_band = EAC371_CODEC_OPTION_AUTO;
            } else if (parse_int(argv[i], 0, 15,
                                 &opt->coupling_start_band) < 0) {
                fprintf(stderr, "%s: %s requires auto or 0..15\n",
                        PROGRAM_NAME, arg);
                return -1;
            }
        } else if (!strcmp(arg, "--downmix-mode")) {
            if (++i >= argc ||
                parse_downmix_mode(argv[i], &opt->downmix_mode) < 0) {
                fprintf(stderr,
                        "%s: --downmix-mode requires unset, notindicated, "
                        "ltrt, loro, or dplii\n",
                        PROGRAM_NAME);
                return -1;
            }
        } else if (arg[0] == '-' && strcmp(arg, "-")) {
            fprintf(stderr, "%s: unknown option: %s\n", PROGRAM_NAME, arg);
            return -1;
        } else if (!opt->input_path) {
            opt->input_path = arg;
        } else if (!opt->output_path) {
            opt->output_path = arg;
        } else {
            fprintf(stderr, "%s: too many arguments\n", PROGRAM_NAME);
            return -1;
        }
    }

    if (!opt->input_path || !opt->output_path) {
        usage(stderr);
        return -1;
    }
    if (opt->raw_pcm) {
        if (opt->raw_sample_rate <= 0) {
            fprintf(stderr, "%s: --raw-pcm requires --sample-rate\n", PROGRAM_NAME);
            return -1;
        }
        if (!opt->layout)
            opt->layout = default_layout(opt->raw_channels);
        if (!opt->layout || (opt->raw_channels && opt->raw_channels != opt->layout->count)) {
            fprintf(stderr, "%s: raw PCM channels/layout mismatch or unsupported channel count\n", PROGRAM_NAME);
            return -1;
        }
        opt->raw_channels = opt->layout->count;
    }
#if !EAC371_HAVE_SNDFILE
    if (!opt->raw_pcm) {
        fprintf(stderr, "%s: this build has no libsndfile support; use --raw-pcm\n", PROGRAM_NAME);
        return -1;
    }
#endif

    if (opt->aggregate_bitrate_set &&
        (opt->core_bitrate_set || opt->extension_bitrate_set)) {
        fprintf(stderr,
                "%s: --bitrate cannot be combined with --core-bitrate or "
                "--extension-bitrate\n",
                PROGRAM_NAME);
        return -1;
    }
    return 0;
}

static unsigned br_get(BitReader *br, unsigned bits)
{
    unsigned value = 0;

    if (!bits || bits > 24 || br->bitpos + bits > br->size * 8)
        return UINT32_MAX;

    while (bits--) {
        size_t pos = br->bitpos++;
        value = (value << 1) |
                ((br->data[pos >> 3] >> (7 - (pos & 7))) & 1u);
    }
    return value;
}

static int validate_core(const uint8_t *packet, size_t size)
{
    if (size < 7 || packet[0] != 0x0b || packet[1] != 0x77)
        return -1;
    return ((packet[5] >> 3) & 31) <= 10 ? 0 : -1;
}

static int validate_dep(const uint8_t *packet, size_t size, unsigned expected_channel_map)
{
    BitReader br = { packet, size, 0 };
    unsigned sync = br_get(&br, 16);
    unsigned stream_type = br_get(&br, 2);
    unsigned comp_present;
    unsigned channel_map_present;
    unsigned channel_map;

    (void)br_get(&br, 3);  /* substreamid */
    (void)br_get(&br, 11); /* frmsiz */
    (void)br_get(&br, 2);  /* fscod */
    (void)br_get(&br, 2);  /* numblkscod (48 kHz output) */
    (void)br_get(&br, 3);  /* acmod */
    (void)br_get(&br, 1);  /* lfeon */
    (void)br_get(&br, 5);  /* bsid */
    (void)br_get(&br, 5);  /* dialnorm */

    comp_present = br_get(&br, 1);
    if (comp_present)
        (void)br_get(&br, 8);

    channel_map_present = br_get(&br, 1);
    if (channel_map_present != 1)
        return -1;

    channel_map = br_get(&br, 16);
    return sync == 0x0b77 && stream_type == 1 && channel_map == expected_channel_map
               ? 0
               : -1;
}

static float legacy_compand(float x)
{
    static const double input_db[] = { -90.0, -8.0, -6.0, 0.0 };
    static const double output_db[] = { -84.0, -2.0, -1.0, -0.1 };
    double db;
    double out_db;
    double amplitude;

    if (!isfinite(x) || x == 0.0f)
        return 0.0f;

    db = 20.0 * log10(fabs((double)x));
    if (db <= input_db[0]) {
        out_db = db + output_db[0] - input_db[0];
    } else if (db >= input_db[3]) {
        out_db = output_db[3] +
                 (db - input_db[3]) *
                     (output_db[3] - output_db[2]) /
                     (input_db[3] - input_db[2]);
    } else {
        out_db = output_db[0];
        for (int i = 0; i < 3; i++) {
            if (db <= input_db[i + 1]) {
                double t = (db - input_db[i]) /
                           (input_db[i + 1] - input_db[i]);
                out_db = output_db[i] +
                         t * (output_db[i + 1] - output_db[i]);
                break;
            }
        }
    }

    amplitude = pow(10.0, out_db / 20.0);
    if (amplitude > 0.999999)
        amplitude = 0.999999;
    return (float)copysign(amplitude, x);
}

#if EAC371_HAVE_SNDFILE
static int map_channel(int channel)
{
    switch (channel) {
    case SF_CHANNEL_MAP_FRONT_LEFT: case SF_CHANNEL_MAP_LEFT: return C_FL;
    case SF_CHANNEL_MAP_FRONT_RIGHT: case SF_CHANNEL_MAP_RIGHT: return C_FR;
    case SF_CHANNEL_MAP_FRONT_CENTER: case SF_CHANNEL_MAP_CENTER: return C_FC;
    case SF_CHANNEL_MAP_LFE: return C_LFE;
    case SF_CHANNEL_MAP_REAR_LEFT: return C_BL;
    case SF_CHANNEL_MAP_REAR_RIGHT: return C_BR;
    case SF_CHANNEL_MAP_SIDE_LEFT: return C_SL;
    case SF_CHANNEL_MAP_SIDE_RIGHT: return C_SR;
    case SF_CHANNEL_MAP_REAR_CENTER: return C_BC;
    case SF_CHANNEL_MAP_FRONT_LEFT_OF_CENTER: return C_FLC;
    case SF_CHANNEL_MAP_FRONT_RIGHT_OF_CENTER: return C_FRC;
    default: return -1;
    }
}

static const InputLayout *setup_file_layout(SNDFILE *sf, int count,
                                             const InputLayout *requested,
                                             int source_channels[INPUT_CHANNELS],
                                             int quiet)
{
    int map[INPUT_CHANNELS];
    const InputLayout *found = NULL;
    if (!requested && sf_command(sf, SFC_GET_CHANNEL_MAP_INFO, map,
                                  count * (int)sizeof(map[0])) == SF_TRUE) {
        unsigned set = 0;
        int valid = 1;
        for (int c = 0; c < count; c++) {
            source_channels[c] = map_channel(map[c]);
            if (source_channels[c] < 0 || (set & (1u << source_channels[c])))
                valid = 0;
            else
                set |= 1u << source_channels[c];
        }
        if (valid) {
            for (size_t i = 0; i < sizeof(layouts) / sizeof(layouts[0]); i++) {
                unsigned need = 0;
                if (layouts[i].count != count) continue;
                for (int c = 0; c < count; c++) need |= 1u << layouts[i].input[c];
                if (set == need) { found = &layouts[i]; break; }
            }
        }
        if (!found && !quiet)
            fprintf(stderr, "%s: unsupported file channel map; falling back to count-based layout\n", PROGRAM_NAME);
    }
    if (requested) found = requested;
    if (!found) found = default_layout(count);
    if (!found || found->count != count) return NULL;
    if (!requested && !quiet)
        fprintf(stderr, "%s: input layout: %s\n", PROGRAM_NAME, found->name);
    if (requested || !sf_command(sf, SFC_GET_CHANNEL_MAP_INFO, map,
                                   count * (int)sizeof(map[0]))) {
        for (int i = 0; i < count; i++) source_channels[i] = found->input[i];
    } else {
        unsigned set = 0, need = 0;
        for (int c = 0; c < count; c++) {
            source_channels[c] = map_channel(map[c]);
            if (source_channels[c] >= 0) set |= 1u << source_channels[c];
        }
        for (int c = 0; c < count; c++) need |= 1u << found->input[c];
        if (set != need) {
            for (int i = 0; i < count; i++) source_channels[i] = found->input[i];
        }
    }
    return found;
}

#endif

static void pipe_destroy(EncodePipe *pipe)
{
    if (!pipe)
        return;
    eac371_codec_destroy(pipe->core);
    eac371_codec_destroy(pipe->dep);
    free(pipe->core_storage);
    free(pipe->dep_storage);
    memset(pipe, 0, sizeof(*pipe));
}

static int pipe_init(EncodePipe *pipe, FILE *out, const Options *opt)
{
    Eac371CodecOptions core_options, dep_options;
    const InputLayout *l = opt->layout;
    memset(pipe, 0, sizeof(*pipe));
    pipe->out = out;
    pipe->layout = l;
    pipe->compand = opt->compand_surrounds;
    eac371_codec_options_default(&core_options, opt->core_bitrate);
    eac371_codec_options_default(&dep_options, opt->extension_bitrate);
    core_options.cutoff_hz = dep_options.cutoff_hz = opt->cutoff_hz;
    core_options.dialogue_level_db = dep_options.dialogue_level_db = opt->dialnorm_db;
    core_options.coupling = dep_options.coupling = opt->coupling;
    core_options.coupling_start_band = dep_options.coupling_start_band = opt->coupling_start_band;
    core_options.downmix_mode = opt->downmix_mode;
    dep_options.dependent_channel_map = (uint16_t)l->dep_map;

    pipe->core = eac371_codec_create(l->core_codec, &core_options);
    if (l->dep_count)
        pipe->dep = eac371_codec_create(l->dep_codec, &dep_options);
    if (!pipe->core || (l->dep_count && !pipe->dep) ||
        eac371_codec_frame_samples(pipe->core) != FRAME_SAMPLES ||
        (l->dep_count && eac371_codec_frame_samples(pipe->dep) != FRAME_SAMPLES)) {
        pipe_destroy(pipe);
        return -1;
    }
    pipe->core_storage = calloc((size_t)l->core_count * FRAME_SAMPLES, sizeof(float));
    if (l->dep_count)
        pipe->dep_storage = calloc((size_t)l->dep_count * FRAME_SAMPLES, sizeof(float));
    if (!pipe->core_storage || (l->dep_count && !pipe->dep_storage)) {
        pipe_destroy(pipe);
        return -1;
    }
    for (int c = 0; c < l->core_count; c++)
        pipe->core_ch[c] = pipe->core_storage + (size_t)c * FRAME_SAMPLES;
    for (int c = 0; c < l->dep_count; c++)
        pipe->dep_ch[c] = pipe->dep_storage + (size_t)c * FRAME_SAMPLES;
    return 0;
}

static int pipe_encode_frame(EncodePipe *pipe)
{
    const InputLayout *l = pipe->layout;
    const uint8_t *packet;
    size_t size;
    if (eac371_codec_encode(pipe->core, pipe->core_ch, &packet, &size) ||
        validate_core(packet, size) < 0 ||
        fwrite(packet, 1, size, pipe->out) != size)
        return -1;
    if (l->dep_count) {
        if (eac371_codec_encode(pipe->dep, pipe->dep_ch, &packet, &size) ||
            validate_dep(packet, size, l->dep_map) < 0 ||
            fwrite(packet, 1, size, pipe->out) != size)
            return -1;
    }
    pipe->pairs++;
    pipe->fill = 0;
    memset(pipe->core_storage, 0, (size_t)l->core_count * FRAME_SAMPLES * sizeof(float));
    if (l->dep_count)
        memset(pipe->dep_storage, 0, (size_t)l->dep_count * FRAME_SAMPLES * sizeof(float));
    return 0;
}

static float fold_surround(const EncodePipe *pipe, const float sample[CANON_CHANNELS], int right)
{
    /* Keep the old 7.1 50:50 mix unchanged.  6.1 center-rear is also
     * mixed equally into the two core surround speakers (with headroom). */
    const InputLayout *l = pipe->layout;
    int side = right ? C_SR : C_SL;
    int back = right ? C_BR : C_BL;
    int has_side = 0, has_back = 0, has_bc = 0;
    float result;
    for (int c = 0; c < l->count; c++) {
        has_side |= (l->input[c] == side);
        has_back |= (l->input[c] == back);
        has_bc |= (l->input[c] == C_BC);
    }
    if (has_side && has_back) result = 0.5f * (sample[side] + sample[back]);
    else if (has_side && has_bc) result = 0.5f * (sample[side] + sample[C_BC]);
    else if (has_back && has_bc) result = 0.5f * (sample[back] + sample[C_BC]);
    else result = has_side ? sample[side] : has_back ? sample[back] : sample[C_BC];
    return pipe->compand ? legacy_compand(result) : result;
}

static int pipe_sample(EncodePipe *pipe, const float sample[CANON_CHANNELS])
{
    const InputLayout *l = pipe->layout;
    const int i = pipe->fill;
    for (int c = 0; c < l->core_count; c++) {
        int src = l->core_src[c];
        pipe->core_ch[c][i] = src == MIX_L ? fold_surround(pipe, sample, 0) :
                              src == MIX_R ? fold_surround(pipe, sample, 1) :
                              sample[src];
    }
    for (int c = 0; c < l->dep_count; c++)
        pipe->dep_ch[c][i] = sample[l->dep_src[c]];
    pipe->fill++;
    pipe->samples_in++;
    return pipe->fill == FRAME_SAMPLES ? pipe_encode_frame(pipe) : 0;
}

static int pipe_finish(EncodePipe *pipe)
{
    int final_fill = pipe->fill, need_drain = 0;
    if (final_fill) {
        need_drain = final_fill > FRAME_SAMPLES - EAC371_CODEC_DELAY_SAMPLES;
        if (pipe_encode_frame(pipe) < 0) return -1;
    } else if (pipe->pairs > 0) need_drain = 1;
    if (need_drain) return pipe_encode_frame(pipe);
    return 0;
}

static int raw_bytes_per_sample(int format)
{
    switch (format) { case 1: return 2; case 2: return 3; case 3: case 4: return 4; default: return 0; }
}

static float raw_sample_to_float(const uint8_t *p, int format)
{
    if (format == 1) {
        int16_t v = (int16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
        return (float)v / 32768.0f;
    }
    if (format == 2) {
        int32_t v = (int32_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16));
        if (v & 0x00800000) v |= (int32_t)0xff000000;
        return (float)v / 8388608.0f;
    }
    if (format == 3) {
        uint32_t u = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
        int32_t v; memcpy(&v, &u, sizeof(v));
        return (float)((double)v / 2147483648.0);
    }
    if (format == 4) {
        uint32_t u = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
        float v; memcpy(&v, &u, sizeof(v));
        return isfinite(v) ? v : 0.0f;
    }
    return 0.0f;
}

static int feed_canonical_frames(EncodePipe *pipe, Eac371Resampler *resampler,
                                 const float *canonical, size_t frames)
{
    if (!resampler) {
        for (size_t i = 0; i < frames; i++)
            if (pipe_sample(pipe, &canonical[i * CANON_CHANNELS]) < 0) return -1;
        return 0;
    }
    if (eac371_resampler_push(resampler, canonical, frames) < 0) return -1;
    for (;;) {
        float sample[CANON_CHANNELS];
        int available = eac371_resampler_pull(resampler, sample);
        if (available < 0) return -1;
        if (!available) break;
        if (pipe_sample(pipe, sample) < 0) return -1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    Options opt;
    FILE *out = NULL;
    FILE *raw_file = NULL;
#if EAC371_HAVE_SNDFILE
    SF_INFO info = { 0 };
    SNDFILE *sf = NULL;
#endif
    float *raw = NULL;
    float *canonical = NULL;
    uint8_t *raw_bytes = NULL;
#if EAC371_HAVE_SNDFILE
    int source_channels[INPUT_CHANNELS] = {0};
#endif
    EncodePipe pipe = { 0 };
    int pipe_ready = 0;
    Eac371Resampler *resampler = NULL;
    int rc = 1;
    int input_rate = 0;
    int64_t total_read = 0;
    int64_t known_frames = 0;

    if (parse_options(argc, argv, &opt) < 0) return 2;

    if (opt.raw_pcm) {
        input_rate = opt.raw_sample_rate;
        raw_file = !strcmp(opt.input_path, "-") ? stdin : fopen(opt.input_path, "rb");
        if (!raw_file) {
            fprintf(stderr, "%s: cannot open raw PCM input: %s\n", PROGRAM_NAME, strerror(errno));
            goto done;
        }
    } else {
#if EAC371_HAVE_SNDFILE
        sf = sf_open(opt.input_path, SFM_READ, &info);
        if (!sf) {
            fprintf(stderr, "%s: cannot open input: %s\n", PROGRAM_NAME, sf_strerror(NULL));
            goto done;
        }
        if (info.channels < 1 || info.channels > INPUT_CHANNELS || info.samplerate <= 0) {
            fprintf(stderr, "%s: input must have 1..8 channels with a valid sample rate\n", PROGRAM_NAME);
            goto done;
        }
        opt.layout = setup_file_layout(sf, info.channels, opt.layout,
                                       source_channels, opt.quiet);
        if (!opt.layout) {
            fprintf(stderr, "%s: unsupported input channel layout\n", PROGRAM_NAME);
            goto done;
        }
        input_rate = info.samplerate;
        known_frames = info.frames;
#else
        goto done;
#endif
    }

    /* Codec/rate selection must happen after opening file input: its channel
     * count and speaker map may not be known at option-parsing time. */
    if (!opt.layout) { fprintf(stderr, "%s: input layout unavailable\n", PROGRAM_NAME); goto done; }
    if (opt.layout->dep_count == 0) {
        if (opt.extension_bitrate_set) {
            fprintf(stderr, "%s: --extension-bitrate requires more than 5.1 channels\n", PROGRAM_NAME);
            goto done;
        }
        if (opt.aggregate_bitrate_set) opt.core_bitrate = opt.aggregate_bitrate;
    } else if (opt.aggregate_bitrate_set &&
               allocate_aggregate_bitrate(opt.aggregate_bitrate, &opt.core_bitrate,
                                          &opt.extension_bitrate) < 0) {
        fprintf(stderr, "%s: invalid aggregate bitrate for core + extension\n", PROGRAM_NAME);
        goto done;
    }
    if (opt.core_bitrate > 640000 || opt.core_bitrate < 32000) {
        fprintf(stderr, "%s: AC-3 bitrate must be between 32k and 640k\n", PROGRAM_NAME);
        goto done;
    }
    if (opt.layout->dep_count &&
        (opt.extension_bitrate < MIN_DEP_BITRATE || opt.extension_bitrate > MAX_DEP_BITRATE)) {
        fprintf(stderr, "%s: dependent E-AC-3 bitrate must be 40k..1024k\n", PROGRAM_NAME);
        goto done;
    }
    out = !strcmp(opt.output_path, "-") ? stdout : fopen(opt.output_path, "wb");
    if (!out) {
        fprintf(stderr, "%s: cannot open output: %s\n", PROGRAM_NAME, strerror(errno));
        goto done;
    }
    if (pipe_init(&pipe, out, &opt) < 0) {
        fprintf(stderr, "%s: cannot initialize AC-3/E-AC-3 encoder (check bitrates)\n", PROGRAM_NAME);
        goto done;
    }
    pipe_ready = 1;

    raw = malloc((size_t)READ_FRAMES * (size_t)opt.layout->count * sizeof(float));
    canonical = calloc((size_t)READ_FRAMES * CANON_CHANNELS, sizeof(float));
    if (!raw || !canonical) { fprintf(stderr, "%s: out of memory\n", PROGRAM_NAME); goto done; }

    if (opt.raw_pcm) {
        const int bps = raw_bytes_per_sample(opt.raw_sample_format);
        raw_bytes = malloc((size_t)READ_FRAMES * (size_t)opt.layout->count * (size_t)bps);
        if (!raw_bytes) { fprintf(stderr, "%s: out of memory\n", PROGRAM_NAME); goto done; }
    }
    if (input_rate != OUTPUT_SAMPLE_RATE) {
        resampler = eac371_resampler_create(input_rate, OUTPUT_SAMPLE_RATE, CANON_CHANNELS);
        if (!resampler) { fprintf(stderr, "%s: cannot initialize resampler\n", PROGRAM_NAME); goto done; }
    }

    if (!opt.quiet) {
        fprintf(stderr, "%s %s\nInput: %s (%d Hz, %s, %d channels%s)\nOutput: %s\n",
                PROGRAM_NAME, PROGRAM_VERSION, opt.input_path, input_rate,
                opt.layout->name, opt.layout->count,
                opt.raw_pcm ? ", raw PCM" : "", opt.output_path);
        if (opt.layout->dep_count)
            fprintf(stderr, "Core: AC-3 %s @ %.0f kbit/s; extension: dependent E-AC-3 %d channels @ %.0f kbit/s (map 0x%04X)\n",
                    opt.layout->core_count == 6 ? "5.1" : "5.0",
                    opt.core_bitrate / 1000.0, opt.layout->dep_count,
                    opt.extension_bitrate / 1000.0, opt.layout->dep_map);
        else
            fprintf(stderr, "Plain AC-3 %s @ %.0f kbit/s\n", opt.layout->name,
                    opt.core_bitrate / 1000.0);
        if (opt.aggregate_bitrate_set && opt.layout->dep_count)
            fprintf(stderr, "Aggregate bitrate target: %.0f kbit/s; quality-balanced split selected automatically\n", opt.aggregate_bitrate / 1000.0);
        if (resampler) fprintf(stderr, "Resampling internally to 48000 Hz\n");
    }

    for (;;) {
        size_t n = 0;
        if (opt.raw_pcm) {
            const int bps = raw_bytes_per_sample(opt.raw_sample_format);
            const size_t frame_bytes = (size_t)opt.layout->count * (size_t)bps;
            const size_t got = fread(raw_bytes, 1, (size_t)READ_FRAMES * frame_bytes, raw_file);
            if (ferror(raw_file)) { fprintf(stderr, "%s: raw PCM read error\n", PROGRAM_NAME); goto done; }
            if (!got) break;
            if (got % frame_bytes) { fprintf(stderr, "%s: raw PCM input ends with an incomplete sample frame\n", PROGRAM_NAME); goto done; }
            n = got / frame_bytes;
            for (size_t i = 0; i < n; i++)
                for (int c = 0; c < opt.layout->count; c++)
                    canonical[i * CANON_CHANNELS + opt.layout->input[c]] =
                        raw_sample_to_float(raw_bytes + (i * opt.layout->count + c) * (size_t)bps, opt.raw_sample_format);
        } else {
#if EAC371_HAVE_SNDFILE
            sf_count_t got = sf_readf_float(sf, raw, READ_FRAMES);
            if (got < 0) { fprintf(stderr, "%s: input read error\n", PROGRAM_NAME); goto done; }
            if (!got) break;
            n = (size_t)got;
            for (size_t i = 0; i < n; i++)
                for (int c = 0; c < opt.layout->count; c++)
                    canonical[i * CANON_CHANNELS + source_channels[c]] =
                        raw[i * opt.layout->count + c];
#endif
        }
        total_read += (int64_t)n;
        if (feed_canonical_frames(&pipe, resampler, canonical, n) < 0) {
            fprintf(stderr, "%s: encoding/resampling failed%s%s\n", PROGRAM_NAME,
                    resampler ? ": " : "", resampler ? eac371_resampler_error(resampler) : "");
            goto done;
        }
        if (!opt.quiet && known_frames > 0) {
            double percent = 100.0 * (double)total_read / (double)known_frames;
            if (percent > 100.0) percent = 100.0;
            fprintf(stderr, "\rEncoding: %6.2f%%", percent); fflush(stderr);
        }
    }

    if (resampler) {
        float sample[CANON_CHANNELS];
        eac371_resampler_finish(resampler);
        for (;;) {
            int available = eac371_resampler_pull(resampler, sample);
            if (available < 0) { fprintf(stderr, "%s: resampling failed: %s\n", PROGRAM_NAME, eac371_resampler_error(resampler)); goto done; }
            if (!available) break;
            if (pipe_sample(&pipe, sample) < 0) { fprintf(stderr, "%s: encoding failed\n", PROGRAM_NAME); goto done; }
        }
    }
    if (pipe_finish(&pipe) < 0 || fflush(out)) { fprintf(stderr, "%s: final output write failed\n", PROGRAM_NAME); goto done; }
    if (!opt.quiet)
        fprintf(stderr, "\rEncoding: 100.00%%\nWrote %lld %s for %lld output-rate samples (%.3f s)\n",
                (long long)pipe.pairs, opt.layout->dep_count ? "core/extension frame pairs" : "AC-3 frames",
                (long long)pipe.samples_in, pipe.samples_in / (double)OUTPUT_SAMPLE_RATE);
    rc = 0;

done:
    if (out && out != stdout) fclose(out);
    if (rc && opt.output_path && strcmp(opt.output_path, "-")) remove(opt.output_path);
    if (raw_file && raw_file != stdin) fclose(raw_file);
#if EAC371_HAVE_SNDFILE
    if (sf) sf_close(sf);
#endif
    if (pipe_ready) pipe_destroy(&pipe);
    eac371_resampler_destroy(resampler);
    free(raw_bytes); free(raw); free(canonical);
    return rc;
}
