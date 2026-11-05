// Experimental JSC conservative-root probe: one mode per fresh process.
// Qualification requires auditing the target assembly and native stack window;
// this source alone does not prove exclusive raw-pointer placement.
#include <cmakeconfig.h>
#include <JavaScriptCore/JavaScript.h>
#include <wtf/MainThread.h>
#include <wtf/Threading.h>
#include <atomic>
#include <cstdint>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Pinned upstream API/JSWeakPrivate.h and API/JSBase.cpp.
extern "C" {
typedef const struct OpaqueJSWeak* JSWeakRef;
JSWeakRef JSWeakCreate(JSContextGroupRef,JSObjectRef);
void JSWeakRelease(JSContextGroupRef,JSWeakRef);
JSObjectRef JSWeakGetObject(JSWeakRef);
void JSSynchronousGarbageCollectForDebugging(JSContextRef);
void jsc_gc_roots_park(uintptr_t,unsigned,std::atomic<unsigned>*,std::atomic<unsigned>*);
}
static std::atomic<JSWeakRef> weakRef { nullptr };
static std::atomic<unsigned> finalized { 0 };
static void finalize(JSObjectRef) { finalized.fetch_add(1,std::memory_order_relaxed); }
static void fail(const char* what) { fprintf(stderr,"jsc-gc-root: failed %s\n",what); abort(); }
static void check(bool value,const char* what) { if(!value) fail(what); }
static void waitFlag(std::atomic<unsigned>& flag,const char* why) {
    for(unsigned i=0;i<1000000 && !flag.load(std::memory_order_acquire);i++) WTF::Thread::yield();
    check(flag.load(std::memory_order_acquire),why);
}

// Raw object pointer is confined to this completed helper frame. The temporary
// protect bridges allocation to assembly publication; a separate short-lived
// thread removes it before collection. The live worker caller gets only ~ptr.
__attribute__((noinline)) static uintptr_t makeEncoded(JSGlobalContextRef context,JSContextGroupRef group,JSClassRef klass) {
    JSObjectRef object=JSObjectMake(context,klass,nullptr);
    check(object,"allocate object");
    JSValueProtect(context,object);
    JSWeakRef weak=JSWeakCreate(group,object);
    check(weak,"create weak handle");
    weakRef.store(weak,std::memory_order_release);
    return ~reinterpret_cast<uintptr_t>(object);
}

// The retained target profile must use -mno-red-zone. Clear caller-saved
// remnants and the collecting thread's red zone after GC before any later GC.
__attribute__((always_inline)) static inline void scrubCollectorScratch() {
    asm volatile(
        "movq $0,-8(%%rsp)\n\t movq $0,-16(%%rsp)\n\t movq $0,-24(%%rsp)\n\t movq $0,-32(%%rsp)\n\t"
        "movq $0,-40(%%rsp)\n\t movq $0,-48(%%rsp)\n\t movq $0,-56(%%rsp)\n\t movq $0,-64(%%rsp)\n\t"
        "movq $0,-72(%%rsp)\n\t movq $0,-80(%%rsp)\n\t movq $0,-88(%%rsp)\n\t movq $0,-96(%%rsp)\n\t"
        "movq $0,-104(%%rsp)\n\t movq $0,-112(%%rsp)\n\t movq $0,-120(%%rsp)\n\t movq $0,-128(%%rsp)\n\t"
        "xorl %%eax,%%eax\n\t xorl %%ecx,%%ecx\n\t xorl %%edx,%%edx\n\t xorl %%esi,%%esi\n\t xorl %%edi,%%edi\n\t"
        "xorl %%r8d,%%r8d\n\t xorl %%r9d,%%r9d\n\t xorl %%r10d,%%r10d\n\t xorl %%r11d,%%r11d\n\t"
        "pxor %%xmm0,%%xmm0\n\t pxor %%xmm1,%%xmm1\n\t pxor %%xmm2,%%xmm2\n\t pxor %%xmm3,%%xmm3\n\t"
        "pxor %%xmm4,%%xmm4\n\t pxor %%xmm5,%%xmm5\n\t pxor %%xmm6,%%xmm6\n\t pxor %%xmm7,%%xmm7\n\t"
        "pxor %%xmm8,%%xmm8\n\t pxor %%xmm9,%%xmm9\n\t pxor %%xmm10,%%xmm10\n\t pxor %%xmm11,%%xmm11\n\t"
        "pxor %%xmm12,%%xmm12\n\t pxor %%xmm13,%%xmm13\n\t pxor %%xmm14,%%xmm14\n\t pxor %%xmm15,%%xmm15"
        : : : "rax","rcx","rdx","rsi","rdi","r8","r9","r10","r11",
              "xmm0","xmm1","xmm2","xmm3","xmm4","xmm5","xmm6","xmm7",
              "xmm8","xmm9","xmm10","xmm11","xmm12","xmm13","xmm14","xmm15","memory","cc");
}

