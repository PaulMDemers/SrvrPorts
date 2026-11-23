#include "profile.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <magic.h>

#define INPUT_LIMIT (256u * 1024u)
static unsigned char input_buffer[INPUT_LIMIT];
extern unsigned char file_magic_db_start[], file_magic_db_end[];
extern int __posix_signal_dispatch_pending(void);
static volatile sig_atomic_t pending_signal;

static void record_signal(int sig) {
    if (!pending_signal) pending_signal = sig;
}
void file_magic_poll(void) {
    int saved = errno;
    (void)__posix_signal_dispatch_pending();
    if (pending_signal) _exit(128 + pending_signal);
    errno = saved;
}
static int fail(const char *message) {
    file_magic_poll();
    fprintf(stderr, "file-magic: %s\n", message);
    return 2;
}
static int syserror(const char *name) {
    int saved = errno;
    file_magic_poll();
    fprintf(stderr, "file-magic: %s: %s\n", name, strerror(saved));
    return 2;
}

int main(int argc, char **argv) {
    file_magic_runtime_init();
    const char *name = NULL;
    int brief = 0, mime = 0, end_options = 0;
    struct sigaction action;
    memset(&action, 0, sizeof action);
    action.sa_handler = record_signal;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGINT, &action, NULL) || sigaction(SIGTERM, &action, NULL))
        return syserror("signal setup");
    action.sa_handler = SIG_IGN;
    if (sigaction(SIGPIPE, &action, NULL)) return syserror("signal setup");
#ifdef SRV_FILE_MAGIC_DIAGNOSTIC
    (void)atexit(file_magic_heap_report);
#endif
    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (!end_options && !strcmp(arg, "--")) {
            end_options = 1;
            continue;
        }
        if (!end_options && !strcmp(arg, "-b")) {
            brief = 1;
            continue;
        }
        if (!end_options && !strcmp(arg, "--mime-type")) {
            mime = 1;
            continue;
        }
        if (!end_options && arg[0] == '-' && arg[1]) return fail("unsupported option");
        if (name) return fail("exactly one file is required");
        name = arg;
    }
    file_magic_poll();
    if (!name) return fail("usage: file-magic [-b] [--mime-type] [--] FILE");
    int fd = open(name, O_RDONLY | O_NONBLOCK);
    file_magic_poll();
    if (fd < 0) return syserror(name);
    struct stat st;
    int stat_result = fstat(fd, &st);
    file_magic_poll();
    if (stat_result) {
        int saved = errno;
        (void)close(fd);
        file_magic_poll();
        errno = saved;
        return syserror(name);
    }
    if (!S_ISREG(st.st_mode)) {
        (void)close(fd);
        file_magic_poll();
        return fail("input is not a regular file");
    }
    size_t size = 0;
    while (size < INPUT_LIMIT) {
        ssize_t n = read(fd, input_buffer + size, INPUT_LIMIT - size);
        file_magic_poll();
        if (n < 0) {
            if (errno == EINTR) continue;
            int saved = errno;
            (void)close(fd);
            file_magic_poll();
            errno = saved;
            return syserror(name);
        }
        if (!n) break;
        size += (size_t)n;
    }
    int closed = close(fd);
    file_magic_poll();
    if (closed) return syserror(name);
    int flags = MAGIC_NO_CHECK_COMPRESS | MAGIC_NO_CHECK_ELF | MAGIC_NO_CHECK_CDF;
    if (mime) flags |= MAGIC_MIME_TYPE;
    magic_t ms = magic_open(flags);
    file_magic_poll();
    if (!ms) return fail("cannot initialize magic");
    void *db = file_magic_db_start;
    size_t db_size = (uintptr_t)file_magic_db_end - (uintptr_t)file_magic_db_start;
    int load_status = magic_load_buffers(ms, &db, &db_size, 1);
    file_magic_poll();
    if (load_status) {
        fprintf(stderr, "file-magic: database: %s\n", magic_error(ms) ? magic_error(ms) : "load failed");
        magic_close(ms);
        return 2;
    }
    unsigned iterations = file_magic_match_iterations();
    const char *description = NULL;
    for (unsigned i = 0; i < iterations; ++i) {
        description = magic_buffer(ms, input_buffer, size);
        if (!description) break;
    }
#ifdef SRV_FILE_MAGIC_DIAGNOSTIC
    if (iterations > 1) fputs("FILE_MAGIC_MATCH_FINISHED\n", stderr);
#endif
    file_magic_poll();
    if (!description) {
        fprintf(stderr, "file-magic: classify: %s\n", magic_error(ms) ? magic_error(ms) : "failed");
        magic_close(ms);
        return 2;
    }
    int rc = brief ? printf("%s\n", description) : printf("%s: %s\n", name, description);
    if (rc >= 0) rc = fflush(stdout);
    file_magic_poll();
    int output_error = rc < 0 || ferror(stdout);
    magic_close(ms);
    if (output_error) return syserror("standard output");
    return 0;
}
