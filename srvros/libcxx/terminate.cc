#include <stdlib.h>
#include <stdio.h>
namespace std {
[[noreturn]] void terminate() noexcept { fputs("libc++ terminate\n", stderr); abort(); }
}
