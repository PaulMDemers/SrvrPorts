#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "sqlite3.h"

#include <sys/random.h>
#include <time.h>

#define SQLITEDEMO_PATH_MAX 512

struct srv_sqlite_file {
    sqlite3_file base;
    int fd;
    int delete_on_close;
    int lock_level;
    int main_db;
    char path[SQLITEDEMO_PATH_MAX];
};

/* The shell must not close any descriptor for the locked inode: POSIX
 * record locks are process-owned and any such close releases them. */
#define SHELL_FILES_MAX 32
static FILE *shell_files[SHELL_FILES_MAX];
static int shell_deferred[SHELL_FILES_MAX];
static int deferred_fds[SHELL_FILES_MAX];
static struct stat main_stat, journal_dir_stat;
static char journal_path[SQLITEDEMO_PATH_MAX], journal_name[SQLITEDEMO_PATH_MAX];

/* Resolve the journal's parent by inode, so ./, .. and directory aliases
 * cannot bypass the guard. Non-ASCII names in that directory are refused:
 * exFAT uses Unicode case folding that ASCII strcasecmp cannot reproduce. */
static int journal_key(const char *db) {
    char full[SQLITEDEMO_PATH_MAX], dir[SQLITEDEMO_PATH_MAX];
    const char *slash;
    int n;
    if (db[0] == '/') n = snprintf(full, sizeof(full), "%s-journal", db);
    else {
        char cwd[SQLITEDEMO_PATH_MAX];
        if (!getcwd(cwd, sizeof(cwd))) return 0;
        n = snprintf(full, sizeof(full), "%s%s%s-journal", cwd,
                     strcmp(cwd, "/") ? "/" : "", db);
    }
    if (n < 0 || n >= (int)sizeof(full)) return 0;
    slash = strrchr(full, '/');
    if (!slash || !slash[1]) return 0;
    memcpy(journal_path, full, (size_t)n + 1);
    memcpy(journal_name, slash + 1, strlen(slash + 1) + 1);
    size_t length = (size_t)(slash - full);
    if (!length) length = 1;
    memcpy(dir, full, length); dir[length] = 0;
    return stat(dir, &journal_dir_stat) == 0;
}
static int ascii_equal_or_nonascii(const char *a, const char *b) {
    int equal = 1;
    while (*a || *b) {
        unsigned char ca = (unsigned char)*a, cb = (unsigned char)*b;
        if (ca >= 128 || cb >= 128) return 1;
        if (ca >= 'A' && ca <= 'Z') ca += 'a' - 'A';
        if (cb >= 'A' && cb <= 'Z') cb += 'a' - 'A';
        if (ca != cb) equal = 0;
        if (*a) ++a;
        if (*b) ++b;
    }
    return equal;
}
static int same_inode(const struct stat *a, const struct stat *b) {
    return a->st_dev == b->st_dev && a->st_ino == b->st_ino;
}
static int protected_inode(const struct stat *st) {
    struct stat journal;
    if (same_inode(st, &main_stat)) return 1;
    if (stat(journal_path, &journal) == 0) return same_inode(st, &journal);
    return errno != ENOENT;
}
static int protected_path(const char *path) {
    struct stat st, parent;
    char dir[SQLITEDEMO_PATH_MAX];
    const char *slash = strrchr(path, '/');
    if (stat(path, &st) == 0) {
        if (protected_inode(&st)) return 1;
    } else if (errno != ENOENT) return 1;
    size_t length = slash ? (size_t)(slash - path) : 0;
    if (length >= sizeof(dir)) return 1;
    if (!slash) { dir[0] = '.'; dir[1] = 0; }
    else if (!length) { dir[0] = '/'; dir[1] = 0; }
    else { memcpy(dir, path, length); dir[length] = 0; }
    if (stat(dir, &parent) != 0) return 1;
    return same_inode(&parent, &journal_dir_stat) &&
           ascii_equal_or_nonascii(slash ? slash + 1 : path, journal_name);
}
static int shell_has_inode(const struct stat *st) {
    for (int fd = 0; fd < 3; ++fd) {
        struct stat f;
        if (fstat(fd, &f) == 0) {
            if (same_inode(st, &f)) return 1;
        } else if (errno != EBADF) return 1;
    }
    for (int i = 0; i < SHELL_FILES_MAX; ++i) {
        struct stat f;
        if (shell_files[i] && (fstat(fileno(shell_files[i]), &f) != 0 || same_inode(st, &f))) return 1;
    }
    return 0;
}
static void shell_release_deferred(void) {
    for (int i = 0; i < SHELL_FILES_MAX; ++i) {
        if (deferred_fds[i]) { close(deferred_fds[i] - 1); deferred_fds[i] = 0; }
    }
    for (int i = 0; i < SHELL_FILES_MAX; ++i) {
        if (shell_deferred[i]) {
            fclose(shell_files[i]);
            shell_files[i] = NULL;
            shell_deferred[i] = 0;
        }
    }
}
static int active_main;

