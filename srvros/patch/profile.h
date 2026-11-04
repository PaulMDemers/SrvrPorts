#ifndef SRVROS_PATCH_PROFILE_H
#define SRVROS_PATCH_PROFILE_H
#include <stddef.h>
#include <stdio.h>
#include <sys/stat.h>
void srv_patch_admit(int argc, char **argv);
int srv_patch_source_fd(void);
int srv_patch_patch_fd(void);
int srv_patch_output_fd(void);
void srv_patch_source_stat(struct stat *st);
void srv_patch_set_output(FILE *stream);
void srv_patch_output_progress(FILE *stream, size_t written);
void srv_patch_output_bytes(FILE *stream, const void *bytes, size_t length);
void srv_patch_success(void);
void srv_patch_cleanup(void);
void srv_patch_signal_checkpoint(void);
#endif
