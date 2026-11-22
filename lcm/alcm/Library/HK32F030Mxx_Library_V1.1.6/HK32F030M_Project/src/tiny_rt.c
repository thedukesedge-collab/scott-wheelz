/*
 * Refloat Glow: small replacements for the runtime routines GCC pulls
 * in from libgcc / newlib-nano, to make room for the LCM bootloader (the
 * Cortex-M0 has no divide instruction, and libgcc's unrolled Thumb-1
 * division alone is ~730 bytes). Same results, a little slower - fine for
 * the LCM's few divisions per LED frame.
 *
 * Only linked into the GCC firmware build (gcc/Makefile); the host unit
 * tests use the host's own runtime. tests/tiny_rt_test.c checks these
 * against the host's division and mem* functions.
 *
 * GPL-3.0-or-later.
 */
#include <stddef.h>
#include <stdint.h>

#ifdef TINY_RT_HOST_TEST
#define RT(name) tiny_##name
#else
#define RT(name) name
#endif

/* No loop -> library-call rewriting inside the replacements themselves */
#define RT_ATTR __attribute__((used, optimize("no-tree-loop-distribute-patterns")))

/* ------------------------------------------------------------ division */

/* Unsigned 32-bit division by shift-and-subtract. Division by zero returns
 * quotient 0xFFFFFFFF, remainder n (libgcc's result is unspecified too;
 * ALCM never divides by zero). Returns quotient | remainder << 32, which
 * the AEABI *divmod functions return in r0 / r1. */
static uint64_t udivmod32(uint32_t n, uint32_t d)
{
    uint32_t q = 0, bit = 1;

    if (d == 0u)
    {
        return 0xFFFFFFFFu | ((uint64_t)n << 32);
    }
    /* line d up under n (stop before it would overflow) */
    while (d <= n && (d & 0x80000000u) == 0u)
    {
        d <<= 1;
        bit <<= 1;
    }
    while (bit != 0u)
    {
        if (n >= d)
        {
            n -= d;
            q |= bit;
        }
        d >>= 1;
        bit >>= 1;
    }
    return q | ((uint64_t)n << 32);
}

RT_ATTR uint64_t RT(__aeabi_uidivmod)(uint32_t n, uint32_t d)
{
    return udivmod32(n, d);
}

RT_ATTR uint32_t RT(__aeabi_uidiv)(uint32_t n, uint32_t d)
{
    return (uint32_t)udivmod32(n, d);
}

/* Signed: truncates toward zero, remainder takes the dividend's sign (C
 * semantics). INT32_MIN / -1 gives INT32_MIN, as libgcc does. */
RT_ATTR int64_t RT(__aeabi_idivmod)(int32_t n, int32_t d)
{
    uint32_t un = n < 0 ? 0u - (uint32_t)n : (uint32_t)n;
    uint32_t ud = d < 0 ? 0u - (uint32_t)d : (uint32_t)d;
    uint64_t r = udivmod32(un, ud);
    uint32_t q = (uint32_t)r, m = (uint32_t)(r >> 32);

    if ((n < 0) != (d < 0))
    {
        q = 0u - q;
    }
    if (n < 0)
    {
        m = 0u - m;
    }
    return (int64_t)(q | ((uint64_t)m << 32));
}

RT_ATTR int32_t RT(__aeabi_idiv)(int32_t n, int32_t d)
{
    return (int32_t)(uint32_t)RT(__aeabi_idivmod)(n, d);
}

/* ---------------------------------------------------------------- mem* */

RT_ATTR void *RT(memset)(void *dst, int c, size_t n)
{
    uint8_t *p = dst;

    while (n--)
    {
        *p++ = (uint8_t)c;
    }
    return dst;
}

RT_ATTR void *RT(memcpy)(void *dst, const void *src, size_t n)
{
    uint8_t *d = dst;
    const uint8_t *s = src;

    while (n--)
    {
        *d++ = *s++;
    }
    return dst;
}

RT_ATTR int RT(memcmp)(const void *a, const void *b, size_t n)
{
    const uint8_t *p = a, *q = b;

    for (; n; n--, p++, q++)
    {
        if (*p != *q)
        {
            return (int)*p - (int)*q;
        }
    }
    return 0;
}
