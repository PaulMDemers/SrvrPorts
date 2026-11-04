#include <unicode/ubrk.h>
#include <unicode/uclean.h>
#include <unicode/unum.h>
#include <unicode/ucol.h>
#include <unicode/unorm2.h>
#include <unicode/ustring.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>

static void check(bool condition, const char *what) {
    if (!condition) { fprintf(stderr, "icuprobe: failed %s\n", what); abort(); }
}
static_assert(sizeof(wchar_t) == 4, "SrvrOS ICU requires 32-bit wchar_t");
#ifndef U_WCHAR_IS_UTF32
#error SrvrOS ICU wchar API must be UTF-32
#endif
static int ready;
static void *worker(void *) {
    __atomic_add_fetch(&ready, 1, __ATOMIC_SEQ_CST);
    while (__atomic_load_n(&ready, __ATOMIC_SEQ_CST) != 2) sched_yield();
    for (int i = 0; i < 40; ++i) {
        UErrorCode e = U_ZERO_ERROR;
        const UNormalizer2 *n = unorm2_getNFCInstance(&e);
        check(U_SUCCESS(e) && n, "NFC instance");
        sched_yield();
        UChar input[] = {0x0065, 0x0301}; UChar output[8];
        int32_t size = unorm2_normalize(n, input, 2, output, 8, &e);
        check(U_SUCCESS(e) && size == 1 && output[0] == 0x00e9, "NFC");
    }
    return nullptr;
}
extern "C" int main() {
    UErrorCode init = U_ZERO_ERROR;
    u_init(&init);
    check(U_SUCCESS(init), "ICU data initialization");
    pthread_t a, b;
    check(!pthread_create(&a, nullptr, worker, nullptr), "thread a");
    check(!pthread_create(&b, nullptr, worker, nullptr), "thread b");
    check(!pthread_join(a, nullptr) && !pthread_join(b, nullptr), "join");
    UErrorCode e = U_ZERO_ERROR;
    UChar input[] = {0x0053, 0x0074, 0x0072, 0x0061, 0x00df, 0x0065};
    UChar out[20];
    int32_t len = u_strToUpper(out, 20, input, 6, "de", &e);
    const UChar expected[] = {0x0053, 0x0054, 0x0052, 0x0041, 0x0053, 0x0053, 0x0045};
    check(U_SUCCESS(e) && len == 7 && u_memcmp(out, expected, 7) == 0, "upper");
    e = U_ZERO_ERROR;
    const UChar supplementary[] = {0xd83d, 0xde00};
    wchar_t wide[4]; int32_t wide_len = 0;
    u_strToWCS(wide, 4, &wide_len, supplementary, 2, &e);
    check(U_SUCCESS(e) && wide_len == 1 && wide[0] == 0x1f600, "UTF32 wchar output");
    e = U_ZERO_ERROR; int32_t roundtrip_len = 0;
    u_strFromWCS(out, 20, &roundtrip_len, wide, wide_len, &e);
    check(U_SUCCESS(e) && roundtrip_len == 2 && u_memcmp(out, supplementary, 2) == 0, "UTF32 wchar roundtrip");
    e = U_ZERO_ERROR;
    u_strFromUTF8(out, 20, nullptr, "\xc3\x28", 2, &e);
    check(e == U_INVALID_CHAR_FOUND, "invalid UTF8");
    e = U_ZERO_ERROR;
    UCollator *col = ucol_open("en_US", &e);
    const UChar aa[] = {0x0061}; const UChar bb[] = {0x0062};
    check(U_SUCCESS(e) && col && ucol_strcoll(col, aa, 1, bb, 1) == UCOL_LESS, "collation");
    ucol_close(col);
    e = U_ZERO_ERROR;
    col = ucol_open("sv_SE", &e);
    const UChar z[] = {0x007a}; const UChar a_diaeresis[] = {0x00e4};
    check(U_SUCCESS(e) && col && ucol_strcoll(col, z, 1, a_diaeresis, 1) == UCOL_LESS,
          "Swedish collation");
    ucol_close(col);
    e = U_ZERO_ERROR;
    const UChar text[] = {0x0068,0x0065,0x006c,0x006c,0x006f,0x0020,0x0077,0x006f,0x0072,0x006c,0x0064};
    UBreakIterator *br = ubrk_open(UBRK_WORD, "en_US", text, 11, &e);
    check(U_SUCCESS(e) && br && ubrk_first(br) == 0 && ubrk_next(br) == 5 &&
          ubrk_next(br) == 6 && ubrk_next(br) == 11, "word breaks");
    ubrk_close(br);
    e = U_ZERO_ERROR;
    UNumberFormat *fmt = unum_open(UNUM_DECIMAL, nullptr, 0, "en_US", nullptr, &e);
    check(U_SUCCESS(e) && fmt, "number formatter");
    int32_t n = unum_formatDouble(fmt, 1234.5, out, 20, nullptr, &e);
    const UChar number[] = {0x31,0x2c,0x32,0x33,0x34,0x2e,0x35};
    check(U_SUCCESS(e) && fmt && n == 7 && u_memcmp(out, number, 7) == 0, "number format");
    unum_close(fmt);
    e = U_ZERO_ERROR;
    fmt = unum_open(UNUM_DECIMAL, nullptr, 0, "de_DE", nullptr, &e);
    check(U_SUCCESS(e) && fmt, "German formatter");
    n = unum_formatDouble(fmt, 1234.5, out, 20, nullptr, &e);
    const UChar german_number[] = {0x31,0x2e,0x32,0x33,0x34,0x2c,0x35};
    check(U_SUCCESS(e) && n == 7 && u_memcmp(out, german_number, 7) == 0, "German number format");
    unum_close(fmt);
    u_cleanup();
    init = U_ZERO_ERROR;
    u_init(&init);
    check(U_SUCCESS(init), "ICU reinitialization");
    init = U_ZERO_ERROR;
    const UNormalizer2 *again = unorm2_getNFCInstance(&init);
    check(U_SUCCESS(init) && again, "NFC after reinitialization");
    u_cleanup();
    puts("icuprobe: ok");
    return 0;
}
