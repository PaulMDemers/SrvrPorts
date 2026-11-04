/* SrvrOS admission and immutable-input adapter; GNU Patch applies the hunks. */
#include "profile.h"
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#define SOURCE_MAX (256U*1024U)
#define PATCH_MAX (1024U*1024U)
#define SOURCE_LINES 8192U
#define PATCH_LINES 32768U
#define SOURCE_LINE (64U*1024U)
#define PATCH_LINE (128U*1024U)
#define PATH_BUF 1024
static int source_fd=-1, patch_fd=-1, output_fd=-1, owned, complete;
static char output_name[PATH_BUF];
static struct stat output_identity, source_identity;
static FILE *output_stream;
static size_t output_size, output_lines, output_column;
static void die(const char *message) { srv_patch_signal_checkpoint(); fprintf(stderr,"patch: %s\n",message); srv_patch_signal_checkpoint(); exit(2); }
static void close_fd(int *fd) { if (*fd >= 0) { close(*fd); *fd=-1; } }
void srv_patch_cleanup(void) {
 close_fd(&source_fd); close_fd(&patch_fd); close_fd(&output_fd);
 if (owned && !complete) {
  struct stat now;
  if (stat(output_name,&now)==0 && now.st_dev==output_identity.st_dev && now.st_ino==output_identity.st_ino)
   (void)unlink(output_name);
 }
 owned=0;
}
void srv_patch_success(void) { complete=1; }
int srv_patch_source_fd(void) { return source_fd; }
int srv_patch_patch_fd(void) { return patch_fd; }
int srv_patch_output_fd(void) { int fd=output_fd; output_fd=-1; return fd; }
void srv_patch_source_stat(struct stat *st) { *st=source_identity; }
void srv_patch_set_output(FILE *stream) {
 output_stream=stream;
#ifdef SRV_PATCH_DIAGNOSTIC
 extern void srv_patch_output_diagnostic(void);
 srv_patch_output_diagnostic();
#endif
}
void srv_patch_output_bytes(FILE *stream, const void *data, size_t n) {
 if (stream != output_stream) return;
 const unsigned char *p=data;
 for (size_t i=0;i<n;i++) {
  if (++output_size > SOURCE_MAX || !p[i] || ++output_column > SOURCE_LINE)
   die("output exceeds profile limits or contains NUL");
  if (p[i]=='\n') { ++output_lines; output_column=0; }
  if (output_lines+(output_column!=0) > SOURCE_LINES) die("output exceeds 8192 lines");
 }
}
static void write_all(int fd, const void *data, size_t size) {
 const unsigned char *p=data;
 while (size) { srv_patch_signal_checkpoint(); ssize_t n=write(fd,p,size); srv_patch_signal_checkpoint(); if (n<=0) die("snapshot write failed"); p+=n; size-=(size_t)n; }
}
static int temporary(const char *output, const char *suffix) {
 char path[PATH_BUF];
 if (snprintf(path,sizeof(path),"%s.%s.XXXXXX",output,suffix) >= (int)sizeof(path)) die("output path too long");
 int fd=mkstemp(path); if (fd<0) die("cannot create snapshot");
 if (unlink(path)<0) { close(fd); fprintf(stderr,"patch: cannot unlink snapshot %s; path may remain\n",path); srv_patch_signal_checkpoint(); exit(2); }
 return fd;
}
static int open_regular(const char *path, struct stat *st) {
 int fd=open(path,O_RDONLY|O_NONBLOCK|O_NOFOLLOW);
 if (fd<0) die("cannot open input");
 if (fstat(fd,st)<0 || !S_ISREG(st->st_mode)) { close(fd); die("input must be a regular file"); }
 return fd;
}
static int snapshot(const char *path,const char *output,const char *suffix,size_t cap,size_t lines_cap,size_t line_cap,struct stat *st) {
 int in=open_regular(path,st), tmp=temporary(output,suffix);
 unsigned char buf[4096]; size_t total=0, lines=0, column=0;
 for (;;) {
  srv_patch_signal_checkpoint(); ssize_t n=read(in,buf,sizeof(buf));
  srv_patch_signal_checkpoint();
  if (n<0) die("input read failed");
  if (!n) break;
  for (ssize_t i=0;i<n;i++) {
   if (++total>cap || !buf[i] || ++column>line_cap) die("input exceeds profile limits or contains NUL");
   if (buf[i]=='\n') { ++lines; column=0; }
   if (lines+(column!=0)>lines_cap) die("input has too many lines");
  }
  write_all(tmp,buf,(size_t)n);
 }
 close(in); if (lseek(tmp,0,SEEK_SET)<0 || fstat(tmp,st)<0) die("cannot rewind snapshot");
 return tmp;
}
/* The snapshot is immutable; buffered reads keep validation bounded. */
static unsigned char patch_read_buffer[4096];
static size_t patch_read_pos, patch_read_end;
static size_t record(int fd,char *line,size_t cap) {
 size_t n=0;
 for (;;) {
  if (patch_read_pos==patch_read_end) {
   srv_patch_signal_checkpoint(); ssize_t got=read(fd,patch_read_buffer,sizeof(patch_read_buffer));
   srv_patch_signal_checkpoint();
   if (got<0) die("patch snapshot read failed");
   if (!got) { if (n) die("patch record lacks newline"); return 0; }
   patch_read_pos=0; patch_read_end=(size_t)got;
  }
  char c=(char)patch_read_buffer[patch_read_pos++];
  if (n==cap) die("patch record too long");
  line[n++]=c; if (c=='\n') return n;
 }
}
static int safe_label(const char *p,size_t n,char side) {
 if (!n || n>255) return 0;
 if (n>2 && p[1]=='/' && p[0]==side) { p+=2; n-=2; }
 if (!n || (n==1 && p[0]=='.') || (n==2 && p[0]=='.' && p[1]=='.')) return 0;
 for (size_t i=0;i<n;i++) { unsigned char c=(unsigned char)p[i];
  if (!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.')) return 0;
 }
 return 1;
}
static unsigned number(const char **p,const char *end) {
 unsigned n=0; if (*p==end || **p<'0' || **p>'9') die("invalid unified hunk number");
 while (*p<end && **p>='0' && **p<='9') { unsigned d=(unsigned)(*(*p)++-'0'); if(n>(8193U-d)/10U) die("hunk number exceeds limits"); n=n*10+d; }
 return n;
}
static void range(const char **p,const char *end,char sign,unsigned *count) {
 if (*p==end || *(*p)++!=sign) die("invalid unified hunk range");
 unsigned first=number(p,end), n=1;
 if (*p<end && **p==',') { ++*p; n=number(p,end); }
 if (first>8192 || n>8192 || (n && (!first || first-1>8192-n))) die("hunk range exceeds limits");
 *count=n;
}
static void validate_patch(int fd) {
 char *line=malloc(PATCH_LINE+1); if (!line) die("out of memory");
 size_t n=record(fd,line,PATCH_LINE);
 if (n<5 || memcmp(line,"--- ",4) || !safe_label(line+4,n-5,'a')) die("expected safe old unified header");
 n=record(fd,line,PATCH_LINE);
 if (n<5 || memcmp(line,"+++ ",4) || !safe_label(line+4,n-5,'b')) die("expected safe new unified header");
 unsigned old=0,new=0,hunks=0; int marker_ok=0;
 while ((n=record(fd,line,PATCH_LINE))!=0) {
  if (old || new) {
   if (line[0]=='\\') {
    static const char marker[]="\\ No newline at end of file\n";
    if (!marker_ok || n!=sizeof(marker)-1 || memcmp(line,marker,n)) die("invalid missing-newline marker");
    marker_ok=0; continue;
   }
   if (line[0]==' ') { if (!old||!new) die("hunk count mismatch"); --old; --new; }
   else if (line[0]=='-') { if (!old) die("hunk count mismatch"); --old; }
   else if (line[0]=='+') { if (!new) die("hunk count mismatch"); --new; }
   else die("invalid unified hunk record");
   marker_ok=1; continue;
  }
  if (line[0]=='\\') {
   static const char marker[]="\\ No newline at end of file\n";
   if (!marker_ok || n!=sizeof(marker)-1 || memcmp(line,marker,n)) die("invalid missing-newline marker");
   marker_ok=0; continue;
  }
  if (n<8 || memcmp(line,"@@ ",3)) die("multiple files or unsupported patch metadata");
  const char *p=line+3,*end=line+n-1; range(&p,end,'-',&old);
  if (p==end || *p++!=' ') die("invalid unified hunk separator");
  range(&p,end,'+',&new);
  if (end-p<3 || memcmp(p," @@",3)) die("invalid unified hunk header");
  p+=3; if (p!=end && *p!=' ') die("invalid unified hunk header suffix");
  if (!old && !new) die("empty hunk"); ++hunks; marker_ok=0;
 }
 if (old||new||!hunks) die("incomplete or empty unified patch");
 free(line); if (lseek(fd,0,SEEK_SET)<0) die("cannot rewind patch snapshot");
}
void srv_patch_admit(int argc,char **argv) {
 if (argc==2 && (!strcmp(argv[1],"--help") || !strcmp(argv[1],"--version"))) {
  if (!strcmp(argv[1],"--help")) puts("Usage: patch -i PATCH -o NEW_OUTPUT SOURCE\nApply one bounded unified patch to a regular file; output must not exist.\nExit status: 0 applied, 1 hunk failed, 2 trouble.");
  else puts("patch (GNU Patch) 2.8.1\nLicense GPLv3+: GNU GPL version 3 or later.\nThis is free software; there is NO WARRANTY.");
  srv_patch_signal_checkpoint();
  int result=fclose(stdout);
  srv_patch_signal_checkpoint();
  if (result==EOF) exit(2); exit(0);
 }
 if (argc!=6 || strcmp(argv[1],"-i") || strcmp(argv[3],"-o") || !strcmp(argv[2],"-") || !strcmp(argv[4],"-") || !strcmp(argv[5],"-"))
  die("expected patch -i PATCH -o NEW_OUTPUT SOURCE");
 if (strlen(argv[4])>=sizeof(output_name) || !*argv[4]) die("invalid output path");
 strcpy(output_name,argv[4]);
 if (atexit(srv_patch_cleanup)) die("cannot register cleanup");
 struct stat ignored;
 source_fd=snapshot(argv[5],output_name,"source",SOURCE_MAX,SOURCE_LINES,SOURCE_LINE,&source_identity);
 patch_fd=snapshot(argv[2],output_name,"patch",PATCH_MAX,PATCH_LINES,PATCH_LINE,&ignored);
 validate_patch(patch_fd);
#ifdef SRV_PATCH_DIAGNOSTIC
 extern void srv_patch_snapshot_diagnostic(void);
 srv_patch_snapshot_diagnostic();
#endif
 output_fd=open(output_name,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);
 if (output_fd<0) die("output must be a new file");
 if (fstat(output_fd,&output_identity)<0) { close_fd(&output_fd); die("cannot verify output identity; created path may remain"); }
 owned=1;
}
