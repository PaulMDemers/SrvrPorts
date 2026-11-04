#!/usr/bin/env python3
import pathlib, shutil, sys
src, dst = map(pathlib.Path, sys.argv[1:3])
if dst.exists(): shutil.rmtree(dst)
shutil.copytree(src, dst)
for path in dst.rglob('*'):
 if path.is_file(): path.chmod(path.stat().st_mode | 0o200)
shutil.copyfile('ports/srvros/less/config.h', dst / 'defines.h')
# SDK sig_atomic_t is int; volatile prevents caching across callbacks.
for path in dst.glob('*.c'):
 data = path.read_text()
 data = data.replace('extern int sigs;', 'extern volatile int sigs;')
 data = data.replace('public int sigs;', 'public volatile int sigs;')
 path.write_text(data)
def edit(name, old, new):
 p = dst / name; s = p.read_text(); assert old in s, name; p.write_text(s.replace(old, new))
# Callbacks only set flags; the SDK signal trampoline must return normally.
p = dst / 'signal.c'; s = p.read_text()
a = s.index('static RETSIGTYPE u_interrupt('); b = s.index('\n#endif\n\n#ifdef SIGTSTP', a)
s = s[:a] + 'static RETSIGTYPE u_interrupt(int type)\n{\n (void) type; sigs |= S_INTERRUPT;\n}\n' + s[b:]
a = s.index('static RETSIGTYPE stop('); b = s.index('\n#endif', a)
s = s[:a] + 'static RETSIGTYPE stop(int type)\n{\n (void) type; sigs |= S_INTERRUPT;\n}\n' + s[b:]
a = s.index('public RETSIGTYPE lwinch('); b = s.index('\n#endif\n\n#if MSDOS_COMPILER==WIN32C', a)
s = s[:a] + 'public RETSIGTYPE lwinch(int type)\n{\n (void) type; sigs |= S_WINCH;\n}\n' + s[b:]
s = s.replace('quit(15);', 'sigs |= (1<<4);')
s = s.replace('(void) LSIGNAL(SIGUSR1, sigusr1);', '(void) LSIGNAL(SIGUSR1, SIG_IGN);')
s = s.replace('sigs = 0;\n\n#ifdef SIGTSTP', 'sigs = 0;\n\tif (tsignals & (1<<4)) quit(15);\n\n#ifdef SIGTSTP')
p.write_text(s)
edit('os.c', 'start:\n#if MSDOS_COMPILER==WIN32C', 'start:\n if (sigs) return fd != tty && !ABORT_SIGS() ? READ_AGAIN : READ_INTR;\n#if MSDOS_COMPILER==WIN32C')
edit('os.c', '\tflush();\n\treading = TRUE;', '\tflush();\n if (sigs) return fd != tty && !ABORT_SIGS() ? READ_AGAIN : READ_INTR;\n\treading = TRUE;')
edit('os.c', 'if (errno == EINTR)\n\t\t\tgoto start;', 'if (errno == EINTR)\n\t\t{\n\t\t\tif (sigs) return fd != tty && !ABORT_SIGS() ? READ_AGAIN : READ_INTR;\n\t\t\tgoto start;\n\t\t}')
edit('ttyin.c', 'if (fd < 0)\n\t\tfd = open_tty_device("/dev/tty");', '/* SrvrOS has no controlling /dev/tty; fd 2 is the terminal. */')
edit('ttyin.c', 'tty = open_tty();', 'tty = open_tty();\n { struct termios mode; int flags = fcntl(tty, F_GETFL);\n   if (tcgetattr(tty, &mode) < 0 || flags < 0 || (flags & O_ACCMODE) == O_WRONLY) {\n     fprintf(stderr, \"less: fd 2 must be a readable terminal\\n\"); exit(1);\n   } }')
edit('ttyin.c', '#include "less.h"', '#include "less.h"\n#include <termios.h>')
edit('screen.c', 'if (tgetent(termbuf, term) != TGETENT_OK)\n\t\thardcopy = TRUE;', 'if (tgetent(termbuf, term) != TGETENT_OK) {\n\t\tfprintf(stderr, "less: unsupported TERM: %s\\n", term);\n\t\texit(1);\n\t}')
(dst / '.prepared').touch()
