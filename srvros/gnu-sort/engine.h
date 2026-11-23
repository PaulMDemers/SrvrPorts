#ifndef GSORT_ENGINE_H
#define GSORT_ENGINE_H
#include <stddef.h>
struct gsort_line { char *text; size_t length; };
struct gsort_options { int numeric, reverse, unique, stable; unsigned field; unsigned char delimiter; };
void gsort_checkpoint(void);
void gsort_order(struct gsort_line *, struct gsort_line *, size_t, const struct gsort_options *);
int gsort_compare(const struct gsort_line *, const struct gsort_line *, const struct gsort_options *);
#endif
