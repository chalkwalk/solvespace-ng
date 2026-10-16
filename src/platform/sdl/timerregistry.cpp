//-----------------------------------------------------------------------------
// Timer serial bookkeeping for the SDL backend.
//-----------------------------------------------------------------------------
#include <algorithm>
#include "platform/sdl/timerregistry.h"

namespace SolveSpace {
namespace Platform {

uint64_t TimerRegistry::Arm(std::weak_ptr<Timer> owner, uint64_t dueAt) {
    uint64_t serial = nextSerial++;
    pending[serial] = { owner, dueAt };
    return serial;
}

void TimerRegistry::Cancel(uint64_t serial) {
    pending.erase(serial);
}

std::shared_ptr<Timer> TimerRegistry::Take(uint64_t serial) {
    auto it = pending.find(serial);
    if(it == pending.end()) return nullptr;
    std::shared_ptr<Timer> owner = it->second.owner.lock();
    pending.erase(it);
    return owner;
}

uint64_t TimerRegistry::EarliestDue() const {
    uint64_t earliest = UINT64_MAX;
    for(const auto &kv : pending) {
        if(!kv.second.owner.expired()) earliest = std::min(earliest, kv.second.dueAt);
    }
    return earliest;
}

}
}
