# Included from the matched SrvrOS Makefile; never run inside upstream.
AWK_DIR := build/ports/awk-srvros
AWK_NAMES := awkgram.tab.c b.c main.c parse.c proctab.c tran.c lib.c run.c lex.c
AWK_OBJ := $(addprefix $(AWK_DIR)/,$(AWK_NAMES:.c=.c.o)) $(AWK_DIR)/compat.c.o
AWK_INPUTS := $(wildcard ports/upstream/awk/*.c ports/upstream/awk/*.h ports/upstream/awk/*.y)
AWK_BISON ?= bison
AWK_HOSTCC ?= $(HOST_CC)

$(AWK_DIR)/.prepared: ports/srvros/awk/prepare.py $(AWK_INPUTS)
	BISON="$(AWK_BISON)" HOSTCC="$(AWK_HOSTCC)" $(PYTHON) ports/srvros/awk/prepare.py ports/upstream/awk $(AWK_DIR)

$(addprefix $(AWK_DIR)/,$(AWK_NAMES)): $(AWK_DIR)/.prepared
	@test -f $@ || { echo "Missing $@; remove $(AWK_DIR)/.prepared and rebuild." >&2; exit 1; }

$(AWK_DIR)/%.c.o: $(AWK_DIR)/%.c $(AWK_DIR)/.prepared ports/srvros/awk/config.h $(USER_SYSROOT_HEADERS) $(ZIG)
	$(CC) $(USER_CFLAGS) -I $(AWK_DIR) -include ports/srvros/awk/config.h -Wno-error -c $< -o $@

$(AWK_DIR)/compat.c.o: ports/srvros/awk/compat.c ports/srvros/awk/config.h $(AWK_DIR)/.prepared $(USER_SYSROOT_HEADERS) $(ZIG)
	$(CC) $(USER_CFLAGS) -I ports/srvros/awk -c $< -o $@

$(USER_AWK): $(ZIG) $(USER_CRT0_OBJ) $(AWK_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(AWK_OBJ) $(USER_LIB_OBJ) -o $@

.PHONY: test-awk-port
test-awk-port:
	sh ports/srvros/awk/test-host.sh

-include $(AWK_OBJ:.o=.d)
