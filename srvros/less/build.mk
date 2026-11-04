# Included by SrvrOS; adapted sources live under build/.
LESS_DIR := build/ports/less-srvros
LESS_NAMES := main screen brac ch charset cmdbuf command cvt decode edit evar filename forwback help ifile input jump line linenum lmsg lsystem mark optfunc option opttbl os output pattern position prompt search signal tags ttyin version xbuf
LESS_OBJ := $(addprefix $(LESS_DIR)/,$(addsuffix .o,$(LESS_NAMES))) $(LESS_DIR)/termcap.o
LESS_INPUTS := $(shell find ports/upstream/less -type f 2>/dev/null)
$(LESS_DIR)/.prepared: ports/srvros/less/prepare.py ports/srvros/less/config.h $(LESS_INPUTS)
	$(PYTHON) ports/srvros/less/prepare.py ports/upstream/less $(LESS_DIR)
$(LESS_DIR)/%.o: $(LESS_DIR)/.prepared $(USER_SYSROOT_HEADERS) $(ZIG)
	$(CC) $(USER_CFLAGS) -Wno-error -I ports/srvros/less -I $(LESS_DIR) -DBINDIR='"/fat/bin"' -DLIBEXECDIR='"/fat/bin"' -DSYSDIR='"/etc"' -c $(LESS_DIR)/$*.c -o $@
$(LESS_DIR)/termcap.o: ports/srvros/less/termcap.c ports/srvros/less/termcap.h $(LESS_DIR)/.prepared $(USER_SYSROOT_HEADERS) $(ZIG)
	$(CC) $(USER_CFLAGS) -Wno-error -I ports/srvros/less -c $< -o $@
$(USER_LESS): $(ZIG) $(USER_CRT0_OBJ) $(LESS_OBJ) $(USER_LIB_OBJ) userspace/app.ld
	mkdir -p $(dir $@)
	$(LD) $(USER_APP_LDFLAGS) $(USER_CRT0_OBJ) $(LESS_OBJ) $(USER_LIB_OBJ) -o $@
-include $(LESS_OBJ:.o=.d)
