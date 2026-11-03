#!/usr/bin/env python3
"""Prepare pinned One True Awk in an out-of-tree directory."""
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys

PIN = "5739fd79bcfc75ba7526773d0cf634521f8aca3c"
FILES = ("awk.h", "proto.h", "awkgram.y", "maketab.c", "b.c", "main.c",
         "parse.c", "tran.c", "lib.c", "run.c", "lex.c")


def replace_once(text, old, new, name):
    if text.count(old) != 1:
        raise SystemExit(f"{name}: expected exactly one matching upstream fragment")
    return text.replace(old, new, 1)


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def replace_region(text, start, end, expected_hash, replacement):
    if text.count(start) != 1 or text.count(end) != 1:
        raise SystemExit("main.c: expected unique signal block delimiters")
    a = text.index(start)
    b = text.index(end, a) + len(end)
    if sha256(text[a:b].encode()) != expected_hash:
        raise SystemExit("main.c: unexpected signal block contents")
    return text[:a] + replacement + text[b:]


def main():
    if len(sys.argv) != 3:
        raise SystemExit("usage: prepare.py UPSTREAM OUTPUT")
    source, dest = map(lambda x: Path(x).resolve(), sys.argv[1:])
    if source == dest or source in dest.parents or dest in source.parents:
        raise SystemExit("output and upstream must be separate directories")
    # Invalidate an earlier result before any verification or regeneration.
    dest.mkdir(parents=True, exist_ok=True)
    (dest / ".prepared").unlink(missing_ok=True)
    rev = subprocess.check_output(["git", "-C", str(source), "rev-parse", "HEAD"], text=True).strip()
    if rev != PIN:
        raise SystemExit(f"expected upstream {PIN}, got {rev}")
    # Refuse modified inputs even at the pinned commit.
    subprocess.run(["git", "-C", str(source), "diff", "--quiet", "HEAD", "--", *FILES], check=True)
    for name in FILES:
        shutil.copyfile(source / name, dest / name)
    main_path = dest / "main.c"
    main = main_path.read_text()
    main = replace_region(main, "static noreturn void fpecatch(int n\n",
                          "}\n\n/* Can this work with recursive calls?",
                          "24b5480bb051c496525fd6acd9b99c102d307958e92556287889a0d3d244cc2d",
                          "/* SrvrOS has no SIGFPE delivery. */\n\n/* Can this work with recursive calls?")
    main = replace_region(main, "#ifdef SA_SIGINFO\n\t{\n\t\tstruct sigaction sa;",
                          "\t/*signal(SIGSEGV, segvcatch); experiment */",
                          "7650642aedb3859f0b82402d29a64a0a54c4b0b38e4f1188d3500f4b3b646e88",
                          "/* No floating-point signal handler on SrvrOS. */")
    main = replace_once(main, "awk_mb_cur_max = MB_CUR_MAX;", "awk_mb_cur_max = 1; /* SrvrOS C locale */", "main.c locale")
    main = replace_once(main, "srandom((unsigned long) srand_seed);", "srvros_awk_srandom((unsigned long) srand_seed);", "main.c seed")
    main_path.write_text(main)
    run_path = dest / "run.c"
    run = run_path.read_text()
    for old, new in (("wctomb(NULL, L'\\0')", "srvros_awk_wctomb(NULL, L'\\0')"),
                     ("wctomb(pbuf, fun_wc(wc))", "srvros_awk_wctomb(pbuf, fun_wc(wc))"),
                     ("(Awkfloat) random()", "(Awkfloat) srvros_awk_random()"),
                     ("srandom((unsigned long) u)", "srvros_awk_srandom((unsigned long) u)")):
        run = replace_once(run, old, new, "run.c")
    run_path.write_text(run)
    bison = shlex.split(os.environ.get("BISON", "bison"))
    hostcc = shlex.split(os.environ.get("HOSTCC", "cc"))
    subprocess.run(bison + ["-d", "awkgram.y"], cwd=dest, check=True)
    subprocess.run(hostcc + ["maketab.c", "-o", "maketab"], cwd=dest, check=True)
    with (dest / "proctab.c").open("w") as output:
        subprocess.run([str(dest / "maketab"), "awkgram.tab.h"], cwd=dest, stdout=output, check=True)
    outputs = (*FILES, "awkgram.tab.c", "awkgram.tab.h", "proctab.c")
    manifest = {
        "upstream_commit": rev,
        "inputs_sha256": {name: sha256((source / name).read_bytes()) for name in FILES},
        "outputs_sha256": {name: sha256((dest / name).read_bytes()) for name in outputs},
        "tools": {
            "bison": {"argv": bison + ["-d", "awkgram.y"],
                      "version": subprocess.check_output(bison + ["--version"], text=True).strip()},
            "hostcc": {"argv": hostcc + ["maketab.c", "-o", "maketab"],
                       "version": subprocess.check_output(hostcc + ["--version"], text=True).strip()},
            "maketab": {"argv": ["./maketab", "awkgram.tab.h"]},
        },
    }
    (dest / "manifest.json").write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n")
    (dest / ".prepared").write_text(PIN + "\n")

if __name__ == "__main__":
    main()
