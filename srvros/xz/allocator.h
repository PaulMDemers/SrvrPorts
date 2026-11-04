#ifndef SRVROS_XZ_ALLOCATOR_H
#define SRVROS_XZ_ALLOCATOR_H
#include <stddef.h>
#include <lzma.h>
extern const lzma_allocator srv_xz_allocator;
/* Reset diagnostics only with no live allocations; SIZE_MAX disables injection. */
int srv_xz_allocator_reset(size_t fail_after);
void srv_xz_allocator_limit(size_t bytes);
size_t srv_xz_allocator_live(void);
size_t srv_xz_allocator_bytes(void);
size_t srv_xz_allocator_peak(void);
size_t srv_xz_allocator_attempts(void);
#endif
