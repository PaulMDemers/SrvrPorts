# GNU Diffutils 3.12: diff

`ports/upstream/diffutils` is the unmodified GNU
[diffutils-3.12.tar.xz](https://ftp.gnu.org/gnu/diffutils/diffutils-3.12.tar.xz)
release, SHA-256
`7c8b7f9fc8609141fdea9cece85249d308624391ff61dedaf528fcb337727dfd`.
The detached GNU release signature was checked with PGPy against the
[Savannah diffutils release keyring](https://savannah.gnu.org/project/release-gpgkeys.php?group=diffutils&download=1)
and the release announcement's primary fingerprint
`155D 3FC5 00C8 3448 6D1E EA67 7FD9 FCCB 000B EEEE` (Jim Meyering).
The signer is the primary key. PGPy warns it does not implement self-signature
or revocation checks, so this records a direct cryptographic verification against
the pinned key, not a trust or revocation assessment. GNU GPL and bundled gnulib
license files are retained in the upstream release.

The bounded profile is a static C-locale GNU diff of two regular text files:
normal, `-q`, `-u` with three context lines, `-L` labels, `--`, help and version.
Status 0 means equal, 1 different and 2 trouble. Inputs are preserved and output
is stdout only. The input bounds are 256 KiB per file, 8192 physical lines and
64 KiB per physical line including any terminating newline; NUL bytes are refused. Recursion, standard input,
configurable context, binary mode, ignore/regex options, color and other output
styles are outside the profile.

`prepare.py` applies the profile to a generated copy; the release tree remains
unchanged. It adds checks on bytes actually read before line-table allocation,
disables input-skipping comparison shortcuts, and places cooperative checkpoints
in the comparison loop. The genuine GNU core and configured gnulib have linked against the SDK, and
corrected two-boot qualification passes GNU references, timestamp-matched
unlabeled output, caps/refusals, calloc/malloc injection and recovery, native
GNU Patch roundtrips, preserved inputs, reboot bytes and fsck. Exact SIGINT/
SIGTERM statuses 130/143 pass after comparison progress and observed held-output
stall with unchanged flags. Real output failures return 2. Production excludes
diagnostic hooks; no realloc-family or in-call interruption-latency claim.

Two clean core/gnulib and diagnostic builds match against the same SDK objects,
not independent SDK or whole-image builds. Selected native upstream scripts:
three PASS and the upstream expected large-subopt XFAIL. Guest input fixtures
are separately labeled, without a full guest-suite claim. Actual Terminal,
expert screenshot review, CLI and ports regressions pass in disposable QEMU.

Draft gate: the later combined 256 KiB/8192-line/64 KiB-line changed fixture still
needs its own two-boot run. Final current-head review and dependency-order merge
remain pending. Partial stdout may remain on failure/cancellation; sampled
mappings are not RSS/true peak. No shared libc/kernel change or fake capability.
No physical-device or user-study acceptance. Retain earlier failed receipts.
