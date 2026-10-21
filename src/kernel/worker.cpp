//-----------------------------------------------------------------------------
// The kernel's background thread.
//-----------------------------------------------------------------------------
#include "kernel/worker.h"

LatestTaskWorker::~LatestTaskWorker() {
    {
        std::lock_guard<std::mutex> lock(mutex);
        quit = true;
    }
    wake.notify_all();
    if(thread.joinable()) thread.join();
}

void LatestTaskWorker::Submit(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lock(mutex);
        queued = std::move(task);
        if(!thread.joinable()) thread = std::thread([this] { Loop(); });
    }
    wake.notify_all();
}

void LatestTaskWorker::Wait() {
    std::unique_lock<std::mutex> lock(mutex);
    idle.wait(lock, [this] { return !queued && !running; });
}

bool LatestTaskWorker::Busy() {
    std::lock_guard<std::mutex> lock(mutex);
    return queued || running;
}

void LatestTaskWorker::Loop() {
    std::unique_lock<std::mutex> lock(mutex);
    for(;;) {
        wake.wait(lock, [this] { return quit || queued; });
        if(quit) return;
        std::function<void()> task = std::move(queued);
        queued  = nullptr;
        running = true;
        lock.unlock();
        task();
        lock.lock();
        running = false;
        if(!queued) idle.notify_all();
    }
}
