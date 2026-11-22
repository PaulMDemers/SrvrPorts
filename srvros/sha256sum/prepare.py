#!/usr/bin/env python3
"""Stage authenticated coreutils 9.12 and apply the SrvrOS profile hooks."""
import hashlib
import pathlib
import sys
import tarfile

EXPECTED = 'a480198559733e9b3da999e90543ac6f888a2caa544d8d664c5a1f17e528e210'
if len(sys.argv) != 3:
    sys.exit('usage: prepare.py ARCHIVE EMPTY_STAGE')
archive, stage = map(pathlib.Path, sys.argv[1:])
if hashlib.sha256(archive.read_bytes()).hexdigest() != EXPECTED:
    sys.exit('coreutils archive SHA256 mismatch')
stage.mkdir(parents=True, exist_ok=True)
if any(stage.iterdir()):
    sys.exit('staging directory must be empty')
with tarfile.open(archive, 'r:xz') as tar:
    for member in tar:
        parts = pathlib.PurePosixPath(member.name).parts
        if not parts or parts[0] != 'coreutils-9.12' or '..' in parts:
            sys.exit('unexpected archive member')
        if len(parts) == 1:
            continue
        member.name = str(pathlib.PurePosixPath(*parts[1:]))
        # The release contains symlinks. Refuse links that escape the staging tree.
        if member.issym() or member.islnk():
            target = pathlib.PurePosixPath(member.linkname)
            base = pathlib.PurePosixPath(member.name).parent if member.issym() else pathlib.PurePosixPath('.')
            if target.is_absolute(): sys.exit('absolute archive link')
            depth = 0
            for part in (base / target).parts:
                depth += -1 if part == '..' else 0 if part == '.' else 1
                if depth < 0: sys.exit('escaping archive link')
        tar.extract(member, stage, filter='data')

def edit(path, changes):
    path = stage / path
    data = path.read_text()
    for old, new in changes:
        if data.count(old) != 1:
            sys.exit(f'expected exactly one edit anchor in {path}: {old[:60]!r}')
        data = data.replace(old, new, 1)
    path.write_text(data)

edit('src/cksum.c', [
    ('#include <config.h>', '#include <config.h>\n#include "profile.h"'),
    ('  setlocale (LC_ALL, "");', '  setlocale (LC_ALL, "C");\n  srvr_sha_init (argc, argv);'),
    ('fp = fopen (filename, O_BINARY ? (*binary ? "rb" : "rt") : "r");', 'fp = srvr_sha_open_regular (filename);'),
    ('checkfile_stream = fopen (checkfile_name, "r");', 'checkfile_stream = srvr_sha_open_regular (checkfile_name);'),
    ('line_length = getline (&line, &line_chars_allocated, checkfile_stream);', 'line_length = srvr_sha_getline (&line, &line_chars_allocated, checkfile_stream);'),
    ('if (! (split_3 (line, line_length, &digest, &d_len, &binary, &filename)', 'if (! (srvr_sha_record_ok (line, line_length)\n             && split_3 (line, line_length, &digest, &d_len, &binary, &filename)'),
    ('  char **operand_lim = argv + argc;', '  if (do_check && argc - optind != 1)\n    error (EXIT_FAILURE, 0, "check mode takes one explicit manifest");\n  if (do_check)\n    strict = true;\n\n  char **operand_lim = argv + argc;'),
])
edit('lib/sha256-stream.c', [
    ('#include <config.h>', '#include <config.h>\n#include "profile.h"'),
    ('#include <stdlib.h>', '#include <errno.h>\n#include <stdlib.h>'),
    ('{\n#if GNULIB_AF_ALG\n  switch', '{\n  srvr_sha_checkpoint ();\n#if GNULIB_AF_ALG\n  switch'),
    ('          sum += n;', '          srvr_sha_checkpoint ();\n          sum += n;'),
    ('''          if (n == 0)
            {
              /* Check for the error flag IFF N == 0, so that we don't
                 exit the loop after a partial read due to e.g., EAGAIN
                 or EWOULDBLOCK.  */
              if (ferror (stream))
                {
                  free (buffer);
                  return 1;
                }
              goto process_partial_block;
            }''',
     '''          if (n == 0)
            goto process_partial_block;'''),
    ('          sum += n;', '''          if (ferror (stream))
            {
              int read_error = errno;
              free (buffer);
              errno = read_error ? read_error : EIO;
              return 1;
            }
          sum += n;'''),
    ('      sha256_process_block (buffer, BLOCKSIZE, &ctx);', '      sha256_process_block (buffer, BLOCKSIZE, &ctx);\n      srvr_sha_checkpoint ();'),
    ('  finish_ctx (&ctx, resblock);', '  finish_ctx (&ctx, resblock);\n  srvr_sha_checkpoint ();'),
])