// Confine the raw return from JSWeakGetObject to a joined observer's stack.
// The collecting thread retains only this boolean, allowing a later collection
// after the target drops the root without a stale JSObjectRef in its frame.
static bool weakAlive(JSWeakRef weak) {
    std::atomic<unsigned> observed {0};
    auto observer=WTF::Thread::create("jsc-root-observe"_s,[&] {
        observed.store(JSWeakGetObject(weak)?2u:1u,std::memory_order_release);
    });
    check(!observer->waitForCompletion(),"join weak observer");
    unsigned value=observed.load(std::memory_order_acquire);
    check(value==1 || value==2,"weak observation");
    return value==2;
}

int main(int argc,char** argv) {
    if(argc!=2) fail("mode argument");
    unsigned mode;
    if(!strcmp(argv[1],"control")) mode=0;
    else if(!strcmp(argv[1],"gp")) mode=1;
    else if(!strcmp(argv[1],"fx")) mode=2;
    else if(!strcmp(argv[1],"redzone")) mode=3;
    else fail("unknown mode");
    static_assert(std::atomic<unsigned>::is_always_lock_free);
    static_assert(std::atomic<JSWeakRef>::is_always_lock_free);
    WTF::initialize();WTF::initializeMainThread();
    JSContextGroupRef group=JSContextGroupCreate();check(group,"create group");
    JSGlobalContextRef context=JSGlobalContextCreateInGroup(group,nullptr);check(context,"create context");
    JSClassDefinition definition=kJSClassDefinitionEmpty;definition.finalize=finalize;
    JSClassRef klass=JSClassCreate(&definition);check(klass,"create class");
    std::atomic<unsigned> ready {0},release {0},unprotected {0};
    auto target=WTF::Thread::create("jsc-root-target"_s,[&] {
        uintptr_t encoded=makeEncoded(context,group,klass);
        jsc_gc_roots_park(encoded,mode,&ready,&release);
    });
    waitFlag(ready,"park target deadline");
    JSWeakRef weak=weakRef.load(std::memory_order_acquire);check(weak,"published weak");
    // Any raw object reference used for unprotect lives only on this other
    // thread's stack, which must retire before the main thread collects.
    auto dropper=WTF::Thread::create("jsc-root-drop"_s,[&] {
        JSObjectRef object=JSWeakGetObject(weak);
        if(!object) fail("protected object disappeared");
        JSValueUnprotect(context,object);
        unprotected.store(1,std::memory_order_release);
    });
    check(!dropper->waitForCompletion(),"join unprotect worker");
    check(unprotected.load(std::memory_order_acquire),"unprotect completed");
    // Public JSGarbageCollect is only a hint in this pinned JSC revision.
    JSSynchronousGarbageCollectForDebugging(context);
    scrubCollectorScratch();
    bool alive=weakAlive(weak);
    check(alive==(mode!=0),"weak object after full collection");
    check(mode==0 || !finalized.load(std::memory_order_acquire),"live object finalized");
    release.store(1,std::memory_order_release);
    check(!target->waitForCompletion(),"join target");
    if(mode!=0) {
        JSSynchronousGarbageCollectForDebugging(context);
        check(!weakAlive(weak),"object remained after target root retired");
    }
    JSWeakRelease(group,weak);
    JSClassRelease(klass);JSGlobalContextRelease(context);JSContextGroupRelease(group);
    printf("jsc-gc-root: %s %s; finalizers=%u\n",argv[1],alive?"retained":"collected",finalized.load());
    puts("jsc-gc-root: ok");
    return 0;
}
