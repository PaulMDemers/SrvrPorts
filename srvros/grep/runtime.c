/* Heap admission and diagnostic progress for the single-threaded GNU port. */
#include "profile.h"
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <srvros/sys.h>
#include <srvros/syscall_numbers.h>

#define HEAP_PAGE 4096u
#define HEAP_LIMIT (16u * 1024u * 1024u)

long __real_srv_sbrk(int64_t increment, uint64_t *previous_out);

#ifdef SRV_GREP_DIAGNOSTIC
static uint64_t heap_limit(void) {
    const char *text = getenv("SRV_GREP_TEST_HEAP_LIMIT");
    uint64_t value = 0;
    if (!text || !*text) return HEAP_LIMIT;
    for (; *text; ++text) {
        if (*text < '0' || *text > '9' || value > HEAP_LIMIT / 10u) return HEAP_LIMIT;
        value = value * 10u + (unsigned)(*text - '0');
    }
    return value <= HEAP_LIMIT ? value : HEAP_LIMIT;
}
#endif

long __wrap_srv_sbrk(int64_t increment, uint64_t *previous_out) {
    static uint64_t base;
    uint64_t current;
    if (increment <= 0) return __real_srv_sbrk(increment, previous_out);
    long status = __real_srv_sbrk(0, &current);
    if (status < 0) return status;
    /* The loader's initial program break is page-aligned. Keep the ceiling
       relative to this address even if a later brk call lowers the break. */
    if (!base) base = current;
    uint64_t limit = HEAP_LIMIT;
#ifdef SRV_GREP_DIAGNOSTIC
    limit = heap_limit();
#endif
    limit -= limit % HEAP_PAGE;
    if (current < base || current - base > limit || (uint64_t)increment > limit - (current - base)) {
#ifdef SRV_GREP_DIAGNOSTIC
        static const char marker[] = "GREP_HEAP_LIMIT\n";
        (void)write(STDERR_FILENO, marker, sizeof marker - 1);
#endif
        return SRV_ERR_NOMEM;
    }
    return __real_srv_sbrk(increment, previous_out);
}

void srvr_grep_progress(int phase) {
#ifdef SRV_GREP_DIAGNOSTIC
    static int selected = -1;
    if (selected == -1) {
        const char *value = getenv("SRV_GREP_TEST_PAUSE");
        selected = value && value[0] && !value[1] ? value[0] : 0;
    }
    if (phase == selected) {
        selected = 0;
        char byte = (char)phase;
        if (write(3, &byte, 1) != 1) return;
        for (;;) {
            ssize_t result = read(4, &byte, 1);
            srvr_grep_checkpoint();
            if (result >= 0 || errno != EINTR) break;
        }
    }
#else
    (void)phase;
#endif
}
