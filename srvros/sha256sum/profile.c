/* SrvrOS admission and bounded input around GNU coreutils sha256sum. */
#include "profile.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <srvros/path_limits.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MANIFEST_LIMIT (256u * 1024u)
#define RECORD_LIMIT 4096u
#define PATH_LIMIT (SRV_PATH_MAX - 1u)
_Static_assert(SRV_PATH_MAX == 1024, "revisit sha256sum path and record bounds for this SDK");
/* Optional escape prefix + digest + separators + twice the path + newline. */
#define LINE_LIMIT (1u + 64u + 2u + 2u * PATH_LIMIT + 2u)
static volatile sig_atomic_t pending;
extern int __posix_signal_dispatch_pending(void);
static void record_signal(int sig) {
    if (!pending)
        pending = sig;
}
void srvr_sha_checkpoint(void) {
    int saved = errno;
    (void)__posix_signal_dispatch_pending();
    errno = saved;
    if (pending)
        _exit(128 + pending);
}
void srvr_sha_write_error(void) {
    srvr_sha_checkpoint();
    int saved = errno;
    if (saved)
        fprintf(stderr, "sha256sum: write error: %s\n", strerror(saved));
    else
        fputs("sha256sum: write error\n", stderr);
    (void)fflush(stderr);
    srvr_sha_checkpoint();
    _exit(1);
}
static void die(const char *s) {
    fprintf(stderr, "sha256sum: %s\n", s);
    srvr_sha_checkpoint();
    exit(1);
}
void srvr_sha_init(int argc, char **argv) {
    struct sigaction action;
    memset(&action, 0, sizeof action);
    action.sa_handler = record_signal;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGINT, &action, NULL) || sigaction(SIGTERM, &action, NULL))
        die("cannot install signal handlers");
    int end = 0;
    for (int i = 1; i < argc; i++) {
        const char *s = argv[i];
        if (!end && !strcmp(s, "--")) {
            end = 1;
            continue;
        }
        if (!end && s[0] == '-' && s[1]) {
            if (!strcmp(s, "--help")) {
                puts("Usage: sha256sum [-b | -t] [--] [FILE]...\n"
                     "       sha256sum -c [--] MANIFEST\n"
                     "GNU coreutils 9.12; SrvrOS bounded profile. FILE or MANIFEST\n"
                     "may be - for finite standard input. Check mode accepts untagged\n"
                     "SHA256 records, up to 262144 bytes and 4096 records; paths are\n"
                     "limited to 1023 bytes. Only regular files are supported.");
                int failed = ferror(stdout) || fflush(stdout);
                srvr_sha_checkpoint();
                exit(failed ? 1 : 0);
            }
            if (!strcmp(s, "--version"))
                continue;
            if (s[1] == '-')
                die("unsupported option");
            for (s++; *s; s++)
                if (!strchr("btc", *s))
                    die("unsupported option");
        } else if (strlen(s) > PATH_LIMIT) {
            die("operand path exceeds 1023 bytes");
        }
    }
}
FILE *srvr_sha_open_regular(const char *name) {
    int fd = open(name, O_RDONLY | O_NONBLOCK);
    if (fd < 0)
        return NULL;
    struct stat st;
    if (fstat(fd, &st)) {
        int saved = errno;
        close(fd);
        errno = saved;
        return NULL;
    }
    if (!S_ISREG(st.st_mode)) {
        close(fd);
        errno = EINVAL;
        return NULL;
    }
    FILE *f = fdopen(fd, "r");
    if (!f) {
        int saved = errno;
        close(fd);
        errno = saved;
    }
    return f;
}
ssize_t srvr_sha_getline(char **line, size_t *capacity, FILE *f) {
    static size_t bytes, records;
    size_t n = 0;
    srvr_sha_checkpoint();
    if (!*line) {
        *line = malloc(LINE_LIMIT + 1);
        if (!*line)
            die("out of memory");
        *capacity = LINE_LIMIT + 1;
    }
    for (;;) {
        int c = fgetc(f);
        if (c == EOF) {
            srvr_sha_checkpoint();
            break;
        }
        if (++bytes > MANIFEST_LIMIT)
            die("manifest exceeds 262144 bytes");
        if (n == LINE_LIMIT)
            die("manifest record exceeds size limit");
        (*line)[n++] = (char)c;
        if (c == '\n')
            break;
    }
    srvr_sha_checkpoint();
    /* A read error invalidates even a nonempty partial final record.  Leave
       ferror set for the upstream caller's read-error reporting. */
    if (ferror(f) || !n)
        return -1;
    if (++records > RECORD_LIMIT)
        die("manifest exceeds 4096 records");
    (*line)[n] = '\0';
    return (ssize_t)n;
}
int srvr_sha_record_ok(const char *s, size_t n) {
    size_t i = 0, path = 0;
    int escaped = 0;
    if (n && s[0] == '\\') {
        escaped = 1;
        i++;
    }
    if (n < i + 67)
        return 0;
    for (size_t j = 0; j < 64; j++) {
        unsigned char c = (unsigned char)s[i++];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
              (c >= 'A' && c <= 'F')))
            return 0;
    }
    if (s[i++] != ' ' || (s[i] != ' ' && s[i] != '*'))
        return 0;
    i++;
    for (; i < n; i++) {
        if (!s[i])
            return 0;
        if (escaped && s[i] == '\\') {
            if (++i == n || (s[i] != '\\' && s[i] != 'n' && s[i] != 'r'))
                return 0;
        }
        if (++path > PATH_LIMIT)
            return 0;
    }
    return path != 0;
}

/* Gnulib diagnostics use the basename of its captured argv[0]. */
const char *getprogname(void) {
    extern const char *program_name;
    if (!program_name)
        return "sha256sum";
    const char *slash = strrchr(program_name, '/');
    return slash ? slash + 1 : program_name;
}
