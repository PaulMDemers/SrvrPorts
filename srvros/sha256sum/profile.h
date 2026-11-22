#ifndef SRVROS_SHA256_PROFILE_H
#define SRVROS_SHA256_PROFILE_H
#include <stdio.h>
#include <stddef.h>
#include <sys/types.h>
void srvr_sha_init(int, char **);
void srvr_sha_checkpoint(void);
void srvr_sha_write_error(void);
FILE *srvr_sha_open_regular(const char *);
ssize_t srvr_sha_getline(char **, size_t *, FILE *);
int srvr_sha_record_ok(const char *, size_t);
#endif
