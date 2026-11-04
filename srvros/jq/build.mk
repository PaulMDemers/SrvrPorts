# Included by the SrvrOS root Makefile; source remains unchanged.
JQ_DIR := build/ports/jq-srvros
JQ_SRC := ports/upstream/jq
JQ_NAMES := main builtin bytecode compile execute jq_test jv jv_alloc jv_aux jv_dtoa jv_file jv_parse jv_print jv_unicode linker locfile util jv_dtoa_tsd lexer parser
JQ_OBJ := $(addprefix $(JQ_DIR)/,$(addsuffix .o,$(JQ_NAMES))) $(JQ_DIR)/decContext.o $(JQ_DIR)/decNumber.o $(JQ_DIR)/entropy.o $(JQ_DIR)/dirname.o

$(JQ_DIR)/.prepared: ports/srvros/jq/prepare.py $(JQ_SRC)/src/builtin.jq
	$(PYTHON) ports/srvros/jq/prepare.py $(JQ_SRC) $(JQ_DIR)
	touch $@

$(addprefix $(JQ_DIR)/src/,builtin.inc config_opts.inc version.h): $(JQ_DIR)/.prepared
	@test -f $@ || { echo "Missing $@; remove $(JQ_DIR)/.prepared and rebuild." >&2; exit 1; }

$(JQ_DIR)/main.o: $(JQ_DIR)/src/version.h $(JQ_DIR)/src/config_opts.inc
$(JQ_DIR)/builtin.o: $(JQ_DIR)/src/builtin.inc

$(JQ_DIR)/%.o: $(JQ_SRC)/src/%.c $(JQ_DIR)/.prepared ports/srvros/jq/config.h $(USER_SYSROOT_HEADERS) $(ZIG)
	$(CC) $(USER_CFLAGS) -Wno-error -I ports/srvros/jq -I $(JQ_DIR) -I $(JQ_SRC)/src -I $(JQ_SRC)/vendor -include ports/srvros/jq/config.h -c $< -o $@

$(JQ_DIR)/dec%.o: $(JQ_SRC)/vendor/decNumber/dec%.c $(JQ_DIR)/.prepared ports/srvros/jq/config.h $(USER_SYSROOT_HEADERS) $(ZIG)
	$(CC) $(USER_CFLAGS) -Wno-error -I ports/srvros/jq -I $(JQ_DIR) -I $(JQ_SRC)/src -I $(JQ_SRC)/vendor -include ports/srvros/jq/config.h -c $< -o $@

$(JQ_DIR)/entropy.o: ports/srvros/jq/entropy.c $(JQ_DIR)/.prepared $(USER_SYSROOT_HEADERS) $(ZIG)
	$(CC) $(USER_CFLAGS) -Wno-error -c $< -o $@

$(JQ_DIR)/dirname.o: ports/srvros/jq/dirname.c $(JQ_DIR)/.prepared $(USER_SYSROOT_HEADERS) $(ZIG)
	$(CC) $(USER_CFLAGS) -Wno-error -c $< -o $@

$(USER_JQ): $(ZIG) $(USER_CRT0_OBJ) $(JQ_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(JQ_OBJ) $(USER_LIB_OBJ) -o $@

-include $(JQ_OBJ:.o=.d)

$(JQ_DIR)/test_output.o: ports/srvros/jq/test_output.c $(JQ_DIR)/.prepared $(USER_SYSROOT_HEADERS) $(ZIG)
	$(CC) $(USER_CFLAGS) -c $< -o $@

build/userspace/jq-output-test.elf: $(ZIG) $(USER_CRT0_OBJ) $(JQ_DIR)/test_output.o $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(JQ_DIR)/test_output.o $(USER_LIB_OBJ) -o $@
