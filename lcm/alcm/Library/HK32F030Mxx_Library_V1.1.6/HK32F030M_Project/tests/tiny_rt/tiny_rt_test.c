/*
 * Checks src/tiny_rt.c (built with TINY_RT_HOST_TEST, functions renamed
 * tiny_*) against the host's own division and mem* functions.
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

uint64_t tiny___aeabi_uidivmod(uint32_t n, uint32_t d);
uint32_t tiny___aeabi_uidiv(uint32_t n, uint32_t d);
int64_t tiny___aeabi_idivmod(int32_t n, int32_t d);
int32_t tiny___aeabi_idiv(int32_t n, int32_t d);
void *tiny_memset(void *dst, int c, size_t n);
void *tiny_memcpy(void *dst, const void *src, size_t n);
int tiny_memcmp(const void *a, const void *b, size_t n);

static uint64_t rng = 88172645463325252ull;
static uint32_t rnd(void)
{
    rng ^= rng << 13, rng ^= rng >> 7, rng ^= rng << 17;
    return (uint32_t)rng;
}

static void check_u(uint32_t n, uint32_t d)
{
    if (d == 0)
        return;
    uint64_t r = tiny___aeabi_uidivmod(n, d);
    assert((uint32_t)r == n / d && (uint32_t)(r >> 32) == n % d);
    assert(tiny___aeabi_uidiv(n, d) == n / d);
}

static void check_s(int32_t n, int32_t d)
{
    if (d == 0)
        return;
    int32_t q, m;
    if (n == INT32_MIN && d == -1)
        q = INT32_MIN, m = 0; /* libgcc's result; C leaves it undefined */
    else
        q = n / d, m = n % d;
    int64_t r = tiny___aeabi_idivmod(n, d);
    assert((int32_t)(uint32_t)r == q && (int32_t)(uint32_t)((uint64_t)r >> 32) == m);
    assert(tiny___aeabi_idiv(n, d) == q);
}

int main(void)
{
    static const uint32_t edge[] = {0, 1, 2, 3, 7, 10, 100, 255, 256, 1000, 65535, 65536, 115200,
                                    32000000, 0x7FFFFFFF, 0x80000000u, 0x80000001u, 0xFFFFFFFE,
                                    0xFFFFFFFF};
    size_t ne = sizeof(edge) / sizeof(edge[0]);
    for (size_t i = 0; i < ne; i++)
        for (size_t j = 0; j < ne; j++)
        {
            check_u(edge[i], edge[j]);
            check_s((int32_t)edge[i], (int32_t)edge[j]);
            check_s(-(int32_t)edge[i], (int32_t)edge[j]);
            check_s((int32_t)edge[i], -(int32_t)edge[j]);
        }
    for (long k = 0; k < 20000000; k++)
    {
        uint32_t n = rnd(), d = rnd() >> (rnd() % 32);
        check_u(n, d);
        check_s((int32_t)n, (int32_t)d);
        check_s((int32_t)n >> (rnd() % 32), (int32_t)d);
    }
    /* d == 0 must not hang or crash */
    (void)tiny___aeabi_uidivmod(5, 0);
    (void)tiny___aeabi_idivmod(-5, 0);

    uint8_t a[300], b[310], c[310];
    for (int i = 0; i < 300; i++)
        a[i] = (uint8_t)rnd();
    for (size_t n = 0; n < 300; n++)
    {
        memset(b, 0x11, sizeof b);
        memset(c, 0x11, sizeof c);
        assert(tiny_memcpy(b + 3, a, n) == b + 3);
        memcpy(c + 3, a, n);
        assert(memcmp(b, c, sizeof b) == 0);
        assert(tiny_memset(b + 1, 0xA5, n) == b + 1);
        memset(c + 1, 0xA5, n);
        assert(memcmp(b, c, sizeof b) == 0);
        int x = tiny_memcmp(a, b, n), y = memcmp(a, b, n);
        assert((x < 0) == (y < 0) && (x > 0) == (y > 0));
    }
    assert(tiny_memcmp(a, a, 300) == 0);
    printf("tiny_rt: division (edge cases + 20M random) and mem* match the host\n");
    return 0;
}
