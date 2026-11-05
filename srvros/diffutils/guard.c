/* SrvrOS command-line profile gate; the comparison remains GNU diff. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
static void help(void) {
 puts("Usage: diff [-q|-u] [-L LABEL] [-L LABEL] FILE1 FILE2\n"
      "Compare two regular text files with normal, quiet, or three-line unified output.\n"
      "Limits per file: 256 KiB, 8192 lines, 64 KiB per line including newline; NUL is unsupported.\n"
      "Exit status: 0 same, 1 different, 2 trouble.");
}
static void done_output(void) { if (fflush(stdout) == EOF || ferror(stdout)) exit(2); exit(0); }
static void bad(const char *s) { fprintf(stderr,"diff: unsupported or invalid argument: %s\n", s); exit(2); }
void srv_diff_guard(int argc, char **argv) {
 int end=0, operands=0, labels=0;
 for (int i=1;i<argc;i++) {
  const char *s=argv[i];
  if (!end && !strcmp(s,"--")) { end=1; continue; }
  if (!end && s[0]=='-' && s[1]) {
   if (!strcmp(s,"--help")) { help(); done_output(); }
   if (!strcmp(s,"--version")) { puts("diff (GNU diffutils) 3.12\nCopyright (C) 2025 Free Software Foundation, Inc.\nLicense GPLv3+: GNU GPL version 3 or later <https://gnu.org/licenses/gpl.html>.\nThis is free software: you are free to change and redistribute it.\nThere is NO WARRANTY, to the extent permitted by law."); done_output(); }
   if (!strcmp(s,"-L")) { if (++labels>2 || ++i==argc) bad(s); continue; }
   if (s[1]=='L' && s[2]) { if (++labels>2) bad(s); continue; }
   if (s[1]!='-') { const char *q=s+1; while (*q && strchr("qu",*q)) ++q; if (!*q) continue; }
   bad(s);
  }
  if (!strcmp(s,"-")) bad(s);
  struct stat st;
  if (stat(s, &st) < 0) { perror(s); exit(2); }
  if (!S_ISREG(st.st_mode)) bad(s);
  ++operands;
 }
 if (operands != 2) bad("two regular file operands required");
}
