# Build the standalone GNU command with its selected gnulib subset.
SHA256SUM_PORT := ports/srvros/sha256sum
SHA256SUM_ARCHIVE := ports/upstream/coreutils/coreutils-9.12.tar.xz
SHA256SUM_DIR := build/ports/sha256sum
SHA256SUM_SOURCE := $(SHA256SUM_DIR)/source
SHA256SUM_GENERATED := $(SHA256SUM_PORT)/generated
SHA256SUM_GNULIB := \
	c32isprint.c close-stream.c closeout.c error.c exitfail.c fadvise.c \
	free.c getopt.c getopt1.c lc-charset-dispatch.c localcharset.c malloc.c \
	mbrtoc32.c progname.c propername-lite.c quotearg.c sha256-stream.c \
	sha256.c unictype/ctype_print.c version-etc-fsf.c version-etc.c \
	xalloc-die.c xmalloc.c
SHA256SUM_UPSTREAM_OBJECTS := $(SHA256SUM_DIR)/src/cksum.o \
	$(addprefix $(SHA256SUM_DIR)/lib/,$(SHA256SUM_GNULIB:.c=.o))
SHA256SUM_OBJECTS := $(SHA256SUM_UPSTREAM_OBJECTS) $(SHA256SUM_DIR)/version.o $(SHA256SUM_DIR)/profile.o
SHA256SUM_HEADERS := $(shell find $(SHA256SUM_GENERATED) -name '*.h' 2>/dev/null | LC_ALL=C sort)
SHA256SUM_FLAGS := $(filter-out -I shared/include userspace/lib/include,$(USER_CFLAGS)) \
	-std=gnu2x -Wno-error -Werror=format -Werror=incompatible-pointer-types \
	-DHASH_ALGO_SHA256=1 -DHAVE_CONFIG_H -I $(SHA256SUM_PORT) \
	-I $(SHA256SUM_GENERATED)/lib -I $(SHA256SUM_SOURCE)/lib \
	-I $(SHA256SUM_GENERATED)/src -I $(SHA256SUM_SOURCE)/src \
	-isystem userspace/lib/include -isystem shared/include \
	-include userspace/lib/include/limits.h -include $(SHA256SUM_PORT)/sdk-stdio.h

$(SHA256SUM_SOURCE)/.prepared: $(SHA256SUM_ARCHIVE) $(SHA256SUM_PORT)/prepare.py
	rm -rf $(SHA256SUM_SOURCE)
	$(PYTHON) $(SHA256SUM_PORT)/prepare.py $(SHA256SUM_ARCHIVE) $(SHA256SUM_SOURCE)
	touch $@

$(SHA256SUM_UPSTREAM_OBJECTS): $(SHA256SUM_DIR)/%.o: $(SHA256SUM_SOURCE)/.prepared $(SHA256SUM_PORT)/sdk-stdio.h $(SHA256SUM_PORT)/profile.h $(SHA256SUM_HEADERS) $(USER_SYSROOT_HEADERS) $(ZIG)
	mkdir -p $(dir $@)
	$(CC) $(SHA256SUM_FLAGS) -MMD -MP -c $(SHA256SUM_SOURCE)/$*.c -o $@

$(SHA256SUM_DIR)/version.o: $(SHA256SUM_GENERATED)/src/version.c $(SHA256SUM_SOURCE)/.prepared $(SHA256SUM_PORT)/sdk-stdio.h $(SHA256SUM_HEADERS) $(USER_SYSROOT_HEADERS) $(ZIG)
	mkdir -p $(dir $@)
	$(CC) $(SHA256SUM_FLAGS) -MMD -MP -c $< -o $@

$(SHA256SUM_DIR)/profile.o: $(SHA256SUM_PORT)/profile.c $(SHA256SUM_PORT)/profile.h $(USER_SYSROOT_HEADERS) $(ZIG)
	mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(USER_SHA256SUM): $(ZIG) $(USER_CRT0_OBJ) $(SHA256SUM_OBJECTS) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(SHA256SUM_OBJECTS) $(USER_LIB_OBJ) -o $@

-include $(SHA256SUM_UPSTREAM_OBJECTS:.o=.d) $(SHA256SUM_DIR)/version.d
