#include <stdio.h>
#include <stdlib.h>
extern "C" int __cxa_thread_atexit(void (*)(void *), void *, void *);
extern "C" int __cxa_atexit(void (*)(void *), void *, void *);
static int tls_done;
static void tls_final(void *) { tls_done=1; puts("cxxthreads: tls-first tls"); }
static void global_final(void *) {
    if (!tls_done) { fputs("cxxthreads: tls-first wrong order\n", stderr); abort(); }
    puts("cxxthreads: tls-first global");
}
extern "C" int main() {
    if (__cxa_thread_atexit(tls_final, nullptr, nullptr)) abort();
    if (__cxa_atexit(global_final, nullptr, nullptr)) abort();
    puts("cxxthreads: tls-first registered");
    return 0;
}
