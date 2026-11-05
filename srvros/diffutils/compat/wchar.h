#ifndef SRVROS_DIFF_WCHAR_COMPAT_H
#define SRVROS_DIFF_WCHAR_COMPAT_H
#include_next <wchar.h>
/* C standard concatenation, built from the SDK's wide-string primitives. */
static inline wchar_t *wcscat(wchar_t *dest, const wchar_t *src) {
 wmemcpy(dest + wcslen(dest), src, wcslen(src) + 1);
 return dest;
}
#endif
