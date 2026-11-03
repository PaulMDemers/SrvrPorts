# WPE WebKit JSCOnly platform exploration

The overlay targets the unmodified official WPE WebKit 2.54.2 tarball,
`https://wpewebkit.org/releases/wpewebkit-2.54.2.tar.xz`, SHA-256
`f3284e19e47fcb5ac8c63c2e1c113dbd65b2423eb8a545f5b345707829004f56`.
Apply `srvros-platform.patch` to a fresh verified extraction using `patch -p1`.
The patch identifies SrvrOS explicitly in CMake and WTF; it selects generic Unix
code paths but does not assert that their required interfaces work. It is not an
upstream-supported port, browser, renderer, or guest runtime.

Use `toolchain.cmake` with CMake and Ninja to explore only `PORT=JSCOnly`,
`ENABLE_STATIC_JSC=ON`, `ENABLE_JIT=OFF`, `ENABLE_WEBASSEMBLY=OFF`,
`USE_SYSTEM_MALLOC=ON`, `USE_HEADER_MAPS=OFF`, and
`ENABLE_REMOTE_INSPECTOR=OFF`. These constraints
are required for the current probe; dynamic JSC, JIT, WebAssembly and browser
builds have not been implemented. The driver assumes the repository's prepared
`build/tooling/zig` and `build/sysroot/srvros` SDK, and places the SDK startup,
linker script and library around test objects for static ELF links. It is an
experimental configure driver, not a complete C++ runtime or a production
cross compiler. Compiler resource headers and Zig's C++ headers come from the
pinned local Zig tooling; OS headers and libraries come from the SrvrOS SDK.
Target libraries and headers must never be taken from the host.

Do not force CMake's thread-detection results or set
`CMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY`: actual executable links are
necessary to discover unresolved pthread interfaces. Configure may honestly
stop before ICU, including during compiler validation, until the matching C++
startup/runtime and SDK are sufficient. Inspect the CMake logs and command
lines, and treat link success as symbol evidence only, not proof of runtime
semantics. Do not add compatibility stubs to make a check pass. No guest or
engine execution is implied by configuration.

C++ consumers use the exact threaded subset configuration in
`../libcxx/config.h` and `../libcxx/PORT.md`, and link the matching
`build/browser-runtime/libcxx-threaded.a` before `libsrvros.a`. Build that
archive with the corresponding `../libcxx/build.py` recipe against the same
SDK and Zig toolchain before C++ linking. Missing runtime archives fail the
link rather than falling back to the host. This subset is not a complete C++
runtime; the SDK and archive must be rebuilt together when their inputs change.
The compiler wrapper rejects shared-library and alternate target modes, which
are outside this probe.

C++ is compiled with `-fhosted` on the freestanding x86_64 target so normal
C++ `main()` and `main(int, char**)` have the C entry-point linkage expected by
SrvrOS `crt0`. The compiler consequently gives `main` the standard implicit
`return 0` behavior and defines `__STDC_HOSTED__` as 1; that compiler language
mode does not establish complete hosted library or OS support. Includes and
links remain restricted to the explicit SDK and threaded runtime. Other missing
hosted facilities remain real port blockers. The C driver remains freestanding.

For the pinned ICU milestone, run the parent repository's
`python3 tools/browser_platform_probe.py --icu` after building the ICU recipe.
This verifies its input/output receipt before adding only
`build/browser-icu/sysroot` to target discovery. The ICU install stays separate
from the base SDK. The toolchain accepts `SRVROS_ICU_ROOT` as an additional
cross-build root; the harness supplies the same path as `ICU_ROOT`. No host
ICU library satisfies this check. Without `--icu`, the original SDK-only probe
continues to expose the missing ICU prerequisite.

The WPE release archive omits `Tools/Scripts/hmaptool`, although JSCOnly with
Clang defaults to using it. The probe selects upstream `USE_HEADER_MAPS=OFF`
(the normal WPE/GTK default) and uses ordinary include paths. This removes an
optional host build optimization without substituting a missing target API.
