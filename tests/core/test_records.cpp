#include "core/records.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <latch>
#include <string>
#include <thread>
#include <vector>

namespace
{

class Crowd
{
  public:
    Crowd(biyos::Records &records, std::size_t size)
        : filled_(static_cast<std::ptrdiff_t>(size)), seen_(size)
    {
        for (std::size_t i = 0; i < size; ++i)
        {
            threads_.emplace_back([this, &records, i] { stay(records, i); });
        }
        filled_.wait();
    }

    ~Crowd() { leave(); }

    Crowd(const Crowd &) = delete;
    Crowd &operator=(const Crowd &) = delete;

    std::vector<std::string> leave()
    {
        if (!threads_.empty())
        {
            release_.count_down();
        }
        for (std::thread &thread : threads_)
        {
            thread.join();
        }
        threads_.clear();
        return seen_;
    }

  private:
    void stay(biyos::Records &records, std::size_t i)
    {
        records.put(BIYOS_ERR_BADARG, nullptr, std::to_string(i));
        filled_.count_down();
        release_.wait();
        seen_[i] = records.get().text;
    }

    std::latch filled_;
    std::latch release_{1};
    std::vector<std::string> seen_;
    std::vector<std::thread> threads_;
};

const BIYOS_model *fake_model(const int &address)
{
    return reinterpret_cast<const BIYOS_model *>(&address);
}

}

TEST(RecordTest, Empty)
{
    const biyos::Records records;
    const biyos::Record &record = records.get();
    EXPECT_EQ(record.status, BIYOS_OK);
    EXPECT_EQ(record.model, nullptr);
    EXPECT_EQ(record.text, "");
}

TEST(RecordTest, Kept)
{
    const int address = 0;
    biyos::Records records;
    records.put(BIYOS_ERR_BADARG, fake_model(address), "first");
    records.put(BIYOS_ERR_NOMEMORY, nullptr, "second");
    const biyos::Record &record = records.get();
    EXPECT_EQ(record.status, BIYOS_ERR_NOMEMORY);
    EXPECT_EQ(record.model, nullptr);
    EXPECT_EQ(record.text, "second");
    records.put(BIYOS_ERR_BADARG, fake_model(address), "third");
    EXPECT_EQ(records.get().model, fake_model(address));
}

TEST(RecordTest, PerThread)
{
    biyos::Records records;
    std::latch both(2);
    std::vector<std::string> seen(2);
    const auto fail = [&](std::size_t i)
    {
        records.put(BIYOS_ERR_BADARG, nullptr, std::to_string(i));
        both.arrive_and_wait();
        seen[i] = records.get().text;
    };
    std::thread first(fail, 0);
    std::thread second(fail, 1);
    first.join();
    second.join();
    EXPECT_EQ(seen, (std::vector<std::string>{"0", "1"}));
    EXPECT_EQ(records.get().text, "");
}

TEST(RecordTest, DropsOldest)
{
    biyos::Records records;
    records.put(BIYOS_ERR_BADARG, nullptr, "main");
    Crowd crowd(records, biyos::kRecords);
    EXPECT_EQ(records.get().status, BIYOS_OK);
    EXPECT_EQ(records.get().text, "");
    const std::vector<std::string> seen = crowd.leave();
    EXPECT_EQ(seen.front(), "0");
    EXPECT_EQ(seen.back(), std::to_string(biyos::kRecords - 1));
    EXPECT_EQ(std::ranges::count(seen, std::string()), 0);
}

TEST(RecordTest, Refreshed)
{
    biyos::Records records;
    records.put(BIYOS_ERR_BADARG, nullptr, "main");
    Crowd first(records, biyos::kRecords - 1);
    records.put(BIYOS_ERR_BADARG, nullptr, "again");
    Crowd second(records, 1);
    EXPECT_EQ(records.get().text, "again");
    const std::vector<std::string> seen = first.leave();
    EXPECT_EQ(std::ranges::count(seen, std::string()), 1);
    EXPECT_EQ(second.leave(), std::vector<std::string>{"0"});
}

TEST(RecordTest, Swapped)
{
    const int address = 0;
    biyos::Records records;
    records.put(BIYOS_ERR_BADARG, fake_model(address), "kept");
    biyos::Record held;
    records.swap(held);
    EXPECT_EQ(held.status, BIYOS_ERR_BADARG);
    EXPECT_EQ(held.model, fake_model(address));
    EXPECT_EQ(held.text, "kept");
    EXPECT_EQ(records.get().status, BIYOS_OK);
    records.swap(held);
    EXPECT_EQ(records.get().text, "kept");
    EXPECT_EQ(held.text, "");
}

TEST(RecordTest, SwapEmpty)
{
    biyos::Records records;
    biyos::Record held{.status = BIYOS_ERR_BADARG, .model = nullptr, .text = "held"};
    records.swap(held);
    EXPECT_EQ(held.text, "held");
    EXPECT_EQ(records.get().status, BIYOS_OK);
}
