#!/usr/bin/env python3
"""Build pinned ICU for SrvrOS in a fresh, bounded cross-build directory."""
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import signal
import subprocess
import sys
import tarfile
import tempfile
import time
import zipfile

root = Path(__file__).resolve().parents[3]
out = root / 'build/browser-icu'
port = root / 'ports/srvros/icu'
sdk = root / 'build/sysroot/srvros'
zig = root / 'build/tooling/zig/zig'
runtime = root / 'build/browser-runtime'
source_archive = out / 'icu4c-79.1-sources.tgz'
data_archive = out / 'icu4c-79.1-data-bin-l.zip'
inputs = {source_archive: (28162455, '06fa63ecb521048f8082f12ad988f6b678d90cbd88aafce24cbc94902b191d99'),
          data_archive: (12863498, '72489fb316a60ae1c1da205866edd01b17e8fbf8354ad0615e6cc60f74e08811')}
reserve = 1536 * 1024 * 1024
out.mkdir(parents=True, exist_ok=True)
run = Path(tempfile.mkdtemp(prefix='run-', dir=out))
receipt = {'result': 'failed', 'scope': 'compile and link only; guest not run', 'commands': [], 'sha256': {}}

def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()

def space():
    if shutil.disk_usage(out).free < reserve:
        raise RuntimeError('less than 1.5 GiB free; build stopped')

def safe_copy(src, dest):
    with open(src, 'rb') as incoming, open(dest, 'wb') as outgoing:
        while True:
            block = incoming.read(1024 * 1024)
            if not block:
                break
            space()
            outgoing.write(block)
    return dest