FILE *srv_shell_fopen(const char *path, const char *mode) {
    struct stat st;
    int i;
    for (i = 0; i < SHELL_FILES_MAX && shell_files[i]; ++i) {}
    if (i == SHELL_FILES_MAX) { errno = EMFILE; return NULL; }
    /* Check before fopen so a write mode cannot truncate the database. */
    if (active_main) {
        if (protected_path(path)) {
            errno = EACCES; return NULL;
        }
    }
    FILE *f = fopen(path, mode);
    if (!f) return NULL;
    shell_files[i] = f;
    if (active_main && (fstat(fileno(f), &st) != 0 || protected_inode(&st))) {
        shell_deferred[i] = 1;
        errno = EACCES; return NULL;
    }
    return f;
}
int srv_shell_unlink(const char *path) {
    if (active_main) {
        if (protected_path(path)) {
            errno = EACCES;
            return -1;
        }
    }
    return unlink(path);
}
int srv_shell_fclose(FILE *f) {
    for (int i = 0; i < SHELL_FILES_MAX; ++i) {
        if (shell_files[i] == f) {
            struct stat st;
            if (active_main && (fstat(fileno(f), &st) != 0 || protected_inode(&st))) {
                shell_deferred[i] = 1;
                return 0;
            }
            shell_files[i] = NULL;
            return fclose(f);
        }
    }
    return fclose(f);
}

static int srv_sqlite_close(sqlite3_file *file) {
    struct srv_sqlite_file *srv_file = (struct srv_sqlite_file *)file;
    int rc = SQLITE_OK;
    if (srv_file->fd >= 0) {
        if (close(srv_file->fd) != 0) rc = SQLITE_IOERR_CLOSE;
        srv_file->fd = -1;
    }
    if (srv_file->delete_on_close && srv_file->path[0] != '\0') {
        if (unlink(srv_file->path) != 0 && errno != ENOENT) rc = SQLITE_IOERR_DELETE;
    }
    if (srv_file->main_db) { active_main = 0; shell_release_deferred(); }
    return rc;
}

static int srv_sqlite_read(sqlite3_file *file, void *buffer, int amount, sqlite3_int64 offset) {
    struct srv_sqlite_file *srv_file = (struct srv_sqlite_file *)file;
    unsigned char *out = (unsigned char *)buffer;
    int total = 0;
    while (total < amount) {
        ssize_t n = pread(srv_file->fd, out + total, (size_t)(amount - total), offset + total);
        if (n < 0) {
            return SQLITE_IOERR_READ;
        }
        if (n == 0) {
            memset(out + total, 0, (size_t)(amount - total));
            return SQLITE_IOERR_SHORT_READ;
        }
        total += (int)n;
    }
    return SQLITE_OK;
}

static int srv_sqlite_write(sqlite3_file *file, const void *buffer, int amount, sqlite3_int64 offset) {
    struct srv_sqlite_file *srv_file = (struct srv_sqlite_file *)file;
    const unsigned char *in = (const unsigned char *)buffer;
    int total = 0;
    while (total < amount) {
        ssize_t n = pwrite(srv_file->fd, in + total, (size_t)(amount - total), offset + total);
        if (n <= 0) {
            return SQLITE_IOERR_WRITE;
        }
        total += (int)n;
    }
    return SQLITE_OK;
}

