#!/usr/bin/env python3
"""Prepare the pinned bc release without modifying upstream."""
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import sys

src, out = map(Path, sys.argv[1:3])
h = hashlib.sha256()
for f in sorted(x for x in src.rglob('*') if x.is_file()):
    h.update(f.relative_to(src).as_posix().encode() + b'\0' + hashlib.sha256(f.read_bytes()).digest())
if h.hexdigest() != '504ada1bae8de865cc1a4df3ea1ab3811dd1397023d9c1644139352974a96dea':
    sys.exit('bc release tree differs from pinned 7.0.3 snapshot')
if out.exists():
    shutil.rmtree(out)
out.mkdir(parents=True)
for name in ('src', 'include', 'gen'):
    shutil.copytree(src / name, out / name)

def replace(path, old, new):
    p = out / path
    s = p.read_text()
    if s.count(old) != 1:
        sys.exit(f'unexpected upstream {path}')
    p.write_text(s.replace(old, new))

replace('src/main.c', '#include <libgen.h>', '/* basename is not used in this profile. */')
replace('include/status.h', '#include <stdint.h>', '#include <stdint.h>\n#include <setjmp.h>\n#define sigjmp_buf jmp_buf\n#define sigsetjmp(j, s) setjmp(j)\n#define siglongjmp longjmp')
p = out / 'src/vm.c'
s = p.read_text()
start = s.index('static void\nbc_vm_sig(int sig)')
end = s.index('\n/**\n * Sets up signal handling.', start)
s = s[:start] + '''static void
bc_vm_sig(int sig)
{
    /* Return to SrvrOS's dispatcher before any nonlocal jump or I/O. */
    if (vm->status == (sig_atomic_t) BC_STATUS_QUIT || vm->sig != 0) {
        if (!BC_I || sig != SIGINT) vm->status = BC_STATUS_QUIT;
        return;
    }
    vm->sig = sig;
    if (!(sig == SIGINT && BC_SIGINT && BC_I)) vm->status = BC_STATUS_QUIT;
}
''' + s[end:]
s = s.replace('sa.sa_flags = BC_ENABLE_EDITLINE ? 0 : SA_NODEFER;', 'sa.sa_flags = 0;')
p.write_text(s)
replace('src/program.c', '#include <program.h>', '#include <program.h>\n#include <sched.h>')
replace('src/program.c', '\t\tBC_SIG_ASSERT_NOT_LOCKED;\n\n#if BC_HAS_COMPUTED_GOTO', '\t\tBC_SIG_ASSERT_NOT_LOCKED;\n\t\t/* Dispatch pending signals only at a safe instruction boundary. */\n\t\tif ((++srvros_ticks & 1023U) == 0) {\n\t\t\tsched_yield();\n\t\t\tif (vm->sig) BC_JMP;\n\t\t}\n\n#if BC_HAS_COMPUTED_GOTO')
replace('src/program.c', '\tsize_t idx;\n\tBcResult r;', '\tsize_t idx;\n\tunsigned int srvros_ticks = 0;\n\tBcResult r;')
gen = out / 'gen/strgen'
subprocess.run([os.environ.get('HOSTCC', 'cc'), '-I' + str(out / 'include'), '-o', str(gen), str(out / 'gen/strgen.c')], check=True)
for inp, output, args in [('bc_help.txt', 'bc_help.c', ['bc_help', '', 'BC_ENABLED', '0']), ('lib.bc', 'lib.c', ['bc_lib', 'bc_lib_name', 'BC_ENABLED', '1', '', '', '1'])]:
    subprocess.run([str(gen), str(out / 'gen' / inp), str(out / 'gen' / output), '1', *args], check=True)
(out / '.prepared').touch()
