# SrvrPorts

This repository mounts at `ports/` in SrvrOS and is the staging area for third-party libraries and applications
that will be brought up on srvros through the POSIX-compat layer.

## Upstream Source

- `upstream/diffutils`: GNU Diffutils 3.12 release snapshot; see [port contract](srvros/diffutils/PORT.md).

- `upstream/xz`: XZ Utils 5.8.4 release snapshot; see [port contract](srvros/xz/PORT.md).

- `upstream/less`: less 710 release snapshot; see [port contract](srvros/less/PORT.md).

- `upstream/bc`: bc 7.0.3 release snapshot; see [port contract](srvros/bc/PORT.md).

- `upstream/jq`: jq 1.8.2 release snapshot; see [port contract](srvros/jq/PORT.md).
- `upstream/awk`: One True Awk, pinned to `5739fd79bcfc75ba7526773d0cf634521f8aca3c`; see [port contract](srvros/awk/PORT.md).
- `upstream/uacpi`: MIT AML interpreter, pinned to 7.0.0; offline/isolated probes, Q35 runtime fixtures and separately gated native runtime (see [port contract](srvros/uacpi/PORT.md)).
- `upstream/zlib`: zlib compression library, pinned to `v1.3.2`.
- `upstream/lua`: Lua runtime, pinned to `v5.4.8`.
- `upstream/cjson`: cJSON parser/printer, pinned to `v1.7.19`.
- `upstream/inih`: inih INI parser, pinned to `r62`.
- `upstream/linenoise`: linenoise line editing API, pinned to `2.0`.
- `upstream/sqlite`: SQLite amalgamation snapshot, pinned to `3.53.1`.
- `upstream/node`: Node.js runtime source, pinned to `v24.16.0` LTS.

Port manifests for srvros-local staging tools live under `ports/srvros/*/PORT.md`.
They record what is intentionally local, which upstream target it is preparing
for, and which compatibility gaps remain.

The zlib core sources are linked directly into `/fat/bin/zlibdemo`, which
compresses data, writes the compressed bytes to exFAT, reads them back, and
verifies decompression inside srvros.

Lua is built as `/fat/bin/lua` from a generated copy under
`build/ports/lua-srvros`, so the upstream submodule stays clean. The current
port uses Lua's normal floating-number profile and disables OS/process/native
dynamic loading features while the libc and kernel process ABI mature.

cJSON and inih are linked directly into `/fat/bin/jsondemo` and
`/fat/bin/inidemo`. These apps verify parse/create/file roundtrips without
patching upstream sources.

linenoise is used through `ports/srvros/linenoise.c`, a small srvros adapter
for the upstream public API. `srvsh` uses it for editable prompt input and
`/fat/.srvsh_history`; `/fat/bin/linedemo` exercises history save/load behavior.

The first libuv-facing bridge lives in `ports/srvros/uv.c` and
`ports/srvros/uv.h`. It is intentionally a small srvros compatibility shim, not
upstream libuv: `/fat/bin/uvdemo` uses it to smoke-test a libuv-shaped loop,
timers, filesystem requests, and initial TCP/UDP handle entry points.

SQLite is kept as an official amalgamation snapshot rather than a submodule.
`/fat/bin/sqlitedemo` builds the amalgamation with `SQLITE_OS_OTHER`, registers
a small srvros VFS, and verifies create/insert/query/reopen behavior on exFAT.
The VFS maps SQLite shared/reserved/pending/exclusive states onto srvros
advisory byte-range locks.

Node.js is staged as a pinned upstream source checkout for the long-running
runtime port. The first probe is documented in `docs/node-port.md` and
`ports/srvros/node/PORT.md`; it confirms that upstream configure does not yet
recognize `--dest-os=srvros`, while a stripped Linux stand-in profile can
generate a build graph. The previous port gitlink was unavailable; the checkout
was restored to clean upstream. Later runtime milestones in the port notes are
historical and have not been reproduced from this restored source. See
[Node recovery](https://github.com/PaulMDemers/SrvrOS/blob/main/docs/development.md#node-port-recovery) before resuming it.

## Porting Rules

- Keep upstream repos clean. Put srvros-specific build glue outside the
  submodule when possible.
- Prefer small compatibility shims in `userspace/lib` over patching upstream.
- Add one smoke app per port once it builds.
- Add or update a `PORT.md` manifest for each staged port or local
  compatibility tool.
- Document unsupported POSIX calls explicitly instead of silently pretending.

## Current porting priorities

File metadata, mapped memory and substantial stdio/socket/thread support now
exist. Presence of an API does not establish full POSIX conformance: use the
[supported profile](https://github.com/PaulMDemers/SrvrOS/blob/main/docs/posix-supported-profile.md) for validated behavior,
bounds and remaining gaps, and [libuv notes](https://github.com/PaulMDemers/SrvrOS/blob/main/docs/libuv-port.md) for the local
adapter's scope. A full upstream libuv backend and a reproducible Node runtime
remain separate milestones. Recover Node's missing port history before treating
its historical runtime results as current acceptance.

## Repository integration

Clone [SrvrOS](https://github.com/PaulMDemers/SrvrOS) recursively and work in
`ports/`. Build from the OS root with `tools/host-env.sh make -j2 all`, or
`sh tools/macos/build.sh -j2 all` on Apple Silicon. This repository supplies
source, adapters and port recipes; it requires the matched SrvrOS SDK and
image tooling. Keep upstream submodules at their recorded commits.

History was extracted from SrvrOS `74d38d59869f9e6201c6cad6547461cd9fa8dac6`
without changing upstream source or pins. Publish a port commit first, then
update the OS submodule pointer and verify the combined image.
