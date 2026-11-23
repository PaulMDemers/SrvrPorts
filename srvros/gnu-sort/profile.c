#include "profile.h"
#include "engine.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define INPUT_LIMIT (256u * 1024u)
#define RECORD_LIMIT 8192u
#define LINE_LIMIT (64u * 1024u)
extern int __posix_signal_dispatch_pending(void);
static volatile sig_atomic_t pending_signal;

static void record_signal(int sig) {
    if (!pending_signal) pending_signal = sig;
}
void gsort_poll(void) {
    int saved = errno;
    (void)__posix_signal_dispatch_pending();
    if (pending_signal) _exit(128 + pending_signal);
    errno = saved;
}
static int fail(const char *message) {
    gsort_poll();
    fprintf(stderr, "gsort: %s\n", message);
    return 2;
}
static int syserror(const char *name) {
    int saved = errno;
    gsort_poll();
    fprintf(stderr, "gsort: %s: %s\n", name, strerror(saved));
    return 2;
}
static int field(const char *s, unsigned *out) {
    unsigned n = 0, m = 0;
    if (*s < '0' || *s > '9') return -1;
    while (*s >= '0' && *s <= '9') {
        if (n > (UINT_MAX - (unsigned)(*s - '0')) / 10u) return -1;
        n = n * 10u + (unsigned)(*s++ - '0');
    }
    if (!n || *s++ != ',' || *s < '0' || *s > '9') return -1;
    while (*s >= '0' && *s <= '9') {
        if (m > (UINT_MAX - (unsigned)(*s - '0')) / 10u) return -1;
        m = m * 10u + (unsigned)(*s++ - '0');
    }
    if (*s || m != n) return -1;
    *out = n;
    return 0;
}
static int write_all(int fd, const char *buf, size_t length) {
    while (length) {
        ssize_t n = write(fd, buf, length);
        gsort_poll();
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) { if (!n) errno = EIO; return -1; }
        buf += n;
        length -= (size_t)n;
    }
    return 0;
}
int main(int argc, char **argv) {
    struct gsort_options options = {0};
    const char *name = NULL;
    int end_options = 0, key_seen = 0, delim_seen = 0;
    struct sigaction action;
    gsort_runtime_init();
    memset(&action, 0, sizeof action);
    action.sa_handler = record_signal;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGINT, &action, NULL) || sigaction(SIGTERM, &action, NULL))
        return syserror("signal setup");
    action.sa_handler = SIG_IGN;
    if (sigaction(SIGPIPE, &action, NULL)) return syserror("signal setup");
#ifdef SRV_GSORT_DIAGNOSTIC
    (void)atexit(gsort_heap_report);
#endif
    if (argc == 2 && !strcmp(argv[1], "--help")) {
        static const char help[] =
            "Usage: gsort [-nrsu] [-t CHAR] [-k N,N] [--] [FILE|-]\n"
            "Sort a regular file or finite standard input in the C locale.\n"
            "Limits: 256 KiB input, 8192 records, 64 KiB per record including LF.\n";
        if (write_all(STDOUT_FILENO, help, sizeof help - 1))
            return syserror("standard output");
        int rc = close(STDOUT_FILENO);
        gsort_poll();
        return rc ? syserror("standard output") : 0;
    }
    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (!end_options && !strcmp(arg, "--")) {
            end_options = 1;
            continue;
        }
        if (!end_options && arg[0] == '-' && arg[1]) {
            if (arg[1] == '-') return fail("unsupported option");
            for (const char *p = arg + 1; *p; ++p) {
                if (*p == 'n') options.numeric = 1;
                else if (*p == 'r') options.reverse = 1;
                else if (*p == 's') options.stable = 1;
                else if (*p == 'u') options.unique = 1;
                else if (*p == 't' || *p == 'k') {
                    char opt = *p;
                    const char *value = p[1] ? p + 1 : (++i < argc ? argv[i] : NULL);
                    if (!value) return fail("option requires an argument");
                    if (opt == 't') {
                        if (delim_seen++ || !value[0] || value[1]) return fail("invalid delimiter");
                        options.delimiter = (unsigned char)value[0];
                    } else if (key_seen++ || field(value, &options.field)) return fail("invalid key");
                    break;
                } else return fail("unsupported option");
            }
        } else {
            if (name) return fail("at most one input is supported");
            name = arg;
        }
    }
    int fd = STDIN_FILENO;
    if (name && strcmp(name, "-")) {
        fd = open(name, O_RDONLY | O_NONBLOCK);
        gsort_poll();
        if (fd < 0) return syserror(name);
        struct stat st;
        if (fstat(fd, &st)) {
            int e = errno;
            (void)close(fd);
            errno = e;
            return syserror(name);
        }
        if (!S_ISREG(st.st_mode)) {
            (void)close(fd);
            return fail("input is not a regular file");
        }
    }
    char *input = malloc(INPUT_LIMIT + 1u);
    struct gsort_line *lines = malloc(RECORD_LIMIT * sizeof *lines);
    struct gsort_line *scratch = malloc(RECORD_LIMIT * sizeof *scratch);
    if (!input || !lines || !scratch) {
        if (fd != STDIN_FILENO) (void)close(fd);
        return fail("out of memory");
    }
    size_t size = 0;
    while (size <= INPUT_LIMIT) {
        ssize_t n = read(fd, input + size, INPUT_LIMIT + 1u - size);
        gsort_poll();
        if (n < 0) {
            if (errno == EINTR) continue;
            int e = errno;
            if (fd != STDIN_FILENO) (void)close(fd);
            errno = e;
            return syserror(name ? name : "standard input");
        }
        if (!n) break;
        size += (size_t)n;
    }
    if (fd != STDIN_FILENO) {
        int rc = close(fd);
        gsort_poll();
        if (rc) return syserror(name);
    }
    if (size > INPUT_LIMIT) return fail("input exceeds limit");
    size_t count = 0, start = 0;
    for (size_t i = 0; i < size; ++i) {
        if (!input[i]) return fail("NUL in input");
        if (input[i] == '\n') {
            if (i - start >= LINE_LIMIT || count == RECORD_LIMIT) return fail("record limit exceeded");
            input[i] = 0;
            lines[count++] = (struct gsort_line){input + start, i - start};
            start = i + 1;
        } else if (i - start >= LINE_LIMIT) return fail("record limit exceeded");
    }
    if (start < size) {
        if (count == RECORD_LIMIT) return fail("record limit exceeded");
        input[size] = 0;
        lines[count++] = (struct gsort_line){input + start, size - start};
    }
    unsigned iterations = gsort_sort_iterations();
    gsort_poll();
    for (unsigned i = 0; i < iterations; ++i)
        gsort_order(lines, scratch, count, &options);
    gsort_poll();
#ifdef SRV_GSORT_DIAGNOSTIC
    if (iterations > 1) {
        static const char done[] = "GSORT_SORT_FINISHED\n";
        (void)write(STDERR_FILENO, done, sizeof done - 1);
    }
#endif
    for (size_t i = 0; i < count; ++i) {
        if (options.unique && i && !gsort_compare(&lines[i - 1], &lines[i], &options)) continue;
        if (write_all(STDOUT_FILENO, lines[i].text, lines[i].length) ||
            write_all(STDOUT_FILENO, "\n", 1))
            return syserror("standard output");
    }
    int rc = close(STDOUT_FILENO);
    gsort_poll();
    if (rc) return syserror("standard output");
    return 0;
}
