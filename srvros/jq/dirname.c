#include <string.h>

char *srv_jq_dirname(char *path) {
    static char dot[] = ".";
    if (!path || !*path) return dot;
    size_t len = strlen(path);
    while (len > 1 && path[len - 1] == '/') path[--len] = '\0';
    while (len && path[len - 1] != '/') --len;
    if (!len) return dot;
    while (len > 1 && path[len - 1] == '/') --len;
    path[len] = '\0';
    return path;
}

/* SrvrOS calendar is UTC-only. */
static char utc[] = "UTC";
char *srv_jq_tzname[2] = { utc, utc };
