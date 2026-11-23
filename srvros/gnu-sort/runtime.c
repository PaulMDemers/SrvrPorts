#include "profile.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <srvros/sys.h>

#define HEAP_PAGE 4096u
#define HEAP_LIMIT (16u * 1024u * 1024u)

long __real_srv_sbrk(int64_t increment, uint64_t *previous_out);

#ifdef SRV_GSORT_DIAGNOSTIC
static uint64_t high_water;
static int repeat_sorting;
static uint64_t heap_limit = HEAP_LIMIT;
#endif

/* Cache the diagnostic limit before the driver makes allocation calls.  The
   wrapper itself must not consult the environment or allocate recursively. */
void gsort_runtime_init(void) {
#ifdef SRV_GSORT_DIAGNOSTIC
    const char *value = getenv("SRV_GSORT_MAX_HEAP");
    if (value && !strcmp(value, "0")) heap_limit = 0;
    value = getenv("SRV_GSORT_REPEAT");
    repeat_sorting = value && !strcmp(value, "1");
#endif
}

long __wrap_srv_sbrk(int64_t increment, uint64_t *previous_out) {
    static uint64_t base;
    uint64_t current;
    if (increment <= 0) return __real_srv_sbrk(increment, previous_out);
    long status = __real_srv_sbrk(0, &current);
    if (status < 0) return status;
    if (!base) base = current;
    uint64_t limit = HEAP_LIMIT;
#ifdef SRV_GSORT_DIAGNOSTIC
    limit = heap_limit;
#endif
    /* The initial break is page aligned and the kernel maps whole pages.
       A page-aligned ceiling therefore bounds the rounded mapping as well as
       the logical break; keep the original base if the break later shrinks. */
    if (current < base || current - base > limit ||
        (uint64_t)increment > limit - (current - base)) {
#ifdef SRV_GSORT_DIAGNOSTIC
        static const char marker[] = "GSORT_HEAP_LIMIT\n";
        (void)write(STDERR_FILENO, marker, sizeof marker - 1);
#endif
        return SRV_ERR_NOMEM;
    }
    status = __real_srv_sbrk(increment, previous_out);
#ifdef SRV_GSORT_DIAGNOSTIC
    if (status >= 0) {
        uint64_t used = current - base + (uint64_t)increment;
        used = (used + HEAP_PAGE - 1) / HEAP_PAGE * HEAP_PAGE;
        if (used > high_water) high_water = used;
    }
#endif
    return status;
}

void gsort_heap_report(void) {
#ifdef SRV_GSORT_DIAGNOSTIC
    const char *enabled = getenv("SRV_GSORT_HEAP");
    if (enabled && !strcmp(enabled, "1")) {
        char line[64];
        int len = snprintf(line, sizeof line, "GSORT_HEAP=%llu\n",
                           (unsigned long long)high_water);
        if (len > 0 && (size_t)len < sizeof line)
            (void)write(STDERR_FILENO, line, (size_t)len);
    }
#endif
}

unsigned gsort_sort_iterations(void) {
#ifdef SRV_GSORT_DIAGNOSTIC
    if (repeat_sorting) return 32768;
#endif
    return 1;
}

void gsort_checkpoint(void) {
    static unsigned steps;
    if (++steps == 1024) {
        steps = 0;
        gsort_poll();
    }
#ifdef SRV_GSORT_DIAGNOSTIC
    /* Mark the first real rule, then continue on the production polling path. */
    static int marked;
    if (repeat_sorting && !marked) {
        marked = 1;
        static const char marker[] = "GSORT_SORT_READY\n";
        (void)write(STDERR_FILENO, marker, sizeof marker - 1);
    }
#endif
}
