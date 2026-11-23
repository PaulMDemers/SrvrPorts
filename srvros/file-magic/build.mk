# Static libmagic and the selected upstream rules used by the CLI.
include ports/srvros/file-magic/engine.mk

FILE_MAGIC_BUILD := $(FILE_MAGIC_DIR)
FILE_MAGIC_CLI_SOURCES := $(FILE_MAGIC_PORT)/profile.c $(FILE_MAGIC_PORT)/runtime.c
FILE_MAGIC_CLI_OBJECTS := $(patsubst $(FILE_MAGIC_PORT)/%.c,$(FILE_MAGIC_BUILD)/cli/%.o,$(FILE_MAGIC_CLI_SOURCES))
FILE_MAGIC_DIAG_OBJECTS := $(patsubst $(FILE_MAGIC_PORT)/%.c,$(FILE_MAGIC_BUILD)/diagnostic/%.o,$(FILE_MAGIC_CLI_SOURCES))
FILE_MAGIC_SHARED_OBJECTS = $(FILE_MAGIC_ENGINE_OBJECTS) $(FILE_MAGIC_BUILD)/database.o

$(FILE_MAGIC_CLI_OBJECTS): $(FILE_MAGIC_BUILD)/cli/%.o: $(FILE_MAGIC_PORT)/%.c $(FILE_MAGIC_PORT)/profile.h $(FILE_MAGIC_DB) $(USER_SYSROOT_HEADERS) $(ZIG)
	mkdir -p $(@D)
	$(CC) $(USER_CFLAGS) $(FILE_MAGIC_INCLUDE) -c $< -o $@

$(FILE_MAGIC_DIAG_OBJECTS): $(FILE_MAGIC_BUILD)/diagnostic/%.o: $(FILE_MAGIC_PORT)/%.c $(FILE_MAGIC_PORT)/profile.h $(FILE_MAGIC_DB) $(USER_SYSROOT_HEADERS) $(ZIG)
	mkdir -p $(@D)
	$(CC) $(USER_CFLAGS) $(FILE_MAGIC_INCLUDE) -DSRV_FILE_MAGIC_DIAGNOSTIC -c $< -o $@

$(FILE_MAGIC_BUILD)/database.o: $(FILE_MAGIC_PORT)/database.S $(FILE_MAGIC_DB) $(ZIG)
	$(CC) -target x86_64-freestanding-none -c $< -o $@

$(USER_FILE_MAGIC): $(ZIG) $(USER_CRT0_OBJ) $(FILE_MAGIC_CLI_OBJECTS) $(FILE_MAGIC_SHARED_OBJECTS) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(@D)
	$(LD) $(USER_APP_LDFLAGS) --wrap=srv_sbrk $(USER_CRT0_OBJ) $(FILE_MAGIC_CLI_OBJECTS) $(FILE_MAGIC_SHARED_OBJECTS) $(USER_LIB_OBJ) -o $@

build/userspace/file-magic-diagnostic.elf: $(ZIG) $(USER_CRT0_OBJ) $(FILE_MAGIC_DIAG_OBJECTS) $(FILE_MAGIC_SHARED_OBJECTS) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(@D)
	$(LD) $(USER_APP_LDFLAGS) --wrap=srv_sbrk $(USER_CRT0_OBJ) $(FILE_MAGIC_DIAG_OBJECTS) $(FILE_MAGIC_SHARED_OBJECTS) $(USER_LIB_OBJ) -o $@

-include $(FILE_MAGIC_CLI_OBJECTS:.o=.d) $(FILE_MAGIC_DIAG_OBJECTS:.o=.d)
