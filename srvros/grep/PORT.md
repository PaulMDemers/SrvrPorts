# GNU Grep 3.12 on SrvrOS

The `upstream/grep` directory imports the GNU Grep 3.12 release archive.
The SHA-256 is `2649b27c0e90e632eadcd757be06c6e9a4f48d941de51e7c0f83ff76408a07b9`.
The archive's detached signature verifies against Jim Meyering's published
fingerprint `155D3FC500C834486D1EEA677FD9FCCB000BEEEE`. The direct signature
check used PGPy, which does not check key revocation. The original GNU and
gnulib license files are included.
[GNU release announcement](https://lists.gnu.org/archive/html/info-gnu/2025-04/msg00008.html)

The bounded command is `ggrep [-FGEivnqc] [--] PATTERN [FILE]`. It uses the
GNU engine for C locale basic and extended regular expressions and fixed
strings. The pattern is one argument of at most 1024 bytes without a newline.
Input is one existing regular file or finite standard input. The adapter checks
actual reads for NUL and caps them at 256 KiB, 8192 physical lines and 64 KiB
per line including the newline. It then searches an unlinked snapshot. Even
quiet mode waits for complete admission so a later input error returns status
2. Status 0 means a line was selected and 1 means no lines were selected.
The OS continues to provide compact `grep` for its existing callers.

The adapter adds normal flow cancellation checks to working copies of the GNU
matching sources. The candidate heap ceiling applies to SDK break growth.
The two-boot and regression evidence below qualifies the bounded profile.

The configure pass used the SrvrOS x86-64 SDK and target static linker with
`--host=x86_64-unknown-none --disable-nls --disable-perl-regexp`. It selected
the included GNU regex implementation. `generated/` contains its headers.
`sources.mk` records the configured source set plus dependent replacements
resolved by the actual ELF link. stdout close status is checked through the
SDK directly, so the unported gnulib FILE-introspection modules are unused.
The C locale wide-character helpers come from the existing diffutils adapter.
Production and diagnostic ELF links succeed.

The target passed on clean OS `0241ba87` with composite Ports `9fdd187`.
The composite changes the battery logging call relative to Ports main solely
to retain the byte-identical behavior of the existing OS pin. The Grep-only
port head is `075938d`, and its Grep sources match those used to build the test
candidate. The two-boot workflow matches GNU 3.12 in 19 admitted cases and
verifies 14 refusals, including a late NUL from a pipe in quiet mode. Checks
preserved input and saved output across reboot, found no temporary snapshot
residue, and passed guest filesystem checks.

The 16 MiB ceiling covers SDK break growth, including allocator fragmentation
and installed heap pages. The combined 256 KiB, 8192-line and 64 KiB-line
fixture reached 368,640 bytes of mapped heap. A zero-cap diagnostic refused a
real allocation during startup, and a fresh process recovered. This measures
the mapped heap arena for the tested workload. Fixed heap growth excludes
stack, executable mappings, filesystem snapshot storage, and total RSS.

The guest distinguishes read-only stdout error status 2 and default
SIGPIPE status 141, matching native GNU signal behavior for a closed consumer.
INT and TERM produce 130 and 143 after observed held output and after gated
input read, compilation, and matching progress. The engine checkpoints
periodically dispatch pending SDK signals. Diagnostic phase signals arrive
while the helper holds the process at the progress boundary. Earlier failed
stdout checks remain recorded, including the original stream-error defect and
the superseded broken-pipe expectation.

Two clean GNU core, gnulib, adapter and diagnostic rebuilds match the tested
ELFs against the same SDK objects. Native upstream tests have five PASS and
one SKIP for the macOS host lacking `timeout`. The normal staged program
contains no diagnostic heap or phase controls. Target evidence includes the
workflow and regressions, with the complete guest suite limited to the
SrvrOS profile described above.

The full ports regression and 498-command CLI regression pass. The Terminal
walkthrough verifies readable numbered matches, a missing file diagnostic and
successful count retry. [OS integration draft](https://github.com/PaulMDemers/SrvrOS/pull/503)
retains the tested pin while the separate Pro logging review is pending.
