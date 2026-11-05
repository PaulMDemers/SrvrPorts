#!/usr/bin/env python3
"""Exercise actual patched WTF TLS retirement with deterministic scheduling.

Supply ThreadingPOSIX.cpp from the matching patched WebKit source. No VM runs.
The old source must fail the two valid Running handoff cases, not extraction.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess


def function(source, signature, optional=False):
    start = source.find(signature)
    if start < 0:
        if optional:
            return ''
        raise ValueError('missing actual function: ' + signature)
    opening = source.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end] + '\n'


HOST = r'''
#include <atomic>
#include <cassert>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstring>
#define OS(x) OS_##x
#define OS_SRVROS 1
#define HAVE(x) 0
#define ASSERT(x) assert(x)
using HostSigSet = uint64_t;
#define sigset_t HostSigSet
#undef SIGUSR1
#undef SIG_BLOCK
#define SIGUSR1 10
#define SIG_BLOCK 1
enum : unsigned { srvIdle, srvRequested, srvSuspended, srvResume, srvRunning };
struct Thread;
static Thread* srvCurrentThread;
static Thread* target;
static const char* scenario;
static unsigned blocked, exited, derefed, yielded, clockReads, ack;
static long nativeTid = 17;
static bool publishedIdle;
static bool is(const char* s) { return !strcmp(scenario, s); }
struct Fatal { };
[[noreturn]] static void srvFatal() { throw Fatal { }; }
static int host_empty(HostSigSet* p) { *p = 0; return 0; }
static int host_add(HostSigSet* p, int n) { *p |= uint64_t { 1 } << n; return 0; }
static int host_mask(int how, const HostSigSet* p, void*) {
    assert(how == SIG_BLOCK && *p == (uint64_t { 1 } << SIGUSR1));
    ++blocked;
    return 0;
}
static int host_pending(HostSigSet* p) {
    assert(blocked && !exited && !derefed);
    *p = (is("pending") || (is("pending-after-drain") && yielded)) ? (uint64_t { 1 } << SIGUSR1) : 0;
    return 0;
}
static int host_member(const HostSigSet* p, int n) { return (*p >> n) & 1; }
#define sigemptyset host_empty
#define sigaddset host_add
#define pthread_sigmask host_mask
#define sigpending host_pending
#define sigismember host_member
static long srv_thread_self() { assert(blocked); return nativeTid; }
static long srv_monotonic_us() {
    ++clockReads;
    if (is("negative-clock")) return -1;
    if (is("overflow-clock")) return LONG_MAX;
    if (is("backward-clock")) return clockReads == 1 ? 100 : 99;
    if (is("later-backward-clock")) return clockReads < 4 ? 100 + clockReads : 101;
    if (is("late-idle")) return clockReads < 3 ? 100 + clockReads : 10000101;
    if (is("stalled-clock")) return 100;
    return 100 + static_cast<long>(clockReads - 1) * 1000000;
}
struct TrackedCount {
    unsigned value = 0;
    operator unsigned() const { assert(!publishedIdle); return value; }
    void operator--() { assert(!publishedIdle && value); --value; }
};
struct Phase {
    std::atomic<unsigned> value { srvIdle };
    unsigned load(std::memory_order order = std::memory_order_seq_cst) { return value.load(order); }
    void store(unsigned phase, std::memory_order order = std::memory_order_seq_cst) {
        value.store(phase, order);
        if (is("resume-final") && phase == srvIdle) publishedIdle = true;
    }
};
struct ThreadSuspendLocker { };
static int pthread_kill(int handle, int signal) { assert(handle == 17 && signal == SIGUSR1); return 0; }
struct Thread {
    bool m_isDestroyedOnce = true, m_srvInitialized = true;
    Phase m_srvPhase;
    std::atomic<uint64_t> m_srvTicket { 0 }, m_srvAckTicket { 0 };
    uint64_t m_srvTid = 17;
    int m_handle = 17;
    TrackedCount m_suspendCount;
    void* m_clientData = reinterpret_cast<void*>(1);
    static constexpr int s_key = 1, InvalidThreadSpecificKey = 0;
    void waitSrvrOSSignalAck(unsigned phase, uint64_t ticket) {
        assert(phase == srvRunning && ticket == 1 && ack == 1);
        --ack;
        m_srvAckTicket.store(ticket);
        m_srvPhase.store(srvRunning, std::memory_order_release);
    }
    void resume(const ThreadSuspendLocker&);
    static void destructTLS(void*);
    void didExit() { assert(m_srvPhase.load() == srvIdle && !srvCurrentThread); ++exited; }
    void deref() { assert(exited == 1); ++derefed; }
};
static void threadSpecificSet(int key, Thread* p) { assert(key == 1 && p == target); }
static void srv_yield() {
    ++yielded;
    assert(blocked && srvCurrentThread == target && !exited && !derefed);
    if (is("posted") || is("consumed") || is("pending-after-drain") || is("last-ticket") || is("late-idle")) {
        // Controller consumes the matching final semaphore ACK, validates its
        // ticket/phase, then publishes Idle. It still owns the Thread reference.
        assert(target->m_srvPhase.load() == srvRunning);
        assert(target->m_srvAckTicket.load() == target->m_srvTicket.load());
        if (ack) --ack;
        target->m_srvPhase.store(srvIdle, std::memory_order_release);
    }
    if (is("ticket-change")) target->m_srvTicket.fetch_add(1);
    if (is("ack-change")) target->m_srvAckTicket.fetch_add(1);
    if (is("phase-change")) target->m_srvPhase.store(srvRequested);
    if (is("tls-change")) srvCurrentThread = nullptr;
    if (is("tid-change")) ++nativeTid;
    if (is("init-change")) target->m_srvInitialized = false;
}
#include "retirement-under-test.inc"
int main(int argc, char** argv) {
    assert(argc == 2);
    scenario = argv[1];
    Thread thread;
    target = srvCurrentThread = &thread;
    if (!strncmp(scenario, "resume-", 7)) {
        thread.m_srvTicket.store(is("resume-zero-ticket") ? 0 : 1);
        thread.m_srvPhase.store(is("resume-wrong-phase") ? srvRequested : srvSuspended);
        thread.m_suspendCount.value = is("resume-zero-count") ? 0 : (is("resume-nested") ? 2 : 1);
        ack = 1;
        bool success = is("resume-final") || is("resume-nested");
        try {
            thread.resume(ThreadSuspendLocker { });
            if (!success) { puts("FAIL invalid resume accepted"); return 1; }
        } catch (Fatal&) {
            if (success) { puts("FAIL valid resume rejected"); return 1; }
        }
        if (is("resume-final")) assert(publishedIdle && !thread.m_suspendCount.value && !ack);
        if (is("resume-nested")) assert(thread.m_suspendCount.value == 1 && ack == 1 && !publishedIdle);
        printf("retirement %s: PASS\n", scenario);
        return 0;
    }
    bool success = is("idle") || is("idle-completed") || is("first-pass") || is("posted") || is("consumed") || is("last-ticket");
    bool running = !is("idle") && !is("first-pass") && !is("pending") && !is("foreign")
        && !is("wrong-tid") && !is("uninitialized") && !is("requested") && !is("suspended") && !is("resume");
    if (running) {
        thread.m_srvTicket.store(1); thread.m_srvAckTicket.store(1);
        thread.m_srvPhase.store(srvRunning);
    }
    if (is("requested")) thread.m_srvPhase.store(srvRequested);
    if (is("suspended")) thread.m_srvPhase.store(srvSuspended);
    if (is("resume")) thread.m_srvPhase.store(srvResume);
    if (is("first-pass")) thread.m_isDestroyedOnce = false;
    if (is("foreign")) srvCurrentThread = nullptr;
    if (is("wrong-tid")) nativeTid = 18;
    if (is("uninitialized")) thread.m_srvInitialized = false;
    if (is("zero-ticket")) thread.m_srvTicket.store(0);
    if (is("wrong-ack")) thread.m_srvAckTicket.store(2);
    if (is("idle-completed") || is("idle-wrong-ack")) {
        thread.m_srvPhase.store(srvIdle);
        thread.m_srvTicket.store(1);
        thread.m_srvAckTicket.store(is("idle-wrong-ack") ? 2 : 1);
    }
    if (is("last-ticket")) { thread.m_srvTicket.store(UINT64_MAX); thread.m_srvAckTicket.store(UINT64_MAX); }
    ack = is("consumed") ? 0 : 1;
    try {
        Thread::destructTLS(&thread);
        if (!success) { puts("FAIL unsafe retirement accepted"); return 1; }
    } catch (Fatal&) {
        assert(!exited && !derefed);
        if (success) { puts("FAIL valid retirement rejected"); return 1; }
    }
    if (success && !is("first-pass")) {
        assert(exited == 1 && derefed == 1 && !srvCurrentThread && blocked == 1);
        if (is("posted") || is("consumed") || is("last-ticket")) assert(yielded && !ack);
    }
    if (is("first-pass")) assert(thread.m_isDestroyedOnce && !blocked && !exited && !derefed);
    if (is("stalled-clock")) assert(yielded == 100000);
    printf("retirement %s: PASS yields=%u\n", scenario, yielded);
}
'''

CASES = ('idle', 'idle-completed', 'idle-wrong-ack', 'first-pass', 'posted', 'consumed', 'last-ticket', 'pending', 'pending-after-drain',
         'requested', 'suspended', 'resume', 'foreign', 'wrong-tid', 'uninitialized',
         'zero-ticket', 'wrong-ack', 'ticket-change', 'ack-change', 'phase-change',
         'tls-change', 'tid-change', 'init-change', 'negative-clock', 'overflow-clock', 'backward-clock',
         'later-backward-clock', 'late-idle', 'timeout', 'stalled-clock', 'resume-final', 'resume-nested',
         'resume-zero-count', 'resume-zero-ticket', 'resume-wrong-phase')


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--cxx', default='c++')
    p.add_argument('--sanitizers', action='store_true')
    p.add_argument('--baseline', type=Path, help='assert INFO handler is byte-identical to the qualified source')
    p.add_argument('--expect-old-failure', action='store_true', help='run only the two valid retirement schedules against the old source')
    a = p.parse_args()
    raw = a.source.read_bytes()
    text = raw.decode()
    body = function(text, 'void Thread::destructTLS(void* data)')
    body += function(text, 'void Thread::resume(const ThreadSuspendLocker&)')
    handler = function(text, 'void Thread::signalHandlerSuspendResume(int signal, siginfo_t* info, void* opaque)')
    if a.baseline:
        assert handler == function(a.baseline.read_text(), 'void Thread::signalHandlerSuspendResume(int signal, siginfo_t* info, void* opaque)')
    a.output.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, TMPDIR=str(a.output.resolve()),
               ASAN_OPTIONS='halt_on_error=1:detect_leaks=0',
               UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    (a.output / 'retirement-under-test.inc').write_text(body)
    fixture = a.output / 'host.cpp'
    fixture.write_text(HOST)
    profiles = [('strict', [])]
    if a.sanitizers:
        profiles.append(('asan-ubsan', ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']))
    record = {'source': str(a.source.resolve()), 'source_sha256': hashlib.sha256(raw).hexdigest(),
              'body_sha256': hashlib.sha256(body.encode()).hexdigest(), 'commands': [], 'cases': []}
    record['unchanged_handler_sha256'] = hashlib.sha256(handler.encode()).hexdigest()
    failed = False
    for name, flags in profiles:
        exe = a.output / name
        cmd = shlex.split(a.cxx) + ['-std=c++17', '-O2', '-g', '-Wall', '-Wextra', '-Werror',
                                  '-Wno-unused-function', *flags, str(fixture), '-o', str(exe)]
        record['commands'].append(cmd)
        subprocess.run(cmd, check=True, timeout=60, env=env)
        for case in (('posted', 'consumed') if a.expect_old_failure else CASES):
            r = subprocess.run([str(exe), case], capture_output=True, text=True, timeout=15, env=env)
            record['cases'].append({'profile': name, 'case': case, 'exit': r.returncode,
                                    'stdout': r.stdout, 'stderr': r.stderr})
            print(name, case, r.returncode, r.stdout.strip())
            failed |= (r.returncode != 1 or 'FAIL valid retirement rejected' not in r.stdout) if a.expect_old_failure else r.returncode != 0
    record['result'] = 'FAIL' if failed else 'PASS'
    (a.output / 'receipt.json').write_text(json.dumps(record, indent=2) + '\n')
    return int(failed)


if __name__ == '__main__':
    raise SystemExit(main())
