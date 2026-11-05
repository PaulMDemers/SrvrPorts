# Experimental SrvrOS JavaScriptCore port

This overlay targets WPE WebKit 2.54.2:
<https://wpewebkit.org/releases/wpewebkit-2.54.2.tar.xz>, SHA-256
`f3284e19e47fcb5ac8c63c2e1c113dbd65b2423eb8a545f5b345707829004f56`.
Its goal is a statically linked JSCOnly shell and JavaScriptCore library, not
a browser, renderer, production port, or JavaScript runtime acceptance.

After preparing the matched SrvrOS SDK, threaded libc++, RTTI, filesystem, ICU,
and supplemental math proofs, run:

```sh
python3 tools/browser_jsc_build.py \
  --filesystem-guest-receipt build/browser-filesystem/guest-.../receipt.json \
  --math-guest-receipt build/browser-runtime/guest-.../receipt.json
```

The builder verifies the pinned archive, SDK, dependencies and guest evidence,
then retains a separate `build/browser-jsc/run-*` source and build directory.
It applies the listed patches, configures static JSCOnly with JIT, WebAssembly,
remote inspector, header maps, unified compilation and sampling profiler off,
and builds Release with at most two jobs. Header maps are disabled because the
release archive omits the optional host tool; individual compilation makes
portability defects visible. A 2 GiB disk reserve and 6 GiB per-run limit apply.
A successful static link does not establish safe interpreter execution.

The SDK and ICU installs stay separate. The opt-in JSC toolchain enables the
same filesystem and no-localization libc++ configuration used by the tested
target filesystem archive. Actual executable CMake checks link the filesystem,
thread, RTTI and SDK archives; the script verifies their flags and paths.
WebKit's filesystem feature detection uses those results rather than header
presence. The check itself exercises only path/status; other filesystem behavior
is supported only where the separate target fixture provides evidence. The
JSC-only supplemental math archive supplies otherwise missing math symbols, but
it does not replace or qualify the SDK's existing public `exp`, `log`, or
`sqrt` implementations. Their accuracy along actual JSC code paths remains an
independent runtime requirement.

The platform patches identify SrvrOS explicitly, use target ELF archive tools,
and select the existing 512 MiB memory-sizing guess where a physical-memory
query was not wired into WebKit. Current private mapped bytes are obtained from
the self-PID process snapshot; that metric excludes kernel memory and shared
surfaces, and query failure terminates rather than fabricating a footprint.
File-backed mappings use the real descriptor-based API; unsupported shared
mappings still fail. The target has no `mincore`, so page-residency statistics
are unavailable. Sampling profiling is disabled because a signal machine context
is unavailable.

SIMDUTF uses its scalar implementation until the kernel's vector-state contract
supports wider dispatch; Unicode validation and conversion remain enabled. The
optional C++ iostream overloads for fallback 128-bit integers are unavailable in
this no-localization configuration. Integer arithmetic and WTF `PrintStream`
formatting remain. SrvrOS filesystem conversion passes UTF-8 bytes directly; it
does not assert that every filesystem accepts every non-ASCII filename.

The bytecode-cache version uses a deterministic identifier over the verified
patched source tree, toolchain, matched dependency proofs and build settings.
Only the cache-version source receives it, and date/time reproducibility warnings
remain enabled. Persistent bytecode-cache compatibility is not qualified.

Resume with the same guest receipts and `--resume build/browser-jsc/run-...`.
The builder checks every retained source path, mode and content, applied patches,
toolchain, dependency proofs and CMake cache. Add a new reviewed patch only via
`--add-patch NAME.patch`; old exploratory runs without state require a fresh
build. `--refresh-runtime-proofs 'reason'` permits a verified packaging-only
runtime update with the same SDK headers and binaries. The narrower
`--refresh-sdk-headers 'reason'` permits only the recorded semaphore/regex
C++-linkage guards after reconstructing and verifying the previous full SDK
header tree and matching C preprocessing; other SDK or compiler changes require
a fresh build. Transitions record old/new evidence and let Ninja rebuild real
dependents. `python3 tools/browser_jsc_replay.py build/browser-jsc/run-...`
re-extracts the archive, reapplies the recorded stack and verifies the whole
source fingerprint.

Normal public signal dispatch remains disabled. PR334 supplies an explicitly
gated experimental kernel/libc contract and a separate WTF overlay selected by
`--experimental-signal-context`. Its reviewed native context/nested-wait probe
passes, but this does not establish interpreter or garbage-collector acceptance.
The first freshly linked actual-WTF guest exits134 after the main-context marker;
the remaining utility, GC-root and JavaScript cases have not run.

`signal-retirement.patch` corrects an independently reproduced WTF lifetime
race: a resumed worker can reach its TLS destructor before the controller
consumes the final acknowledgement. Retirement retains TLS/group ownership until
the matching controller publishes Idle, without consuming its acknowledgement.
The controller updates the suspend count before its final Idle publication.
Invalid identity, pending INFO, ticket changes and invalid phases remain fatal;
clock checks and a finite yield budget bound the drain attempts. The INFO handler
and class headers are unchanged. This candidate is host-tested and awaiting
owner review and matched target/guest verification; it is not attribution of the
original134 or a JavaScript pass.

`test_signal_retirement.py --source PATH --output DIR --sanitizers` extracts the
actual patched TLS destructor and resume method. Its35 cases pass in strict and
ASan/UBSan profiles; `--expect-old-failure` confirms the original source rejects
both valid posted/consumed-acknowledgement schedules. Optional `--baseline PATH`
requires the INFO handler to remain byte-identical. These deterministic host
schedules supplement the required real guest run.

`random-entropy.patch` replaces `/dev/urandom` access on SrvrOS with the native
`srv_random` strict-entropy flag. It accepts only positive bounded results,
advances on short reads, and aborts on unavailable providers, old-kernel refusal,
zero progress or invalid overreturns. It makes no fallback to legacy flags0.
The retained RandomDevice class layout is unchanged and its SrvrOS constructor
and destructor perform no file I/O. Provider qualification remains the kernel's
responsibility; passing a QEMU test does not accept physical entropy sources.

The authoritative ABI header is SrvrOS `shared/include/srvros/random.h`. The patch
carries an exact copy as `wtf/srvros/RandomContract.h` so previously qualified
SDK/libc++/ICU artifacts can keep their existing attribution; that retained SDK
does not contain the new header. To update it, run
`python3 ports/srvros/webkit/update_random_contract.py shared/include/srvros/random.h`
from the OS checkout. Never edit the copied definitions independently. The JSC
builder compares applied and authoritative bytes before every qualifying build,
records both hashes and source/ports revisions, and rechecks after compilation.
New SDK builds export the shared header normally. Legacy flags0 and POSIX
`getrandom` behavior are unchanged; cryptographic callers must request the named
native strict mode and fail closed on refusal.
