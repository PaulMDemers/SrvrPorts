#include <typeinfo>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Base { virtual ~Base() {} int base = 11; };
struct Side { virtual ~Side() {} int side = 22; };
struct Derived : Base, Side { int value = 33; };
struct Unrelated : Base { int other = 44; };

__attribute__((noinline)) static Base *as_base(Derived *value) { return value; }
__attribute__((noinline)) static Derived *downcast(Base *value) { return dynamic_cast<Derived *>(value); }
__attribute__((noinline)) static Side *crosscast(Base *value) { return dynamic_cast<Side *>(value); }
__attribute__((noinline)) static Unrelated *failed_cast(Base *value) { return dynamic_cast<Unrelated *>(value); }
__attribute__((noinline)) static Unrelated &bad_reference(Base *value) { return dynamic_cast<Unrelated &>(*value); }
__attribute__((noinline)) static const std::type_info &dynamic_type(Base *value) { return typeid(*value); }

extern "C" int main(int argc, char **argv) {
    Derived object;
    Base *base = as_base(&object);
    if (argc > 1 && strcmp(argv[1], "badcast") == 0) {
        puts("rttiprobe: badcast must abort");
        fflush(stdout);
        (void)bad_reference(base);
        puts("rttiprobe: unexpected success");
        abort();
    }
    Side *side = crosscast(base);
    if (downcast(base) != &object || side != static_cast<Side *>(&object) ||
        reinterpret_cast<void *>(side) == reinterpret_cast<void *>(base) ||
        side->side != 22 || failed_cast(base) != nullptr ||
        dynamic_type(base) != typeid(Derived) || dynamic_type(base) == typeid(Unrelated)) abort();
    puts("rttiprobe: ok");
    return 0;
}
