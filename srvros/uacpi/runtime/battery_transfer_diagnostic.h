#ifndef BATTERY_TRANSFER_DIAGNOSTIC_H
#define BATTERY_TRANSFER_DIAGNOSTIC_H
#include <stdint.h>
#include <stdbool.h>
/* Failure-only CPU0 RAM observation. No retry, port access or permission grant.
 * Entry flags/identity describe admission; EC fields describe the refusal or
 * completed poll failure. A previous successful EC result is not a timeout. */
enum battery_transfer_failure {
    BATTERY_TRANSFER_IRQ = 1, BATTERY_TRANSFER_WORKER,
    BATTERY_TRANSFER_LEASE, BATTERY_TRANSFER_POISON,
    BATTERY_TRANSFER_BEGIN, BATTERY_TRANSFER_POLL
};
struct battery_transfer_diagnostic {
    uint64_t at_us, flags, current, expected, started_us, last_us, timeout_us;
    unsigned reason, polls, phase, failed_phase, error;
    int result;
    uint8_t status;
    bool valid, query, write, lease, poisoned, begun;
};
#endif
