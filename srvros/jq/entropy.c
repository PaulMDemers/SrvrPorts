#include <errno.h>
#include <stddef.h>
#include <sys/random.h>
#include <sys/types.h>

/* jq requests four bytes; handle short reads and EINTR as well. */
int srv_jq_getentropy(void *buffer, size_t length) {
    unsigned char *p = buffer;
    if (length > 256) { errno = EIO; return -1; }
    while (length) {
        ssize_t n = getrandom(p, length, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) { if (n == 0) errno = EIO; return -1; }
        p += n;
        length -= (size_t)n;
    }
    return 0;
}
