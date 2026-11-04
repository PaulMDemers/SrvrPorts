#!/usr/bin/env python3
"""Link the experimental JSC GC-root guest with the retained Release build.

No guest is run. The generated receipt must contain gc_root_probe_adapter.
"""
import hashlib
import json
from pathlib import Path
import sys

ROOT=Path(__file__).resolve().parents[3]
TOOLS=ROOT/'tools'
sys.path.insert(0,str(TOOLS))


def digest(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    args=sys.argv[1:]
    if '--jsc-receipt' not in args or args.index('--jsc-receipt')+1>=len(args):
        raise SystemExit('usage: build_jsc_gc_roots_probe.py --jsc-receipt PATH [existing builder options]')
    receipt=Path(args[args.index('--jsc-receipt')+1]).resolve()
    if json.loads(receipt.read_text()).get('experimental_signal_context') is not True:
        raise SystemExit('experimental signal-context JSC receipt required')
    builder=TOOLS/'browser_wtf_probe.py';source=builder.read_text()
    substitutions={
        "FIXTURE = ROOT / 'ports/srvros/webkit/wtf_guest.cpp'":
            "FIXTURE = ROOT / 'ports/srvros/webkit/jsc_gc_roots_guest.cpp'",
        "prefix='wtfprobe-'": "prefix='jsc-gc-roots-'",
        "command[-1] = str(FIXTURE); result['ninja_profile'] = entry":
            "command[-1] = str(FIXTURE); command.insert(1, '-I'+str(build/'JavaScriptCore/Headers')); result['ninja_profile'] = entry",
        "archives = [build/'lib/libWTF.a'":
            "archives = [build/'lib/libJavaScriptCore.a',build/'lib/libWTF.a'",
        "invoke('link',link,entry['directory'],120,result,stem.with_suffix('.link.log'))":
            "asm_source=ROOT/'ports/srvros/webkit/jsc_gc_roots_guest.S'\n"
            "    asm_obj=stem.with_suffix('.asm.o')\n"
            "    invoke('assemble',[str(zig),'cc','-target','x86_64-freestanding-none','-mno-red-zone','-c',str(asm_source),'-o',str(asm_obj)],ROOT,60,result,stem.with_suffix('.asm.log'))\n"
            "    link.insert(link.index(str(obj))+1,str(asm_obj))\n"
            "    invoke('link',link,entry['directory'],120,result,stem.with_suffix('.link.log'))",
        "if args.build_helper_snapshot: paths.append(args.build_helper_snapshot.resolve())":
            "paths.extend([asm_source,asm_obj])\n    if args.build_helper_snapshot: paths.append(args.build_helper_snapshot.resolve())",
    }
    for old,new in substitutions.items():
        if source.count(old)!=1: raise SystemExit('fixture builder changed; review this adapter')
        source=source.replace(old,new,1)
    namespace={'__name__':'__main__','__file__':str(builder)}
    exec(compile(source,str(builder),'exec'),namespace)
    out=namespace.get('receipt_path')
    if not out: raise SystemExit('fixture builder did not produce a receipt')
    out=Path(out);record=json.loads(out.read_text())
    if record.get('result')!='linked; not run':raise SystemExit('fixture link failed')
    record['gc_root_probe_adapter']={'path':str(Path(__file__).resolve()),'sha256':digest(__file__),
                                     'adapted_builder_sha256':hashlib.sha256(source.encode()).hexdigest(),
                                     'experimental_jsc_receipt_sha256':digest(receipt)}
    temp=out.with_suffix('.json.tmp');temp.write_text(json.dumps(record,indent=2)+'\n');temp.replace(out)
    print('GC root probe receipt:',out)


if __name__=='__main__':main()
