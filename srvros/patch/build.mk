# Included by SrvrOS after removal of the compact-patch USER_PATCH rule.
PATCH_DIR := build/ports/patch-srvros
PATCH_CORE_NAMES := patch pch inp util safe version
PATCH_CORE_OBJ := $(addprefix $(PATCH_DIR)/src/,$(addsuffix .o,$(PATCH_CORE_NAMES)))
PATCH_FLAGS := $(filter-out -I shared/include userspace/lib/include,$(USER_CFLAGS)) -std=gnu2x -Wno-error -DENOTSUP=EOPNOTSUPP -include ports/srvros/patch/generated/config.h -include userspace/lib/include/limits.h -I ports/srvros/patch/generated -I $(PATCH_DIR)/lib -I $(PATCH_DIR)/src -I ports/srvros/patch -isystem ports/srvros/patch/compat -isystem userspace/lib/include -isystem shared/include -DHAVE_CONFIG_H
PATCH_INPUTS := $(shell find ports/upstream/patch -type f 2>/dev/null)
PATCH_CONFIG := $(shell find ports/srvros/patch/generated ports/srvros/patch/compat -type f 2>/dev/null)
$(PATCH_DIR)/.prepared: ports/srvros/patch/prepare.py $(PATCH_INPUTS) $(PATCH_CONFIG)
	$(PYTHON) ports/srvros/patch/prepare.py ports/upstream/patch $(PATCH_DIR)
include ports/srvros/patch/sources.mk
$(PATCH_DIR)/src/%.o: $(PATCH_DIR)/.prepared $(USER_SYSROOT_HEADERS) $(ZIG)
	$(CC) $(PATCH_FLAGS) -c $(PATCH_DIR)/src/$*.c -o $@
$(PATCH_DIR)/profile.o: ports/srvros/patch/profile.c ports/srvros/patch/profile.h $(PATCH_DIR)/.prepared $(USER_SYSROOT_HEADERS) $(ZIG)
	$(CC) $(PATCH_FLAGS) -c $< -o $@
$(PATCH_DIR)/libpatch.a: $(PATCH_GNULIB_OBJ)
	$(AR) rcs $@ $(PATCH_GNULIB_OBJ)
$(USER_PATCH): $(ZIG) $(USER_CRT0_OBJ) $(PATCH_CORE_OBJ) $(PATCH_DIR)/profile.o $(PATCH_DIR)/libpatch.a $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(PATCH_CORE_OBJ) $(PATCH_DIR)/profile.o $(PATCH_DIR)/libpatch.a $(USER_LIB_OBJ) -o $@
$(PATCH_DIR)/patch-diagnostic.o: $(PATCH_DIR)/.prepared $(USER_SYSROOT_HEADERS) $(ZIG)
	$(CC) $(PATCH_FLAGS) -DSRV_PATCH_DIAGNOSTIC -c $(PATCH_DIR)/src/patch.c -o $@
$(PATCH_DIR)/profile-diagnostic.o: ports/srvros/patch/profile.c ports/srvros/patch/profile.h $(PATCH_DIR)/.prepared $(USER_SYSROOT_HEADERS) $(ZIG)
	$(CC) $(PATCH_FLAGS) -DSRV_PATCH_DIAGNOSTIC -c $< -o $@
$(PATCH_DIR)/diagnostic.o $(PATCH_DIR)/failalloc.o: $(PATCH_DIR)/%.o: ports/srvros/patch/%.c $(PATCH_DIR)/.prepared $(USER_SYSROOT_HEADERS) $(ZIG)
	$(CC) $(PATCH_FLAGS) -c $< -o $@
build/userspace/patch-diagnostic.elf: $(ZIG) $(USER_CRT0_OBJ) $(filter-out $(PATCH_DIR)/src/patch.o,$(PATCH_CORE_OBJ)) $(PATCH_DIR)/patch-diagnostic.o $(PATCH_DIR)/profile-diagnostic.o $(PATCH_DIR)/diagnostic.o $(PATCH_DIR)/failalloc.o $(PATCH_DIR)/libpatch.a $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) --wrap=malloc --wrap=calloc --wrap=realloc $(USER_CRT0_OBJ) $(filter-out $(PATCH_DIR)/src/patch.o,$(PATCH_CORE_OBJ)) $(PATCH_DIR)/patch-diagnostic.o $(PATCH_DIR)/profile-diagnostic.o $(PATCH_DIR)/diagnostic.o $(PATCH_DIR)/failalloc.o $(PATCH_DIR)/libpatch.a $(USER_LIB_OBJ) -o $@
-include $(PATCH_CORE_OBJ:.o=.d) $(PATCH_GNULIB_OBJ:.o=.d)
