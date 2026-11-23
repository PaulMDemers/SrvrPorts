GSORT_ENGINE_PORT := ports/srvros/gnu-sort
GSORT_ENGINE_DIR := build/ports/gnu-sort/engine
GSORT_ENGINE_ARCHIVE := ports/upstream/coreutils/coreutils-9.12.tar.xz
GSORT_ENGINE_OBJECTS := $(GSORT_ENGINE_DIR)/engine.o

$(GSORT_ENGINE_DIR)/.prepared: $(GSORT_ENGINE_ARCHIVE) $(GSORT_ENGINE_PORT)/prepare.py
	mkdir -p $(GSORT_ENGINE_DIR)
	$(PYTHON) $(GSORT_ENGINE_PORT)/prepare.py $(GSORT_ENGINE_ARCHIVE) $(GSORT_ENGINE_DIR)
	touch $@

$(GSORT_ENGINE_DIR)/engine.o: $(GSORT_ENGINE_PORT)/engine.c $(GSORT_ENGINE_PORT)/engine.h $(GSORT_ENGINE_DIR)/.prepared $(USER_SYSROOT_HEADERS) $(ZIG)
	mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -std=gnu11 -I $(GSORT_ENGINE_DIR) -c $< -o $@
