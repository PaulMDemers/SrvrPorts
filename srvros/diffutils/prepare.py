#!/usr/bin/env python3
import pathlib,shutil,sys
src,dst=map(pathlib.Path,sys.argv[1:3])
if dst.exists(): shutil.rmtree(dst)
shutil.copytree(src,dst)
for p in dst.rglob('*'):
 if p.is_file(): p.chmod(p.stat().st_mode|0o200)
def edit(path,old,new):
 p=dst/path; s=p.read_text(); assert old in s,(path,old[:80]);p.write_text(s.replace(old,new,1))
edit('lib/strptime.c', '__strptime_internal (rp, fmt, tm, decided, era_cnt LOCALE_PARAM)\n     const char *rp;\n     const char *fmt;\n     struct tm *tm;\n     enum ptime_locale_status *decided;\n     int era_cnt;\n     LOCALE_PARAM_DECL', '__strptime_internal (const char *rp, const char *fmt, struct tm *tm, enum ptime_locale_status *decided, int era_cnt LOCALE_PARAM_PROTO)')
edit('lib/strptime.c', 'strptime (buf, format, tm LOCALE_PARAM)\n     const char *restrict buf;\n     const char *restrict format;\n     struct tm *restrict tm;\n     LOCALE_PARAM_DECL', 'strptime (const char *restrict buf, const char *restrict format, struct tm *restrict tm LOCALE_PARAM_PROTO)')
edit('lib/getprogname.c', '#  error "getprogname module not ported to this OS"', '  extern const char *program_name;\n  return program_name ? last_component (program_name) : "?";')
# No-follow mode is rejected by the profile; SDK has no ELOOP errno.
p = dst/'src/diff.c'; data=p.read_text(); data=data.replace('ELOOP','SRV_DIFF_NOFOLLOW_UNSUPPORTED'); p.write_text('#define SRV_DIFF_NOFOLLOW_UNSUPPORTED (-1)\n' + data)
edit('src/diff.c','  /* Do our initializations.  */','  extern void srv_diff_guard (int, char **);\n  signal (SIGPIPE, SIG_IGN); /* report failed output as trouble */\n  srv_diff_guard (argc, argv);\n  /* Do our initializations.  */')
edit('src/diff.c','setlocale (LC_ALL, "");','setlocale (LC_ALL, "C");')
edit('src/diff.c','  switch_string = option_list (argv + 1, optind - 1);','  files_can_be_treated_as_binary = false;\n  switch_string = option_list (argv + 1, optind - 1);')
# Same-file and brief shortcuts skip reading, so bypass them for validated input.
edit('src/diff.c','  if (same_files & no_diff_means_no_output)','  if (false && same_files && no_diff_means_no_output)')
edit('src/diff.c','\tif (f && same_files)','\tif (false && f && same_files)')
edit('src/diff.c','      if (f && file_name_cmp (cmp.file[f].name, cmp.file[0].name) == 0)','      if (false && f && file_name_cmp (cmp.file[f].name, cmp.file[0].name) == 0)')
edit('src/diff.c','  int oflags = ((binary ? O_BINARY : 0) | O_CLOEXEC', '  int oflags = (O_NONBLOCK | (binary ? O_BINARY : 0) | O_CLOEXEC')
edit('src/diff.c', '  if (toplevel)\n    {\n      if (!no_directory', '  if (toplevel)\n    for (int f = 0; f < 2; ++f)\n      if (!cmp.file[f].err && !S_ISREG (cmp.file[f].stat.st_mode))\n        error (EXIT_TROUBLE, 0, "%s: not a regular file", cmp.file[f].name);\n\n  if (toplevel)\n    {\n      if (!no_directory')
edit('src/diff.c','  if (status == EXIT_SUCCESS)\n    status = compare_prepped_files (parent, &cmp, O_RDONLY | oflags);', '''  if (status == EXIT_SUCCESS)
    for (int f = 0; f < 2; ++f)
      if (!S_ISREG (cmp.file[f].stat.st_mode) || cmp.file[f].stat.st_size > 262144)
        { error (0, 0, "%s: not a regular file or exceeds 256 KiB", cmp.file[f].name); status = EXIT_TROUBLE; }
  if (status == EXIT_SUCCESS)
    status = compare_prepped_files (parent, &cmp, O_RDONLY | oflags);''')
# Validate bytes and physical lines as they are read; stat is only an early bound.
edit('src/io.c','#include "diff.h"','#include "diff.h"\n#include <error.h>\n#include <diagnose.h>')
edit('src/io.c','void\nfile_block_read (struct file_data *current, idx_t size)\n{', '''struct srv_diff_read_state { struct file_data *file; size_t bytes, lines, column; };
static struct srv_diff_read_state srv_states[2];
void
file_block_read (struct file_data *current, idx_t size)
{
  struct srv_diff_read_state *st = &srv_states[0];
  if (st->file && st->file != current) st = &srv_states[1];
  if (!st->file) st->file = current;
  if (size > (idx_t)(262145 - st->bytes)) size = 262145 - st->bytes;''')
edit('src/io.c','      current->buffered += s;','''      const unsigned char *p = (const unsigned char *) file_buffer (current) + current->buffered;
      for (ptrdiff_t i = 0; i < s; ++i)
        {
          if (++st->bytes > 262144) error (EXIT_TROUBLE, 0, "%s: input exceeds 256 KiB", current->name);
          if (!p[i]) error (EXIT_TROUBLE, 0, "%s: NUL byte is unsupported", current->name);
          if (++st->column > 65536) error (EXIT_TROUBLE, 0, "%s: line exceeds 64 KiB", current->name);
          if (p[i] == '\\n') { ++st->lines; st->column = 0; }
          if (st->lines + (st->column != 0) > 8192) error (EXIT_TROUBLE, 0, "%s: input exceeds 8192 lines", current->name);
        }
      current->buffered += s;''')
# Checkpoint only in normal execution, never inside signal handlers.
edit('src/analyze.c','#include "diff.h"','#include "diff.h"\n#include <sched.h>')
edit('lib/diffseq.h','  for (c = 1;; ++c)\n    {','''  for (c = 1;; ++c)
    {
      if ((c & 63) == 0) (void) sched_yield ();
#ifdef SRV_DIFF_DIAGNOSTIC
      if (c == 2) { extern void srv_diff_codec_marker (void); srv_diff_codec_marker (); }
#endif''')
(dst/'src/version.h').write_text('extern const char *Version;\n')
(dst/'src/version.c').write_text('const char *Version = "3.12";\n')
(dst/'src/paths.h').write_text('#define DEFAULT_DIFF_PROGRAM "diff"\n#define LOCALEDIR "/"\n')
(dst/'.prepared').touch()
