# GNU sort's sequential engine plus the bounded SrvrOS CLI.
include ports/srvros/gnu-sort/engine.mk

GSORT_PORT := ports/srvros/gnu-sort
GSORT_BUILD := build/ports/gnu-sort
GSORT_SOURCES := profile.c runtime.c
GSORT_OBJECTS := $(addprefix $(GSORT_BUILD)/cli/,$(GSORT_SOURCES:.c=.o))
GSORT_DIAG_OBJECTS := $(addprefix $(GSORT_BUILD)/diagnostic/,$(GSORT_SOURCES:.c=.o))
GSORT_HEADERS := $(GSORT_PORT)/profile.h $(GSORT_PORT)/engine.h $(USER_SYSROOT_HEADERS)

$(GSORT_OBJECTS): $(GSORT_BUILD)/cli/%.o: $(GSORT_PORT)/%.c $(GSORT_HEADERS) $(ZIG)
	mkdir -p $(@D)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(GSORT_DIAG_OBJECTS): $(GSORT_BUILD)/diagnostic/%.o: $(GSORT_PORT)/%.c $(GSORT_HEADERS) $(ZIG)
	mkdir -p $(@D)
	$(CC) $(USER_CFLAGS) -DSRV_GSORT_DIAGNOSTIC -c $< -o $@

$(USER_GSORT): $(ZIG) $(USER_CRT0_OBJ) $(GSORT_OBJECTS) $(GSORT_ENGINE_OBJECTS) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(@D)
	$(LD) $(USER_APP_LDFLAGS) --wrap=srv_sbrk $(USER_CRT0_OBJ) $(GSORT_OBJECTS) $(GSORT_ENGINE_OBJECTS) $(USER_LIB_OBJ) -o $@

build/userspace/gsort-diagnostic.elf: $(ZIG) $(USER_CRT0_OBJ) $(GSORT_DIAG_OBJECTS) $(GSORT_ENGINE_OBJECTS) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(@D)
	$(LD) $(USER_APP_LDFLAGS) --wrap=srv_sbrk $(USER_CRT0_OBJ) $(GSORT_DIAG_OBJECTS) $(GSORT_ENGINE_OBJECTS) $(USER_LIB_OBJ) -o $@

-include $(GSORT_OBJECTS:.o=.d) $(GSORT_DIAG_OBJECTS:.o=.d)
