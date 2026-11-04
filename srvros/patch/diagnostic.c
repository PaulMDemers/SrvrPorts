/* Linked only into the diagnostic executable. */
#include "profile.h"
#include <signal.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <unistd.h>
void srv_patch_match_marker(void) {
 static int sent;
 if (!sent) { sent=1; (void)write(3,"C",1); }
}
void srv_patch_output_diagnostic(void) {
 const char *enable=getenv("SRV_PATCH_TEST_OUTPUT_LIMIT");
 if (!enable || !*enable) return;
 struct rlimit limit={ .rlim_cur=16, .rlim_max=16 };
 if (signal(SIGXFSZ,SIG_IGN)==SIG_ERR || setrlimit(RLIMIT_FSIZE,&limit)<0) {
  static const char message[]="PATCH_OUTPUT_LIMIT_ARM_FAILED\n";
  (void)write(2,message,sizeof(message)-1); exit(2);
 }
 static const char message[]="PATCH_OUTPUT_LIMIT_ARMED\n";
 (void)write(2,message,sizeof(message)-1);
}

void srv_patch_snapshot_diagnostic(void) {
 const char *enable=getenv("SRV_PATCH_TEST_PAUSE_SNAPSHOT");
 if (!enable || !*enable) return;
 srv_patch_signal_checkpoint();
 ssize_t sent=write(3,"S",1);
 srv_patch_signal_checkpoint();
 if (sent!=1) exit(2);
 char token;
 ssize_t n=read(4,&token,1);
 srv_patch_signal_checkpoint();
 if (n!=1) exit(2);
}
