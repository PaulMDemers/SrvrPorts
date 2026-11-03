/* Local C-locale compatibility; not a replacement for POSIX random(). */
#include "config.h"
#include <stdint.h>
#include <wchar.h>

static uint32_t random_state = 1;

void srvros_awk_srandom(unsigned long seed)
{
    random_state = (uint32_t)seed;
}

long srvros_awk_random(void)
{
    /* A full-period 32-bit LCG, with the less useful low bit discarded. */
    random_state = random_state * UINT32_C(1664525) + UINT32_C(1013904223);
    return (long)(random_state >> 1);
}

int srvros_awk_wctomb(char *out, wchar_t wc)
{
    static mbstate_t state;
    size_t n;
    if (!out) {
        state = (mbstate_t){0};
        return 0;
    }
    n = wcrtomb(out, wc, &state);
    return n == (size_t)-1 ? -1 : (int)n;
}
