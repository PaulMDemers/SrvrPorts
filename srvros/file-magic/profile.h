#ifndef SRVROS_FILE_MAGIC_PROFILE_H
#define SRVROS_FILE_MAGIC_PROFILE_H

/* Inserted in softmagic's matching path, never in the driver I/O path. */
void file_magic_checkpoint(void);
void file_magic_poll(void);
void file_magic_runtime_init(void);
unsigned file_magic_match_iterations(void);
void file_magic_heap_report(void);

#endif
