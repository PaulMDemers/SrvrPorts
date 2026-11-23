# Bounded upstream libmagic and same-release native database compiler.
FILE_MAGIC_PORT := ports/srvros/file-magic
FILE_MAGIC_DIR := build/ports/file-magic
FILE_MAGIC_SRC := $(FILE_MAGIC_DIR)/file-5.48/src
FILE_MAGIC_INCLUDE := -I $(FILE_MAGIC_PORT)/engine -I $(FILE_MAGIC_SRC)
FILE_MAGIC_SOURCES := buffer magic apprentice softmagic ascmagic encoding is_csv is_json is_simh is_tar readelf print fsmagic funcs apptype der cdf cdf_time readcdf swap strlcpy strlcat strcasestr fmtcheck
FILE_MAGIC_ENGINE_OBJECTS := $(addprefix $(FILE_MAGIC_DIR)/obj/,$(addsuffix .o,$(FILE_MAGIC_SOURCES))) $(FILE_MAGIC_DIR)/obj/compat.o
FILE_MAGIC_DB := $(FILE_MAGIC_DIR)/bounded.magic.mgc
FILE_MAGIC_FLAGS := $(USER_CFLAGS) -Wno-error -DHAVE_CONFIG_H $(FILE_MAGIC_INCLUDE)

$(FILE_MAGIC_DB): $(FILE_MAGIC_PORT)/prepare.py ports/upstream/file/file-5.48.tar.gz
	HOSTCC="$(HOST_CC)" $(PYTHON) $(FILE_MAGIC_PORT)/prepare.py ports/upstream/file/file-5.48.tar.gz $(FILE_MAGIC_DIR)

$(filter-out $(FILE_MAGIC_DIR)/obj/compat.o,$(FILE_MAGIC_ENGINE_OBJECTS)): $(FILE_MAGIC_DIR)/obj/%.o: $(FILE_MAGIC_DB) $(FILE_MAGIC_PORT)/engine/config.h $(USER_SYSROOT_HEADERS) $(ZIG)
	mkdir -p $(dir $@)
	$(CC) $(FILE_MAGIC_FLAGS) -c $(FILE_MAGIC_SRC)/$*.c -o $@

-include $(FILE_MAGIC_ENGINE_OBJECTS:.o=.d)

$(FILE_MAGIC_DIR)/obj/compat.o: $(FILE_MAGIC_PORT)/engine/compat.c $(FILE_MAGIC_DB) $(USER_SYSROOT_HEADERS) $(ZIG)
	mkdir -p $(dir $@)
	$(CC) $(FILE_MAGIC_FLAGS) -c $< -o $@
