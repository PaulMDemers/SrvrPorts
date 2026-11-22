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
Its memory and cancellation behavior require guest evidence. The bounded
profile and the generated SDK build are under qualification.

The configure pass used the SrvrOS x86-64 SDK and target static linker with
`--host=x86_64-unknown-none --disable-nls --disable-perl-regexp`. It selected
the included GNU regex implementation. `generated/` contains its headers.
`sources.mk` records the configured source set plus dependent replacements
resolved by the actual ELF link. stdout close status is checked through the
SDK directly, so the unported gnulib FILE-introspection modules are unused.
The C locale wide-character helpers come from the existing diffutils adapter.
Production and diagnostic ELF links succeed. Guest validation and the memory
ceiling measurement are pending.
