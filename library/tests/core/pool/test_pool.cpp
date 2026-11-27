#include "core/env/env.hpp"
#include "core/env/pool.hpp"
#include "core/timing/clock.hpp"

#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <thread>
#include <vector>

namespace
{

using biyos::Env;
using biyos::hardware_threads;
using biyos::Pool;
using biyos::SystemClock;

constexpr std::size_t kChunks = 100;
constexpr std::size_t kLimit = 8;
constexpr std::chrono::seconds kPatience{10};

struct Tally
{
    std::vector<int> runs = std::vector<int>(kChunks, 0);
    std::vector<std::size_t> workers = std::vector<std::size_t>(kChunks, 0);

    void operator()(std::size_t chunk, std::size_t worker) noexcept
    {
        ++runs[chunk];
        workers[chunk] = worker;
    }
};

struct Meeting
{
    std::atomic<std::size_t> arrived{0};
    std::atomic<std::size_t> met{0};

    void operator()(std::size_t, std::size_t) noexcept
    {
        ++arrived;
        const auto deadline = std::chrono::steady_clock::now() + kPatience;
        while (arrived < 2 && std::chrono::steady_clock::now() < deadline)
        {
            std::this_thread::yield();
        }
        if (arrived == 2)
        {
            ++met;
        }
    }
};

struct Inner
{
    std::span<int> runs;

    void operator()(std::size_t chunk, std::size_t) noexcept { ++runs[chunk]; }
};

struct Nested
{
    Pool &pool;
    std::vector<int> runs = std::vector<int>(kChunks * kChunks, 0);

    void operator()(std::size_t chunk, std::size_t) noexcept
    {
        Inner inner{.runs = std::span(runs).subspan(chunk * kChunks, kChunks)};
        pool.run(4, kChunks, inner);
    }
};

}

TEST(PoolTest, EveryChunkOnce)
{
    for (const std::size_t threads : std::array<std::size_t, 4>{1, 2, 3, 8})
    {
        Pool pool(kLimit);
        Tally tally;
        pool.run(threads, kChunks, tally);
        for (std::size_t chunk = 0; chunk < kChunks; ++chunk)
        {
            EXPECT_EQ(tally.runs[chunk], 1);
            EXPECT_LT(tally.workers[chunk], threads);
        }
    }
}

TEST(PoolTest, Concurrent)
{
    Pool pool(kLimit);
    Meeting meeting;
    pool.run(2, 2, meeting);
    EXPECT_EQ(meeting.met, 2);
}

TEST(PoolTest, StartedAtFirstUse)
{
    Pool pool(kLimit);
    EXPECT_EQ(pool.workers(), 0);
    Tally tally;
    pool.run(4, kChunks, tally);
    EXPECT_EQ(pool.workers(), 3);
}

TEST(PoolTest, Resized)
{
    Pool pool(kLimit);
    Tally tally;
    pool.run(8, kChunks, tally);
    pool.run(2, kChunks, tally);
    EXPECT_EQ(pool.workers(), 1);
}

TEST(PoolTest, SerialStartsNothing)
{
    Pool pool(kLimit);
    Tally tally;
    pool.run(1, kChunks, tally);
    pool.run(8, 1, tally);
    EXPECT_EQ(pool.workers(), 0);
    EXPECT_EQ(tally.runs[0], 2);
}

TEST(PoolTest, Stopped)
{
    Pool pool(kLimit);
    Tally tally;
    pool.run(4, kChunks, tally);
    pool.stop();
    EXPECT_EQ(pool.workers(), 0);
    pool.run(4, kChunks, tally);
    EXPECT_EQ(tally.runs[kChunks - 1], 2);
}

TEST(PoolTest, NestedRunsSerially)
{
    Pool pool(kLimit);
    Nested nested{.pool = pool};
    pool.run(4, kChunks, nested);
    for (const int runs : nested.runs)
    {
        EXPECT_EQ(runs, 1);
    }
}

TEST(PoolTest, StoppedWithEnvironment)
{
    std::optional<Env> env(std::in_place, std::make_unique<SystemClock>(), kLimit);
    Tally tally;
    env->pool().run(4, kChunks, tally);
    EXPECT_EQ(env->pool().workers(), 3);
    env.reset();
}

TEST(PoolTest, Capped)
{
    Pool pool(2);
    Tally tally;
    pool.run(kLimit, kChunks, tally);
    EXPECT_EQ(pool.workers(), 1);
    for (std::size_t chunk = 0; chunk < kChunks; ++chunk)
    {
        EXPECT_EQ(tally.runs[chunk], 1);
        EXPECT_LT(tally.workers[chunk], 2U);
    }
}

TEST(PoolTest, Hardware)
{
    EXPECT_EQ(Pool().limit(), hardware_threads());
    EXPECT_EQ(Env().pool().limit(), hardware_threads());
    EXPECT_GE(hardware_threads(), 1U);
}
