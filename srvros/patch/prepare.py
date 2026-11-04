#!/usr/bin/env python3
"""Prepare a bounded SrvrOS adapter; never modify the upstream checkout."""
from pathlib import Path
import shutil, sys, re
src, dst = map(Path, sys.argv[1:3])
if dst.exists(): shutil.rmtree(dst)
shutil.copytree(src, dst)
for p in dst.rglob('*'):
 if p.is_file(): p.chmod(p.stat().st_mode | 0o200)
def edit(name,old,new):
 p=dst/name; s=p.read_text(); assert old in s,(name,old[:100]); p.write_text(s.replace(old,new,1))
edit('lib/renameatu.c', '#undef renameat', '#undef renameat\n/* SDK declares renameat in unistd; gnulib may rename its prototype. */\nextern int renameat (int, const char *, int, const char *);')
edit('lib/getprogname.c', '#  error "getprogname module not ported to this OS"', '  extern const char *program_name;\n  return program_name ? last_component (program_name) : "?";')
# SDK has no ELOOP; unsupported symlink traversal fails closed as EOPNOTSUPP.
for name in ('lib/canonicalize-lgpl.c','src/safe.c','src/patch.c'):
 p=dst/name; data=p.read_text(); assert 'ELOOP' in data; p.write_text(re.sub(r'\bELOOP\b', 'EOPNOTSUPP', data))
edit('src/safe.c', '      || errno == EMLINK  /* FreeBSD 10.1: Too many links */\n', '')
for name in ('src/patch.c','src/pch.c','src/inp.c','src/util.c'):
 edit(name,'#include <common.h>','#include <common.h>\n#include "profile.h"')
edit('src/patch.c', 'enum conflict_style conflict_style;', '#ifdef ENABLE_MERGE\nenum conflict_style conflict_style;\n#endif')
edit('src/patch.c','#include <common.h>','#include <common.h>\n#include <signal.h>')
edit('src/patch.c','    exit_failure = EXIT_TROUBLE;', '''    exit_failure = EXIT_TROUBLE;
    signal (SIGPIPE, SIG_IGN);
    init_signals ();
    srv_patch_admit (argc, argv);
    /* The profile does not consume ambient patch/backup/temp policy. */
    unsetenv ("PATCH_GET"); unsetenv ("POSIXLY_CORRECT");
    unsetenv ("QUOTING_STYLE"); unsetenv ("SIMPLE_BACKUP_SUFFIX");
    unsetenv ("PATCH_VERSION_CONTROL"); unsetenv ("VERSION_CONTROL");
    unsetenv ("TMPDIR"); unsetenv ("TMP"); unsetenv ("TEMP");''')
edit('src/patch.c','    atexit (close_stdout);', '    /* stdout is closed before output commit, so close errors remove output. */')
edit('src/patch.c','    get_some_switches (argc, argv);','''    char *srv_args[] = { argv[0], (char *)"-i", argv[2], (char *)"-o", argv[4], (char *)"--", argv[5], nullptr };
    get_some_switches (7, srv_args);
    force = true; batch = false; maxfuzz = 0; patch_get = 0;
    posixly_correct = true; backup_if_mismatch_specified = true;
    backup_if_mismatch = false; make_backups = false;
    srv_patch_source_stat (&instat); inerrno = 0;''')
edit('src/patch.c','    init_signals ();\n\n    /* When the file to patch', '    /* When the file to patch')
edit('src/patch.c','''      int outfd = make_tempfile (&tmpout, 'o', outname,
				 O_WRONLY | binary_transput,
				 instat.st_mode & S_IRWXUGO);
      if (outfd < 0)''','''      int outfd = outfile ? -1 : make_tempfile (&tmpout, 'o', outname,
                                 O_WRONLY | binary_transput,
                                 instat.st_mode & S_IRWXUGO);
      if (outfd < 0 && !outfile)''')
edit('src/patch.c','''      intmax_t hunk = 0;''','''      skip_reject_file = true;
      intmax_t hunk = 0;''')
edit('src/patch.c','''		failed++;
		if (verbosity''','''		failed++;
                somefailed = true;
		if (verbosity''')
edit('src/patch.c','''		ifd = safe_open (inname, oflags, 0);''','''                (void) oflags;
                if (lseek (srv_patch_source_fd (), 0, SEEK_SET) < 0)
                  pfatal ("cannot rewind source snapshot");
                ifd = dup (srv_patch_source_fd ());''')
edit('src/patch.c','''  if (strcmp (name, "-") != 0)
    return create_output_file (&(struct outfile) { .name = name }, 0);
  else''','''  if (strcmp (name, "-") != 0)
    {
      int fd = srv_patch_output_fd ();
      FILE *stream = fdopen (fd, "wb");
      if (!stream) { close (fd); pfatal ("cannot open output stream"); }
      srv_patch_set_output (stream);
      return stream;
    }
  else''')
edit('src/patch.c','''    output_files (nullptr, 1);
    delete_files ();
    return somefailed ? EXIT_FAILURE : EXIT_SUCCESS;''','''    output_files (nullptr, 1);
    delete_files ();
    srv_patch_signal_checkpoint ();
    int stdout_result = fclose (stdout);
    srv_patch_signal_checkpoint ();
    if (stdout_result == EOF) return EXIT_TROUBLE;
    if (!somefailed) srv_patch_success ();
    return somefailed ? EXIT_FAILURE : EXIT_SUCCESS;''')