def command(name, args, cwd, env=None, timeout=3600, required=True):
    args = list(map(str, args)); space()
    with (run / (name + '.log')).open('wb') as log:
        log.write((json.dumps(args) + '\n').encode()); log.flush()
        proc = subprocess.Popen(args, cwd=cwd, env=env, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
        start = time.monotonic(); reason = None
        try:
            while proc.poll() is None:
                if shutil.disk_usage(out).free < reserve:
                    reason = 'disk reserve reached'
                elif time.monotonic() - start > timeout:
                    reason = 'timeout'
                if reason:
                    os.killpg(proc.pid, signal.SIGTERM)
                    try: proc.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        os.killpg(proc.pid, signal.SIGKILL); proc.wait()
                    break
                time.sleep(1)
        except BaseException:
            os.killpg(proc.pid, signal.SIGTERM)
            proc.wait(timeout=5)
            raise
        receipt['commands'].append({'name': name, 'argv': args, 'returncode': proc.returncode,
                                    'seconds': round(time.monotonic() - start, 3), 'stop_reason': reason})
        if reason:
            raise RuntimeError(name + ' stopped: ' + reason)
        if proc.returncode:
            if required:
                raise RuntimeError(name + ' failed; see ' + str(run / (name + '.log')))
            return False
        return True

def verify_receipt(path, required):
    data = json.loads(path.read_text())
    if not all(name in data['sha256'] for name in required):
        raise RuntimeError(str(path) + ': receipt missing required inputs or outputs')
    for name, expected in data['sha256'].items():
        rel = Path(name)
        if rel.is_absolute() or '..' in rel.parts or not (root / rel).is_file() or sha(root / rel) != expected:
            raise RuntimeError('runtime receipt mismatch: ' + name)
    return data

try:
    space()
    receipt['source_observation_only'] = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip()
    receipt['dirty_observation_only'] = subprocess.check_output(['git', 'status', '--porcelain'], cwd=root, text=True)
    if ' ' in str(root):
        raise RuntimeError('ICU configure does not support this recipe in a path containing spaces')
    for path, (size, expected) in inputs.items():
        if not path.is_file() or path.stat().st_size != size or sha(path) != expected:
            raise RuntimeError('missing or mismatched pinned archive: ' + str(path))
    sys.path.insert(0, str(root / 'tools'))
    from browser_evidence import verify_runtime
    runtime_provenance = verify_runtime()
    rtti_required = ('build/browser-runtime/libcxxabi-rtti.a', 'build/browser-runtime/rttiprobe.elf',
                'ports/srvros/libcxx/build_rtti.py', 'ports/srvros/libcxx/rtti_guest.cc',
                'ports/srvros/libcxx/config.h', 'build/sysroot/srvros/lib/libsrvros.a',
                'build/browser-runtime/libcxx-threaded.a',
                'build/tooling/zig/lib/libcxxabi/src/private_typeinfo.cpp',
                'build/tooling/zig/lib/libcxxabi/src/stdlib_typeinfo.cpp',
                'build/tooling/zig/lib/libcxxabi/src/stdlib_exception.cpp',
                'build/tooling/zig/lib/libcxxabi/src/cxa_aux_runtime.cpp',
                'build/tooling/zig/lib/libcxxabi/src/abort_message.cpp')
    rtti_receipt = runtime / 'rtti-receipt.json'
    rtti = verify_receipt(rtti_receipt, rtti_required)
    if rtti.get('sdk_headers_sha256') != runtime_provenance['sdk_headers_sha256']:
        raise RuntimeError('RTTI receipt SDK header mismatch')
    if rtti.get('result') != 'success':
        raise RuntimeError('RTTI receipt is not successful')
    receipt['sha256'].update(rtti['sha256'])
    math_receipt = runtime / 'math-receipt.json'
    math_required = ('userspace/lib/src/posix_math_hyperbolic.c', 'userspace/lib/include/math.h',
                     'tools/test_math_hyperbolic.c', 'tools/mathprobe.c', 'tools/browser_math_probe.py',
                     'build/tooling/zig/lib/libc/musl/src/math/tanhf.c',
                     'build/tooling/zig/lib/libc/musl/src/math/expm1f.c',
                     'build/browser-runtime/mathprobe.elf', 'build/sysroot/srvros/lib/libsrvros.a')
    math = verify_receipt(math_receipt, math_required)
    if math.get('result') != 'success' or math.get('sdk_headers_sha256') != runtime_provenance['sdk_headers_sha256']:
        raise RuntimeError('math receipt failed or SDK header mismatch')
    receipt['sha256'].update(math['sha256'])
    with tarfile.open(source_archive, 'r:gz') as archive:
        total = 0
        for member in archive:
            name = PurePosixPath(member.name)
            if name.is_absolute() or '..' in name.parts or not (member.isdir() or member.isfile()):
                raise RuntimeError('unsafe source archive member')
            space()
            total += member.size
            if total > 512 * 1024 * 1024:
                raise RuntimeError('source exceeds 512 MiB limit')
            target = run.joinpath(*name.parts)
            if member.isdir(): target.mkdir(parents=True, exist_ok=True)
            else:
                target.parent.mkdir(parents=True, exist_ok=True)
                with archive.extractfile(member) as src, target.open('wb') as dst:
                    while True:
                        block = src.read(1024 * 1024)
                        if not block: break
                        space(); dst.write(block)
                target.chmod(member.mode & 0o777)
    receipt['source_extracted_bytes'] = total
    with zipfile.ZipFile(data_archive) as data:
        info = data.getinfo('icudt79l.dat')
        if info.file_size != 33558160:
            raise RuntimeError('unexpected ICU data size')
        with data.open(info) as src, (run / 'icudt79l.dat').open('wb') as dst:
            while True:
                block = src.read(1024 * 1024)
                if not block: break
                space(); dst.write(block)
    source = run / 'icu/source'
    shutil.copyfile(port / 'mh-srvros', source / 'config/mh-srvros')
    command('source-patch', ['patch', '--batch', '--fuzz=0', '-p1', '-i', port / 'srvros-wchar.patch'], run / 'icu', None)
    host, target = run / 'host', run / 'target'
    host.mkdir(); target.mkdir()
    baseenv = os.environ.copy()
    for key in ('CC', 'CXX', 'CFLAGS', 'CXXFLAGS', 'CPPFLAGS', 'LDFLAGS', 'LIBS', 'CPATH',
                'C_INCLUDE_PATH', 'CPLUS_INCLUDE_PATH', 'LIBRARY_PATH', 'PKG_CONFIG_PATH',
                'PKG_CONFIG_LIBDIR', 'PKG_CONFIG_SYSROOT_DIR', 'CONFIG_SITE', 'MAKEFLAGS',
                'MFLAGS', 'CMAKE_PREFIX_PATH', 'ICU_ROOT', 'DYLD_LIBRARY_PATH', 'LD_LIBRARY_PATH',
                'ZIG_LIB_DIR', 'ZIG_LIBC'):
        baseenv.pop(key, None)
    baseenv['ZIG_GLOBAL_CACHE_DIR'] = str(out / 'zig-global-cache')
    baseenv['ZIG_LOCAL_CACHE_DIR'] = str(out / 'zig-local-cache')
    configure = source / 'configure'
    flags = ['--disable-shared', '--enable-static', '--disable-tests', '--disable-samples', '--disable-extras']
    command('host-configure', [configure, *flags, '--with-data-packaging=archive'], host, baseenv)
    command('host-cross-config', ['make', 'config/icucross.mk', 'config/icucross.inc'], host, baseenv)
    build_triplet = subprocess.check_output(['sh', str(source / 'config.guess')], text=True, env=baseenv).strip()
    env = baseenv.copy()
    env.update({'CC': str(root / 'ports/srvros/webkit/cc'), 'CXX': str(root / 'ports/srvros/webkit/cxx'),
                'AR': str(zig) + ' ar', 'RANLIB': str(zig) + ' ranlib', 'icu_cv_host_frag': 'mh-srvros',
                'CPPFLAGS': '-DU_HAVE_NL_LANGINFO_CODESET=0', 'CXXFLAGS': '-frtti -O2'})
    command('target-configure', [configure, '--build=' + build_triplet, '--host=x86_64-unknown-none',
            '--with-cross-build=' + str(host), *flags, '--with-data-packaging=static'], target, env)
    (target / 'lib').mkdir(exist_ok=True)
    command('target-common', ['make', '-C', 'common', '-j2'], target, env)
    command('target-i18n', ['make', '-C', 'i18n', '-j2'], target, env)
    asm = run / 'icudata.S'
    asm.write_text('.section .rodata\n.balign 16\n.globl icudt79_dat\n.type icudt79_dat,@object\nicudt79_dat:\n.incbin ' + json.dumps(str(run / 'icudt79l.dat')) + '\n.size icudt79_dat, .-icudt79_dat\n')
    obj = run / 'icudata.o'
    command('data-object', [root / 'ports/srvros/webkit/cc', '-c', asm, '-o', obj], run, env)
    staged = run / 'sysroot'; (staged / 'include/unicode').mkdir(parents=True); (staged / 'lib').mkdir()
    for part in ('common', 'i18n'):
        for header in (source / part / 'unicode').glob('*.h'):
            shutil.copyfile(header, staged / 'include/unicode' / header.name)
    for name in ('libicuuc.a', 'libicui18n.a'):
        shutil.copyfile(target / 'lib' / name, staged / 'lib' / name)
    command('data-archive', [zig, 'ar', 'rcs', staged / 'lib/libicudata.a', obj], run, env)
    (staged / 'share/licenses/icu').mkdir(parents=True)
    safe_copy(run / 'icu/LICENSE', staged / 'share/licenses/icu/LICENSE')
    elf = run / 'icuprobe.elf'
    command('guest-link', [root / 'ports/srvros/webkit/cxx', '-frtti', '-I', staged / 'include',
        port / 'guest.cc', '-Wl,--start-group', staged / 'lib/libicui18n.a', staged / 'lib/libicuuc.a',
        staged / 'lib/libicudata.a', runtime / 'libcxxabi-rtti.a', runtime / 'libcxx-threaded.a',
        sdk / 'lib/libsrvros.a', '-Wl,--end-group', '-o', elf], run, env)
    audit = run / 'whole-archive.elf'
    audit_ok = command('whole-archive-audit', [root / 'ports/srvros/webkit/cxx', '-frtti', '-I', staged / 'include',
        port / 'guest.cc', '-Wl,--no-gc-sections', '-Wl,--whole-archive', staged / 'lib/libicui18n.a', staged / 'lib/libicuuc.a',
        staged / 'lib/libicudata.a', '-Wl,--no-whole-archive', '-Wl,--start-group',
        runtime / 'libcxxabi-rtti.a', runtime / 'libcxx-threaded.a', sdk / 'lib/libsrvros.a',
        '-Wl,--end-group', '-o', audit], run, env, required=False)
    receipt['whole_archive_audit'] = 'linked' if audit_ok else 'failed; see whole-archive-audit.log'
    install = out / 'sysroot'
    if install.exists():
        raise RuntimeError('existing ICU install present; move it aside before publishing a fresh install')
    space()
    shutil.copytree(staged, install, copy_function=safe_copy)
    safe_copy(elf, out / 'icuprobe.elf')
    paths = [*inputs, port / 'build.py', port / 'guest.cc', port / 'mh-srvros', port / 'srvros-wchar.patch',
             root / 'ports/srvros/webkit/compiler.py', root / 'ports/srvros/webkit/cc', root / 'ports/srvros/webkit/cxx',
             sdk / 'lib/libsrvros.a', sdk / 'lib/crt0.o', sdk / 'lib/app.ld',
             runtime / 'libcxx-threaded.a', runtime / 'receipt.json', rtti_receipt, math_receipt, out / 'icuprobe.elf',
             *[p for p in install.rglob('*') if p.is_file()]]
    receipt['sha256'].update({str(p.relative_to(root)): sha(p) for p in paths})
    receipt['result'] = 'success'
except (OSError, ValueError, KeyError, RuntimeError, subprocess.SubprocessError, tarfile.TarError, zipfile.BadZipFile) as error:
    receipt['error'] = str(error)
finally:
    text = json.dumps(receipt, indent=2) + '\n'
    (run / 'receipt.json').write_text(text)
    (out / 'receipt.json').write_text(text)
    print('ICU receipt:', out / 'receipt.json', 'result:', receipt['result'])
if receipt['result'] != 'success':
    sys.exit(1)
