#ifndef SRVROS_AWK_CONFIG_H
#define SRVROS_AWK_CONFIG_H
#include <wchar.h>
#include <stdio.h>
/* Awk uses this as an allocation increment, not an OS descriptor limit. */
#ifndef FOPEN_MAX
#define FOPEN_MAX 20
#endif
long srvros_awk_random(void);
void srvros_awk_srandom(unsigned long seed);
int srvros_awk_wctomb(char *out, wchar_t wc);
#endif
