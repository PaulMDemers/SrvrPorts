/* Host-side contract checks; compile this file alone with the upstream header.
 * No SQLite engine or guest kernel is needed. */
#include <assert.h>
#include <stdlib.h>
#include <sys/wait.h>
#ifdef __APPLE__
#include <sys/types.h>
static ssize_t host_getrandom(void *buf, size_t len, unsigned int flags) {
    (void)flags;
    arc4random_buf(buf, len);
    return (ssize_t)len;
}
#define getrandom host_getrandom
#endif
#include "vfs.c"

int sqlite3_vfs_register(sqlite3_vfs *vfs, int makeDefault) {
    (void)vfs; (void)makeDefault; return SQLITE_OK;
}

int main(void) {
    char dir[] = "/tmp/srvsqlite-test-XXXXXX", path[512], cwd[512], resolved[512];
    struct srv_sqlite_file db, duplicate;
    sqlite3_int64 now;
    struct timespec ts;
    int status, flags = SQLITE_OPEN_MAIN_DB | SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
    assert(mkdtemp(dir));
    assert(chdir(dir) == 0 && getcwd(cwd, sizeof(cwd)));
    assert(srv_sqlite_full_pathname(&srv_sqlite_vfs, "test.db", sizeof(resolved), resolved) == SQLITE_OK);
    assert(snprintf(path, sizeof(path), "%s/test.db", cwd) > 0 && strcmp(path, resolved) == 0);
    assert(srv_sqlite_full_pathname(&srv_sqlite_vfs, path, 3, resolved) == SQLITE_CANTOPEN);
    assert(srv_sqlite_current_time_int64(&srv_sqlite_vfs, &now) == SQLITE_OK);
    assert(clock_gettime(CLOCK_REALTIME, &ts) == 0);
    assert(llabs(now - (210866760000000LL + (sqlite3_int64)ts.tv_sec * 1000 + ts.tv_nsec / 1000000)) < 2000);
    assert(srv_sqlite_open(&srv_sqlite_vfs, path, &db.base, flags, NULL) == SQLITE_OK);
    assert(srv_sqlite_open(&srv_sqlite_vfs, path, &duplicate.base, flags, NULL) != SQLITE_OK);
    assert(link(path, "alias.db") == 0);
    assert(srv_shell_fopen("alias.db", "w") == NULL);
    assert(srv_shell_fopen("./test.db-journal", "w") == NULL);
    assert(srv_shell_fopen("./TEST.DB-JOURNAL", "w") == NULL);
    assert(srv_shell_unlink("./test.db-journal") == -1 && errno == EACCES);
    FILE *journal = fopen("test.db-journal", "w");
    assert(journal && fclose(journal) == 0 && link("test.db-journal", "journal-alias") == 0);
    assert(srv_shell_fopen("journal-alias", "w") == NULL);
    assert(srv_shell_unlink("journal-alias") == -1 && errno == EACCES);
    assert(unlink("journal-alias") == 0 && unlink("test.db-journal") == 0);
    assert(srv_shell_unlink("alias.db") == -1 && errno == EACCES);
    assert(srv_sqlite_open(&srv_sqlite_vfs, "alias.db", &duplicate.base,
                           SQLITE_OPEN_MAIN_JOURNAL | SQLITE_OPEN_READWRITE, NULL) != SQLITE_OK);
    pid_t child = fork();
    assert(child >= 0);
    if (!child) {
        active_main = 0;
        _exit(srv_sqlite_open(&srv_sqlite_vfs, path, &duplicate.base, flags, NULL) == SQLITE_BUSY ? 0 : 1);
    }
    assert(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    assert(srv_sqlite_close(&db.base) == SQLITE_OK);
    child = fork();
    assert(child >= 0);
    if (!child) {
        int fd = open(path, O_RDONLY);
        if (fd < 0 || dup2(fd, 0) < 0) _exit(2);
        if (fd != 0) close(fd);
        _exit(srv_sqlite_open(&srv_sqlite_vfs, path, &duplicate.base, flags, NULL) == SQLITE_CANTOPEN ? 0 : 1);
    }
    assert(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    assert(srv_sqlite_delete(&srv_sqlite_vfs, dir, 0) == SQLITE_IOERR_DELETE);
    assert(srv_sqlite_open(&srv_sqlite_vfs, path, &db.base, flags, NULL) == SQLITE_OK);
    assert(close(db.fd) == 0);
    assert(srv_sqlite_sync(&db.base, 0) == SQLITE_IOERR_FSYNC);
    assert(srv_sqlite_close(&db.base) == SQLITE_IOERR_CLOSE);
    if (access("/fat", W_OK) == 0) {
        struct srv_sqlite_file t1, t2;
        int tf = SQLITE_OPEN_TEMP_DB | SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_DELETEONCLOSE;
        assert(srv_sqlite_open(&srv_sqlite_vfs, NULL, &t1.base, tf, NULL) == SQLITE_OK);
        assert(srv_sqlite_open(&srv_sqlite_vfs, NULL, &t2.base, tf, NULL) == SQLITE_OK);
        assert(strcmp(t1.path, t2.path));
        assert(srv_sqlite_close(&t1.base) == SQLITE_OK && srv_sqlite_close(&t2.base) == SQLITE_OK);
    }
    FILE *journal_open = srv_shell_fopen("test.db-journal", "w");
    assert(journal_open);
    assert(srv_sqlite_open(&srv_sqlite_vfs, path, &db.base, flags, NULL) != SQLITE_OK);
    assert(srv_shell_fclose(journal_open) == 0 && unlink("test.db-journal") == 0);
    FILE *f = srv_shell_fopen(path, "r");
    assert(f);
    assert(srv_sqlite_open(&srv_sqlite_vfs, path, &db.base, flags, NULL) != SQLITE_OK);
    assert(srv_shell_fclose(f) == 0);
    assert(unlink("alias.db") == 0);
    assert(unlink(path) == 0 && chdir("/") == 0 && rmdir(dir) == 0);
    return 0;
}
