#ifndef SRVROS_DIFF_WCTYPE_COMPAT_H
#define SRVROS_DIFF_WCTYPE_COMPAT_H
#include_next <wctype.h>
/* SDK exposes the underlying named C-locale classifications. */
static inline int iswalnum(wint_t c) { return iswctype(c, wctype("alnum")); }
static inline int iswgraph(wint_t c) { return iswctype(c, wctype("graph")); }
#endif
