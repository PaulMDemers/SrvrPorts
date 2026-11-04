# Included from the matched SrvrOS Makefile; keep the CLI engine separate
# from sqlitedemo, whose SQLite configuration is independently exercised.
SQLITE_CLI_DIR := build/ports/sqlite-srvros
SQLITE_CLI_OBJ := $(SQLITE_CLI_DIR)/sqlite3.c.o $(SQLITE_CLI_DIR)/vfs.c.o $(SQLITE_CLI_DIR)/shell.c.o
SQLITE_CLI_FLAGS := -DSQLITE_OS_OTHER=1 -DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_WAL \
	-DSQLITE_OMIT_LOAD_EXTENSION \
	-DSQLITE_OMIT_POPEN -DSQLITE_NOHAVE_SYSTEM -DHAVE_READLINE=0 \
	-DHAVE_EDITLINE=0 -DHAVE_LINENOISE=0 -DSQLITE_OMIT_DESERIALIZE \
	-DSQLITE_DEFAULT_MEMSTATUS=0 \
	-DSQLITE_OMIT_SHARED_CACHE -DSQLITE_OMIT_LOCALTIME
SQLITE_CLI_CFLAGS := $(USER_CFLAGS) -I ports/srvros/sqlite -I ports/upstream/sqlite $(SQLITE_CLI_FLAGS) -Wno-error

$(SQLITE_CLI_DIR)/sqlite3.c.o: ports/upstream/sqlite/sqlite3.c ports/upstream/sqlite/sqlite3.h ports/srvros/sqlite/build.mk $(USER_SYSROOT_HEADERS) $(ZIG)
	mkdir -p $(dir $@)
	$(CC) $(SQLITE_CLI_CFLAGS) -c $< -o $@

$(SQLITE_CLI_DIR)/vfs.c.o: ports/srvros/sqlite/vfs.c ports/upstream/sqlite/sqlite3.h ports/srvros/sqlite/build.mk $(USER_SYSROOT_HEADERS) $(ZIG)
	mkdir -p $(dir $@)
	$(CC) $(SQLITE_CLI_CFLAGS) -c $< -o $@

$(SQLITE_CLI_DIR)/shell.c.o: ports/srvros/sqlite/shell.c ports/srvros/sqlite/memory.h ports/upstream/sqlite/shell.c ports/upstream/sqlite/sqlite3.h ports/srvros/sqlite/build.mk $(USER_SYSROOT_HEADERS) $(ZIG)
	mkdir -p $(dir $@)
	$(CC) $(SQLITE_CLI_CFLAGS) -c $< -o $@

$(USER_SQLITE3): $(ZIG) $(USER_CRT0_OBJ) $(SQLITE_CLI_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(SQLITE_CLI_OBJ) $(USER_LIB_OBJ) -o $@

-include $(SQLITE_CLI_OBJ:.o=.d)

# Diagnostic probe, injected only into disposable smoke-test disks.
build/userspace/sqlite-vfs-test.elf: ports/srvros/sqlite/test_guest.c ports/srvros/sqlite/vfs.c $(USER_CRT0_OBJ) $(USER_LIB_OBJ) $(ZIG)
	mkdir -p build/userspace build/ports/sqlite-srvros
	$(CC) $(SQLITE_CLI_CFLAGS) -c $< -o $(SQLITE_CLI_DIR)/test_guest.c.o
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(SQLITE_CLI_DIR)/test_guest.c.o $(USER_LIB_OBJ) -o $@
