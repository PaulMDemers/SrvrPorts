#include <filesystem>
#include <system_error>
#include <sys/statvfs.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
namespace fs = std::filesystem;
static void check(bool ok, const char* step) { if (!ok) { fprintf(stderr,"fsprobe: failed %s\n",step); abort(); } }
static void put(const fs::path& path, const char* text) {
    int fd = open(path.c_str(), O_CREAT | O_TRUNC | O_WRONLY, 0644);
    check(fd >= 0, "open write");
    size_t n = strlen(text);
    if (n) check(write(fd, text, n) == (ssize_t)n, "write");
    check(close(fd) == 0, "close write");
}
static void content(const fs::path& path, const char* expected) {
    int fd = open(path.c_str(), O_RDONLY); check(fd >= 0, "open read");
    char buf[64] = {}; ssize_t n = read(fd, buf, sizeof(buf));
    check(n == (ssize_t)strlen(expected) && !memcmp(buf, expected, n), "content");
    check(close(fd) == 0, "close read");
}
int main(int argc, char** argv) {
    if (argc != 2) { fputs("fsprobe: expected writable test directory\n", stderr); return 2; }
    fs::path dir(argv[1]); std::error_code ec;
    check(fs::is_directory(dir, ec) && !ec, "test directory");
    fs::path a=dir/"src", b=dir/"copy", empty=dir/"empty", emptycopy=dir/"emptycopy", renamed=dir/"renamed";
    put(a,"source data"); put(empty, "");
    check(fs::copy_file(a,b,ec) && !ec, "copy"); content(b,"source data");
    check(fs::copy_file(empty,emptycopy,ec) && !ec && fs::file_size(emptycopy,ec)==0 && !ec, "copy empty");
    ec.clear(); check(!fs::copy_file(a,b,ec) && !!ec, "copy exists error");
    put(a,"updated"); ec.clear();
    check(fs::copy_file(a,b,fs::copy_options::overwrite_existing,ec) && !ec, "copy overwrite"); content(b,"updated");
    ec.clear(); fs::rename(b,renamed,ec); check(!ec && fs::exists(renamed,ec) && !ec, "rename");
    bool seen=false; for (fs::directory_iterator it(dir,ec), end; !ec && it!=end; it.increment(ec)) if (it->path().filename()=="renamed") seen=true;
    check(!ec && seen, "directory iteration");
    ec.clear(); check(!fs::exists(dir/"missing",ec) && !ec, "missing exists");
    ec.clear(); check(!fs::copy_file(dir/"missing",b,ec) && !!ec, "missing copy error");
    struct statvfs st; (void)statvfs(dir.c_str(), &st); // C++ declaration must link to real C ABI.
    for (const auto& path : {a,empty,emptycopy,renamed}) { ec.clear(); check(fs::remove(path,ec) && !ec,"cleanup"); }
    puts("fsprobe: ok"); return 0;
}
