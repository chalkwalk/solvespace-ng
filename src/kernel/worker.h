//-----------------------------------------------------------------------------
// One background thread for kernel work (AGENTS.md section 6: regeneration runs on a worker
// thread, keeping only the newest request). A task submitted while another waits replaces it;
// a running task finishes.
//-----------------------------------------------------------------------------
#ifndef SOLVESPACE_KERNEL_WORKER_H
#define SOLVESPACE_KERNEL_WORKER_H

#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

class LatestTaskWorker {
public:
    LatestTaskWorker() = default;
    ~LatestTaskWorker();
    LatestTaskWorker(const LatestTaskWorker &) = delete;
    LatestTaskWorker &operator=(const LatestTaskWorker &) = delete;

    // Runs task on the worker thread (started on first use), replacing any task still queued.
    void Submit(std::function<void()> task);
    // Waits until no task is queued or running.
    void Wait();
    bool Busy();

private:
    void Loop();

    std::thread             thread;
    std::mutex              mutex;
    std::condition_variable wake, idle;
    std::function<void()>   queued;
    bool                    running = false, quit = false;
};

#endif
