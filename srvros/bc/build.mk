# Included by SrvrOS from its root; generated files remain outside upstream.
BC_DIR := build/ports/bc-srvros
BC_NAMES := args bc bc_lex bc_parse data file lang lex main num opt parse program read vector vm
BC_OBJ := $(addprefix $(BC_DIR)/src/,$(addsuffix .o,$(BC_NAMES))) $(BC_DIR)/gen/bc_help.o $(BC_DIR)/gen/lib.o
BC_INPUTS := $(shell find ports/upstream/bc -type f 2>/dev/null)
BC_DEFS := -DBC_ENABLED=1 -DDC_ENABLED=0 -DBC_NO_COMPUTED_GOTO -DBUILD_TYPE=EHN -DEXECPREFIX= -DMAINEXEC=bc -DBC_NUM_KARATSUBA_LEN=32 -DBC_ENABLE_NLS=0 -DBC_ENABLE_EXTRA_MATH=0 -DBC_ENABLE_HISTORY=0 -DBC_ENABLE_LIBRARY=0 -DBC_ENABLE_MEMCHECK=0 -DBC_ENABLE_AFL=0 -DBC_ENABLE_OSSFUZZ=0 -DBC_ENABLE_EDITLINE=0 -DBC_ENABLE_READLINE=0 -DBC_DEFAULT_BANNER=0 -DBC_DEFAULT_SIGINT_RESET=1 -DBC_DEFAULT_TTY_MODE=1 -DBC_DEFAULT_PROMPT=1 -DBC_DEFAULT_EXPR_EXIT=1 -DBC_DEFAULT_DIGIT_CLAMP=0 -DNDEBUG

$(BC_DIR)/.prepared: ports/srvros/bc/prepare.py $(BC_INPUTS)
	HOSTCC="$(HOST_CC)" $(PYTHON) ports/srvros/bc/prepare.py ports/upstream/bc $(BC_DIR)

$(BC_DIR)/%.o: $(BC_DIR)/.prepared $(USER_SYSROOT_HEADERS) $(ZIG)
	$(CC) $(USER_CFLAGS) $(BC_DEFS) -I $(BC_DIR)/include -Wno-error -c $(BC_DIR)/$*.c -o $@

$(USER_BC): $(ZIG) $(USER_CRT0_OBJ) $(BC_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(BC_OBJ) $(USER_LIB_OBJ) -o $@

-include $(BC_OBJ:.o=.d)

$(BC_DIR)/test_%.o: ports/srvros/bc/test_%.c $(BC_DIR)/.prepared $(USER_SYSROOT_HEADERS) $(ZIG)
	$(CC) $(USER_CFLAGS) -c $< -o $@

build/userspace/bc-output-test.elf: $(ZIG) $(USER_CRT0_OBJ) $(BC_DIR)/test_output.o $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(BC_DIR)/test_output.o $(USER_LIB_OBJ) -o $@

build/userspace/bc-signal-test.elf: $(ZIG) $(USER_CRT0_OBJ) $(BC_DIR)/test_signal.o $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(BC_DIR)/test_signal.o $(USER_LIB_OBJ) -o $@
