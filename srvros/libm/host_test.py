#!/usr/bin/env python3
"""Compare renamed musl target algorithms and private dependencies to host libm."""
import json,os,platform,shutil,subprocess,sys,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[3];port=r/'ports/srvros/libm';out=r/'build/browser-libm';out.mkdir(parents=True,exist_ok=True);run=Path(tempfile.mkdtemp(prefix='host-',dir=out));musl=r/'build/tooling/zig/lib/libc/musl';m=musl/'src/math';cc=shutil.which('cc')
if not cc:sys.exit('host cc missing')
if platform.machine() not in ('x86_64','AMD64') and not (sys.platform=='darwin' and platform.machine()=='arm64'):sys.exit('requires x86_64 host or Darwin Rosetta')
shutil.copyfile(musl/'src/internal/libm.h',run/'libm.h');shutil.copyfile(musl/'arch/generic/fp_arch.h',run/'fp_arch.h')
for h in ('exp_data.h','exp2f_data.h','log_data.h','logf_data.h'):shutil.copyfile(m/h,run/h)
if sys.platform=='darwin': (run/'features.h').write_text('/* musl visibility is supplied by compat.h */\n')
pub=['sinh','sinhf','cosh','coshf','tanh','asinh','asinhf','acosh','acoshf','atanh','atanhf','cbrt','cbrtf','expm1','log1p','log1pf','hypot'];private=['exp','expf','log','logf','sqrt','sqrtf','expm1f'];internal=['__expo2','__expo2f','__exp_data','__exp2f_data','__log_data','__logf_data','__math_xflow','__math_uflow','__math_oflow','__math_divzero','__math_invalid','__math_xflowf','__math_uflowf','__math_oflowf','__math_divzerof','__math_invalidf'];units=pub+['expm1f','exp','expf','log','logf','exp_data','exp2f_data','log_data','logf_data','__expo2','__expo2f','__math_xflow','__math_uflow','__math_oflow','__math_divzero','__math_invalid','__math_xflowf','__math_uflowf','__math_oflowf','__math_divzerof','__math_invalidf']
arch=['-arch','x86_64'] if sys.platform=='darwin' else []
flags=arch+['-std=c11','-O2','-fno-builtin','-I',str(port),'-I',str(run),'-include',str(port/'compat.h')]+[f'-D{x}=test_{x}' for x in pub]+[f'-D{x}=test_private_{x.lstrip("_")}' for x in private+internal];objs=[];commands=[]
def cmd(argv):
 argv=list(map(str,argv));commands.append(argv)
 with (run/'build.log').open('a') as log:
  log.write(json.dumps(argv)+'\n');log.flush();p=subprocess.run(argv,stdout=log,stderr=subprocess.STDOUT,timeout=60)
 if p.returncode:raise RuntimeError('host command failed; see '+str(run/'build.log'))
try:
 for name in units+['sqrt','sqrtf']:
  if shutil.disk_usage(out).free<2*1024**3:raise RuntimeError('2 GiB reserve reached')
  source=m/'x86_64'/f'{name}.c' if name in ('sqrt','sqrtf') else m/f'{name}.c';obj=run/(name+'.o');extra=['-Uacosh','-Dacosh=test_private_acosh'] if name=='acosh' else ['-Uacoshf','-Dacoshf=test_private_acoshf'] if name=='acoshf' else [];cmd([cc,*flags,*extra,'-c',source,'-o',obj]);objs.append(obj)
 domain=run/'domain.o';cmd([cc,*flags,'-DORIG_ACOSH=test_private_acosh','-DORIG_ACOSHF=test_private_acoshf','-DINVALID_DOUBLE=test_private_math_invalid','-DINVALID_FLOAT=test_private_math_invalidf','-c',port/'domain.c','-o',domain]);objs.append(domain)
 exe=run/'host-test';cmd([cc,*arch,'-std=c11','-O2','-fno-builtin','-DHOST_COMPARE',port/'probe.c',*objs,'-lm','-o',exe])
 result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=60);(run/'test.log').write_text(result.stdout+result.stderr)
 if result.returncode or 'libmprobe-host: ok' not in result.stdout:raise RuntimeError('numeric comparison failed: '+str(run/'test.log'))
 data=json.dumps({'result':'success','commands':commands,'host_test':result.stdout.strip(),'run':str(run.relative_to(r))},indent=2)+'\n';(run/'receipt.json').write_text(data);(out/'host-receipt.json').write_text(data);print(result.stdout,end='');print('Host receipt:',run/'receipt.json')
except (OSError,RuntimeError,subprocess.SubprocessError) as e:
 (run/'receipt.json').write_text(json.dumps({'result':'failed','error':str(e),'commands':commands},indent=2)+'\n');print(e,file=sys.stderr);sys.exit(1)
