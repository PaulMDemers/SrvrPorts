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

**Not built or accepted.** Cross-configured gnulib sources and headers, the
SDK link recipe, signal and output failure behavior, snapshot and filesystem
semantics, memory use, GNU patch interchange and reproducible builds remain
unverified. No physical-device acceptance is implied.
