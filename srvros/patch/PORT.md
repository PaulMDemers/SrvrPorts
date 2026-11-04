# GNU Patch 2.8.1: bounded single-file port

`upstream/patch` is the pristine official
[patch-2.8.1.tar.xz](https://ftp.gnu.org/gnu/patch/patch-2.8.1.tar.xz),
SHA-256 `3240c7959c5347c8ed347e3c4201c0337bf4240d5cf09d68889e58eeaba30e17`.
All 578 regular archive files match; GNU GPL and bundled gnulib licenses remain.
The release signature was directly verified using PGPy against primary fingerprint
`259B 3792 B3D6 D319 212C C4DC D5BF 9FEB 0313 653A`. This checks the direct
cryptographic signature; PGPy does not establish trust or revocation status.

Use `patch -i PATCH -o NEW_OUTPUT SOURCE`, or `--help` / `--version`.
The genuine GNU parser and application engine support one unified file section,
safe leaf header labels (including `a/name` / `b/name`), counted hunks, zero-count
insertions/deletions, header-like deleted records, missing final newlines and
multiple hunks. Explicit inputs must be existing regular text files; output must
be a distinct, exclusively created new file. Status 0 means applied, 1 means
unapplied hunks, and 2 means trouble. Existing empty source/output content works;
metadata creation/deletion is excluded. The profile is forward-only, zero fuzz,
noninteractive, with no reverse, backup/reject files, in-place operation, other
patch formats, Git/rename metadata or multiple file sections.

Actual source/output limits are 256 KiB, 8192 physical lines and 64 KiB per line,
including newline. Patch limits are 1 MiB, 32768 physical records and 128 KiB per
record; numeric hunk ranges/counts are bounded before GNU allocates hunk arrays.
NUL is refused. Original files are read into unlinked open temporary snapshots;
GNU uses duplicated snapshot descriptors and snapshot metadata. Later path
removal/replacement/growth cannot change the admitted bytes. Output is counted
as GNU writes it. On ordinary failure or caught cancellation, cleanup removes
its own partial output. Unverifiable ownership or failed snapshot/output cleanup
reports the possibly remaining path. Stat-then-unlink is not atomic: adversarial
namespace replacement and power loss are outside this guarantee.

Adaptations apply only to a generated upstream copy. SDK C-locale primitives are
real; no shared libc/kernel change or fake FILE introspection is introduced.
Unused closeout/close-stream/fpending/fseterr objects are omitted; the adapter
checks stream-close errors directly. Unsupported symlink-loop failures map to
`EOPNOTSUPP` where the SDK has no `ELOOP`; this is not general symlink safety.
Configure did not recognize `--disable-nls`; there is no `ENABLE_NLS` setting and
the bounded command uses the C locale. Signal callbacks only flag pending work;
normal flow checks progress, cleans up and terminates. Diagnostic controls and
markers are absent from production.

The bounded qualification passes on clean OS `60baca64`, runtime port `de7adcea`,
after reconciliation with OS main `72fde837`. Kernel, initramfs, base disk,
production/diagnostic/probe ELFs and complete archive/disk inventories were
frozen and readback-verified outside build before seven fresh sequential
512 MiB guests. Evidence covers exact GNU references and refusal statuses,
simultaneous source caps, allocation-failure/recovery, snapshot removal/growth
races, real post-admission EFBIG write failure, repeated SIGINT/SIGTERM after
matching and positive output progress, cleanup, reboot persistence and fsck.
Fresh diff interchange, actual Terminal expert walkthrough, all 498 CLI commands
and full ports regression pass. The shipped miniport example builds a separate
patched copy and verifies the original source remains unchanged.

Two clean GNU core/gnulib/diagnostic rebuilds match against the same SDK objects;
those objects remain byte-identical. Selected native upstream tests report two
PASS and one expected XFAIL outside this single-file profile. This is not a full
upstream guest-suite, independent SDK/image reproducibility, total-process RAM
or RSS/true-peak bound, traced in-syscall cancellation/latency, representative-user
study or physical-device acceptance. Earlier setup/artifact/identity failures
remain recorded as failures. The missing old initramfs member comparison was
resolved by fresh qualification on the retained final payload, not an exemption.

[Port PR12](https://github.com/PaulMDemers/SrvrPorts/pull/12) and dependent
[OS PR370](https://github.com/PaulMDemers/SrvrOS/pull/370) record ordered delivery.
Portable evidence is published with the OS documentation; raw proof, frozen
payload and source archives survive outside build cleanup. No further feature
is required for this bounded profile.
