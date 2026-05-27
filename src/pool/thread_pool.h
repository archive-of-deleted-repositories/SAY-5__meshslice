#ifndef MESHSLICE_THREAD_POOL_H
#define MESHSLICE_THREAD_POOL_H

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace meshslice {

// Fixed-partition thread pool with a bounded number of in-flight units.
//
// The pool runs `workers` threads. At most `max_in_flight` units may be
// "checked out" (popped but not yet completed) at any instant; a worker that
// wants the next unit blocks until an in-flight slot frees. This caps peak
// memory at roughly `max_in_flight * max_unit_bytes` instead of holding every
// unit's working set simultaneously. With max_in_flight == workers, each worker
// owns exactly one unit at a time.
class ThreadPool {
   public:
    ThreadPool(size_t workers, size_t max_in_flight);
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // Run `task(i)` for i in [0, n), distributing across workers under the
    // in-flight cap. Blocks until all tasks complete. The caller's task body is
    // responsible for writing results into per-index storage (no shared mutable
    // state inside the pool itself).
    void run(size_t n, const std::function<void(size_t)>& task);

    size_t worker_count() const {
        return workers_;
    }
    size_t max_in_flight() const {
        return max_in_flight_;
    }

    // High-water mark of simultaneously in-flight units observed by run().
    size_t peak_in_flight() const {
        return peak_in_flight_.load();
    }

   private:
    void worker_loop();

    size_t workers_;
    size_t max_in_flight_;

    std::vector<std::thread> threads_;

    std::mutex mtx_;
    std::condition_variable cv_slot_;  // signalled when an in-flight slot frees.
    std::condition_variable cv_work_;  // signalled when work or shutdown arrives.
    std::condition_variable cv_done_;  // signalled when a batch finishes.

    const std::function<void(size_t)>* task_ = nullptr;
    size_t next_index_ = 0;
    size_t total_ = 0;
    size_t completed_ = 0;
    size_t in_flight_ = 0;
    bool shutdown_ = false;

    std::atomic<size_t> peak_in_flight_{0};
};

}  // namespace meshslice

#endif  // MESHSLICE_THREAD_POOL_H
