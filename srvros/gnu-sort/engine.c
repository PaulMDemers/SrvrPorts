/* C-locale bridge to the GNU Coreutils 9.12 sort algorithms.
   GPL-3.0-or-later; extracted routines retain their upstream provenance. */
#include "engine.h"
#include <stdbool.h>
#include <string.h>

#define c_isdigit(c) ((unsigned int)(unsigned char)(c) - '0' <= 9)
#include "numeric.inc"

static int blank(unsigned char c) { return c == ' ' || c == '\t'; }
static int bytecmp(const char *a, size_t na, const char *b, size_t nb)
{
    size_t n = na < nb ? na : nb;
    int d = n ? memcmp(a, b, n) : 0;
    return d ? d : (na > nb) - (na < nb);
}
/* GNU -k N,N includes the leading blanks in the selected field. */
static void field(const struct gsort_line *line, const struct gsort_options *o,
                  char **start, char **end)
{
    char *p = line->text, *limit = p + line->length;
    unsigned i;
    for (i = 1; i < o->field && p < limit; ++i) {
        if (o->delimiter) {
            char *sep = memchr(p, o->delimiter, (size_t)(limit - p));
            p = sep ? sep + 1 : limit;
        } else {
            while (p < limit && blank((unsigned char)*p)) ++p;
            while (p < limit && !blank((unsigned char)*p)) ++p;
        }
    }
    *start = p;
    if (o->delimiter) {
        char *sep = memchr(p, o->delimiter, (size_t)(limit - p));
        p = sep ? sep : limit;
    } else {
        while (p < limit && blank((unsigned char)*p)) ++p;
        while (p < limit && !blank((unsigned char)*p)) ++p;
    }
    *end = p;
}
int gsort_compare(const struct gsort_line *a, const struct gsort_line *b,
                  const struct gsort_options *o)
{
    int d = 0;
    gsort_checkpoint();
    if (o->field || o->numeric) {
        char *as = a->text, *ae = as + a->length;
        char *bs = b->text, *be = bs + b->length;
        if (o->field) { field(a, o, &as, &ae); field(b, o, &bs, &be); }
        if (o->numeric) {
            char ac = *ae, bc = *be;
            *ae = 0; *be = 0;
            while (blank((unsigned char)*as)) ++as;
            while (blank((unsigned char)*bs)) ++bs;
            d = numcompare(as, bs, '.', -1);
            *ae = ac; *be = bc;
        } else d = bytecmp(as, (size_t)(ae-as), bs, (size_t)(be-bs));
        if (d || o->stable || o->unique)
            return o->reverse ? (d < 0) - (d > 0) : d;
    }
    d = bytecmp(a->text, a->length, b->text, b->length);
    return o->reverse ? (d < 0) - (d > 0) : d;
}
static const struct gsort_options *active_options;
static int compare(const struct gsort_line *a, const struct gsort_line *b)
{ return gsort_compare(a, b, active_options); }
#include "merge.inc"
void gsort_order(struct gsort_line *lines, struct gsort_line *scratch,
                 size_t count, const struct gsort_options *options)
{
    if (count < 2) return;
    /* GNU's internal arrays are in reverse order. */
    for (size_t i = 0; i < count / 2; ++i) {
        struct gsort_line t = lines[i]; lines[i] = lines[count-1-i]; lines[count-1-i] = t;
    }
    active_options = options;
    sequential_sort(lines + count, count, scratch + count, false);
    for (size_t i = 0; i < count / 2; ++i) {
        struct gsort_line t = lines[i]; lines[i] = lines[count-1-i]; lines[count-1-i] = t;
    }
}
