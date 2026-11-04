// Experimental actual-WTF suspend/register probe. No GC root-retention claim.
#include <cmakeconfig.h>
#include <wtf/MainThread.h>
#include <wtf/Threading.h>
#include <atomic>
#include <cstdint>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

static constexpr uint64_t gpMarker = UINT64_C(0x526f6f7452313234);
static constexpr uint64_t fxMarker = UINT64_C(0x584d4d31356d6172);
static constexpr uint64_t redMarker = UINT64_C(0x5265645a6f6e6531);
struct Spin {
    std::atomic<unsigned> ready { 0 }, stop { 0 };
    uint64_t gpAfter { 0 }, fxAfter { 0 }, redAfter { 0 };
};
static void fail(const char* why) { fprintf(stderr,"wtf-signal: failed %s\n",why); abort(); }
static void check(bool value,const char* why) { if (!value) fail(why); }

// No calls or RSP changes in the marked window. Its red-zone word is not a GC
// object; this checks the captured stack range and byte preservation only.
__attribute__((noinline)) static void markedSpin(Spin& spin)
{
    static_assert(std::atomic<unsigned>::is_always_lock_free);
    asm volatile(
        "movabsq $0x526f6f7452313234, %%r12\n\t"
        "movabsq $0x584d4d31356d6172, %%rax\n\t"
        "movq %%rax, %%xmm15\n\t"
        "movabsq $0x5265645a6f6e6531, %%rax\n\t"
        "movq %%rax, -128(%%rsp)\n\t"
        "movl $1, (%[ready])\n\t"
        "1: cmpl $0, (%[stop])\n\t"
        "je 1b\n\t"
        "movq %%r12, (%[gp])\n\t"
        "movq %%xmm15, %%rax\n\t"
        "movq %%rax, (%[fx])\n\t"
        "movq -128(%%rsp), %%rax\n\t"
        "movq %%rax, (%[red])\n\t"
        : : [ready] "r" (&spin.ready), [stop] "r" (&spin.stop),
            [gp] "r" (&spin.gpAfter), [fx] "r" (&spin.fxAfter), [red] "r" (&spin.redAfter)
        : "rax", "r12", "xmm15", "memory", "cc");
}

static void waitReady(Spin& spin)
{
    for (unsigned i=0; i<1000000 && !spin.ready.load(std::memory_order_acquire); ++i)
        WTF::Thread::yield();
    check(spin.ready.load(std::memory_order_acquire),"target startup deadline");
}

static void checkResumed(const Spin& spin)
{
    check(spin.gpAfter==gpMarker && spin.fxAfter==fxMarker && spin.redAfter==redMarker,"resumed register and red-zone bytes");
}

static void checkSuspended(WTF::Thread& thread, const WTF::ThreadSuspendLocker& locker)
{
    WTF::PlatformRegisters regs { };
    check(thread.getRegisters(locker,regs)==sizeof(regs),"register size");
    check(regs.snapshot.r12==gpMarker,"interrupted r12");
    uint64_t xmm=0;
    memcpy(&xmm,regs.snapshot.fpu+160+15*16,sizeof(xmm));
    check(xmm==fxMarker,"interrupted xmm15");
    check(regs.snapshot.rsp>=128,"stack underflow");
    auto* word=reinterpret_cast<const uint64_t*>(regs.snapshot.rsp-128);
    check(thread.stack().contains(const_cast<uint64_t*>(word)),"red-zone stack bounds");
    check(*word==redMarker,"red-zone bytes");
}

int main()
{
    WTF::initialize();
    WTF::initializeMainThread();
    WTF::Thread& mainThread=WTF::Thread::currentSingleton();
    Spin mainSpin;
    auto controller=WTF::Thread::create("wtf-signal-main"_s,[&] {
        waitReady(mainSpin);
        WTF::ThreadSuspendLocker locker;
        check(!!mainThread.suspend(locker),"suspend main");
        checkSuspended(mainThread,locker);
        mainSpin.stop.store(1,std::memory_order_release);
        mainThread.resume(locker);
    });
    markedSpin(mainSpin);
    check(!controller->waitForCompletion(),"join main controller");
    checkResumed(mainSpin);
    puts("wtf-signal: main registers and red zone ok");

    Spin a,b;
    auto first=WTF::Thread::create("wtf-signal-one"_s,[&] { markedSpin(a); });
    auto second=WTF::Thread::create("wtf-signal-two"_s,[&] { markedSpin(b); });
    waitReady(a);waitReady(b);
    {
        WTF::ThreadSuspendLocker locker;
        check(!!first->suspend(locker),"suspend first");
        check(!!second->suspend(locker),"suspend second");
        checkSuspended(first.get(),locker);checkSuspended(second.get(),locker);
        check(first->signal(SIGUSR1),"spurious signal send");
        for(unsigned i=0;i<256;i++) WTF::Thread::yield();
        checkSuspended(first.get(),locker);
        a.stop.store(1,std::memory_order_release);b.stop.store(1,std::memory_order_release);
        second->resume(locker);first->resume(locker);
    }
    check(!first->waitForCompletion() && !second->waitForCompletion(),"join workers");
    checkResumed(a);checkResumed(b);
    puts("wtf-signal: two suspended targets and extra signal ok");
    for(unsigned i=0;i<8;i++) {
        Spin cycle;
        auto worker=WTF::Thread::create("wtf-signal-cycle"_s,[&] { markedSpin(cycle); });
        waitReady(cycle);
        {
            WTF::ThreadSuspendLocker locker;
            check(!!worker->suspend(locker),"cycle suspend");
            checkSuspended(worker.get(),locker);
            cycle.stop.store(1,std::memory_order_release);
            worker->resume(locker);
        }
        check(!worker->waitForCompletion(),"cycle join");
        checkResumed(cycle);
    }
    puts("wtf-signal: startup and retirement cycles ok");
    puts("wtf-signal: ok");
    return 0;
}
