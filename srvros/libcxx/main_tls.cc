#include <stdio.h>
struct MainLocal { ~MainLocal() { puts("cxxthreads: main tls destructor"); } };
static thread_local MainLocal main_local;
extern "C" void touch_main_tls() { (void)&main_local; }
