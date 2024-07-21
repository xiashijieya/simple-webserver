#ifndef SWS_THREAD_POOL_H
#define SWS_THREAD_POOL_H

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace sws {

class ThreadPool {
public:
    typedef std::function<void()> Task;

    explicit ThreadPool(size_t thread_count);
    ~ThreadPool();

    void submit(Task task);
    void shutdown();

private:
    void worker_loop();
    Task take_task();

    std::vector<std::thread> workers_;
    std::queue<Task> tasks_;
    std::mutex mutex_;
    std::condition_variable not_empty_;
    bool stopped_;
};

} // namespace sws

#endif
