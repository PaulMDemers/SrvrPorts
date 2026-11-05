#!/usr/bin/env python3
"""Regenerate the WTF patch's exact copy of the authoritative native ABI header."""
import argparse
import difflib
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('native_header', type=Path)
args = parser.parse_args()
patch = Path(__file__).with_name('random-entropy.patch')
marker = '--- /dev/null\n+++ b/Source/WTF/wtf/srvros/RandomContract.h\n'
text = patch.read_text()
if text.count(marker) != 1:
    parser.error('expected exactly one final generated header hunk')
prefix, previous = text.split(marker)
if '\n--- ' in previous:
    parser.error('generated header must be the last file in the patch')
header = args.native_header.read_text()
patch.write_text(prefix + ''.join(difflib.unified_diff(
    [], header.splitlines(True), fromfile='/dev/null',
    tofile='b/Source/WTF/wtf/srvros/RandomContract.h')))
