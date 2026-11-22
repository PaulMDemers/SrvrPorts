#ifndef SRVROS_GREP_PROFILE_H
#define SRVROS_GREP_PROFILE_H

/* Call only from normal execution, including bounded matching loops.  The
   signal handler itself only records the signal.  This function exits with
   128 + signal when an interrupt or termination is pending. */
void srvr_grep_checkpoint(void);
void srvr_grep_progress(int event);
int srvr_grep_main(int argc, char **argv);

#endif
