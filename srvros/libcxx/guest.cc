#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <chrono>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

static std::mutex mutex;
static std::condition_variable cv;
static std::once_flag once;
static std::atomic<int> starts{0}, destructed{0}, initialized{0};
extern "C" int __cxa_atexit(void (*)(void *), void *, void *);
extern "C" void __cxa_finalize(void *);
extern "C" int __cxa_thread_atexit(void (*)(void *), void *, void *);
extern "C" int __cxa_guard_acquire(unsigned long long *);
extern "C" void __cxa_guard_abort(unsigned long long *);
extern "C" void __cxa_guard_release(unsigned long long *);
static int sequence;
static int final_sequence;
static int dso_a, dso_b;
static void final_one(void *) { final_sequence = final_sequence * 10 + 1; }
static void final_two(void *) { final_sequence = final_sequence * 10 + 2; }
static void final_three(void *) { final_sequence = final_sequence * 10 + 3; }
static void final_recursive(void *) {
    final_sequence = final_sequence * 10 + 4;
    if (__cxa_atexit(final_three, nullptr, &dso_a)) abort();
    __cxa_finalize(&dso_a);
}
static void tls_extra(void *) { sequence = sequence * 10 + 3; }
static void tls_first(void *) { sequence = sequence * 10 + 1; }
static void tls_second(void *) { sequence = sequence * 10 + 2; if (__cxa_thread_atexit(tls_extra, nullptr, nullptr)) abort(); }
struct StaticFinal { ~StaticFinal() { puts("cxxthreads: static destructor"); } };
static StaticFinal static_final;
static int count, once_count;
static bool ready;
struct Local { ~Local() { ++destructed; } };
static thread_local Local local;
extern "C" void touch_main_tls();
static void await_starts(int expected) {
    struct timespec begin, now;
    if (clock_gettime(CLOCK_MONOTONIC, &begin)) abort();
    while (starts.load()!=expected) {
        if (clock_gettime(CLOCK_MONOTONIC, &now)) abort();
        if (now.tv_sec - begin.tv_sec >= 15) { fputs("cxxthreads: readiness timeout\n", stderr); abort(); }
        std::this_thread::yield();
    }
}
static int init_static() { ++initialized; for (int i=0;i<100;i++) std::this_thread::yield(); return 42; }
static int get_static() { static int value=init_static(); return value; }
static void worker() {
    (void)&local;
    ++starts;
    std::call_once(once, [] { ++once_count; });
    if (get_static()!=42) abort();
    for (int i=0;i<200;i++) { std::lock_guard<std::mutex> lock(mutex); int current=count; std::this_thread::yield(); count=current+1; }
    std::unique_lock<std::mutex> lock(mutex);
    cv.wait(lock, [] { return ready; });
}
extern "C" int main(int argc, char **argv) {
    puts("cxxthreads: start");
    if (argc > 1 && strcmp(argv[1], "exhaust") == 0) {
        std::thread threads[9];
        for (int i=0;i<8;i++) threads[i]=std::thread([] { ++starts; for (;;) std::this_thread::yield(); });
        await_starts(8);
        puts("cxxthreads: capacity=8; next construction must abort");
        fflush(stdout);
        threads[8]=std::thread([] {});
        puts("EXHAUSTION_UNEXPECTED_SUCCESS"); abort();
    }
    if (__cxa_atexit(final_one, nullptr, &dso_a) ||
        __cxa_atexit(final_two, nullptr, &dso_b) ||
        __cxa_atexit(final_recursive, nullptr, &dso_a)) abort();
    __cxa_finalize(&dso_a);
    if (final_sequence != 431) abort();
    __cxa_finalize(&dso_a);
    if (final_sequence != 431) abort();
    __cxa_finalize(&dso_b);
    if (final_sequence != 4312) abort();
    unsigned long long guard=0;
    if (!__cxa_guard_acquire(&guard)) abort();
    __cxa_guard_abort(&guard);
    if (!__cxa_guard_acquire(&guard)) abort();
    __cxa_guard_release(&guard);
    if (__cxa_guard_acquire(&guard)) abort();
    std::thread tls_test([] { if (__cxa_thread_atexit(tls_first,nullptr,nullptr) || __cxa_thread_atexit(tls_second,nullptr,nullptr)) abort(); });
    tls_test.join();
    if (sequence != 231) abort();
    touch_main_tls();
    std::thread threads[8];
    for (auto &t: threads) t=std::thread(worker);
    await_starts(8);
    {
        std::unique_lock<std::mutex> lock(mutex);
        if (cv.wait_for(lock, std::chrono::milliseconds(10), [] { return ready; })) abort();
        ready=true;
    }
    cv.notify_all();
    for (auto &t: threads) t.join();
    if (count!=1600 || once_count!=1 || initialized!=1 || destructed!=8) abort();
    for (int i=0;i<20;i++) { std::thread t([] { (void)&local; }); t.join(); }
    if (destructed!=28) abort();
    puts("cxxthreads: ok");
    return 0;
}