static int srv_sqlite_truncate(sqlite3_file *file, sqlite3_int64 size) {
    struct srv_sqlite_file *srv_file = (struct srv_sqlite_file *)file;
    return ftruncate(srv_file->fd, (off_t)size) == 0 ? SQLITE_OK : SQLITE_IOERR_TRUNCATE;
}

static int srv_sqlite_sync(sqlite3_file *file, int flags) {
    (void)flags;
    struct srv_sqlite_file *srv_file = (struct srv_sqlite_file *)file;
    return fsync(srv_file->fd) == 0 ? SQLITE_OK : SQLITE_IOERR_FSYNC;
}

static int srv_sqlite_file_size(sqlite3_file *file, sqlite3_int64 *size) {
    struct srv_sqlite_file *srv_file = (struct srv_sqlite_file *)file;
    struct stat st;
    if (fstat(srv_file->fd, &st) != 0) {
        return SQLITE_IOERR_FSTAT;
    }
    *size = (sqlite3_int64)st.st_size;
    return SQLITE_OK;
}

static int srv_sqlite_file_lock(struct srv_sqlite_file *srv_file,
    short type,
    off_t start,
    off_t length) {
    struct flock lock;
    lock.l_type = type;
    lock.l_whence = SEEK_SET;
    lock.l_start = start;
    lock.l_len = length;
    lock.l_pid = 0;
    return fcntl(srv_file->fd, F_SETLK, &lock) == 0 ? SQLITE_OK : SQLITE_BUSY;
}

/* The main database holds a write lock for its entire lifetime. SQLite's
 * lock transitions are logical only; never release the underlying lock early. */
static int srv_sqlite_lock(sqlite3_file *file, int lock) {
    struct srv_sqlite_file *f = (struct srv_sqlite_file *)file;
    if (lock > f->lock_level) f->lock_level = lock;
    return SQLITE_OK;
}
static int srv_sqlite_unlock(sqlite3_file *file, int lock) {
    struct srv_sqlite_file *f = (struct srv_sqlite_file *)file;
    f->lock_level = lock;
    return SQLITE_OK;
}
static int srv_sqlite_check_reserved_lock(sqlite3_file *file, int *reserved) {
    struct srv_sqlite_file *f = (struct srv_sqlite_file *)file;
    *reserved = f->lock_level >= SQLITE_LOCK_RESERVED;
    return SQLITE_OK;
}

static int srv_sqlite_file_control(sqlite3_file *file, int op, void *arg) {
    (void)file;
    (void)op;
    (void)arg;
    return SQLITE_NOTFOUND;
}

static int srv_sqlite_sector_size(sqlite3_file *file) {
    (void)file;
    return 4096;
}

static int srv_sqlite_device_characteristics(sqlite3_file *file) {
    (void)file;
    return 0;
}

static const sqlite3_io_methods srv_sqlite_io = {
    1,
    srv_sqlite_close,
    srv_sqlite_read,
    srv_sqlite_write,
    srv_sqlite_truncate,
    srv_sqlite_sync,
    srv_sqlite_file_size,
    srv_sqlite_lock,
    srv_sqlite_unlock,
    srv_sqlite_check_reserved_lock,
    srv_sqlite_file_control,
    srv_sqlite_sector_size,
    srv_sqlite_device_characteristics,
    0,
    0,
    0,
    0,
    0,
    0
};

