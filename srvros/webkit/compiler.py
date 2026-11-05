#!/usr/bin/env python3
"""Limited Zig driver for SrvrOS configure probes, not a general SDK driver."""
import os
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[3]
zig = root / 'build/tooling/zig/zig'
sdk = root / 'build/sysroot/srvros'
mode, *args = sys.argv[1:]
if mode not in ('cc', 'c++'):
    raise SystemExit('expected cc or c++')
# Never accept command-line flags which change the claimed target or link mode.
for arg in args:
    if mode == 'c++' and arg == '-ffreestanding':
        raise SystemExit('C++ requires hosted main linkage: ' + arg)
    if (arg in ('-shared', '--shared', '-dynamiclib', '-target', '--target',
                '-arch', '-m32', '--sysroot', '-isysroot')
            or arg.startswith(('--target=', '-target=', '--sysroot=', '-isysroot'))
            or (arg.startswith('-Wl,') and any(x in ('-shared', '--shared')
                                              for x in arg[4:].split(',')))):
        raise SystemExit('unsupported target or link mode: ' + arg)
common = ['-target', 'x86_64-freestanding-none', '-ffreestanding',
          '-fno-sanitize=undefined', '-fno-stack-protector', '-mno-red-zone',
          '-D__SRVROS__=1', '-nostdinc']
if mode == 'c++':
    common += ['-fhosted', '-fno-exceptions', '-fno-rtti',
               '-D_LIBCPP_HARDENING_MODE_DEFAULT=_LIBCPP_HARDENING_MODE_NONE',
               '-DUSE_SYSTEM_MALLOC=1', '-nostdinc++',
               '-include', str(root / 'ports/srvros/libcxx/config.h'),
               '-I', str(root / 'build/tooling/zig/lib/libcxx/include'),
               '-I', str(root / 'build/tooling/zig/lib/libcxx/src/include'),
               '-I', str(root / 'build/tooling/zig/lib/libcxxabi/include'),
               '-I', str(root / 'build/tooling/zig/lib/include'),
               '-isystem', str(sdk / 'include')]
    if not any(a.startswith('-std=') or a.startswith('--std=') for a in args):
        common += ['-std=c++23']
else:
    common += ['-isystem', str(sdk / 'include'),
               '-isystem', str(root / 'build/tooling/zig/lib/include')]
# Compiler identification, preprocessing, dependency scans, and compilation
# do not link. Everything else is a genuine SDK executable link.
no_link = any(a in ('-c', '-S', '-E', '-M', '-MM', '--version', '-dumpmachine',
                    '-dumpversion', '-print-search-dirs', '-print-resource-dir',
                    '-fsyntax-only')
              or a.startswith('-print-file-name=') for a in args)
cmd = [str(zig), mode, *common]
if not no_link:
    cmd += ['-nostdlib', '-static', '-fuse-ld=lld',
            '-Xlinker', '-z', '-Xlinker', 'max-page-size=0x1000',
            '-Xlinker', '--gc-sections', '-Xlinker', '-T',
            '-Xlinker', str(sdk / 'lib/app.ld'), str(sdk / 'lib/crt0.o')]
cmd += args
if not no_link:
    if mode == 'c++':
        runtime = root / 'build/browser-runtime/libcxx-threaded.a'
        if not runtime.is_file():
            raise SystemExit('missing threaded libc++ archive: ' + str(runtime))
        cmd += [str(runtime)]
    cmd += [str(sdk / 'lib/libsrvros.a')]
os.execv(str(zig), cmd)
