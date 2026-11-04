#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static void delay(void) { struct timespec t = {0, 200000000}; nanosleep(&t, 0); }
static int expect(int fd, const char *needle) {
    char buf[4096]; size_t n = 0;
    for (int i = 0; i < 50; i++) {
        struct pollfd p = {fd, POLLIN, 0};
        if (poll(&p, 1, 100) < 0) return 0;
        if (p.revents & POLLIN) {
            ssize_t r = read(fd, buf + n, sizeof(buf) - 1 - n);
            if (r <= 0) return 0;
            n += (size_t) r; buf[n] = 0;
            if (strstr(buf, needle)) return 1;
            if (n == sizeof(buf) - 1) return 0;
        }
        if (p.revents & (POLLERR | POLLHUP)) return 0;
    }
    return 0;
}
static int sendtext(int fd, const char *s) { size_t n = strlen(s); return write(fd, s, n) == (ssize_t)n; }
int main(void) {
    int in[2], out[2], status = 0, ok = 0; pid_t child;
    posix_spawn_file_actions_t a;
    char *args[] = {"/fat/bin/bc", "-i", "-q", "-P", 0};
    if (pipe(in) || pipe(out) || posix_spawn_file_actions_init(&a)) return 1;
    if (posix_spawn_file_actions_adddup2(&a, in[0], 0) ||
        posix_spawn_file_actions_adddup2(&a, out[1], 1) ||
        posix_spawn_file_actions_addclose(&a, in[1]) ||
        posix_spawn_file_actions_addclose(&a, out[0])) return 1;
    if (posix_spawn(&child, args[0], &a, 0, args, 0)) return 1;
    posix_spawn_file_actions_destroy(&a); close(in[0]); close(out[1]);
    if (!sendtext(in[1], "12345\n") || !expect(out[0], "12345\n")) goto done;
    if (!sendtext(in[1], "1+*2\n")) goto done;
    delay();
    if (!sendtext(in[1], "12348\n") || !expect(out[0], "12348\n")) goto done;
    if (!sendtext(in[1], "1/0\n")) goto done;
    delay();
    if (!sendtext(in[1], "12349\n") || !expect(out[0], "12349\n")) goto done;
    for (int i = 0; i < 2; i++) {
        delay();
        if (kill(child, SIGINT) || !expect(out[0], "ready for more input")) goto done;
        if (!sendtext(in[1], "12346\n") || !expect(out[0], "12346\n")) goto done;
    }
    for (int i = 0; i < 2; i++) {
        if (!sendtext(in[1], "while(1){}\n")) goto done;
        delay();
        if (kill(child, SIGINT)) goto done;
        if (!sendtext(in[1], "12347\n") || !expect(out[0], "12347\n")) goto done;
    }
    if (!sendtext(in[1], "12350\n") || !expect(out[0], "12350\n")) goto done;
    if (!sendtext(in[1], "while(1){}\n")) goto done;
    delay();
    {
        pid_t observed = waitpid(child, &status, WNOHANG);
        if (observed == child) { child = -1; goto done; }
        if (observed != 0) goto done;
    }
    if (kill(child, SIGTERM)) goto done;
    for (int i = 0; i < 50; i++) {
        if (waitpid(child, &status, WNOHANG) == child) { child = -1; ok = WIFEXITED(status) && WEXITSTATUS(status) == 0; break; }
        delay();
    }
done:
    if (child > 0) { kill(child, SIGKILL); waitpid(child, &status, 0); }
    close(in[1]); close(out[0]);
    if (!ok) return 1;
    puts("bc-signal: PASS"); return 0;
}