edit('src/system.h', [
    ('/* exit with a _single_ "write error" diagnostic.  */',
     '#include "profile.h"\n\n/* Exit with a single write error diagnostic.  */'),
    ('''  int saved_errno = errno;
  fflush (stdout);    /* Last attempt to write any buffered data.  */
  fpurge (stdout);    /* Ensure nothing buffered that might induce an error. */
  clearerr (stdout);  /* Avoid extraneous diagnostic from close_stdout.  */
  error (EXIT_FAILURE, saved_errno, _("write error"));''',
     '  srvr_sha_write_error ();'),
])
edit('lib/close-stream.c', [
    ('#include "fpending.h"\n\n', ''),
    ('  const bool some_pending = (__fpending (stream) != 0);\n', ''),
    ('''  /* Return an error indication if there was a previous failure or if
     fclose failed, with one exception: ignore an fclose failure if
     there was no previous error, no data remains to be flushed, and
     fclose failed with EBADF.  That can happen when a program like cp
     is invoked like this 'cp a b >&-' (i.e., with standard output
     closed) and doesn't generate any output (hence no previous error
     and nothing to be flushed).  */

  if (prev_fail || (fclose_fail && (some_pending || errno != EBADF)))''',
     '''  /* The SDK exposes the prior stream error and the close status.  */
  if (prev_fail || fclose_fail)'''),
])

# Observe cooperative delivery after I/O, including error and shutdown paths.
edit('src/cksum.c', [
    ('fp = srvr_sha_open_regular (filename);',
     'fp = srvr_sha_open_regular (filename);\n      srvr_sha_checkpoint ();'),
    ('checkfile_stream = srvr_sha_open_regular (checkfile_name);',
     'checkfile_stream = srvr_sha_open_regular (checkfile_name);\n      srvr_sha_checkpoint ();'),
    ('''  else if (fclose (fp) != 0 && !err)
    err = errno;''',
     '''  else
    {
      int close_result = fclose (fp);
      srvr_sha_checkpoint ();
      if (close_result != 0 && !err)
        err = errno;
    }'''),
    ('''  else if (fclose (checkfile_stream) != 0 && err < 0)
    err = errno;''',
     '''  else
    {
      int close_result = fclose (checkfile_stream);
      srvr_sha_checkpoint ();
      if (close_result != 0 && err < 0)
        err = errno;
    }'''),
    ('''          if (ferror (stdout))
            write_error ();''',
     '''          srvr_sha_checkpoint ();
          if (ferror (stdout))
            write_error ();'''),
    ('''              if (ferror (stdout))
                write_error ();''',
     '''              srvr_sha_checkpoint ();
              if (ferror (stdout))
                write_error ();'''),
    ('''  if (have_read_stdin && fclose (stdin) == EOF)
    error (EXIT_FAILURE, errno, _("standard input"));

  return ok ? EXIT_SUCCESS : EXIT_FAILURE;''',
     '''  if (have_read_stdin)
    {
      int close_result = fclose (stdin);
      srvr_sha_checkpoint ();
      if (close_result == EOF)
        error (EXIT_FAILURE, errno, _("standard input"));
    }

  srvr_sha_checkpoint ();
  return ok ? EXIT_SUCCESS : EXIT_FAILURE;'''),
])
edit('lib/closeout.c', [
    ('#include <config.h>', '#include <config.h>\n#include "profile.h"'),
    ('''  if (close_stream (stdout) != 0
      && !(ignore_EPIPE && errno == EPIPE))''',
     '''  srvr_sha_checkpoint ();
  int stdout_result = close_stream (stdout);
  srvr_sha_checkpoint ();
  if (stdout_result != 0
      && !(ignore_EPIPE && errno == EPIPE))'''),
    ('''      _exit (exit_failure);
    }

  /* Close stderr''',
     '''      srvr_sha_checkpoint ();
      _exit (exit_failure);
    }

  /* Close stderr'''),
    ('''  if (!SANITIZE_ADDRESS && close_stream (stderr) != 0)
    _exit (exit_failure);''',
     '''  if (!SANITIZE_ADDRESS)
    {
      int stderr_result = close_stream (stderr);
      srvr_sha_checkpoint ();
      if (stderr_result != 0)
        _exit (exit_failure);
    }
  srvr_sha_checkpoint ();'''),
])
