#!/usr/bin/env python3
"""Reuse the retained WTF Release fixture builder for the signal probe.

This builds a target ELF only. It neither runs the guest nor qualifies signals.
"""
import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[3]
TOOLS = ROOT / 'tools'
sys.path.insert(0, str(TOOLS))


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    argv=sys.argv[1:]
    if '--jsc-receipt' not in argv or argv.index('--jsc-receipt')+1 >= len(argv):
        raise SystemExit('usage: build_wtf_signal_probe.py --jsc-receipt PATH [existing builder options]')
    receipt=Path(argv[argv.index('--jsc-receipt')+1]).resolve()
    data=json.loads(receipt.read_text())
    if data.get('experimental_signal_context') is not True:
        raise SystemExit('JSC receipt does not enable experimental signal-context overlay')
    builder=TOOLS/'browser_wtf_probe.py'
    source=builder.read_text()
    substitutions={
        "FIXTURE = ROOT / 'ports/srvros/webkit/wtf_guest.cpp'":
            "FIXTURE = ROOT / 'ports/srvros/webkit/wtf_signal_guest.cpp'",
        "prefix='wtfprobe-'": "prefix='wtf-signal-'",
    }
    for old,new in substitutions.items():
        if source.count(old)!=1: raise SystemExit('fixture builder changed; review adapter before reuse')
        source=source.replace(old,new,1)
    namespace={'__name__':'__main__','__file__':str(builder)}
    exec(compile(source,str(builder),'exec'),namespace)
    out=namespace.get('receipt_path')
    if not out: raise SystemExit('fixture builder did not produce a receipt')
    out=Path(out)
    record=json.loads(out.read_text())
    if record.get('result')!='linked; not run': raise SystemExit('fixture link did not succeed')
    record['signal_probe_adapter']={'path':str(Path(__file__).resolve()),'sha256':digest(__file__),
                                    'adapted_builder_sha256':hashlib.sha256(source.encode()).hexdigest(),
                                    'experimental_jsc_receipt_sha256':digest(receipt)}
    temp=out.with_suffix('.json.tmp')
    temp.write_text(json.dumps(record,indent=2)+'\n')
    temp.replace(out)
    print('Signal probe receipt:',out)


if __name__=='__main__': main()
