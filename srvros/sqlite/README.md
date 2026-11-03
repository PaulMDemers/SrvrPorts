# SrvrOS SQLite port

The upstream shell uses the `srvros` VFS and rollback journals. One persistent
main database may be open per process. Its whole-file POSIX write lock is held
for the lifetime of the connection; competing owners fail rather than share
access. This is a cooperative contract: other programs must respect POSIX
record locks and must not modify, unlink, or replace the database while open.
The shell tracks its file streams to prevent closing a database alias from
releasing the process-owned lock; opening the active database as a shell
input or output file fails. Do not bypass this guard with other raw descriptors
in applications embedding the VFS. The port is single-threaded.

Read-only SQLite connections are supported, but the database file must still
be openable for writing to acquire the lifetime write lock. Named and unnamed
temporary files use the VFS; unnamed files are uniquely created under /fat.
Paths resolve relative to the process working directory, and paths exceeding
the VFS limit fail. SQLite reports clock and random-source failures.
WAL, ATTACH, VACUUM, dynamic extensions, shell system commands, pipes, and
readline are disabled. The shell rejects settings other than DELETE journal
mode and FULL synchronous mode. The VFS itself does not enforce SQL pragmas
for other embedding applications. The shell may read scripts and CSV files and write
output files that are not aliases of the active database. Backup/restore and
other operations needing a second persistent connection fail at VFS open.
Shell file names must not be changed by another process during a file open;
pre-open identity checks prevent write-mode truncation of a database alias.

The bounded profile verifies normal commit/rollback and clean reopen, including
a clean guest reboot. It does not claim power-loss atomicity or crash durability
on the current exFAT implementation. Check explicit SQLite errors; failed writes
may have changed media.

Verification: host VFS tests and the guest diagnostic probe cover ownership,
alias refusal, temporary files, paths, realtime and basic I/O failures. The
matched OS harness `tools/sqlite_cli_smoke.py` checks CSV import/export, scripts,
transactions, restrictions, two-boot reopen and filesystem allocation. Build
its diagnostic first with `make build/userspace/sqlite-vfs-test.elf`.
