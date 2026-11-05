/* Linked only into the probe binary, never the production diff. */
#include <unistd.h>
void srv_diff_codec_marker(void) {
 static int sent;
 if (!sent) { sent=1; (void)write(3,"C",1); }
}
