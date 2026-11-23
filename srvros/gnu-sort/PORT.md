# GNU sort on SrvrOS

`gsort` uses GNU Coreutils 9.12's sequential merge sort and decimal comparison
routines. It sorts C-locale text from one regular file or finite stdin and
writes to stdout. Example for a simple delimiter-separated report:

```sh
gsort -t , -k 2,2 -n -s /fat/home/report.txt
```

The profile accepts `-n`, `-r`, `-s`, `-u`, one single-byte `-t` delimiter, and
one whole-field key `-k N,N`. Without a key it orders the whole line. Embedded
NUL is refused. Limits are 256 KiB of input, 8,192 records, and 64 KiB per
record including the newline when present. An unterminated final record is
sorted and emitted with a newline. Admission errors return status 2 before
any sorted output. Success returns 0. Output failures return 2.

The port sorts entirely in memory. It excludes disk spill, parallel sorting,
locale-aware collation, general numeric mode, additional key fields, and
in-place output. The existing `sort` remains available for existing workflows.
The SDK break wrapper caps the mapped heap arena at 16 MiB. Cooperative
checkpoints dispatch INT and TERM while reading, sorting, or writing. The
arena accounts for heap growth and excludes the stack and executable mappings.

Preparation verifies SHA-256 `a480198559733e9b3da999e90543ac6f888a2caa544d8d664c5a1f17e528e210`
for the signed Coreutils archive already shipped for `sha256sum`. The build
extracts the selected upstream sequential sort and numeric comparison sources
into `build/`. The archive and upstream sources stay intact.
