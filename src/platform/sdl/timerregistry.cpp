//-----------------------------------------------------------------------------
// Timer serial bookkeeping for the SDL backend.
//-----------------------------------------------------------------------------
#include "platform/sdl/timerregistry.h"

namespace SolveSpace {
namespace Platform {

uint64_t TimerRegistry::Arm(std::weak_ptr<Timer> owner) {
    uint64_t serial = nextSerial++;
    pending[serial] = owner;
    return serial;
}

void TimerRegistry::Cancel(uint64_t serial) {
    pending.erase(serial);
}

std::shared_ptr<Timer> TimerRegistry::Take(uint64_t serial) {
    auto it = pending.find(serial);
    if(it == pending.end()) return nullptr;
    std::shared_ptr<Timer> owner = it->second.lock();
    pending.erase(it);
    return owner;
}

}
}
