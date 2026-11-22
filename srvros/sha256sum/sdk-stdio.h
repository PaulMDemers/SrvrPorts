#ifndef SRVROS_SHA256SUM_SDK_STDIO_H
#define SRVROS_SHA256SUM_SDK_STDIO_H

#include <config.h>
#include <stdio.h>

/* The selected command uses the SDK's FILE implementation and its integer
   and string formats directly. Gnulib's target guesses would instead use
   replacements for libc-specific FILE internals and unused printf formats. */
#undef fclose
#undef printf
#undef vfprintf
#undef vprintf

#endif
