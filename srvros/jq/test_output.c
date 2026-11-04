#include <fcntl.h>
#include <spawn.h>
#include <stdio.h>
#include <sys/wait.h>

int main(void) {
    posix_spawn_file_actions_t actions;
    char *args[] = {"/fat/bin/jq", "-n", "-f", "/fat/jq-test/true.jq", 0};
    pid_t child;
    int status;
    if (posix_spawn_file_actions_init(&actions) != 0) return 1;
    if (posix_spawn_file_actions_addopen(&actions, 1, "/fat/jq-test/sales.json", O_RDONLY, 0) != 0) return 1;
    if (posix_spawn(&child, args[0], &actions, 0, args, 0) != 0) return 1;
    posix_spawn_file_actions_destroy(&actions);
    if (waitpid(child, &status, 0) != child || !WIFEXITED(status) || WEXITSTATUS(status) == 0) return 1;
    puts("jq-output-error: PASS");
    return 0;
}
