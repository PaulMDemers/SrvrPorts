# Generated-only GNU diff adaptation; upstream release is untouched.
DIFF_DIR := build/ports/diffutils-srvros
DIFF_CORE_NAMES := analyze context diff dir ed ifdef io normal side system util version
DIFF_CORE_OBJ := $(addprefix $(DIFF_DIR)/src/,$(addsuffix .o,$(DIFF_CORE_NAMES)))
DIFF_FLAGS := $(filter-out -I shared/include userspace/lib/include,$(USER_CFLAGS)) -std=gnu2x -Wno-error -include ports/srvros/diffutils/generated/config.h -include userspace/lib/include/limits.h -I ports/srvros/diffutils/generated -I $(DIFF_DIR)/lib -I $(DIFF_DIR)/src -isystem ports/srvros/diffutils/compat -isystem userspace/lib/include -isystem shared/include -DHAVE_CONFIG_H
DIFF_INPUTS := $(shell find ports/upstream/diffutils -type f 2>/dev/null)
DIFF_CONFIG := $(shell find ports/srvros/diffutils/generated ports/srvros/diffutils/compat -type f 2>/dev/null)
$(DIFF_DIR)/.prepared: ports/srvros/diffutils/prepare.py $(DIFF_INPUTS) $(DIFF_CONFIG)
	$(PYTHON) ports/srvros/diffutils/prepare.py ports/upstream/diffutils $(DIFF_DIR)
include ports/srvros/diffutils/sources.mk
$(DIFF_DIR)/src/%.o: $(DIFF_DIR)/.prepared $(USER_SYSROOT_HEADERS) $(ZIG)
	$(CC) $(DIFF_FLAGS) -c $(DIFF_DIR)/src/$*.c -o $@
$(DIFF_DIR)/guard.o: ports/srvros/diffutils/guard.c $(DIFF_DIR)/.prepared $(USER_SYSROOT_HEADERS) $(ZIG)
	$(CC) $(DIFF_FLAGS) -c $< -o $@
$(DIFF_DIR)/libdiffutils.a: $(DIFF_GNULIB_OBJ)
	$(AR) rcs $@ $(DIFF_GNULIB_OBJ)
$(USER_DIFF): $(ZIG) $(USER_CRT0_OBJ) $(DIFF_CORE_OBJ) $(DIFF_DIR)/guard.o $(DIFF_DIR)/libdiffutils.a $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(DIFF_CORE_OBJ) $(DIFF_DIR)/guard.o $(DIFF_DIR)/libdiffutils.a $(USER_LIB_OBJ) -o $@
$(DIFF_DIR)/analyze-diagnostic.o: $(DIFF_DIR)/.prepared $(USER_SYSROOT_HEADERS) $(ZIG)
	$(CC) $(DIFF_FLAGS) -DSRV_DIFF_DIAGNOSTIC -c $(DIFF_DIR)/src/analyze.c -o $@
$(DIFF_DIR)/diagnostic.o $(DIFF_DIR)/failalloc.o: $(DIFF_DIR)/%.o: ports/srvros/diffutils/%.c $(DIFF_DIR)/.prepared $(USER_SYSROOT_HEADERS) $(ZIG)
	$(CC) $(DIFF_FLAGS) -c $< -o $@
build/userspace/diff-diagnostic.elf: $(ZIG) $(USER_CRT0_OBJ) $(filter-out $(DIFF_DIR)/src/analyze.o,$(DIFF_CORE_OBJ)) $(DIFF_DIR)/guard.o $(DIFF_DIR)/analyze-diagnostic.o $(DIFF_DIR)/diagnostic.o $(DIFF_DIR)/failalloc.o $(DIFF_DIR)/libdiffutils.a $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) --wrap=malloc --wrap=calloc --wrap=realloc $(USER_CRT0_OBJ) $(filter-out $(DIFF_DIR)/src/analyze.o,$(DIFF_CORE_OBJ)) $(DIFF_DIR)/guard.o $(DIFF_DIR)/analyze-diagnostic.o $(DIFF_DIR)/diagnostic.o $(DIFF_DIR)/failalloc.o $(DIFF_DIR)/libdiffutils.a $(USER_LIB_OBJ) -o $@
-include $(DIFF_CORE_OBJ:.o=.d) $(DIFF_GNULIB_OBJ:.o=.d)
