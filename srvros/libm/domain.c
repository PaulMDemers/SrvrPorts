/* Guard acosh's domain before the pinned musl algorithm: for large negative
 * inputs, cancellation in the upstream formula can yield a positive log input.
 * The musl invalid helpers preserve the invalid operation/NaN semantics. */
#include <math.h>
double ORIG_ACOSH(double);
float ORIG_ACOSHF(float);
double INVALID_DOUBLE(double);
float INVALID_FLOAT(float);
double acosh(double x) { return x < 1.0 ? INVALID_DOUBLE(x) : ORIG_ACOSH(x); }
float acoshf(float x) { return x < 1.0f ? INVALID_FLOAT(x) : ORIG_ACOSHF(x); }
