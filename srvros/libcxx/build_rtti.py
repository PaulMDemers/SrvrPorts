#!/usr/bin/env python3
"""Build the experimental upstream libc++abi RTTI subset and guest probe."""
import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import time

root = Path(__file__).resolve().parents[3]
out = root / 'build/browser-runtime'
out.mkdir(parents=True, exist_ok=True)
zig = root / 'build/tooling/zig/zig'
sdk = root / 'build/sysroot/srvros'
z = root / 'build/tooling/zig/lib'
p = root / 'ports/srvros/libcxx'
threaded = out / 'libcxx-threaded.a'
receipt_path = out / 'rtti-receipt.json'
commands = []

def fail(message):
    receipt_path.write_text(json.dumps({'result': 'failed', 'error': message, 'commands': commands}, indent=2) + '\n')
    sys.exit(message)

def run(name, args):
    cmd = list(map(str, args))
    start = time.monotonic()
    with (out / ('rtti-' + name + '.log')).open('w') as log:
        log.write(json.dumps(cmd) + '\n')
        log.flush()
        try:
            proc = subprocess.Popen(cmd, cwd=root, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
            try:
                code = proc.wait(timeout=60)
            except subprocess.TimeoutExpired:
                os.killpg(proc.pid, signal.SIGKILL)
                proc.wait()
                code = None
                log.write('TIMEOUT: process group killed\n')
        except OSError as exc:
            code = None
            log.write(str(exc) + '\n')
    commands.append({'name': name, 'argv': cmd, 'seconds': round(time.monotonic()-start, 3), 'status': code})
    if code != 0:
        fail('failed: ' + str(out / ('rtti-' + name + '.log')))

if not zig.is_file() or not (sdk / '.ready').is_file() or not threaded.is_file():
    fail('SDK or threaded archive missing; run make srvros-sysroot and python3 ports/srvros/libcxx/build.py first')
run('version', [zig, 'version'])
if (out / 'rtti-version.log').read_text().splitlines()[-1].strip() != '0.15.2':
    fail('Zig 0.15.2 required')
flags = ['-target', 'x86_64-freestanding-none', '-std=c++23', '-ffreestanding', '-fno-exceptions', '-frtti', '-fno-sanitize=undefined', '-fno-stack-protector', '-fno-stack-check', '-fno-pic', '-ffunction-sections', '-fdata-sections', '-mno-red-zone', '-O2', '-nostdinc++', '-include', p/'config.h', '-I', z/'libcxx/include', '-I', z/'libcxx/src/include', '-I', z/'libcxxabi/include', '-I', z/'libcxxabi/src', '-I', z/'include', '-isystem', sdk/'include']
units = [z/'libcxxabi/src'/name for name in ('private_typeinfo.cpp', 'stdlib_typeinfo.cpp', 'stdlib_exception.cpp', 'cxa_aux_runtime.cpp', 'abort_message.cpp')]
objects = []
for src in units + [p/'rtti_guest.cc']:
    obj = out / ('rtti-' + src.stem + '.o')
    run(src.stem, [zig, 'c++', *flags, '-c', src, '-o', obj])
    if src != p/'rtti_guest.cc':
        objects.append(obj)
archive = out/'libcxxabi-rtti.a'
archive.unlink(missing_ok=True)
run('archive', [zig, 'ar', 'rcs', archive, *objects])
elf = out/'rttiprobe.elf'
run('link', [zig, 'ld.lld', '-m', 'elf_x86_64', '-nostdlib', '-static', '-z', 'max-page-size=0x1000', '--gc-sections', '-T', sdk/'lib/app.ld', sdk/'lib/crt0.o', out/'rtti-rtti_guest.o', '--start-group', archive, threaded, sdk/'lib/libsrvros.a', '--end-group', '-o', elf])

def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for chunk in iter(lambda: f.read(1024*1024), b''):
            h.update(chunk)
    return h.hexdigest()

tree = hashlib.sha256()
for header in sorted((sdk/'include').rglob('*')):
    if header.is_file():
        tree.update(str(header.relative_to(sdk)).encode() + b'\0' + bytes.fromhex(sha(header)))
inputs = [zig, Path(__file__), p/'config.h', p/'rtti_guest.cc', *units, threaded, sdk/'lib/libsrvros.a', sdk/'lib/crt0.o', sdk/'lib/app.ld', archive, elf]
receipt = {'result': 'success', 'scope': 'RTTI compile/link only; no guest acceptance or exception runtime', 'zig': '0.15.2', 'sdk_headers_sha256': tree.hexdigest(), 'commands': commands, 'sha256': {str(item.relative_to(root)): sha(item) for item in inputs}}
receipt_path.write_text(json.dumps(receipt, indent=2) + '\n')
print(elf)
