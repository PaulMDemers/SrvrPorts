#include <stdlib.h>
#include <stdio.h>
namespace std {
[[noreturn]] void terminate() noexcept { fputs("libc++ terminate\n", stderr); abort(); }
inline namespace _LIBCPP_ABI_NAMESPACE {
[[noreturn]] void __throw_system_error(int error, const char *message) {
    fprintf(stderr, "libc++ system error %d: %s\n", error, message); abort();
}
}
}
