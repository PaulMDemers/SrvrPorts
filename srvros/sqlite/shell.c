/* Wrap shell streams and connections to uphold the single-database contract. */
#include <stdio.h>
#include <string.h>
#include "sqlite3.h"
FILE *srv_shell_fopen(const char *, const char *);
int srv_shell_fclose(FILE *);
int srv_shell_unlink(const char *);
/* Compose the safety authorizer with the shell's optional authorizer. */
#define SRV_AUTH_SLOTS 64
struct srv_auth { sqlite3 *db; int (*callback)(void *, int, const char *, const char *, const char *, const char *); void *arg; };
static struct srv_auth srv_auths[SRV_AUTH_SLOTS];
static int srv_auth_cb(void *ctx, int action, const char *a, const char *b, const char *c, const char *d) {
    struct srv_auth *s = (struct srv_auth *)ctx;
    if (action == SQLITE_ATTACH) return SQLITE_DENY;
    if (action == SQLITE_PRAGMA && a && b) {
        if (sqlite3_stricmp(a, "journal_mode") == 0 && sqlite3_stricmp(b, "delete") != 0) return SQLITE_DENY;
        if (sqlite3_stricmp(a, "synchronous") == 0 && sqlite3_stricmp(b, "full") != 0 && strcmp(b, "2") != 0) return SQLITE_DENY;
    }
    return s->callback ? s->callback(s->arg, action, a, b, c, d) : SQLITE_OK;
}
static int srv_shell_set_authorizer(sqlite3 *db, int (*callback)(void *, int, const char *, const char *, const char *, const char *), void *arg) {
    struct srv_auth *slot = 0;
    for (int i = 0; i < SRV_AUTH_SLOTS; ++i) {
        if (srv_auths[i].db == db) { slot = &srv_auths[i]; break; }
        if (!slot && !srv_auths[i].db) slot = &srv_auths[i];
    }
    if (!slot) return SQLITE_NOMEM;
    slot->db = db; slot->callback = callback; slot->arg = arg;
    return sqlite3_set_authorizer(db, srv_auth_cb, slot);
}
static int srv_shell_close(sqlite3 *db) {
    int rc = sqlite3_close(db);
    if (rc == SQLITE_OK) for (int i = 0; i < SRV_AUTH_SLOTS; ++i)
        if (srv_auths[i].db == db) { memset(&srv_auths[i], 0, sizeof(srv_auths[i])); break; }
    return rc;
}
static int srv_shell_open(const char *name, sqlite3 **db) {
    int rc = sqlite3_open(name, db);
    if (*db) { sqlite3_limit(*db, SQLITE_LIMIT_ATTACHED, 0); if (srv_shell_set_authorizer(*db, 0, 0) != SQLITE_OK) return SQLITE_NOMEM; }
    return rc;
}
static int srv_shell_open_v2(const char *name, sqlite3 **db, int flags, const char *vfs) {
    int rc = sqlite3_open_v2(name, db, flags, vfs);
    if (*db) { sqlite3_limit(*db, SQLITE_LIMIT_ATTACHED, 0); if (srv_shell_set_authorizer(*db, 0, 0) != SQLITE_OK) return SQLITE_NOMEM; }
    return rc;
}
#define unlink srv_shell_unlink
#define sqlite3_set_authorizer srv_shell_set_authorizer
#define sqlite3_close srv_shell_close
#define fopen srv_shell_fopen
#define fclose srv_shell_fclose
#define sqlite3_open srv_shell_open
#define sqlite3_open_v2 srv_shell_open_v2
#include "../../upstream/sqlite/shell.c"
