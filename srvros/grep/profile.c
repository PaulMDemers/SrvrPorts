/* SrvrOS admission boundary for GNU grep.  See the port manifest for scope. */
#include "profile.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <locale.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INPUT_LIMIT (256u * 1024u)
#define LINE_LIMIT 65536u
#define LINES_LIMIT 8192u
#define PATTERN_LIMIT 1024u

const char *srvr_grep_program_name = "ggrep";
static volatile sig_atomic_t pending_signal;
/* The SDK dispatches INT/TERM legacy handlers cooperatively. Its dispatch
   entry is used outside I/O so CPU-only GNU work also observes delivery. */
extern int __posix_signal_dispatch_pending(void);

static void record_signal(int sig) { if (!pending_signal) pending_signal = sig; }
void srvr_grep_checkpoint(void) {
    static unsigned steps;
    if (++steps == 1024) {
        int saved = errno;
        steps = 0;
        (void)__posix_signal_dispatch_pending();
        errno = saved;
    }
    int sig = pending_signal;
    /* The kernel closes the unlinked snapshot and descriptors. Avoid flushing
       buffered stdout during cancellation, which can reblock on a full pipe. */
    if (sig) _exit(128 + sig);
}

/* Checkpoints run outside handlers and preserve the I/O call's errno. */
static void io_checkpoint(void) {
    int saved = errno;
    srvr_grep_checkpoint();
    errno = saved;
}
static int checked_close(int fd) {
    int result = close(fd);
    io_checkpoint();
    return result;
}
static int checked_fclose(FILE *stream) {
    int result = fclose(stream);
    io_checkpoint();
    return result;
}

static int fail(const char *message) {
    fprintf(stderr, "ggrep: %s\n", message);
    return 2;
}
static int syserror(const char *name) {
    fprintf(stderr, "ggrep: %s: %s\n", name, strerror(errno));
    return 2;
}
static void help(void) {
    puts("Usage: ggrep [-FGEivnqc] [--] PATTERN [FILE]\n"
         "SrvrOS bounded GNU grep 3.12: C locale, one pattern and at most one\n"
         "regular file (or finite standard input). Pattern: at most 1024 bytes,\n"
         "without newline. Input: at most 262144 bytes, 8192 lines, and 65536\n"
         "bytes per line including newline; NUL bytes are not supported.\n"
         "Options: -F fixed strings, -G basic regex (default), -E extended\n"
         "regex, -i ignore case, -v invert, -n line number, -q quiet, -c count.\n"
         "Use -- before a pattern beginning with '-'.");
}

/* Capture and validate the exact bytes later given to GNU grep.  A snapshot
   also makes -q inspect the whole input before it can report success. */
static int capture(int input, FILE *snapshot) {
    unsigned char buf[4096];
    size_t total = 0, lines = 0, line = 0;
    for (;;) {
        srvr_grep_checkpoint();
        /* Read one byte past the bound to distinguish a full input from an
           over-limit one, without consuming an unbounded stream. */
        size_t want = sizeof buf;
        if (want > INPUT_LIMIT + 1u - total) want = INPUT_LIMIT + 1u - total;
        ssize_t got = read(input, buf, want);
        io_checkpoint();
        if (got < 0) {
            if (errno == EINTR) continue;
            return syserror("input");
        }
        if (!got) break;
        srvr_grep_progress('I');
        srvr_grep_checkpoint();
        total += (size_t)got;
        if (total > INPUT_LIMIT) return fail("input exceeds 262144 bytes");
        for (ssize_t i = 0; i < got; ++i) {
            if (!buf[i]) return fail("NUL byte in input is unsupported");
            if (!line && ++lines > LINES_LIMIT) return fail("input exceeds 8192 lines");
            if (++line > LINE_LIMIT) return fail("input line exceeds 65536 bytes");
            if (buf[i] == '\n') line = 0;
        }
        size_t written = fwrite(buf, 1, (size_t)got, snapshot);
        io_checkpoint();
        if (written != (size_t)got || ferror(snapshot)) return syserror("temporary snapshot");
    }
    int rc = fflush(snapshot);
    io_checkpoint();
    if (rc) return syserror("temporary snapshot");
    rc = fseek(snapshot, 0, SEEK_SET);
    io_checkpoint();
    if (rc) return syserror("temporary snapshot");
    return 0;
}