static int srv_sqlite_open(sqlite3_vfs *vfs, const char *name,
    sqlite3_file *file, int flags, int *out_flags) {
    struct srv_sqlite_file *f = (struct srv_sqlite_file *)file;
    int fd = -1, open_flags, is_main = (flags & SQLITE_OPEN_MAIN_DB) != 0;
    int defer_slot = -1;
    char temp[SQLITEDEMO_PATH_MAX];
    (void)vfs;
    memset(f, 0, sizeof(*f));
    f->fd = -1;
    if (is_main && active_main) return SQLITE_CANTOPEN;
    if (!is_main && active_main) {
        for (int i = 0; i < SHELL_FILES_MAX; ++i) if (!deferred_fds[i]) { defer_slot = i; break; }
        if (defer_slot < 0) return SQLITE_CANTOPEN;
    }
    if (name) {
        struct stat st;
        if (stat(name, &st) == 0 &&
            ((is_main && shell_has_inode(&st)) || (!is_main && active_main && same_inode(&st, &main_stat))))
            return SQLITE_CANTOPEN;
    }
    if (!name) {
        unsigned char random[16];
        for (int attempt = 0; attempt < 32; ++attempt) {
            if (getrandom(random, sizeof(random), 0) != (ssize_t)sizeof(random)) return SQLITE_CANTOPEN;
            int n = snprintf(temp, sizeof(temp), "/fat/.sqlite-%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                random[0],random[1],random[2],random[3],random[4],random[5],random[6],random[7],random[8],random[9],random[10],random[11],random[12],random[13],random[14],random[15]);
            if (n < 0 || n >= (int)sizeof(temp)) return SQLITE_CANTOPEN;
            fd = open(temp, O_RDWR | O_CREAT | O_EXCL, 0600);
            if (fd >= 0) break;
            if (errno != EEXIST) return SQLITE_CANTOPEN;
        }
        if (fd < 0) return SQLITE_CANTOPEN;
        name = temp;
        f->delete_on_close = 1;
    } else {
        if (strlen(name) >= sizeof(f->path)) return SQLITE_CANTOPEN;
        open_flags = (is_main || (flags & SQLITE_OPEN_READWRITE)) ? O_RDWR : O_RDONLY;
        if (flags & SQLITE_OPEN_CREATE) open_flags |= O_CREAT;
        if (flags & SQLITE_OPEN_EXCLUSIVE) open_flags |= O_EXCL;
        fd = open(name, open_flags, 0666);
        if (fd < 0) return SQLITE_CANTOPEN;
        f->delete_on_close = (flags & SQLITE_OPEN_DELETEONCLOSE) != 0;
    }
    if (defer_slot >= 0) {
        struct stat st;
        if (fstat(fd, &st) != 0 || same_inode(&st, &main_stat)) {
            deferred_fds[defer_slot] = fd + 1;
            return SQLITE_CANTOPEN;
        }
    }
    f->fd = fd;
    if (is_main) {
        /* Reserve before opening any other descriptor in this process. */
        if (srv_sqlite_file_lock(f, F_WRLCK, 0, 0) != SQLITE_OK) {
            close(fd);
            return SQLITE_BUSY;
        }
        struct stat journal_st;
        if (fstat(fd, &main_stat) != 0 || shell_has_inode(&main_stat) || !journal_key(name)) {
            close(fd);
            return SQLITE_CANTOPEN;
        }
        int journal_rc = stat(journal_path, &journal_st);
        if ((journal_rc == 0 && shell_has_inode(&journal_st)) ||
            (journal_rc != 0 && errno != ENOENT)) {
            close(fd);
            return SQLITE_CANTOPEN;
        }
        active_main = 1;
        f->main_db = 1;
    }
    memcpy(f->path, name, strlen(name) + 1);
    if (out_flags) *out_flags = flags;
    f->base.pMethods = &srv_sqlite_io;
    return SQLITE_OK;
}

static int srv_sqlite_delete(sqlite3_vfs *vfs, const char *path, int sync_dir) {
    (void)vfs;
    (void)sync_dir;
    return unlink(path) == 0 || errno == ENOENT ? SQLITE_OK : SQLITE_IOERR_DELETE;
}

static int srv_sqlite_access(sqlite3_vfs *vfs, const char *path, int flags, int *result) {
    (void)vfs;
    int mode = F_OK;
    if (flags == SQLITE_ACCESS_READWRITE) {
        mode = R_OK | W_OK;
    } else if (flags == SQLITE_ACCESS_READ) {
        mode = R_OK;
    }
    *result = access(path, mode) == 0;
    return SQLITE_OK;
}

static int srv_sqlite_full_pathname(sqlite3_vfs *vfs, const char *path, int out_size, char *out) {
    char cwd[SQLITEDEMO_PATH_MAX];
    int n;
    (void)vfs;
    if (!path || !*path || out_size <= 0) return SQLITE_CANTOPEN;
    if (*path == '/') n = snprintf(out, (size_t)out_size, "%s", path);
    else {
        if (!getcwd(cwd, sizeof(cwd))) return SQLITE_CANTOPEN;
        n = snprintf(out, (size_t)out_size, "%s%s%s", cwd,
                     strcmp(cwd, "/") ? "/" : "", path);
    }
    return n < 0 || n >= out_size ? SQLITE_CANTOPEN : SQLITE_OK;
}

static void *srv_sqlite_dl_open(sqlite3_vfs *vfs, const char *path) {
    (void)vfs;
    (void)path;
    return 0;
}

static void srv_sqlite_dl_error(sqlite3_vfs *vfs, int n, char *message) {
    (void)vfs;
    if (n > 0) {
        message[0] = '\0';
    }
}

static void (*srv_sqlite_dl_sym(sqlite3_vfs *vfs, void *handle, const char *symbol))(void) {
    (void)vfs;
    (void)handle;
    (void)symbol;
    return 0;
}

static void srv_sqlite_dl_close(sqlite3_vfs *vfs, void *handle) {
    (void)vfs;
    (void)handle;
}

static int srv_sqlite_randomness(sqlite3_vfs *vfs, int n, char *out) {
    int done = 0;
    (void)vfs;
    while (done < n) {
        ssize_t got = getrandom(out + done, (size_t)(n - done), 0);
        if (got <= 0) break;
        done += (int)got;
    }
    return done;
}

static int srv_sqlite_sleep(sqlite3_vfs *vfs, int microseconds) {
    (void)vfs;
    if (microseconds > 0) {
        usleep((unsigned int)microseconds);
    }
    return microseconds;
}

static int srv_sqlite_current_time_int64(sqlite3_vfs *vfs, sqlite3_int64 *now);
static int srv_sqlite_current_time(sqlite3_vfs *vfs, double *now) {
    sqlite3_int64 ms;
    int rc = srv_sqlite_current_time_int64(vfs, &ms);
    if (rc == SQLITE_OK) *now = (double)ms / 86400000.0;
    return rc;
}

static int srv_sqlite_get_last_error(sqlite3_vfs *vfs, int n, char *message) {
    (void)vfs;
    if (n > 0 && message != 0) {
        message[0] = '\0';
    }
    return 0;
}

static int srv_sqlite_current_time_int64(sqlite3_vfs *vfs, sqlite3_int64 *now) {
    (void)vfs;
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) return SQLITE_ERROR;
    *now = 210866760000000LL + (sqlite3_int64)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
    return SQLITE_OK;
}

static sqlite3_vfs srv_sqlite_vfs = {
    3,
    sizeof(struct srv_sqlite_file),
    SQLITEDEMO_PATH_MAX,
    0,
    "srvros",
    0,
    srv_sqlite_open,
    srv_sqlite_delete,
    srv_sqlite_access,
    srv_sqlite_full_pathname,
    srv_sqlite_dl_open,
    srv_sqlite_dl_error,
    srv_sqlite_dl_sym,
    srv_sqlite_dl_close,
    srv_sqlite_randomness,
    srv_sqlite_sleep,
    srv_sqlite_current_time,
    srv_sqlite_get_last_error,
    srv_sqlite_current_time_int64,
    0,
    0,
    0
};

int sqlite3_os_init(void) {
    return sqlite3_vfs_register(&srv_sqlite_vfs, 1);
}

int sqlite3_os_end(void) {
    return SQLITE_OK;
}

