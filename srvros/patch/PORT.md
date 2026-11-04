# GNU Patch 2.8.1: bounded port draft

`ports/upstream/patch` contains the pristine official
[patch-2.8.1.tar.xz](https://ftp.gnu.org/gnu/patch/patch-2.8.1.tar.xz),
SHA-256 `3240c7959c5347c8ed347e3c4201c0337bf4240d5cf09d68889e58eeaba30e17`.
The release signature was directly verified using PGPy against the published
primary fingerprint `259B 3792 B3D6 D319 212C C4DC D5BF 9FEB 0313 653A`.
PGPy does not check self-signatures or revocation; the result is a direct
cryptographic signature check, not a web-of-trust or revocation assessment.
The parent source record reports all 578 archive files compared with the pristine
tree. GNU GPL and bundled gnulib licenses remain in the upstream release.

The proposed command is `patch -i PATCH -o NEW_OUTPUT SOURCE`, plus `--help`
and `--version`. It admits one unified file section, safe leaf header labels
(including `a/name` and `b/name`), one or more counted hunks and regular file
inputs. It intends to support zero-count insertions and deletions, removed lines
beginning `--- `, missing-final-newline markers and multiple hunks using GNU
Patch's parser and application engine. Status 0 is applied, 1 is unapplied
hunks, 2 is trouble. The output is required to be a new, exclusively created
file; source and patch are not modified. Interactive, reverse, fuzz, backups,
reject files, in-place operation, other patch formats, metadata operations,
file creation/deletion and multi-file patches are outside the profile.

The admission layer reads each original through a regular-file descriptor into
an unlinked, open temporary snapshot; GNU receives duplicate descriptors for
those snapshots, so later path changes cannot swap the validated input. Source
and output bounds are 256 KiB, 8192 physical lines and 64 KiB per physical line
including newline. Patch input is limited to 1 MiB, 32768 physical records and
128 KiB per record; numeric hunk ranges and counts are bounded before upstream
allocates hunk arrays. NUL bytes are rejected. Output is counted as GNU writes
it, and on failure or interruption the adapter intends to remove only its own
partial output. The unlinked snapshots depend on the filesystem's open-unlink
behavior; that behavior and the cleanup identity checks require guest tests.
If output identity cannot be verified, the created path may remain for inspection.
If unlinking a newly created snapshot fails, its path is reported and may remain.
Cleanup uses a stat-then-unlink check, which is not atomic: concurrent or
adversarial replacement in the output namespace is outside this profile.

`prepare.py` applies changes only to a generated copy. The signal callback is
intended to set a flag and return through the SDK dispatcher; cleanup and signal
termination occur in normal control flow. The separate diagnostic variant is
planned to use a matching-progress marker, allocation-failure injection, and a
post-snapshot output-write fault. These hooks are absent from production.

The first SDK cross-configured production and diagnostic ELF targets link.
The configuration and selected gnulib sources are recorded in this port. The
unused `fpending`, `fseterr`, `close-stream` and `closeout` objects are omitted:
the bounded adapter checks stream close errors directly and does not use the
upstream closeout path. The local compatibility layer uses the SDK's C-locale
wide-character primitives; unsupported symlink-loop failures are reported as
`EOPNOTSUPP` where the SDK has no `ELOOP`. This does not establish general
symlink safety. Configure did not recognize `--disable-nls`; Patch has no
`ENABLE_NLS` setting in its generated configuration, and the bounded program
uses the C locale. No target behavior is established by linking.

The first disposable 512 MiB target boot passes basic replacement, zero-count
insertion, an exact header-like deleted record, missing final newlines, a
relative source named --version, existing-output/legacy refusals, failed-hunk
status 1 with output removed, preserved inputs and clean fsck. This is an initial
dirty-source proof with adapter hashes, not final reproducibility or full-profile
acceptance. Source setup failures were retained and corrected without changing
shared runtime contracts.

**Not accepted.** Signal and output failure behavior, snapshot and filesystem
semantics, memory use, GNU patch interchange and reproducible builds remain
unverified. No physical-device acceptance is implied.
