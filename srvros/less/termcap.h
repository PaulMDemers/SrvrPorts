#ifndef SRVROS_LESS_TERMCAP_H
#define SRVROS_LESS_TERMCAP_H
int tgetent(char *, const char *);
int tgetflag(const char *);
int tgetnum(const char *);
char *tgetstr(const char *, char **);
char *tgoto(const char *, int, int);
int tputs(const char *, int, int (*)(int));
#endif
