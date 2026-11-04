#!/usr/bin/env python3
"""Build experimental no-locale libc++ filesystem archive and guest probe."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import tempfile
import time

root = Path(__file__).resolve().parents[3]
out = root/'build/browser-filesystem'
out.mkdir(parents=True, exist_ok=True)
run = Path(tempfile.mkdtemp(prefix='run-', dir=out))
port = root/'ports/srvros/libcxx'
sdk = root/'build/sysroot/srvros'
zig = root/'build/tooling/zig/zig'
src = root/'build/tooling/zig/lib/libcxx/src'
commands=[]
env=os.environ.copy()
for key in ('CC','CXX','CFLAGS','CXXFLAGS','CPPFLAGS','LDFLAGS','LIBS','CPATH','C_INCLUDE_PATH','CPLUS_INCLUDE_PATH','LIBRARY_PATH','SDKROOT','MACOSX_DEPLOYMENT_TARGET','ZIG_LIB_DIR','ZIG_LOCAL_CACHE_DIR','ZIG_GLOBAL_CACHE_DIR'):
    env.pop(key,None)
env['ZIG_LOCAL_CACHE_DIR']=str(run/'zig-cache')
env['ZIG_GLOBAL_CACHE_DIR']=str(run/'zig-global-cache')
receipt={'result':'failed','scope':'experimental compile/link; no guest acceptance', 'commands':commands}
def digest(p):
    h=hashlib.sha256()
    with p.open('rb') as f:
        for b in iter(lambda:f.read(1024*1024),b''): h.update(b)
    return h.hexdigest()
def cmd(name,args):
    if shutil.disk_usage(out).free < 2*1024**3: raise RuntimeError('2 GiB disk reserve')
    args=list(map(str,args)); t=time.monotonic()
    with (run/(name+'.log')).open('w') as log:
        log.write(json.dumps(args)+'\n'); log.flush()
        proc=subprocess.Popen(args,cwd=root,env=env,stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
        try: rc=proc.wait(timeout=60)
        except subprocess.TimeoutExpired:
            os.killpg(proc.pid,signal.SIGKILL);proc.wait();rc=None
    commands.append({'name':name,'argv':args,'status':rc,'seconds':round(time.monotonic()-t,3)})
    if rc != 0: raise RuntimeError(name+' failed: '+str(run/(name+'.log')))
try:
    sys.path.insert(0,str(root/'tools'))
    from browser_evidence import verify_runtime
    runtime=verify_runtime()
    rtti_path=root/'build/browser-runtime/rtti-receipt.json'
    rtti=json.loads(rtti_path.read_text())
    if rtti.get('result')!='success' or rtti.get('sdk_headers_sha256')!=runtime['sdk_headers_sha256']:
        raise ValueError('RTTI receipt SDK mismatch')
    for name in ('build/browser-runtime/libcxxabi-rtti.a','build/browser-runtime/libcxx-threaded.a'):
        if name not in rtti['sha256']: raise ValueError('RTTI receipt missing '+name)
    for name,expected in rtti['sha256'].items():
        path=Path(name)
        if path.is_absolute() or '..' in path.parts or not (root/path).is_file() or digest(root/path)!=expected:
            raise ValueError('RTTI receipt mismatch '+name)
    cmd('zig-version',[zig,'version'])
    if (run/'zig-version.log').read_text().splitlines()[-1].strip()!='0.15.2': raise RuntimeError('Zig 0.15.2 required')
    shutil.copytree(src/'filesystem',run/'filesystem')
    cmd('patch',['patch','--batch','--fuzz=0','-p1','-d',run,'-i',port/'patches/filesystem-copy.patch'])
    tree=hashlib.sha256()
    for item in sorted((run/'filesystem').rglob('*')):
        if item.is_file(): tree.update(str(item.relative_to(run)).encode()+b'\0'+bytes.fromhex(digest(item)))
    receipt['patched_filesystem_tree_sha256']=tree.hexdigest()
    profile=run/'profile.cc'
    profile.write_text('#include <__config>\n#if !_LIBCPP_HAS_FILESYSTEM || _LIBCPP_HAS_LOCALIZATION || !_LIBCPP_HAS_THREADS || !_LIBCPP_HAS_THREAD_API_PTHREAD\n#error invalid_libcxx_profile\n#endif\nint profile;\n')
    cmd('profile-check',[port.parent/'webkit/cxx','-D_LIBCPP_HAS_FILESYSTEM=1','-c',profile,'-o',run/'profile.o'])
    receipt['libcxx_profile']={'filesystem':1,'localization':0,'threads':1,'pthread':1}
    flags=['-D_LIBCPP_HAS_FILESYSTEM=1','-ffunction-sections','-fdata-sections','-I',src/'include','-I',root/'build/tooling/zig/lib/libcxxabi/include','-c']
    units=[run/'filesystem'/n for n in ('directory_entry.cpp','directory_iterator.cpp','filesystem_clock.cpp','filesystem_error.cpp','operations.cpp','path.cpp')]+[src/n for n in ('string.cpp','system_error.cpp','error_category.cpp','memory.cpp','stdexcept.cpp')]
    objects=[]
    for source in units:
        obj=run/(source.stem+'.o');cmd('compile-'+source.stem,[port.parent/'webkit/cxx',*flags,source,'-o',obj]);objects.append(obj)
    archive=out/'libcxx-filesystem.a'
    archive.unlink(missing_ok=True)
    cmd('archive',[zig,'ar','rcs',archive,*objects])
    elf=out/'fsprobe.elf'
    cmd('guest-link',[port.parent/'webkit/cxx','-D_LIBCPP_HAS_FILESYSTEM=1','-ffunction-sections','-fdata-sections',port/'filesystem_guest.cc','-Wl,--start-group',archive,root/'build/browser-runtime/libcxx-threaded.a',root/'build/browser-runtime/libcxxabi-rtti.a',sdk/'lib/libsrvros.a','-Wl,--end-group','-o',elf])
    originals=[src/'filesystem'/n for n in ('directory_entry.cpp','directory_iterator.cpp','filesystem_clock.cpp','filesystem_error.cpp','operations.cpp','path.cpp')]
    inputs=[Path(__file__).resolve(),port/'filesystem_guest.cc',port/'patches/filesystem-copy.patch',port/'config.h',zig,*originals,*units[6:],sdk/'lib/libsrvros.a',sdk/'lib/crt0.o',sdk/'lib/app.ld',sdk/'include/sys/statvfs.h',port.parent/'webkit/compiler.py',port.parent/'webkit/cxx',port.parent/'webkit/cc',root/'build/browser-runtime/libcxx-threaded.a',root/'build/browser-runtime/libcxxabi-rtti.a',root/'build/browser-runtime/receipt.json',rtti_path,archive,elf]
    receipt.update(result='success',runtime=runtime,sha256={str(p.relative_to(root)):digest(p) for p in inputs})
except (OSError, RuntimeError, ValueError, KeyError) as exc:
    receipt['error']=str(exc)
finally:
    data=json.dumps(receipt,indent=2)+'\n';(run/'receipt.json').write_text(data);(out/'receipt.json').write_text(data)
    print('Filesystem receipt:',out/'receipt.json','result:',receipt['result'])
sys.exit(0 if receipt['result']=='success' else 1)
