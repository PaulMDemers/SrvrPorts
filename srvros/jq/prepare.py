#!/usr/bin/env python3
"""Create jq's three configure-generated includes outside the source tree."""
from pathlib import Path
import sys

source, output = map(Path, sys.argv[1:])
generated = output / 'src'
generated.mkdir(parents=True, exist_ok=True)
data = (source / 'src/builtin.jq').read_bytes()
(generated / 'builtin.inc').write_text(''.join(f'0{byte:03o},\n' for byte in data), encoding='ascii')
(generated / 'version.h').write_text('#define JQ_VERSION "1.8.2"\n', encoding='ascii')
(generated / 'config_opts.inc').write_text('#define JQ_CONFIG "SrvrOS static; decNumber; without Oniguruma"\n', encoding='ascii')
