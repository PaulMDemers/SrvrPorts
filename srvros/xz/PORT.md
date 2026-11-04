# XZ Utils 5.8.4

The pristine `ports/upstream/xz` tree comes from the official release
[xz-5.8.4.tar.gz](https://github.com/tukaani-project/xz/releases/download/v5.8.4/xz-5.8.4.tar.gz),
SHA-256 `0014c7886930454fe8bd4228665b51af55eeae560ea135c9c4cd33f55b2591d9`.
All archive regular files were compared with the extracted tree. The detached
release signature was verified using PGPy against the project-published Lasse
Collin primary key, fingerprint `3690 C240 CE51 B467 0D30 AD1C 38EE 757D 6918 4620`;
the signature issuer is the primary key. PGPy reported that self-signature and
revocation checks are not implemented, so this is a direct signature check
against the pinned project-published key, not a web-of-trust or revocation
assessment. See the [project release page](https://tukaani.org/xz/) and its
[security policy](https://github.com/tukaani-project/xz/security). The release
includes the applicable COPYING files, including LGPL-2.1-or-later for bundled
getopt and the XZ Utils license notices.

The target is upstream xz and liblzma in a static, single-threaded, C-locale
profile. Compression defaults to preset `-0`; decompression and integrity
checking support `.xz` only. File inputs require `-c` or `-t`; stdin is supported
and output goes to stdout. File replacement and deletion, legacy and raw formats,
custom filters, other presets, multithreading, progress UI and job control are
outside the profile. Unsupported options and nonempty `XZ_DEFAULTS`/`XZ_OPT`
are rejected. CLI memory limits can lower but cannot raise the 8 MiB compression
and 32 MiB decompression codec ceilings; presets are never silently adjusted.
These ceilings and the allocator's live-byte accounting are not total process
or resident-memory measurements.

The port-local liblzma allocator uses anonymous mappings and keeps allocation
accounting and failure-injection controls for a separately linked test helper;
the production executable has no diagnostic environment switch. A separate
`xz-diagnostic.elf` emits a byte on inherited fd 3 after a codec call, allowing
an active-stream cancellation test without modifying production behavior. Signal
callbacks set flags and return, while normal I/O and codec-loop checkpoints
handle cancellation and cleanup. This does not promise an interruption latency
bound inside a codec call.

`prepare.py` creates an adapted copy under `build/ports/xz-srvros`; upstream
sources remain untouched. Bundled `getopt_long` is prefixed to avoid collisions
with the SDK.

The guest receipt `build/review/xz/20261104-135556-m_qxupp7` passed production
interoperability across two boots, including decoding a host preset-6 stream.
Sampled private mapped bytes were 10,866,688 during that decode and 5,283,840
during encoding; these samples are not peak resident-memory guarantees.
The diagnostic codec-progress test passed two SIGINT runs with status 130 and
a SIGTERM run with status 143. Blocked I/O passed repeated SIGINT and SIGTERM
checks with descriptor flags restored. A read-only output failed with status 1,
and a closed consumer produced status 141.

The allocator probe injected failure at each of 14 encoder and 9 decoder
allocation sites, then ran 64 reuse cycles with zero net mapping growth from
a 2,531,328-byte baseline. Peak requested allocator bytes were 2,833,479 for
encoding and 297,744 for decoding. These are distinct from the sampled process
mappings above. Separately, six native-host upstream test programs reported 22
passes and two skipped threaded subcases; those are host results, not guest
upstream-suite results.

The final guest matrix additionally covers zero-filled data, legacy `.lzma`
rejection, malformed/truncated/checksum-failed streams, immutable codec caps,
failed output and input preservation over reboot. Sparse output is disabled
before file setup, including when stdout is redirected to a regular file.

Actual graphical Terminal receipt
`build/review/xz-terminal/proof-svpdq5tn/receipt.json` passes report compression,
integrity checking, host preset-6 decoding, byte-exact restoration, bad-input
feedback, two actual Ctrl-C pipeline interruptions and shell continuation. Expert
screenshots show the restored report and usable prompt; no user study is claimed.
Existing ports smoke passes with guest fsck `leaks=0 fat=0 errors=0`.

Two clean CLI ELF builds match SHA-256
`e0efaa0d2b08361a0c56d58c247cea01704a4e4b1ad30ba9c68a8d5cc4128e1f`.
Runtime ELF SHA-256 is
`1d5c86a03caa5aa68ba8b7706b20573b37006fcc29f6d3f3393c5e59e770131a`.
This is CLI reproducibility, not whole disk/ISO reproducibility. The committed
OS code tree `425c87d2` retains all three tested payload hashes; later delivery
documentation does not change the implementation. Full local evidence is under
`build/review/xz/`, including source authentication, build comparison and
`upstream-tests/summary.json`. No native/physical-device acceptance is implied.

Reproduce sequentially in the OS checkout:

```sh
sh tools/macos/build.sh -j4 all build/userspace/test_xz_probe.elf build/userspace/test_xz_alloc_probe.elf build/userspace/xz-diagnostic.elf
python3 tools/xz_smoke.py
python3 tools/xz_terminal_smoke.py
python3 tools/ports_smoke.py
```

The graphical harness requires visual screenshot review. For native upstream
checks, extract the authenticated release archive into a separate build directory
(preserving archive timestamps so generated Autotools files do not spuriously
regenerate), configure with `--disable-shared --disable-nls --disable-threads
--disable-scripts --disable-doc --disable-sandbox`, then run GNU Make `check`
with `TESTS='test_check test_stream_flags test_block_header
test_stream_buffer_encode test_stream_buffer_decode test_memlimit'`.
The two threaded subcases intentionally skip in this configuration.

