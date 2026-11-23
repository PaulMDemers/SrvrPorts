#!/usr/bin/env python3
"""Stage the pinned libmagic sources and build a same-release native database."""

import argparse
from hashlib import sha256
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile


ARCHIVE_SHA256 = 'ed14656883b23a364b4057c05595d93252da9bc473d30106519519d0da141283'
DATABASE_SHA256 = '1ac18f5aeee092a9ab01a7ba5b714b17651e190c29e0192136e858863c1fcbf6'
FRAGMENTS = ('archive', 'compress', 'elf', 'images', 'jpeg', 'pdf', 'sql', 'zip')
DISABLED_FEATURES = (
    '--disable-shared', '--disable-libseccomp', '--disable-landlock',
    '--disable-bzlib', '--disable-xzlib', '--disable-zlib', '--disable-zstdlib',
    '--disable-lzlib', '--disable-lrziplib',
)
CHECKPOINTS = (
    '\tfor (magindex = 0; magindex < nmagic; magindex++) {',
    '\t\t\twhile (magindex < nmagic - 1 &&\n\t\t\t    magic[magindex + 1].cont_level != 0) {',
    '\t\twhile (magindex + 1 < nmagic &&\n\t\t    magic[magindex + 1].cont_level != 0) {',
    '\t\tfor (idx = 0; m->str_range == 0 || idx < m->str_range; idx++) {',
)


def extract(archive, destination):
    if sha256(archive.read_bytes()).hexdigest() != ARCHIVE_SHA256:
        raise ValueError('file 5.48 archive SHA-256 mismatch')
    sources = destination / 'file-5.48'
    if sources.exists():
        shutil.rmtree(sources)
    with tarfile.open(archive, 'r:gz') as tar:
        members = tar.getmembers()
        for member in members:
            parts = Path(member.name).parts
            if (not parts or parts[0] != sources.name or '..' in parts
                    or not (member.isfile() or member.isdir())):
                raise ValueError(f'unexpected release archive member {member.name}')
        tar.extractall(destination, members=members, filter='data')
    return sources


def add_checkpoints(source):
    softmagic = source / 'src/softmagic.c'
    text = softmagic.read_text(encoding='utf-8')
    header = '#include "file.h"'
    if text.count(header) != 1:
        raise ValueError('upstream softmagic include changed')
    text = text.replace(header, header + '\nextern void file_magic_checkpoint(void);', 1)
    for needle in CHECKPOINTS:
        if text.count(needle) != 1:
            raise ValueError(f'upstream softmagic loop changed: {needle.strip()}')
        indent = needle[:len(needle) - len(needle.lstrip())]
        text = text.replace(needle, needle + '\n' + indent + '\tfile_magic_checkpoint();', 1)
    softmagic.write_text(text, encoding='utf-8')


def prepare(archive, destination):
    destination = destination.resolve()
    destination.mkdir(parents=True, exist_ok=True)
    source = extract(archive, destination)
    env = os.environ.copy()
    env.pop('MAKEFLAGS', None)
    env['CC'] = env.get('HOSTCC') or 'cc'
    env['LC_ALL'] = 'C'
    subprocess.run(['./configure', *DISABLED_FEATURES], cwd=source, env=env, check=True)
    subprocess.run(['make', '-j1', '-C', 'src', 'magic.h', 'file'], cwd=source, env=env, check=True)

    with tempfile.TemporaryDirectory(prefix='database-', dir=destination) as temporary:
        directory = Path(temporary)
        rules = directory / 'bounded.magic'
        with rules.open('wb') as output:
            for name in FRAGMENTS:
                output.write((source / 'magic/Magdir' / name).read_bytes())
        # The database records this filename. Use the same relative name everywhere.
        subprocess.run([source / 'src/file', '-C', '-m', rules.name],
                       cwd=directory, env=env, check=True)
        database = rules.with_suffix('.magic.mgc')
        if database.stat().st_size != 2001456 or sha256(database.read_bytes()).hexdigest() != DATABASE_SHA256:
            raise ValueError('native compiler produced an unexpected little-endian database')
        add_checkpoints(source)
        database.replace(destination / database.name)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('archive', type=Path)
    parser.add_argument('destination', type=Path)
    args = parser.parse_args()
    prepare(args.archive, args.destination)


if __name__ == '__main__':
    main()
