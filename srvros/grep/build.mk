# Bounded GNU grep is a separate command from the compact grep.
GGREP_PORT := ports/srvros/grep
GGREP_DIR := build/ports/gnu-grep
GGREP_CONFIG := $(GGREP_PORT)/generated
GGREP_COMPAT := ports/srvros/diffutils/compat
GGREP_CORE := grep dfasearch kwsearch kwset searchutils
GGREP_CORE_OBJ := $(addprefix $(GGREP_DIR)/src/,$(addsuffix .o,$(GGREP_CORE)))
include $(GGREP_PORT)/sources.mk
GGREP_LIB_OBJ := $(addprefix $(GGREP_DIR)/lib/,$(GGREP_GNULIB_SOURCES:.c=.o))
GGREP_UPSTREAM := $(shell find ports/upstream/grep -type f 2>/dev/null | LC_ALL=C sort)
GGREP_HEADERS := $(shell find $(GGREP_CONFIG) $(GGREP_COMPAT) -name '*.h' 2>/dev/null | LC_ALL=C sort)
GGREP_FLAGS := $(filter-out -I shared/include userspace/lib/include,$(USER_CFLAGS)) \
	-std=gnu2x -Wno-error -include $(GGREP_CONFIG)/config.h \
	-include userspace/lib/include/limits.h -I $(GGREP_CONFIG) \
	-I $(GGREP_DIR)/lib -I $(GGREP_DIR)/src -isystem $(GGREP_COMPAT) \
	-isystem userspace/lib/include -isystem shared/include -DHAVE_CONFIG_H

$(GGREP_DIR)/.prepared: $(GGREP_PORT)/prepare.py $(GGREP_UPSTREAM)
	rm -rf $(GGREP_DIR)
	$(PYTHON) $(GGREP_PORT)/prepare.py ports/upstream/grep $(GGREP_DIR)
	touch $@

$(GGREP_CORE_OBJ): $(GGREP_DIR)/src/%.o: $(GGREP_DIR)/.prepared $(GGREP_HEADERS) $(USER_SYSROOT_HEADERS) $(ZIG)
	$(CC) $(GGREP_FLAGS) -MMD -MP -c $(GGREP_DIR)/src/$*.c -o $@

$(GGREP_LIB_OBJ): $(GGREP_DIR)/lib/%.o: $(GGREP_DIR)/.prepared $(GGREP_HEADERS) $(USER_SYSROOT_HEADERS) $(ZIG)
	$(CC) $(GGREP_FLAGS) -MMD -MP -c $(GGREP_DIR)/lib/$*.c -o $@

$(GGREP_DIR)/libgreputils.a: $(GGREP_LIB_OBJ) $(GGREP_PORT)/sources.mk
	rm -f $@
	$(AR) rcs $@ $(GGREP_LIB_OBJ)

$(GGREP_DIR)/profile.o $(GGREP_DIR)/runtime.o: $(GGREP_DIR)/%.o: $(GGREP_PORT)/%.c $(GGREP_PORT)/profile.h $(USER_SYSROOT_HEADERS) $(ZIG) $(GGREP_DIR)/.prepared
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(GGREP_DIR)/runtime-diagnostic.o: $(GGREP_PORT)/runtime.c $(GGREP_PORT)/profile.h $(USER_SYSROOT_HEADERS) $(ZIG) $(GGREP_DIR)/.prepared
	$(CC) $(USER_CFLAGS) -DSRV_GREP_DIAGNOSTIC -c $< -o $@

GGREP_LINK_INPUTS := $(USER_CRT0_OBJ) $(GGREP_CORE_OBJ) $(GGREP_DIR)/profile.o $(GGREP_DIR)/libgreputils.a $(USER_LIB_OBJ)
$(USER_GGREP): $(ZIG) $(GGREP_LINK_INPUTS) $(GGREP_DIR)/runtime.o userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) --wrap=srv_sbrk $(GGREP_LINK_INPUTS) $(GGREP_DIR)/runtime.o -o $@

build/userspace/ggrep-diagnostic.elf: $(ZIG) $(GGREP_LINK_INPUTS) $(GGREP_DIR)/runtime-diagnostic.o userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) --wrap=srv_sbrk $(GGREP_LINK_INPUTS) $(GGREP_DIR)/runtime-diagnostic.o -o $@

-include $(GGREP_CORE_OBJ:.o=.d) $(GGREP_LIB_OBJ:.o=.d)
