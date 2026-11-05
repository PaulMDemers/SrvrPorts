// Bounded guest evidence: native 128-bit arithmetic, WTF decimal printing,
// and scalar SIMDUTF Unicode validation/roundtrip. No engine initialization.
#include <cmakeconfig.h>
#include <wtf/Int128.h>
#include <wtf/MemoryFootprint.h>
#include <wtf/SIMDUTF.h>
#include <wtf/StringPrintStream.h>
#include <wtf/text/CString.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void check(bool condition, const char* name)
{
    if (!condition) {
        fprintf(stderr, "wtfprobe: failed %s\n", name);
        abort();
    }
}

static void phase(const char* text)
{
    (void)write(1, text, strlen(text));
}

int main()
{
    phase("wtfprobe: entered\n");
    size_t before = WTF::memoryFootprint();
    phase("wtfprobe: footprint-before\n");
    void* mapping = mmap(nullptr, 8192, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    check(mapping != MAP_FAILED, "private mmap");
    static_cast<volatile char*>(mapping)[0] = 1;
    static_cast<volatile char*>(mapping)[4096] = 2;
    phase("wtfprobe: mapped\n");
    size_t after = WTF::memoryFootprint();
    check(after >= before && after - before >= 8192, "private mapped footprint");
    check(!munmap(mapping, 8192), "private munmap");
    phase("wtfprobe: footprint-checked\n");
    WTF::UInt128 value = (static_cast<WTF::UInt128>(1) << 100) + 12345;
    check((value / 3) * 3 + value % 3 == value, "128-bit division");
    phase("wtfprobe: arithmetic-checked\n");
    WTF::StringPrintStream out;
    WTF::printInternal(out, value);
    auto text = out.toCString();
    check(!strcmp(text.data(), "1267650600228229401496703217721"), "128-bit decimal");
    phase("wtfprobe: printstream-checked\n");
    const char utf8[] = { 'A', static_cast<char>(0xf0), static_cast<char>(0x9f), static_cast<char>(0x98), static_cast<char>(0x80) };
    char16_t utf16[4] { };
    check(simdutf::validate_utf8(utf8, sizeof(utf8)), "valid UTF-8");
    auto units = simdutf::convert_utf8_to_utf16le(utf8, sizeof(utf8), utf16);
    check(units == 3 && utf16[0] == 0x41 && utf16[1] == 0xd83d && utf16[2] == 0xde00, "UTF-16 astral");
    char roundtrip[8] { };
    auto bytes = simdutf::convert_utf16le_to_utf8(utf16, units, roundtrip);
    check(bytes == sizeof(utf8) && !memcmp(roundtrip, utf8, bytes), "UTF-8 roundtrip");
    const char invalid[] = { static_cast<char>(0xc3), static_cast<char>(0x28) };
    check(!simdutf::validate_utf8(invalid, sizeof(invalid)), "invalid UTF-8");
    phase("wtfprobe: unicode-checked\n");
    puts("wtfprobe: ok");
    return 0;
}
