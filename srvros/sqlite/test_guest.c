/* Guest-only VFS contract probe. This translation unit includes the VFS so it
 * can exercise its methods directly without linking the SQLite engine. */
#include <spawn.h>
#include <sys/wait.h>
#include <time.h>
#include "vfs.c"

#define DB_PATH "/fat/sqlite-vfs-test.db"
#define SELF_PATH "/fat/sqlite-test/sqlite-vfs-test"
#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "sqlite-vfs-guest: FAIL line %d: %s (errno=%d)\n", \
            __LINE__, #expr, errno); return 1; } } while (0)

int sqlite3_vfs_register(sqlite3_vfs *vfs, int make_default) {
    (void)vfs; (void)make_default; return SQLITE_OK;
}

int main(int argc, char **argv) {
    int flags = SQLITE_OPEN_MAIN_DB | SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
    struct srv_sqlite_file db, other, tmp1, tmp2;
    if (argc > 1 && strcmp(argv[1], "child") == 0) {
        int rc = srv_sqlite_open(&srv_sqlite_vfs, DB_PATH, &db.base, flags, 0);
        if (rc == SQLITE_OK) srv_sqlite_close(&db.base);
        return rc == SQLITE_BUSY ? 0 : 1;
    }

    char cwd[SQLITEDEMO_PATH_MAX], absolute[SQLITEDEMO_PATH_MAX];
    sqlite3_int64 now;
    struct timespec ts;
    CHECK(getcwd(cwd, sizeof(cwd)) != 0);
    CHECK(chdir("/fat") == 0);
    CHECK(srv_sqlite_full_pathname(&srv_sqlite_vfs, "sqlite-vfs-test.db", sizeof(absolute), absolute) == SQLITE_OK);
    CHECK(strcmp(absolute, DB_PATH) == 0);
    CHECK(srv_sqlite_full_pathname(&srv_sqlite_vfs, DB_PATH, 4, absolute) == SQLITE_CANTOPEN);
    CHECK(srv_sqlite_current_time_int64(&srv_sqlite_vfs, &now) == SQLITE_OK);
    CHECK(clock_gettime(CLOCK_REALTIME, &ts) == 0);
    sqlite3_int64 expected = 210866760000000LL + (sqlite3_int64)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
    CHECK(now >= expected - 2000 && now <= expected + 2000);

    unlink(DB_PATH);
    CHECK(srv_sqlite_open(&srv_sqlite_vfs, DB_PATH, &db.base, flags, 0) == SQLITE_OK);
    CHECK(srv_sqlite_open(&srv_sqlite_vfs, "./sqlite-vfs-test.db", &other.base, flags, 0) != SQLITE_OK);
    CHECK(srv_shell_fopen("./sqlite-vfs-test.db", "w") == 0);
    CHECK(srv_shell_unlink("./sqlite-vfs-test.db") == -1 && errno == EACCES);
    CHECK(srv_sqlite_open(&srv_sqlite_vfs, "./sqlite-vfs-test.db", &other.base,
                          SQLITE_OPEN_MAIN_JOURNAL | SQLITE_OPEN_READWRITE, 0) != SQLITE_OK);
    pid_t child;
    int status;
    char *args[] = { SELF_PATH, "child", 0 };
    CHECK(posix_spawn(&child, SELF_PATH, 0, 0, args, 0) == 0);
    CHECK(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    CHECK(srv_sqlite_close(&db.base) == SQLITE_OK);

    int temp_flags = SQLITE_OPEN_TEMP_DB | SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_DELETEONCLOSE;
    CHECK(srv_sqlite_open(&srv_sqlite_vfs, 0, &tmp1.base, temp_flags, 0) == SQLITE_OK);
    CHECK(srv_sqlite_open(&srv_sqlite_vfs, 0, &tmp2.base, temp_flags, 0) == SQLITE_OK);
    CHECK(strcmp(tmp1.path, tmp2.path) != 0);
    CHECK(srv_sqlite_close(&tmp1.base) == SQLITE_OK);
    CHECK(srv_sqlite_close(&tmp2.base) == SQLITE_OK);

    CHECK(srv_sqlite_open(&srv_sqlite_vfs, DB_PATH, &db.base, flags, 0) == SQLITE_OK);
    CHECK(close(db.fd) == 0);
    CHECK(srv_sqlite_sync(&db.base, 0) == SQLITE_IOERR_FSYNC);
    CHECK(srv_sqlite_close(&db.base) == SQLITE_IOERR_CLOSE);
    CHECK(srv_sqlite_delete(&srv_sqlite_vfs, "/fat", 0) == SQLITE_IOERR_DELETE);
    CHECK(unlink(DB_PATH) == 0);
    CHECK(chdir(cwd) == 0);
    puts("sqlite-vfs-guest: PASS");
    return 0;
}
