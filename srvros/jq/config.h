#ifndef SRVROS_JQ_CONFIG_H
#define SRVROS_JQ_CONFIG_H
#include <stddef.h>
#define USE_DECNUM 1
#define IEEE_8087 1
#define HAVE_GETENTROPY 1
#define HAVE_ISATTY 1
/* Omit upstream test-suite pthread stress; CLI profile is single-threaded. */
#define HAVE_PTHREAD_KEY_CREATE 1
#define HAVE_PTHREAD_ONCE 1
#define HAVE_ATEXIT 1
#define getentropy srv_jq_getentropy
#define tzname srv_jq_tzname
extern char *srv_jq_tzname[2];
int getentropy(void *, size_t);
#endif
