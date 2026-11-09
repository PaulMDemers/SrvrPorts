#ifndef BATTERY_PROTOCOL_DIAGNOSTIC_H
#define BATTERY_PROTOCOL_DIAGNOSTIC_H
#include <srvros/sbs_sample.h>
/* CPU0 first-failure RAM latch. No pointers, retries or ownership authority.
 * Poll gaps are wall observations, not scheduler or hardware completion time. */
enum battery_protocol_kind { BATTERY_PROTOCOL_SAMPLE=1,BATTERY_PROTOCOL_EVENT,
    BATTERY_PROTOCOL_AC_BEFORE,BATTERY_PROTOCOL_AC_AFTER,BATTERY_PROTOCOL_FRESHNESS };
struct battery_protocol_diagnostic {
    uint64_t observed_us,flags,current,expected,generation,completed_us;
    unsigned kind,ec_phase,ec_error,sample_error,sample_index,sample_polls,bus_phase;
    int result;
    bool valid,begun,lease,io,busy,poisoned,active,waiting,bus_pending,bus_failed;
    bool ack_attempted,acknowledged;
    struct sbs_failure sample;
    struct ec_smbus_fault bus;
};
#endif
