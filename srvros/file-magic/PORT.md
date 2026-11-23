# file-magic on SrvrOS

`file-magic` uses the `file` 5.48 libmagic engine to identify file contents in
SrvrOS Terminal or shell. Use `file-magic FILE` for a description and
`file-magic -b --mime-type FILE` for a MIME type alone. `--` ends option
processing. The existing compact `file` command remains available.

The C-locale profile admits one explicit regular file and examines at most its
first 256 KiB. It opens the input read-only, accepts unknown binary data as a
successful classification, and reports usage and I/O errors with status 2.
The bundle qualifies ELF headers, PNG, SQLite, ZIP, gzip, xz, tar, plain text,
empty files, and unknown binary data. The CLI excludes stdin, custom databases,
special files, decompression, and deep ELF and compound-document inspection.
Larger files are classified from their admitted prefix.

The `file` 5.48 archive and detached signature are in `upstream/file`. The archive
SHA-256 is `ed14656883b23a364b4057c05595d93252da9bc473d30106519519d0da141283`.
The signature was verified against release-key fingerprint
`BE04995BA8F90ED0C0C176C471112AB16CB33B3A`. The native same-release compiler
compiles an ordered selection of upstream magic fragments and embeds the
result in the target executable. The ELF includes the libmagic engine and the
port-local SDK compatibility adapters. Upstream archive bytes are kept intact.

The runtime caps SDK break growth at 16 MiB and observes INT and TERM through
cooperative checkpoints. The cap covers the mapped heap arena. The input buffer,
embedded database, stack, and executable mappings occupy separate storage.
Diagnostic builds can repeat genuine classification for the interrupt probe
and report the heap high-water mark.
