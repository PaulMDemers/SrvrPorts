/* Linked only into xz-diagnostic. FD 3 is supplied by the probe. */
#include <unistd.h>
void srv_xz_codec_marker(void) {
 static int sent;
 if (!sent) { sent = 1; (void)write(3, "C", 1); }
}
