XZ_DIR := build/ports/xz-srvros
include ports/srvros/xz/sources.mk
XZ_CLI_SOURCES := $(addprefix src/xz/,args.c coder.c file_io.c hardware.c main.c message.c mytime.c options.c sandbox.c signals.c suffix.c util.c list.c) $(addprefix src/common/,tuklib_open_stdxxx.c tuklib_progname.c tuklib_exit.c tuklib_mbstr_fw.c tuklib_mbstr_nonprint.c tuklib_mbstr_width.c tuklib_mbstr_wrap.c) lib/getopt.c lib/getopt1.c
XZ_SOURCES := $(XZ_CLI_SOURCES) $(XZ_LZMA_SOURCES)
XZ_OBJ := $(addprefix $(XZ_DIR)/,$(XZ_SOURCES:.c=.o)) $(XZ_DIR)/allocator.o $(XZ_DIR)/guard.o $(XZ_DIR)/dirname.o
XZ_INPUTS := $(shell find ports/upstream/xz -type f 2>/dev/null)
XZ_INCLUDES := -I ports/srvros/xz -I $(XZ_DIR) -I $(XZ_DIR)/lib -I $(XZ_DIR)/src/common -I $(XZ_DIR)/src/liblzma/api $(addprefix -I $(XZ_DIR)/src/liblzma/,common check lz rangecoder lzma delta simple)
XZ_DEFS := -D__GETOPT_PREFIX=srv_xz_ -DHAVE_CONFIG_H -DTUKLIB_SYMBOL_PREFIX=lzma_ -DLOCALEDIR='"/"'
$(XZ_DIR)/.prepared: ports/srvros/xz/prepare.py ports/srvros/xz/config.h $(XZ_INPUTS)
	$(PYTHON) ports/srvros/xz/prepare.py ports/upstream/xz $(XZ_DIR)
$(XZ_DIR)/%.o: $(XZ_DIR)/.prepared $(USER_SYSROOT_HEADERS) $(ZIG)
	$(CC) $(USER_CFLAGS) -Wno-error $(XZ_INCLUDES) $(XZ_DEFS) -c $(XZ_DIR)/$*.c -o $@
$(XZ_DIR)/allocator.o $(XZ_DIR)/guard.o $(XZ_DIR)/dirname.o: $(XZ_DIR)/%.o: ports/srvros/xz/%.c $(XZ_DIR)/.prepared $(USER_SYSROOT_HEADERS) $(ZIG)
	$(CC) $(USER_CFLAGS) -Wno-error $(XZ_INCLUDES) $(XZ_DEFS) -c $< -o $@
$(USER_XZ): $(ZIG) $(USER_CRT0_OBJ) $(XZ_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(XZ_OBJ) $(USER_LIB_OBJ) -o $@
-include $(XZ_OBJ:.o=.d)
# Probe-only variant: reports completion of one real codec call on fd 3.
$(XZ_DIR)/coder-diagnostic.o: $(XZ_DIR)/.prepared $(USER_SYSROOT_HEADERS) $(ZIG)
	$(CC) $(USER_CFLAGS) -Wno-error $(XZ_INCLUDES) $(XZ_DEFS) -DSRV_XZ_DIAGNOSTIC -c $(XZ_DIR)/src/xz/coder.c -o $@
$(XZ_DIR)/diagnostic.o: ports/srvros/xz/diagnostic.c $(XZ_DIR)/.prepared $(USER_SYSROOT_HEADERS) $(ZIG)
	$(CC) $(USER_CFLAGS) -Wno-error -c $< -o $@
build/userspace/xz-diagnostic.elf: $(ZIG) $(USER_CRT0_OBJ) $(filter-out $(XZ_DIR)/src/xz/coder.o,$(XZ_OBJ)) $(XZ_DIR)/coder-diagnostic.o $(XZ_DIR)/diagnostic.o $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(filter-out $(XZ_DIR)/src/xz/coder.o,$(XZ_OBJ)) $(XZ_DIR)/coder-diagnostic.o $(XZ_DIR)/diagnostic.o $(USER_LIB_OBJ) -o $@
