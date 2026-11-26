#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <thread>
#include <type_traits>
#include <vector>

namespace biyos
{

template <typename Task>
concept chunk_task = std::is_nothrow_invocable_v<Task &, std::size_t, std::size_t>;

class Pool
{
  public:
    Pool() = default;
    ~Pool();
    Pool(const Pool &) = delete;
    Pool &operator=(const Pool &) = delete;

    template <chunk_task Task> void run(std::size_t threads, std::size_t chunks, Task &task)
    {
        dispatch({.task = &task, .body = &invoke<Task>}, threads, chunks);
    }

    std::size_t workers();
    void stop() noexcept;

  private:
    struct Job
    {
        void *task = nullptr;
        void (*body)(void *task, std::size_t chunk, std::size_t worker) noexcept = nullptr;
    };

    template <chunk_task Task>
    static void invoke(void *task, std::size_t chunk, std::size_t worker) noexcept
    {
        (*static_cast<Task *>(task))(chunk, worker);
    }

    void dispatch(Job job, std::size_t threads, std::size_t chunks) noexcept;
    void share(Job job, std::size_t threads, std::size_t chunks) noexcept;
    bool start(std::size_t count) noexcept;
    void halt() noexcept;
    void work(std::size_t worker, std::size_t seen) noexcept;
    void serve(std::size_t worker) noexcept;

    std::atomic<bool> running_{false};
    std::mutex busy_;
    std::mutex mutex_;
    std::condition_variable wake_;
    std::condition_variable done_;
    std::vector<std::thread> threads_;
    Job job_;
    std::size_t chunks_ = 0;
    std::atomic<std::size_t> next_{0};
    std::size_t generation_ = 0;
    std::size_t active_ = 0;
    bool stopping_ = false;
};

}
