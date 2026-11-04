#include <stdlib.h>
#include <stdio.h>
namespace std {
inline namespace _LIBCPP_ABI_NAMESPACE {
[[noreturn]] void __throw_system_error(int error, const char *message) {
    fprintf(stderr, "libc++ system error %d: %s\n", error, message); abort();
}
}
}
