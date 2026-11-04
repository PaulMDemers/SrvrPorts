# less 710

`ports/upstream/less` is the unmodified official less 710 release archive,
https://www.greenwoodsoftware.com/less/less-710.tar.gz, SHA-256
`d1008fb78dcae1323ddab664bcb352a61f022b1b131bd8018548e021d975ec7a`.
The archive retains the upstream COPYING and LICENSE terms. `prepare.py` copies
it into `build/ports/less-srvros` and applies the SrvrOS changes there.

The bounded profile is static ASCII text from files or finite stdin pipes in
Terminal, with scrolling, line jumps, literal search, resize and quit. This is
built with `SECURE_COMPILE=1`, `NO_REGEX=1`, and OSC 8 link execution disabled.
Shell escapes, editor, preprocessors, log writing, regular expressions, follow
mode, job control and full Unicode-width correctness are outside the accepted
profile. Interactive terminal initialization recognizes only `TERM=vt100`;
other terminal names fail rather than silently assume ANSI. Non-terminal output
uses less's normal passthrough path. The local termcap implementation describes
deskterm's supported operations, not a general-purpose termcap library. Screen
size is obtained from the tty ioctl when available, with less's normal fallback.

The keyboard uses upstream's fd 2 fallback, checked for a readable terminal;
SrvrOS does not provide `/dev/tty`. Signal handlers only set pending flags;
interruptible reads observe `EINTR` after the SDK dispatcher returns and defer
screen work to less's normal signal processing. The SDK's normal `setjmp` is
used, not `sigsetjmp`. Terminal modes are saved and restored by upstream less.

The target PTY probe receipt `build/review/less/20261103-225541-00_67wtr`
passed navigation, literal search, actual SIGWINCH redraw, unknown TERM refusal
and exact non-terminal output. It also tested `G` on a held-open pipe, Ctrl-C
twice and quit before writer EOF, checking exact termios and alternate-screen
restoration. The graphical Terminal receipt `build/review/less-terminal` passed;
visual inspection confirmed resize revealed REPORT197 and REPORT198 and the
shell was restored. These tests are bounded to the profile above.

The upstream `lesstest/lt/seq200.lt` and `wordwrap.lt` fixtures are exercised
separately by a host runner, not the target PTY probe or the guest. Both pass (18 and 88 steps) on macOS with secure/no-regex configuration.
Two clean target builds produce the same CLI ELF SHA-256
`65b16e3ff879ca892f9d9cd45c8f22b8881cc22b31accdae7691f18f619a7f4c`.
This establishes CLI binary reproducibility, not whole ISO or disk identity.
Host fixture results are separate from the target receipts.

Reproduce target checks from the SrvrOS checkout with
`sh tools/macos/build.sh -j4 all build/userspace/test_less_probe.elf`, then run
`python3 tools/less_smoke.py`, `python3 tools/less_terminal_smoke.py` and
`python3 tools/ports_smoke.py` sequentially. Each pager harness uses its own
throwaway disk and VM. The GUI harness also requires visual screenshot review.
The upstream host run used a separate ignored copy, configured with
`./configure --with-regex=none --with-secure`, built with `make LESSTEST=1`
and `make -C lesstest`, then ran
`./runtest -l ../less_t lt/seq200.lt lt/wordwrap.lt` from `lesstest`.
