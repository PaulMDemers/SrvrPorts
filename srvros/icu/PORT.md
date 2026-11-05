# Experimental ICU 79.1 static port

This recipe targets the official `release-79.1` ICU4C source archive
`icu4c-79.1-sources.tgz` (28,162,455 bytes, SHA-256
`06fa63ecb521048f8082f12ad988f6b678d90cbd88aafce24cbc94902b191d99`)
and `icu4c-79.1-data-bin-l.zip` (12,863,498 bytes, SHA-256
`72489fb316a60ae1c1da205866edd01b17e8fbf8354ad0615e6cc60f74e08811`),
from <https://github.com/unicode-org/icu/releases/tag/release-79.1>. The
release's Unicode license is present in both distributions; preserve it and
other applicable third-party notices when distributing binaries or sources.

Download the exact assets with:

```sh
mkdir -p build/browser-icu
gh release download release-79.1 --repo unicode-org/icu --dir build/browser-icu \
  --pattern 'icu4c-79.1-sources.tgz' --pattern 'icu4c-79.1-data-bin-l.zip'
```

The build verifies exact byte sizes and SHA-256 values before extraction. The
uncompressed complete data image is 33,558,160 bytes. Then run `python3
ports/srvros/icu/build.py` after preparing the matched SDK and the experimental
threaded libc++, RTTI, and hyperbolic math build receipts. The ICU receipt
validates and incorporates their exact input and output hashes. The script verifies inputs and extracts
a clean source tree, generates upstream native cross-build configuration and builds target static
common/i18n libraries, and packages the complete upstream little-endian data as a target
ELF static archive. It limits make to two jobs, monitors a 1.5 GiB free disk
reserve, and writes command logs and a success or failure receipt. It installs
only under `build/browser-icu/sysroot`; it will not overwrite an existing
installation. `build/browser-icu/icuprobe.elf` exercises initialization,
concurrent NFC normalization, German uppercase, invalid UTF-8, English and
Swedish collation, word boundaries, English/German number formatting, cleanup/reinitialization, and
supplementary-codepoint wide-character roundtrip. Guest execution must be
recorded separately. A successful selected-object link is not proof that all
ICU APIs have complete dependencies or runtime behavior.

`mh-srvros` selects static ELF build rules without claiming Linux. The port
selects ICU's supported no-`nl_langinfo` codeset path because the target SDK
has no such API, and retains normal conversion, formatting, collation and
break-iterator features. Upstream ICU defaults keep its optional plug-in loader
disabled; the port does not claim support for dynamic loading or the complete
file and locale environment. It uses real compiler RTTI and the matching SrvrOS
libc++/libc++abi archives. `srvros-wchar.patch` defines ICU's wide-character
API in terms of UTF-32 code points on SrvrOS with a 4-byte `wchar_t`. This is
an ICU API contract, not a claim that SrvrOS locale multibyte conversion
functions implement UTF-8 or UTF-32; their byte-oriented limitations remain a
separate OS runtime issue. The patch is present in the installed ICU headers,
so callers using the SrvrOS target macro receive the same contract as the
library. This is not yet a browser or a completed JavaScriptCore port.

Matched acceptance is recorded in SrvrOS `docs/browser-port.md`: clean ICU run
`_0xar8ww`, guest run `guest-f3hw_u3w` (all 11 combined cases pass), and JSCOnly
configure run `run-uhaxo369` (configuration/generation complete with target ICU
79.1). The separate no-GC whole-archive audit fails on unused exception/future
sections from the limited libc++ `future.o`; complete C++/ICU API closure is
not accepted. The selected ICU guest link and actual guest execution pass.
Keep the matched SDK math additions (`tanhf`/`expm1f`) and runtime receipts;
older SDKs do not supply the required text-segmentation math dependency.
