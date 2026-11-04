#include <fcntl.h>
#include <spawn.h>
#include <stdio.h>
#include <sys/wait.h>
int main(void) {
    posix_spawn_file_actions_t a;
    char *args[] = {"/fat/bin/bc", "-e", "1+1", 0};
    pid_t child; int status;
    if (posix_spawn_file_actions_init(&a) != 0) return 1;
    if (posix_spawn_file_actions_addopen(&a, 1, "/fat/bin/bc", O_RDONLY, 0) != 0) return 1;
    if (posix_spawn(&child, args[0], &a, 0, args, 0) != 0) return 1;
    posix_spawn_file_actions_destroy(&a);
    if (waitpid(child, &status, 0) != child || !WIFEXITED(status) || WEXITSTATUS(status) == 0) return 1;
    puts("bc-output-error: PASS");
    return 0;
}
