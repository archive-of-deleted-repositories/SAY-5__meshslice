#include "pool/thread_pool.h"

#include <algorithm>
#include <stdexcept>

namespace meshslice {

ThreadPool::ThreadPool(size_t workers, size_t max_in_flight)
    : workers_(std::max<size_t>(1, workers)), max_in_flight_(std::max<size_t>(1, max_in_flight)) {
    start(nullptr);
}

ThreadPool::ThreadPool(size_t workers, size_t max_in_flight, const NumaTopology& topo)
    : workers_(std::max<size_t>(1, workers)),
      max_in_flight_(std::max<size_t>(1, max_in_flight)),
      topo_(topo) {
    start(&topo_);
}

void ThreadPool::start(const NumaTopology* topo) {
    threads_.reserve(workers_);
    for (size_t i = 0; i < workers_; ++i) {
        const int id = static_cast<int>(i);
        threads_.emplace_back([this, id, topo] { worker_loop(id, topo); });
    }
}

ThreadPool::~ThreadPool() {
    {
        std::lock_guard<std::mutex> lk(mtx_);
        shutdown_ = true;
    }
    cv_work_.notify_all();
    cv_slot_.notify_all();
    for (std::thread& t : threads_) {
        if (t.joinable()) {
            t.join();
        }
    }
}

void ThreadPool::run(size_t n, const std::function<void(size_t)>& task) {
    if (n == 0) {
        return;
    }
    {
        std::lock_guard<std::mutex> lk(mtx_);
        task_ = &task;
        next_index_ = 0;
        total_ = n;
        completed_ = 0;
        in_flight_ = 0;
        peak_in_flight_.store(0);
    }
    cv_work_.notify_all();

    std::unique_lock<std::mutex> lk(mtx_);
    cv_done_.wait(lk, [this] { return completed_ == total_; });
    task_ = nullptr;
}

void ThreadPool::worker_loop(int worker_id, const NumaTopology* topo) {
    // Pin this worker to a NUMA node round-robin. On a single-node box or a host
    // without affinity control this returns false and the worker runs unpinned,
    // identical to the naive scheduler.
    if (topo != nullptr && !topo->nodes.empty()) {
        const uint32_t node_id =
            topo->nodes[static_cast<size_t>(worker_id) % topo->nodes.size()].id;
        if (pin_thread_to_node(*topo, node_id)) {
            pinned_workers_.fetch_add(1);
        }
    }
    for (;;) {
        std::unique_lock<std::mutex> lk(mtx_);

        // Wait until there is a task index available, or shutdown.
        cv_work_.wait(lk,
                      [this] { return shutdown_ || (task_ != nullptr && next_index_ < total_); });
        if (shutdown_) {
            return;
        }

        // Respect the in-flight cap before checking out the next index.
        cv_slot_.wait(lk, [this] { return shutdown_ || in_flight_ < max_in_flight_; });
        if (shutdown_) {
            return;
        }
        if (task_ == nullptr || next_index_ >= total_) {
            continue;  // batch drained while we waited; loop back.
        }

        const size_t idx = next_index_++;
        ++in_flight_;
        const size_t cur = in_flight_;
        size_t prev = peak_in_flight_.load();
        while (cur > prev && !peak_in_flight_.compare_exchange_weak(prev, cur)) {
        }
        const std::function<void(size_t)>* task = task_;
        lk.unlock();

        (*task)(idx);

        lk.lock();
        --in_flight_;
        ++completed_;
        const bool batch_done = (completed_ == total_);
        lk.unlock();

        cv_slot_.notify_one();
        if (batch_done) {
            cv_done_.notify_all();
        }
    }
}

}  // namespace meshslice