edit('src/patch.c','''    for (ptrdiff_t offset = min_offset; offset <= max_offset; offset++) {''','''    for (ptrdiff_t offset = min_offset; offset <= max_offset; offset++) {
        if ((offset & 63) == 0) srv_patch_signal_checkpoint ();
#ifdef SRV_PATCH_DIAGNOSTIC
        if (offset == min_offset + 1) { extern void srv_patch_match_marker (void); srv_patch_match_marker (); }
#endif''')
edit('src/patch.c','''    while (old <= lastline) {''','''    while (old <= lastline) {
        if ((old & 63) == 0) srv_patch_signal_checkpoint ();''')
# Never restat the original path after admission, including GNU's header probe.
p = dst/'src/pch.c'; data=p.read_text(); old='inerrno = stat_file (inname, &instat);'; assert data.count(old)==2; p.write_text(data.replace(old, 'srv_patch_source_stat (&instat); inerrno = 0;'))
edit('src/inp.c', '    if (inerrno == -1)\n      inerrno = stat_file (filename, &instat);', '    srv_patch_source_stat (&instat); inerrno = 0;')
edit('src/pch.c','''    if (!filename || !*filename || strEQ (filename, "-"))
      pfp = stdin;
    else
      {
	pfp = fopen (filename, binary_transput ? "rb" : "r");
	if (!pfp)
	  pfatal ("Can't open patch file %s", quotearg (filename));
      }''','''    (void) filename;
    if (lseek (srv_patch_patch_fd (), 0, SEEK_SET) < 0)
      pfatal ("cannot rewind patch snapshot");
    int snapshot_fd = dup (srv_patch_patch_fd ());
    if (snapshot_fd < 0 || !(pfp = fdopen (snapshot_fd, "rb")))
      pfatal ("cannot open patch snapshot");''')
# Never clean up or raise from the SDK's signal callback.
edit('src/util.c','#include <common.h>','#include <common.h>\n#include <sched.h>')
edit('src/util.c', '''/* Signal SIG has arrived and can be acted on now.\n   Clean up and arrange to terminate.\n   This function is async-signal-safe.  */''', '''/* Cleanup and terminate in normal control flow, after the SDK callback returns.  */''')
edit('src/util.c','''  fatal_cleanup ();

  /* Cause the program''','''  signals_are_deferred = 1; /* Prevent recursive cleanup after dispatch. */
  fatal_cleanup ();
  srv_patch_cleanup ();

  /* Cause the program''')
edit('src/util.c', '''#if ! (HAVE_SIGACTION && HAVE_SIGFILLSET)\n  /* Cater to non-POSIX systems that lack sigaction and reset to SIG_DFL.  */\n  signal (sig, SIG_IGN);\n#endif\n''', '  /* No SDK or I/O calls from the callback. */\n')
edit('src/util.c','''  if (!signals_are_deferred)
    {
      signals_are_deferred = 1;	/* Avoid infinite recursion.  */
      fatal_cleanup_and_terminate (sig);
    }''','''  /* Return through the SDK dispatcher; normal control flow handles it. */''')
edit('src/util.c','''void
init_signals (void)''','''void
srv_patch_signal_checkpoint (void)
{
  int saved_errno = errno;
  (void) sched_yield ();
  if (!signals_are_deferred && signal_received)
    fatal_cleanup_and_terminate (signal_received);
  errno = saved_errno;
}

void
init_signals (void)''')
edit('src/util.c', '''  Fflush (stream);\n  if (fclose (stream) < 0)''', '''  srv_patch_signal_checkpoint ();\n  Fflush (stream);\n  int result = fclose (stream);\n  srv_patch_signal_checkpoint ();\n  if (result < 0)''')
edit('src/util.c', '''  if (fflush (stream) < 0)''', '''  srv_patch_signal_checkpoint ();\n  int result = fflush (stream);\n  srv_patch_signal_checkpoint ();\n  if (result < 0)''')
edit('src/util.c', '''  int nout = vfprintf (stream, format, args);''', '''  srv_patch_signal_checkpoint ();\n  int nout = vfprintf (stream, format, args);''')
edit('src/util.c', '''  va_end (args);\n  if (nout < 0)''', '''  va_end (args);\n  srv_patch_signal_checkpoint ();\n  if (nout < 0)''')
edit('src/util.c','''  if (fputc (c, stream) < 0)''','''  unsigned char byte = (unsigned char)c;
  srv_patch_signal_checkpoint ();
  srv_patch_output_bytes (stream, &byte, 1);
  int result = fputc (c, stream);
  srv_patch_signal_checkpoint ();
  if (result < 0)''')
edit('src/util.c','''  if (fputs (s, stream) < 0)''','''  srv_patch_signal_checkpoint ();
  srv_patch_output_bytes (stream, s, strlen (s));
  int result = fputs (s, stream);
  srv_patch_signal_checkpoint ();
  if (result < 0)''')
edit('src/util.c','''  if (fwrite (ptr, size, nitems, stream) < nitems)''','''  srv_patch_signal_checkpoint ();
  if (size && nitems > SIZE_MAX / size) fatal ("output size overflow");
  srv_patch_output_bytes (stream, ptr, size * nitems);
  size_t result = fwrite (ptr, size, nitems, stream);
  srv_patch_signal_checkpoint ();
  if (result == nitems) srv_patch_output_progress (stream, size * result);
  if (result < nitems)''')
edit('src/util.c','''  ssize_t r = read (filedes, buf, MIN (nbyte, IO_BUFSIZE));''','''  srv_patch_signal_checkpoint ();
  ssize_t r = read (filedes, buf, MIN (nbyte, IO_BUFSIZE));
  srv_patch_signal_checkpoint ();''')
(dst/'.prepared').touch()
