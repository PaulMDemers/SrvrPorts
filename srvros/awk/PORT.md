# One True Awk for SrvrOS

Upstream: https://github.com/onetrueawk/awk, commit
`5739fd79bcfc75ba7526773d0cf634521f8aca3c`; see upstream `LICENSE`.
This port prepares copies of the source; it does not modify the submodule.

From the OS root, run `python3 ports/srvros/awk/prepare.py ports/upstream/awk build/ports/awk-srvros`.
The script requires host Python 3, Git, Bison and a native C compiler; `BISON`
and `HOSTCC` can select the tools. It checks the upstream pin and tracked inputs,
then runs Bison and the host-built `maketab` in the output directory. Compile
`awkgram.tab.c`, `b.c`, `main.c`, `parse.c`, `proctab.c`, `tran.c`, `lib.c`,
`run.c`, and `lex.c` from that directory together with
`ports/srvros/awk/compat.c`, force-include `ports/srvros/awk/config.h`, and link
the SrvrOS libc (which includes math functions). `ports/srvros/awk/build.mk`
integrates this into the OS build; `make test-awk-port` runs the host checks.
Generated sources are target inputs; the host-built `maketab` is never a target
object. Neither belongs in upstream. `manifest.json` records the upstream commit,
input and prepared text SHA-256 hashes, and tool versions and command arguments.
The binary `maketab` is excluded from reproducibility comparisons.

The intended initial profile is the C locale: case conversion is bytewise ASCII,
not locale-aware Unicode conversion. Upstream's own UTF-8 byte handling remains
in place where applicable; this is not a full UTF-8 locale implementation. The
local deterministic 31-bit generator supports repeatable `srand`/`rand`, but its
sequence is not the upstream platform's `random()` sequence and is not suitable
for security. A local `wctomb` adapter uses `wcrtomb`; the multibyte conversion
path is disabled by the C-locale profile. SrvrOS does not provide SIGFPE delivery,
so upstream's floating-point signal handler is omitted. Other process, filesystem,
and stdio behavior is bounded by the SrvrOS runtime; full POSIX behavior is not claimed.

`sh ports/srvros/awk/test-host.sh` compares two fresh preparation manifests,
checks modified-input refusal in a temporary clone, and runs a native smoke build;
it does not establish target compatibility or full upstream regression coverage.
