/* The deskterm vt100 profile, derived from userspace/widgets/src/terminal.c. */
#include <string.h>
#include <stdio.h>
#include "termcap.h"
struct cap { const char *name, *value; };
static const struct cap strings[] = {
 {"cl", "\033[H\033[2J"}, {"cm", "\033[%i%d;%dH"},
 {"ce", "\033[K"}, {"cd", "\033[J"}, {"ho", "\033[H"},
 {"cr", "\r"}, {"bc", "\b"}, {"al", "\033[L"},
 {"so", "\033[7m"}, {"se", "\033[27m"}, {"us", "\033[4m"},
 {"ue", "\033[24m"}, {"md", "\033[1m"}, {"me", "\033[0m"},
 {"ti", "\033[?1049h"}, {"te", "\033[?1049l"},
 {"ku", "\033[A"}, {"kd", "\033[B"}, {"kr", "\033[C"}, {"kl", "\033[D"}
};
int tgetent(char *buf, const char *name) { (void)buf; return name && !strcmp(name,"vt100") ? 1 : 0; }
int tgetflag(const char *id) { return !strcmp(id,"am") || !strcmp(id,"xn") || !strcmp(id,"bs"); }
int tgetnum(const char *id) { (void)id; return -1; }
char *tgetstr(const char *id, char **area) {
 for (unsigned i=0; i<sizeof(strings)/sizeof(strings[0]); ++i) if (!strcmp(id, strings[i].name)) {
  char *result=*area; size_t n=strlen(strings[i].value)+1; memcpy(result,strings[i].value,n); *area+=n; return result;
 } return 0;
}
char *tgoto(const char *cap, int col, int row) {
 static char out[48]; if (!cap || strcmp(cap, "\033[%i%d;%dH")) return ""; snprintf(out,sizeof(out),"\033[%d;%dH",row+1,col+1); return out;
}
int tputs(const char *s, int affected, int (*put)(int)) { (void)affected; while (*s) if (put((unsigned char)*s++) < 0) return -1; return 0; }
