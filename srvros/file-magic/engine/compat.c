/* Time formatting required by libmagic's optional timestamp descriptions. */
#include <time.h>
char *asctime_r(const struct tm *t, char *dst)
{
    return strftime(dst, 26, "%a %b %e %H:%M:%S %Y\n", t) ? dst : 0;
}
char *ctime_r(const time_t *t, char *dst)
{
    struct tm value;
    return localtime_r(t, &value) ? asctime_r(&value, dst) : 0;
}
