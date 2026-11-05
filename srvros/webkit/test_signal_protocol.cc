// Model of the candidate's phases, not a test of signal delivery or WebKit.
#include <cassert>
#include <cstdint>
#include <limits>

enum Phase { idle, requested, suspended, resume, running };
struct Request {
    Phase phase = idle;
    uint64_t ticket = 0, ack = 0, tid = 7;
    bool context = false, tls = true, blocked = false, pending = false;
    bool begin() {
        if (phase != idle || ticket == UINT64_MAX || !tls || blocked) return false;
        ++ticket; phase = requested; return true;
    }
    bool signal(uint64_t actual) {
        if (!tls || actual != tid || blocked) return false;
        if (phase == suspended || phase == resume || phase == running || phase == idle) return true;
        if (phase != requested) return false;
        context = true; ack = ticket; phase = suspended; return true;
    }
    bool wake() {
        if (phase != resume) return false;
        context = false; ack = ticket; phase = running; return true;
    }
    bool retire() {
        blocked = true;
        if (!tls || phase != idle || pending) return false;
        tls = false; return true;
    }
};
int main() {
    Request a, b;
    assert(a.begin() && b.begin() && a.signal(7) && b.signal(7));
    assert(a.context && b.context && a.ack == a.ticket && b.ack == b.ticket);
    assert(a.signal(7) && !a.wake() && a.context); // spurious nested signal
    assert(!a.signal(8) && a.context);
    a.phase = resume;
    assert(a.signal(7) && a.wake() && !a.context && b.context);
    a.phase = idle;
    assert(a.begin() && a.ticket == 2);
    assert(!a.retire() && a.tls); // active request cannot retire
    b.phase = idle; b.context = false; b.pending = true;
    assert(!b.retire() && b.tls); // queued INFO is terminal
    Request c;
    assert(c.retire() && c.blocked && !c.tls && !c.signal(7));
    Request d; d.ticket = std::numeric_limits<uint64_t>::max();
    assert(!d.begin() && d.phase == idle);
}