int main(int argc, char **argv) {
    if (argc > 0 && argv[0][0]) srvr_grep_program_name = argv[0];
    const char *pattern = NULL, *file = NULL;
    unsigned flags = 0;
    const char *const options = "FGEivnqc";
    int end_options = 0;
    struct sigaction action;
    memset(&action, 0, sizeof action);
    action.sa_handler = record_signal;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGINT, &action, NULL) || sigaction(SIGTERM, &action, NULL))
        return syserror("signal setup");
    if (argc == 2 && !strcmp(argv[1], "--help")) {
        help();
        int rc = fflush(stdout); io_checkpoint();
        return rc || ferror(stdout) ? syserror("standard output") : 0;
    }
    if (argc == 2 && !strcmp(argv[1], "--version")) {
        puts("ggrep (GNU grep) 3.12\nSrvrOS bounded profile; use --help for supported scope.");
        int rc = fflush(stdout); io_checkpoint();
        return rc || ferror(stdout) ? syserror("standard output") : 0;
    }
    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (!end_options && !strcmp(arg, "--")) { end_options = 1; continue; }
        if (!end_options && arg[0] == '-' && arg[1]) {
            for (const char *p = arg + 1; *p; ++p) {
                const char *found = strchr(options, *p);
                if (!found) return fail("unsupported option (see --help)");
                flags |= 1u << (found - options);
            }
        } else if (!pattern) pattern = arg;
        else if (!file) file = arg;
        else return fail("only one pattern and one input are supported");
    }
    if ((flags & 7u) && ((flags & 7u) & ((flags & 7u) - 1u)))
        return fail("conflicting matchers specified");
    if (!pattern) return fail("missing pattern (see --help)");
    if (strlen(pattern) > PATTERN_LIMIT || strchr(pattern, '\n'))
        return fail("pattern exceeds 1024 bytes or contains newline");
    /* argv strings cannot contain embedded NUL bytes. */
    if (setenv("LC_ALL", "C", 1) || !setlocale(LC_ALL, "C"))
        return fail("cannot set C locale");
    int input = STDIN_FILENO, own_input = 0;
    if ((!file || !strcmp(file, "-")) && fcntl(input, F_GETFD) < 0)
        return syserror("standard input");
    if (file && strcmp(file, "-")) {
        struct stat st;
        input = open(file, O_RDONLY | O_NONBLOCK);
        if (input < 0) return syserror(file);
        own_input = 1;
        if (fstat(input, &st)) { int result = syserror(file); checked_close(input); return result; }
        if (!S_ISREG(st.st_mode)) { checked_close(input); return fail("input pathname is not a regular file"); }
    }
    FILE *snapshot = tmpfile(); /* SDK tmpfile unlinks its name before returning. */
    io_checkpoint();
    if (!snapshot) { int result = syserror("temporary snapshot"); if (own_input) checked_close(input); return result; }
    int result = capture(input, snapshot);
    if (own_input && checked_close(input) && !result) result = syserror("input close");
    if (result) { checked_fclose(snapshot); return result; }
    srvr_grep_checkpoint();
    int snapshot_fd = fileno(snapshot);
    /* In normal operation stdin or the opened input owns descriptor zero.
       Still handle an unusual zero descriptor without closing the duplicate. */
    if (snapshot_fd == STDIN_FILENO) {
        snapshot_fd = dup(snapshot_fd);
        io_checkpoint();
        if (snapshot_fd < 0) {
            result = syserror("temporary snapshot"); checked_fclose(snapshot); return result;
        }
        if (checked_fclose(snapshot)) {
            result = syserror("temporary snapshot close"); checked_close(snapshot_fd); return result;
        }
        snapshot = NULL;
    }
    int copied = dup2(snapshot_fd, STDIN_FILENO);
    io_checkpoint();
    if (copied < 0) {
        result = syserror("temporary snapshot");
        if (snapshot) checked_fclose(snapshot); else checked_close(snapshot_fd);
        return result;
    }
    if (snapshot ? checked_fclose(snapshot) : checked_close(snapshot_fd))
        return syserror("temporary snapshot close");
    char optargs[8][3];
    char *core_argv[12];
    int core_argc = 0;
    core_argv[core_argc++] = argv[0];
    for (unsigned i = 0; i < 8; ++i) if (flags & (1u << i)) {
        optargs[i][0] = '-'; optargs[i][1] = options[i]; optargs[i][2] = '\0';
        core_argv[core_argc++] = optargs[i];
    }
    core_argv[core_argc++] = "--";
    core_argv[core_argc++] = (char *)pattern;
    core_argv[core_argc] = NULL;
    return srvr_grep_main(core_argc, core_argv);
}
