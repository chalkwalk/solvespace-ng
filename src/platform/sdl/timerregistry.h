//-----------------------------------------------------------------------------
// Maps timer-event serials to their owners for the SDL backend. Only touched on the main
// thread; the SDL timer thread only ever pushes an event carrying the serial.
//-----------------------------------------------------------------------------
#ifndef SOLVESPACE_SDL_TIMERREGISTRY_H
#define SOLVESPACE_SDL_TIMERREGISTRY_H

#include <memory>
#include <unordered_map>
#include "solvespace.h"

namespace SolveSpace {
namespace Platform {

class TimerRegistry {
public:
    // dueAt is the deadline in SDL ticks (ms); 0 means "now".
    uint64_t Arm(std::weak_ptr<Timer> owner, uint64_t dueAt = 0);
    void     Cancel(uint64_t serial);
    // Returns the owner and forgets the serial; nullptr if cancelled, stale, or destroyed.
    std::shared_ptr<Timer> Take(uint64_t serial);
    // The earliest deadline of a timer whose owner is still alive; UINT64_MAX if none.
    uint64_t EarliestDue() const;

private:
    uint64_t                                           nextSerial = 1;
    struct Entry {
        std::weak_ptr<Timer> owner;
        uint64_t             dueAt;
    };
    std::unordered_map<uint64_t, Entry> pending;
};

}
}

#endif
