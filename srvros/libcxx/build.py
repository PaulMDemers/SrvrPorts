#!/usr/bin/env python3
"""Build an experimental threaded libc++ archive and guest; no VM image."""
import hashlib
import json
import os
import signal
from pathlib import Path
import subprocess
import sys
import time

root = Path(__file__).resolve().parents[3]
out = root / 'build/browser-runtime'
out.mkdir(parents=True, exist_ok=True)
zig = root / 'build/tooling/zig/zig'
sysroot = root / 'build/sysroot/srvros'
if not zig.exists() or not (sysroot / '.ready').exists():
    sys.exit('SDK missing: run make srvros-sysroot')
if subprocess.run([str(zig), 'version'], capture_output=True, text=True, timeout=60).stdout.strip() != '0.15.2':
    sys.exit('Zig 0.15.2 required')
z = root / 'build/tooling/zig/lib'
p = root / 'ports/srvros/libcxx'
flags = ['-target','x86_64-freestanding-none','-std=c++23','-ffreestanding','-fno-exceptions','-fno-rtti','-fno-sanitize=undefined','-fno-stack-protector','-fno-stack-check','-fno-pic','-ffunction-sections','-fdata-sections','-mno-red-zone','-O2','-nostdinc++','-include',str(p/'config.h'),'-I',str(z/'libcxx/include'),'-I',str(z/'libcxx/src/include'),'-I',str(z/'libcxxabi/include'),'-I',str(z/'include'),'-isystem',str(sysroot/'include')]
units = [z/'libcxx/src'/n for n in ['thread.cpp','mutex.cpp','mutex_destructor.cpp','condition_variable.cpp','condition_variable_destructor.cpp','call_once.cpp','chrono.cpp','future.cpp']]+[p/'fail_fast.cc']
commands=[]
def run(name, cmd):
    cmd=list(map(str, cmd)); start=time.monotonic()
    with (out/(name+'.log')).open('w') as log:
        log.write(json.dumps(cmd)+'\n'); log.flush()
        try:
            proc=subprocess.Popen(cmd,cwd=root,stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
            try:
                status=proc.wait(timeout=60)
            except subprocess.TimeoutExpired:
                os.killpg(proc.pid,signal.SIGKILL)
                proc.wait()
                status=None
                log.write('TIMEOUT: process group killed\n')
        except OSError as exc:
            status=None
            log.write(str(exc)+'\n')
    commands.append({'name':name,'argv':cmd,'seconds':round(time.monotonic()-start,3),'status':status})
    if status != 0:
        (out/'receipt.json').write_text(json.dumps({'result':'failed','commands':commands,'scope':'compile/link only; no guest result'},indent=2)+'\n')
        sys.exit('failed: '+str(out/(name+'.log')))
objects=[]
for src in units+[p/'guest.cc',p/'main_tls.cc',p/'tlsfirst.cc']:
    obj=out/(src.stem+'.o')
    run(src.stem,[zig,'c++',*flags,'-c',src,'-o',obj])
    if src not in (p/'guest.cc', p/'main_tls.cc', p/'tlsfirst.cc'): objects.append(obj)
archive=out/'libcxx-threaded.a'
archive.unlink(missing_ok=True)
run('archive',[zig,'ar','rcs',archive,*objects])
elf=out/'cxxthreads.elf'
run('link',[zig,'ld.lld','-m','elf_x86_64','-nostdlib','-static','-z','max-page-size=0x1000','--gc-sections','-T',sysroot/'lib/app.ld',sysroot/'lib/crt0.o',out/'guest.o',out/'main_tls.o',archive,sysroot/'lib/libsrvros.a','-o',elf])
tlsfirst = out/'cxxthreads-tlsfirst.elf'
run('tlsfirst_link',[zig,'ld.lld','-m','elf_x86_64','-nostdlib','-static','-z','max-page-size=0x1000','--gc-sections','-T',sysroot/'lib/app.ld',sysroot/'lib/crt0.o',out/'tlsfirst.o',sysroot/'lib/libsrvros.a','-o',tlsfirst])
def sha(path):
    h=hashlib.sha256()
    with path.open('rb') as f:
        for chunk in iter(lambda:f.read(1024*1024),b''): h.update(chunk)
    return h.hexdigest()
inputs=[zig, p/'config.h',p/'guest.cc',p/'main_tls.cc',p/'tlsfirst.cc',Path(__file__),*units,root/'userspace/lib/src/cxx_runtime.cc',sysroot/'lib/libsrvros.a',sysroot/'lib/crt0.o',sysroot/'lib/app.ld',archive,elf,tlsfirst]
tree=hashlib.sha256()
for header in sorted((sysroot/'include').rglob('*')):
    if header.is_file():
        tree.update(str(header.relative_to(sysroot)).encode()+b'\0'+bytes.fromhex(sha(header)))
receipt={'result':'success','sdk_headers_sha256':tree.hexdigest(),'zig':'0.15.2','commands':commands,'sha256':{str(x.relative_to(root)):sha(x) for x in inputs},'scope':'compile/link only; no guest result'}
(out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
print(elf)
