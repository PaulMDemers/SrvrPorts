#include "allocator.h"
#include <stdint.h>
#include <sys/mman.h>
/* mmap returns page aligned storage; header preserves max_align_t alignment. */
union allocation_header { struct { size_t mapping, bytes; } v; max_align_t align; };
static size_t live, bytes, peak, attempts, fail_after = SIZE_MAX, limit = SIZE_MAX;
static void *allocate(void *opaque, size_t nmemb, size_t size) {
 (void)opaque;
 if (attempts++ >= fail_after || (size && nmemb > SIZE_MAX / size)) return NULL;
 size_t n = nmemb * size;
 if (!n) n = 1;
 if (n > SIZE_MAX - sizeof(union allocation_header) - 4095 || bytes > limit || n > limit - bytes) return NULL;
 size_t length = (n + sizeof(union allocation_header) + 4095) & ~(size_t)4095;
 union allocation_header *h = mmap(NULL,length,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
 if (h == MAP_FAILED) return NULL;
 h->v.mapping=length; h->v.bytes=n; ++live; bytes+=n; if (bytes>peak) peak=bytes;
 return h+1;
}
static void release(void *opaque, void *ptr) {
 (void)opaque; if (!ptr) return;
 union allocation_header *h=(union allocation_header *)ptr-1;
 size_t n=h->v.bytes, length=h->v.mapping;
 if (munmap(h,length)==0) { --live; bytes-=n; }
}
const lzma_allocator srv_xz_allocator = { allocate, release, NULL };
int srv_xz_allocator_reset(size_t fail) { if (live) return -1; bytes=peak=attempts=0; fail_after=fail; return 0; }
void srv_xz_allocator_limit(size_t n) { limit=n; }
size_t srv_xz_allocator_live(void) { return live; }
size_t srv_xz_allocator_bytes(void) { return bytes; }
size_t srv_xz_allocator_peak(void) { return peak; }
size_t srv_xz_allocator_attempts(void) { return attempts; }
