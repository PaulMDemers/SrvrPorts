# jq 1.8.2 for SrvrOS

Upstream release: https://github.com/jqlang/jq/releases/tag/jq-1.8.2
Archive: `jq-1.8.2.tar.gz`, SHA-256
`71b8d6e8f5fe81f6c6d0d110e3892251f6ce76ed095abd315e26e6e1193af3af`.
`upstream/jq` is the unmodified release snapshot, including pre-generated parser
and lexer and the bundled decNumber sources. Preserve upstream `COPYING`, the
copyright notices in sources and `vendor/decNumber`, and `AUTHORS` with any
redistribution.

`build.mk` builds the static CLI from the OS root as `build/userspace/jq.elf`.
The deterministic preparation script creates only `builtin.inc`, `version.h`
and `config_opts.inc` in `build/ports/jq-srvros`. No bison, flex, configure,
network, or host jq is needed for target builds. The x86-64 IEEE dtoa path and
upstream default decNumber representation remain enabled. Actual SDK pthread
keys, once, and atexit support back jq's per-thread contexts. Oniguruma is not
included: regex operations report upstream's unsupported-build error. Optional
libm wrappers and date/time builtins gated by configure probes are not enabled;
this is not a full jq feature profile. The SDK calendar is UTC-only, reflected
by the local timezone names used by upstream's bundled strptime. The local
`dirname` adapter supports module/origin path splitting, and `srv_jq_getentropy`
forwards to SDK `getrandom`; upstream's fallback applies if entropy fails.
Filesystem and process capabilities are bounded by SrvrOS. This port targets
JSON stdin/stdout and pipes, filters including select/map/group/sort/add/@csv,
`-f`, `-r`, `-c`, and `-e`.

`tests/upstream-selected.test` contains unmodified groups copied from release
`tests/jq.test`. Stage it in the guest and run `jq --run-tests FILE`; it is a
selected regression subset, not the full upstream suite. The diagnostic target
`build/userspace/jq-output-test.elf` spawns jq with stdout opened read-only and
requires jq to exit unsuccessfully on an output error; it expects the guest
fixture paths `/fat/jq-test/true.jq` and `/fat/jq-test/sales.json`. Guest testing
is required to establish runtime behavior; a successful build alone does not.

An initial guest `--run-tests` run reported all selected cases passing, then
exited 134 with `cannot allocate memory` after the state/recompile tests.
The upstream `HAVE_PTHREAD` gate adds a three-thread jq-state regression after
those tests; it is disabled for this single-thread CLI profile. The allocation
failure was not traced to a specific allocator limit, and this gate does not
establish multithreaded jq support. The separate pthread key/once and atexit
feature gates remain enabled for jq’s actual context storage.
