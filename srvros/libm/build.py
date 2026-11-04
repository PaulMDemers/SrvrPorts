#!/usr/bin/env python3
"""Build pinned musl scalar math supplement with private accurate dependencies."""
import hashlib,json,os,shutil,signal,subprocess,sys,tempfile,time
from pathlib import Path
root=Path(__file__).resolve().parents[3];out=root/'build/browser-libm';out.mkdir(parents=True,exist_ok=True)
run=Path(tempfile.mkdtemp(prefix='run-',dir=out));port=root/'ports/srvros/libm';zig=root/'build/tooling/zig/zig';musl=root/'build/tooling/zig/lib/libc/musl';math=musl/'src/math';sdk=root/'build/sysroot/srvros'
sys.path.insert(0,str(root/'tools'));from browser_evidence import verify_runtime
public=['sinh','sinhf','cosh','coshf','tanh','asinh','asinhf','acosh','acoshf','atanh','atanhf','cbrt','cbrtf','expm1','log1p','log1pf','hypot']
private=['exp','expf','log','logf','sqrt','sqrtf','expm1f']
internal=['__expo2','__expo2f','__exp_data','__exp2f_data','__log_data','__logf_data','__math_xflow','__math_uflow','__math_oflow','__math_divzero','__math_invalid','__math_xflowf','__math_uflowf','__math_oflowf','__math_divzerof','__math_invalidf']
units=public+['expm1f','exp','expf','log','logf','exp_data','exp2f_data','log_data','logf_data','__expo2','__expo2f','__math_xflow','__math_uflow','__math_oflow','__math_divzero','__math_invalid','__math_xflowf','__math_uflowf','__math_oflowf','__math_divzerof','__math_invalidf']
receipt={'result':'failed','scope':'target archive and host numerical tests; no guest acceptance','commands':[]}
env=os.environ.copy()
for key in ('CC','CFLAGS','CPPFLAGS','LDFLAGS','LIBS','CPATH','C_INCLUDE_PATH','LIBRARY_PATH','SDKROOT','ZIG_LIB_DIR','ZIG_LOCAL_CACHE_DIR','ZIG_GLOBAL_CACHE_DIR'):env.pop(key,None)
env['ZIG_LOCAL_CACHE_DIR']=str(run/'zig-cache');env['ZIG_GLOBAL_CACHE_DIR']=str(run/'zig-global-cache')
def sha(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for b in iter(lambda:f.read(1024*1024),b''):h.update(b)
 return h.hexdigest()
def cmd(name,argv):
 if shutil.disk_usage(out).free < 2*1024**3:raise RuntimeError('2 GiB reserve reached')
 argv=list(map(str,argv));t=time.monotonic()
 with (run/(name+'.log')).open('w') as log:
  log.write(json.dumps(argv)+'\n');log.flush();p=subprocess.Popen(argv,cwd=root,env=env,stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
  try:rc=p.wait(timeout=60)
  except subprocess.TimeoutExpired:os.killpg(p.pid,signal.SIGKILL);p.wait();rc=None
 receipt['commands'].append({'name':name,'argv':argv,'status':rc,'seconds':round(time.monotonic()-t,3)})
 if rc!=0:raise RuntimeError(name+' failed: '+str(run/(name+'.log')))
try:
 runtime=verify_runtime();cmd('version',[zig,'version'])
 if (run/'version.log').read_text().splitlines()[-1]!='0.15.2':raise ValueError('Zig 0.15.2 required')
 shutil.copyfile(musl/'src/internal/libm.h',run/'libm.h');shutil.copyfile(musl/'arch/generic/fp_arch.h',run/'fp_arch.h')
 for h in ['exp_data.h','exp2f_data.h','log_data.h','logf_data.h']:shutil.copyfile(math/h,run/h)
 flags=['-std=c11','-O2','-fno-builtin','-ffunction-sections','-fdata-sections','-I',port,'-I',run,'-include',port/'compat.h']+[f'-D{x}=srv_libm_private_{x.lstrip("_")}' for x in private+internal]
 objects=[]
 for name in units+['sqrt','sqrtf']:
  source=(math/'x86_64'/f'{name}.c') if name in ('sqrt','sqrtf') else math/f'{name}.c'
  obj=run/(name+'.o')
  extra=['-Dacosh=srv_libm_private_acosh'] if name=='acosh' else ['-Dacoshf=srv_libm_private_acoshf'] if name=='acoshf' else []
  cmd('cc-'+name,[root/'ports/srvros/webkit/cc',*flags,*extra,'-c',source,'-o',obj]);objects.append(obj)
 domain=run/'domain.o';cmd('cc-domain',[root/'ports/srvros/webkit/cc',*flags,'-DORIG_ACOSH=srv_libm_private_acosh','-DORIG_ACOSHF=srv_libm_private_acoshf','-DINVALID_DOUBLE=srv_libm_private_math_invalid','-DINVALID_FLOAT=srv_libm_private_math_invalidf','-c',port/'domain.c','-o',domain]);objects.append(domain)
 archive=out/'libsrvros-jsc-math.a';archive.unlink(missing_ok=True);cmd('archive',[zig,'ar','rcs',archive,*objects])
 elf=out/'libmprobe.elf';cmd('guest-link',[root/'ports/srvros/webkit/cc','-std=c11','-fno-builtin',port/'probe.c',archive,'-o',elf])
 cmd('host-comparison',[sys.executable,port/'host_test.py'])
 inputs=[Path(__file__).resolve(),port/'host_test.py',out/'host-receipt.json',port/'domain.c',port/'compat.h',port/'endian.h',port/'probe.c',zig,musl/'COPYRIGHT',musl/'src/internal/libm.h',musl/'arch/generic/fp_arch.h',*[math/(x+'.c') for x in units],math/'x86_64/sqrt.c',math/'x86_64/sqrtf.c',*[math/x for x in ('exp_data.h','exp2f_data.h','log_data.h','logf_data.h')],archive,elf]
 receipt.update(result='success',runtime=runtime,public_symbols=public,sha256={str(p.relative_to(root)):sha(p) for p in inputs})
except (OSError,KeyError,ValueError,RuntimeError) as exc:receipt['error']=str(exc)
finally:
 data=json.dumps(receipt,indent=2)+'\n';(run/'receipt.json').write_text(data);(out/'receipt.json').write_text(data);print('Math supplement receipt:',out/'receipt.json','result:',receipt['result'])
sys.exit(0 if receipt['result']=='success' else 1)
