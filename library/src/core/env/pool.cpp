#include "core/env/pool.hpp"

#include <algorithm>
#include <cstddef>
#include <exception>
#include <limits>
#include <mutex>
#include <thread>

namespace biyos
{

namespace
{

template <typename Job> void alone(Job job, std::size_t chunks) noexcept
{
    for (std::size_t chunk = 0; chunk < chunks; ++chunk)
    {
        job.body(job.task, chunk, 0);
    }
}

}

std::size_t hardware_threads() noexcept
{
    const unsigned found = std::thread::hardware_concurrency();
    return found == 0 ? (std::numeric_limits<std::size_t>::max)() : found;
}

Pool::Pool(std::size_t limit) noexcept : limit_(limit) {}

Pool::~Pool()
{
    stop();
}

std::size_t Pool::workers()
{
    const std::lock_guard busy(busy_);
    return threads_.size();
}

std::size_t Pool::limit() const noexcept
{
    return limit_;
}

std::size_t Pool::capped(std::size_t threads) const noexcept
{
    return (std::min)(threads, limit_);
}

void Pool::stop() noexcept
{
    const std::lock_guard busy(busy_);
    halt();
}

void Pool::dispatch(Job job, std::size_t threads, std::size_t chunks) noexcept
{
    const std::size_t used = capped(threads);
    if (used <= 1 || chunks <= 1 || running_.exchange(true))
    {
        alone(job, chunks);
        return;
    }
    share(job, used, chunks);
    running_ = false;
}

void Pool::share(Job job, std::size_t threads, std::size_t chunks) noexcept
{
    const std::lock_guard busy(busy_);
    if (threads_.size() != threads - 1 && !start(threads - 1))
    {
        alone(job, chunks);
        return;
    }
    {
        const std::lock_guard lock(mutex_);
        job_ = job;
        chunks_ = chunks;
        next_ = 0;
        active_ = threads_.size();
        ++generation_;
    }
    wake_.notify_all();
    serve(0);
    std::unique_lock lock(mutex_);
    done_.wait(lock, [this] { return active_ == 0; });
}

bool Pool::start(std::size_t count) noexcept
{
    halt();
    try
    {
        threads_.reserve(count);
        for (std::size_t worker = 1; worker <= count; ++worker)
        {
            threads_.emplace_back(&Pool::work, this, worker, generation_);
        }
        return true;
    }
    catch (const std::exception &)
    {
        halt();
        return false;
    }
}

void Pool::halt() noexcept
{
    {
        const std::lock_guard lock(mutex_);
        stopping_ = true;
    }
    wake_.notify_all();
    for (std::thread &thread : threads_)
    {
        thread.join();
    }
    threads_.clear();
    const std::lock_guard lock(mutex_);
    stopping_ = false;
}

void Pool::work(std::size_t worker, std::size_t seen) noexcept
{
    for (;;)
    {
        {
            std::unique_lock lock(mutex_);
            wake_.wait(lock, [&] { return stopping_ || generation_ != seen; });
            if (stopping_)
            {
                return;
            }
            seen = generation_;
        }
        serve(worker);
        {
            const std::lock_guard lock(mutex_);
            --active_;
        }
        done_.notify_one();
    }
}

void Pool::serve(std::size_t worker) noexcept
{
    for (std::size_t chunk = next_++; chunk < chunks_; chunk = next_++)
    {
        job_.body(job_.task, chunk, worker);
    }
}

}
