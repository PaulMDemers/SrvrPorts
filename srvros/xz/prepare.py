#!/usr/bin/env python3
import pathlib, shutil, sys
src,dst=map(pathlib.Path,sys.argv[1:3])
if dst.exists(): shutil.rmtree(dst)
shutil.copytree(src,dst)
for p in dst.rglob('*'):
 if p.is_file(): p.chmod(p.stat().st_mode|0o200)
shutil.copyfile('ports/srvros/xz/config.h',dst/'config.h')
shutil.copyfile(src/'lib/getopt.in.h',dst/'lib/getopt.h')
def edit(path,old,new):
 p=dst/path;s=p.read_text(); assert old in s,(path,old[:80]);p.write_text(s.replace(old,new))
for name in ('src/xz/message.c', 'src/xz/signals.c', 'src/xz/mytime.c'):
 edit(name, '#include "private.h"', '#include "private.h"\n#undef SIGALRM /* no progress timer */')
edit('src/xz/private.h','#if defined(HAVE_CLOCK_GETTIME) && defined(SIGTSTP) && defined(SIGALRM)','#if 0 /* no job control/progress signal on SrvrOS */')
edit('src/xz/signals.c','\tio_write_to_user_abort_pipe();','\t/* SDK callback must return; read/poll observes EINTR. */')
edit('src/xz/main.c','\targs_info args;\n\targs_parse(&args, argc, argv);','\textern void srv_xz_guard(int, char **);\n\tsrv_xz_guard(argc, argv);\n\targs_info args;\n\targs_parse(&args, argc, argv);\n\tio_no_sparse();')
edit('src/xz/args.c','\t// First the flags from the environment','\tcoder_set_preset(0);\n\topt_auto_adjust = false;\n\t// First the flags from the environment')
edit('src/xz/args.c','\t// If encoder or decoder support was omitted at build time,','\tif (opt_mode == MODE_LIST) message_fatal("Unsupported operation");\n\tif (!opt_stdout && opt_mode != MODE_TEST) {\n\t\tfor (int i=optind; i<argc; ++i)\n\t\t\tif (strcmp(argv[i], "-") != 0)\n\t\t\t\tmessage_fatal("File output requires -c");\n\t}\n\topt_format = FORMAT_XZ;\n\t// If encoder or decoder support was omitted at build time,')
edit('src/xz/hardware.c','return memlimit != 0 ? memlimit : UINT64_MAX;','const uint64_t ceiling = mode == MODE_COMPRESS ? (8U << 20) : (32U << 20);\n\treturn memlimit != 0 && memlimit < ceiling ? memlimit : ceiling;')
edit('src/xz/coder.c','#include "private.h"','#include "private.h"\n#include "allocator.h"\n#include <sched.h>')
edit('src/xz/coder.c','\t\t// Let liblzma do the actual work.','\t\t(void)sched_yield();\n\t\tif (user_abort) break;\n\t\t// Let liblzma do the actual work.')
# Set before every codec initialization, including decoding.
edit('src/xz/coder.c','coder_init(file_pair *pair)\n{','coder_init(file_pair *pair)\n{\n strm.allocator = &srv_xz_allocator;\n srv_xz_allocator_limit(opt_mode == MODE_COMPRESS ? (8U << 20) : (32U << 20));')
# Diagnostic build reports actual codec progress to the probe.
edit('src/xz/coder.c','\t\tret = lzma_code(&strm, action);','\t\tret = lzma_code(&strm, action);\n#ifdef SRV_XZ_DIAGNOSTIC\n\t\textern void srv_xz_codec_marker(void);\n\t\tsrv_xz_codec_marker();\n#endif')
edit('src/xz/file_io.c','#\telse\n\t\tif (errno == ELOOP', '#\telif defined(ELOOP)\n\t\tif (errno == ELOOP')
edit('src/xz/file_io.c','\twhile (true) {\n\t\tconst int ret = poll(pfd, 2, timeout);','\twhile (true) {\n\t\tif (user_abort) return IO_WAIT_ERROR;\n\t\tconst int ret = poll(pfd, 2, timeout);')
edit('src/xz/file_io.c','\twhile (pos < size) {\n\t\tconst ssize_t amount = read(','\twhile (pos < size) {\n\t\tif (user_abort) return SIZE_MAX;\n\t\tconst ssize_t amount = read(')
edit('src/xz/file_io.c','\twhile (size > 0) {\n\t\tconst ssize_t amount = write(','\twhile (size > 0) {\n\t\tif (user_abort) return true;\n\t\tconst ssize_t amount = write(')
(dst/'.prepared').touch()
