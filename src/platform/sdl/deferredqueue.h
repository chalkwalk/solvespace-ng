//-----------------------------------------------------------------------------
// Callbacks deferred until after the current ImGui frame. UI widgets must not call into the core
// while a frame is being built: the core may open a blocking dialog, which renders frames of its
// own, or rebuild the menus being iterated.
//-----------------------------------------------------------------------------
#ifndef SOLVESPACE_SDL_DEFERREDQUEUE_H
#define SOLVESPACE_SDL_DEFERREDQUEUE_H

#include <functional>
#include <vector>

namespace SolveSpace {
namespace Platform {

class DeferredQueue {
public:
    void Post(std::function<void()> fn) { pending.push_back(std::move(fn)); }
    bool Empty() const { return pending.empty(); }
    // Runs what was posted before this call; callbacks posted while running wait for the next.
    void RunAll() {
        std::vector<std::function<void()>> batch;
        batch.swap(pending);
        for(auto &fn : batch) fn();
    }

private:
    std::vector<std::function<void()>> pending;
};

}
}

#endif
