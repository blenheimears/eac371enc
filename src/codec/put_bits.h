/* Simplified big-endian bit writer derived from FFmpeg's put_bits API.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef EAC371_PUT_BITS_H
#define EAC371_PUT_BITS_H
#include "standalone_compat.h"

typedef struct PutBitContext {
    uint8_t *buf;
    size_t size;
    size_t bitpos;
} PutBitContext;

static inline void init_put_bits(PutBitContext *s, uint8_t *buffer, int size)
{
    s->buf = buffer;
    s->size = size > 0 ? (size_t)size : 0;
    s->bitpos = 0;
}

static inline int put_bits_count(PutBitContext *s)
{
    return (int)s->bitpos;
}

static inline uint8_t *put_bits_ptr(PutBitContext *s)
{
    return s->buf + ((s->bitpos + 7) >> 3);
}

/* Standalone equivalent of FFmpeg's newer put_bits_assume_flushed() hint.
 * This writer has no cached bit accumulator; byte alignment is the relevant
 * invariant. Keep the debug assertion and expose it to optimizing compilers. */
static inline void put_bits_assume_flushed(PutBitContext *s)
{
    av_assert2((s->bitpos & 7u) == 0);
#if defined(__clang__) && __has_builtin(__builtin_assume)
    __builtin_assume((s->bitpos & 7u) == 0);
#elif defined(__GNUC__)
    if (s->bitpos & 7u)
        __builtin_unreachable();
#endif
}

static inline void put_bits(PutBitContext *s, int n, uint32_t v)
{
    av_assert2(n >= 0 && n <= 31);
    av_assert2(n == 31 || n == 0 || v < (1u << n));
    av_assert2(s->bitpos + (size_t)n <= s->size * 8);

    for (int i = n - 1; i >= 0; i--) {
        size_t p = s->bitpos++;
        uint8_t mask = (uint8_t)(0x80u >> (p & 7));
        uint8_t *dst = &s->buf[p >> 3];
        if ((v >> i) & 1u)
            *dst |= mask;
        else
            *dst &= (uint8_t)~mask;
    }
}

static inline void put_sbits(PutBitContext *s, int n, int32_t v)
{
    put_bits(s, n, av_zero_extend((uint32_t)v, (unsigned)n));
}

static inline void flush_put_bits(PutBitContext *s)
{
    unsigned used = (unsigned)(s->bitpos & 7u);
    if (used) {
        uint8_t keep = (uint8_t)(0xffu << (8u - used));
        s->buf[s->bitpos >> 3] &= keep;
        s->bitpos = (s->bitpos + 7u) & ~(size_t)7u;
    }
}
#endif
