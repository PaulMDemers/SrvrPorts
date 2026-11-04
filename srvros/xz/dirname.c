#include <string.h>
char *dirname(char *path) {
 static char dot[]="."; if (!path || !*path) return dot;
 size_t n=strlen(path); while (n>1 && path[n-1]=='/') path[--n]=0;
 while (n && path[n-1]!='/') --n; if (!n) return dot;
 while (n>1 && path[n-1]=='/') --n; path[n]=0; return path;
}
