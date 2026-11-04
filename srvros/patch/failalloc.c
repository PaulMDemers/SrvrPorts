/* Diagnostic only: fail allocations from the selected zero-based call index. */
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
void *__real_malloc(size_t);
void *__real_calloc(size_t,size_t);
void *__real_realloc(void *,size_t);
static size_t calls, fail_at=SIZE_MAX;
static int initialized, reported;
static int fail(const char *marker,size_t length) {
 if (!initialized) {
  initialized=1;
  const char *p=getenv("SRV_PATCH_FAIL_AFTER");
  if (p && *p) {
   size_t n=0; int valid=1;
   for (;*p;p++) {
    if (*p<'0'||*p>'9'||n>(SIZE_MAX-(size_t)(*p-'0'))/10) { valid=0; break; }
    n=n*10+(size_t)(*p-'0');
   }
   if (valid) fail_at=n;
  }
 }
 if (calls++ >= fail_at) {
  if (!reported) { reported=1; (void)write(2,marker,length); }
  errno=ENOMEM; return 1;
 }
 return 0;
}
void *__wrap_malloc(size_t n) { return fail("PATCH_ALLOC_FAIL malloc\n",sizeof("PATCH_ALLOC_FAIL malloc\n")-1)?NULL:__real_malloc(n); }
void *__wrap_calloc(size_t n,size_t z) { return fail("PATCH_ALLOC_FAIL calloc\n",sizeof("PATCH_ALLOC_FAIL calloc\n")-1)?NULL:__real_calloc(n,z); }
void *__wrap_realloc(void *p,size_t n) { return fail("PATCH_ALLOC_FAIL realloc\n",sizeof("PATCH_ALLOC_FAIL realloc\n")-1)?NULL:__real_realloc(p,n); }
