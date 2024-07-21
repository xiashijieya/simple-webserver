#include "thread_pool.h"

namespace sws {

ThreadPool::ThreadPool(size_t thread_count) : stopped_(false) {
    if (thread_count == 0) thread_count = 1;
    workers_.reserve(thread_count);
    for (size_t i = 0; i < thread_count; ++i) {
        workers_.push_back(std::thread(&ThreadPool::worker_loop, this));
    }
}

ThreadPool::~ThreadPool() {
    shutdown();
}

void ThreadPool::submit(Task task) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopped_) return;
        tasks_.push(task);
    }
    not_empty_.notify_one();
}

void ThreadPool::shutdown() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopped_) return;
        stopped_ = true;
    }
    not_empty_.notify_all();

    for (size_t i = 0; i < workers_.size(); ++i) {
        if (workers_[i].joinable()) workers_[i].join();
    }
    workers_.clear();
}

ThreadPool::Task ThreadPool::take_task() {
    std::unique_lock<std::mutex> lock(mutex_);
    not_empty_.wait(lock, [this]() { return stopped_ || !tasks_.empty(); });

    if (tasks_.empty()) return Task();

    Task task = tasks_.front();
    tasks_.pop();
    return task;
}

void ThreadPool::worker_loop() {
    while (true) {
        Task task = take_task();
        if (!task) return;
        task();
    }
}

} // namespace sws
