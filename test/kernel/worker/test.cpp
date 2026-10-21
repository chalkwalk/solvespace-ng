// The kernel's background thread (src/kernel/worker.h): the newest task wins.
#include "solvespace.h"
#include "kernel/worker.h"
#include <atomic>

#include "harness.h"

TEST_CASE(a_task_runs_on_another_thread) {
    LatestTaskWorker worker;
    std::thread::id  ran;
    worker.Submit([&] { ran = std::this_thread::get_id(); });
    worker.Wait();
    CHECK_FALSE(worker.Busy());
    CHECK_TRUE(ran != std::thread::id() && ran != std::this_thread::get_id());
}

TEST_CASE(a_queued_task_is_replaced_by_a_newer_one) {
    LatestTaskWorker  worker;
    std::mutex        gate;
    std::atomic<int>  started{0};
    std::vector<int>  done;
    std::mutex        doneMutex;
    gate.lock();
    // The first task holds the thread until the gate opens; the second and third queue up.
    worker.Submit([&] { started++; std::lock_guard<std::mutex> g(gate); });
    while(started == 0) std::this_thread::yield();
    for(int i : { 2, 3 }) {
        worker.Submit([&, i] { std::lock_guard<std::mutex> g(doneMutex); done.push_back(i); });
    }
    gate.unlock();
    worker.Wait();
    CHECK_TRUE(done.size() == 1 && done[0] == 3);
}
