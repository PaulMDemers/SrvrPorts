#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Only options whose semantics have been qualified are admitted. */
static int takes_value(const char *s) {
 return !strcmp(s,"-M") || !strcmp(s,"--memlimit") || !strcmp(s,"--memlimit-compress") || !strcmp(s,"--memlimit-decompress");
}
void srv_xz_guard(int argc, char **argv) {
 if ((getenv("XZ_DEFAULTS") && *getenv("XZ_DEFAULTS")) || (getenv("XZ_OPT") && *getenv("XZ_OPT"))) {
  fputs("xz: XZ_DEFAULTS and XZ_OPT are unsupported in this profile\n",stderr); exit(1);
 }
 int positional=0;
 for (int i=1;i<argc;i++) {
  const char *s=argv[i];
  if (positional || s[0]!='-' || !s[1]) continue;
  if (!strcmp(s,"--help") || !strcmp(s,"-h")) {
   puts("Usage: xz [-0] [-d|-t] [-c] [--no-adjust] [--memlimit-compress=N] [--memlimit-decompress=N] [FILE...]\nSrvrOS profile: .xz only, preset -0 by default, files require -c or -t; output is never replaced. Codec limits: 8 MiB encode, 32 MiB decode."); exit(0);
  }
  if (!strcmp(s,"--")) { positional=1; continue; }
  if (takes_value(s)) { if (++i==argc) { fputs("xz: missing memory limit\n",stderr); exit(1); } continue; }
  if (!strncmp(s,"--memlimit=",11) || !strncmp(s,"--memlimit-compress=",20) || !strncmp(s,"--memlimit-decompress=",22)) continue;
  if (!strcmp(s,"--no-adjust") || !strcmp(s,"--stdout") || !strcmp(s,"--decompress") || !strcmp(s,"--test") || !strcmp(s,"--keep") || !strcmp(s,"--help") || !strcmp(s,"--version")) continue;
  if (s[1]!='-') { const char *p=s+1; while (*p && strchr("0dctkV",*p)) p++; if (!*p) continue; }
  fprintf(stderr,"xz: unsupported option in SrvrOS profile: %s\n",s); exit(1);
 }
}
