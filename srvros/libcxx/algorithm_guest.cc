#include <algorithm>
#include <unordered_set>
#include <stdio.h>
#include <stdlib.h>
extern "C" int main() {
    int values[] = {9, -1, 7, 0, 4, 4, 2, 8, 1, 3, 5, 6};
    std::sort(values, values + sizeof(values)/sizeof(values[0]));
    for (unsigned i=1; i<sizeof(values)/sizeof(values[0]); ++i)
        if (values[i-1] > values[i]) abort();
    std::unordered_set<int> set;
    set.rehash(53);
    for (int i=0;i<100;i++) set.insert(i);
    for (int i=0;i<100;i++) if (set.find(i)==set.end()) abort();
    if (set.size()!=100) abort();
    puts("browser-algorithm: ok");
    return 0;
}
