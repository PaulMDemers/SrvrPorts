/* Pinned musl's x86_64/SSE math evaluation types and internal visibility. */
#ifndef SRVROS_MUSL_MATH_COMPAT_H
#define SRVROS_MUSL_MATH_COMPAT_H
#include <math.h>
#include <stdint.h>
typedef float float_t;
typedef double double_t;
#ifndef hidden
#define hidden __attribute__((visibility("hidden")))
#endif
double expm1(double);
float expm1f(float);
double log1p(double);
float log1pf(float);
#endif
