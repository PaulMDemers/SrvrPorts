/* Provider contract check; byte statistics do not establish entropy quality. */
extern "C" {
#include <srvros/sys.h>
}
#include <wtf/srvros/RandomContract.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static_assert(SRV_RANDOM_REQUIRE_ENTROPY == 1);
static_assert(SRV_RANDOM_UNAVAILABLE == -11);
static_assert(SYS_RANDOM == 92);

int main(int argc, char **argv)
{
    if (argc != 2 || (strcmp(argv[1], "absent") && strcmp(argv[1], "rdrand")))
        return 1;
    bool positive = !strcmp(argv[1], "rdrand");
    uint32_t a, b, c, d;
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(1), "c"(0));
    if (!!(c & (1u << 30)) != positive)
        return 2;
    unsigned char bytes[259];
    memset(bytes, 0xa5, sizeof(bytes));
    if (srv_random(nullptr, 0, SRV_RANDOM_REQUIRE_ENTROPY) != 0
        || srv_random(nullptr, 1, SRV_RANDOM_REQUIRE_ENTROPY) != -1
        || srv_random(bytes + 1, 257, 2) != -1
        || srv_random(bytes + 1, 257, 3) != -1
        || srv_random(bytes + 1, 262145, SRV_RANDOM_REQUIRE_ENTROPY) != -1)
        return 3;
    long received = srv_random(bytes + 1, 257, SRV_RANDOM_REQUIRE_ENTROPY);
    if (!positive) {
        if (received != SRV_RANDOM_UNAVAILABLE)
            return 4;
        for (unsigned char byte : bytes) {
            if (byte != 0xa5)
                return 5;
        }
    } else if (received != 257 || bytes[0] != 0xa5 || bytes[258] != 0xa5)
        return 6;
    if (srv_random(bytes, 1, 0) != 1)
        return 7;
    puts(positive ? "random-entropy: rdrand contract ok" : "random-entropy: absent provider closed");
    return 0;
}
