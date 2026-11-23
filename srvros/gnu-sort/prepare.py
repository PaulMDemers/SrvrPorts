#!/usr/bin/env python3
"""Extract the exact sequential merge and numeric routines from GNU 9.12."""
import hashlib
import pathlib
import sys
import tarfile

EXPECTED = 'a480198559733e9b3da999e90543ac6f888a2caa544d8d664c5a1f17e528e210'
if len(sys.argv) != 3:
    sys.exit('usage: prepare.py ARCHIVE OUTPUT_DIRECTORY')
archive, output = map(pathlib.Path, sys.argv[1:])
if hashlib.sha256(archive.read_bytes()).hexdigest() != EXPECTED:
    sys.exit('coreutils archive SHA256 mismatch')
with tarfile.open(archive, 'r:xz') as tar:
    def read(name):
        f = tar.extractfile('coreutils-9.12/' + name)
        if f is None:
            sys.exit('missing upstream source: ' + name)
        return f.read().decode('utf-8')
    sort = read('src/sort.c')
    numeric = read('lib/strnumcmp-in.h')

def between(text, start, end):
    if text.count(start) != 1 or text.count(end) != 1:
        sys.exit('unexpected upstream extraction anchors')
    return text[text.index(start):text.index(end)]

merge = between(sort, 'static void\nmergelines (', '\nstatic struct merge_node *init_node')
merge = merge.replace('struct line', 'struct gsort_line')
# Keep upstream code intact except for the adapted line type and checkpoint.
merge = merge.replace('  if (nlines == 2)\n', '  gsort_checkpoint ();\n  if (nlines == 2)\n', 1)
numeric = between(numeric, '# define NEGATION_SIGN', '\n#endif')
numeric = numeric.replace('_GL_ATTRIBUTE_PURE', '')
output.mkdir(parents=True, exist_ok=True)
(output / 'numeric.inc').write_text(numeric)
(output / 'merge.inc').write_text(merge)
