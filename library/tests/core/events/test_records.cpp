#include "core/events/records.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <latch>
#include <optional>
#include <string>
#include <thread>
#include <vector>

using biyos::ErrorKind;
using biyos::kRecords;
using biyos::Record;
using biyos::Records;
using biyos::RecordView;
using biyos::Session;

namespace
{

class Crowd
{
  public:
    Crowd(Records &records, std::size_t size)
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
    void stay(Records &records, std::size_t i)
    {
        records.put(ErrorKind::InvalidArgument, nullptr, std::to_string(i));
        filled_.count_down();
        release_.wait();
        seen_[i] = records.get().text;
    }

    std::latch filled_;
    std::latch release_{1};
    std::vector<std::string> seen_;
    std::vector<std::thread> threads_;
};

const Session *fake_model(const int &address)
{
    return reinterpret_cast<const Session *>(&address);
}

}

TEST(RecordTest, Empty)
{
    const Records records;
    const RecordView record = records.get();
    EXPECT_EQ(record.kind, std::nullopt);
    EXPECT_EQ(record.model, nullptr);
    EXPECT_STREQ(record.text, "");
}

TEST(RecordTest, Kept)
{
    const int address = 0;
    Records records;
    records.put(ErrorKind::InvalidArgument, fake_model(address), "first");
    records.put(ErrorKind::NoMemory, nullptr, "second");
    const RecordView record = records.get();
    EXPECT_EQ(record.kind, ErrorKind::NoMemory);
    EXPECT_EQ(record.model, nullptr);
    EXPECT_STREQ(record.text, "second");
    records.put(ErrorKind::InvalidArgument, fake_model(address), "third");
    EXPECT_EQ(records.get().model, fake_model(address));
}

TEST(RecordTest, PerThread)
{
    Records records;
    std::latch both(2);
    std::vector<std::string> seen(2);
    const auto fail = [&](std::size_t i)
    {
        records.put(ErrorKind::InvalidArgument, nullptr, std::to_string(i));
        both.arrive_and_wait();
        seen[i] = records.get().text;
    };
    std::thread first(fail, 0);
    std::thread second(fail, 1);
    first.join();
    second.join();
    EXPECT_EQ(seen, (std::vector<std::string>{"0", "1"}));
    EXPECT_STREQ(records.get().text, "");
}

TEST(RecordTest, DropsOldest)
{
    Records records;
    records.put(ErrorKind::InvalidArgument, nullptr, "main");
    Crowd crowd(records, kRecords);
    EXPECT_EQ(records.get().kind, std::nullopt);
    EXPECT_STREQ(records.get().text, "");
    const std::vector<std::string> seen = crowd.leave();
    EXPECT_EQ(seen.front(), "0");
    EXPECT_EQ(seen.back(), std::to_string(kRecords - 1));
    EXPECT_EQ(std::ranges::count(seen, std::string()), 0);
}

TEST(RecordTest, ReadDropped)
{
    const int address = 0;
    Records records;
    records.put(ErrorKind::InvalidArgument, fake_model(address), "main");
    const auto &read = records.get();
    const Crowd crowd(records, kRecords);
    EXPECT_EQ(read.kind, ErrorKind::InvalidArgument);
    EXPECT_EQ(read.model, fake_model(address));
}

TEST(RecordTest, Refreshed)
{
    Records records;
    records.put(ErrorKind::InvalidArgument, nullptr, "main");
    Crowd first(records, kRecords - 1);
    records.put(ErrorKind::InvalidArgument, nullptr, "again");
    Crowd second(records, 1);
    EXPECT_STREQ(records.get().text, "again");
    const std::vector<std::string> seen = first.leave();
    EXPECT_EQ(std::ranges::count(seen, std::string()), 1);
    EXPECT_EQ(second.leave(), std::vector<std::string>{"0"});
}

TEST(RecordTest, Swapped)
{
    const int address = 0;
    Records records;
    records.put(ErrorKind::InvalidArgument, fake_model(address), "kept");
    Record held;
    records.swap(held);
    EXPECT_EQ(held.kind, ErrorKind::InvalidArgument);
    EXPECT_EQ(held.model, fake_model(address));
    EXPECT_EQ(held.text, "kept");
    EXPECT_EQ(records.get().kind, std::nullopt);
    records.swap(held);
    EXPECT_STREQ(records.get().text, "kept");
    EXPECT_EQ(held.text, "");
}

TEST(RecordTest, SwapEmpty)
{
    Records records;
    Record held{.kind = ErrorKind::InvalidArgument, .model = nullptr, .text = "held"};
    records.swap(held);
    EXPECT_EQ(held.kind, std::nullopt);
    EXPECT_EQ(held.text, "");
    EXPECT_EQ(records.get().kind, ErrorKind::InvalidArgument);
    EXPECT_STREQ(records.get().text, "held");
}

TEST(RecordTest, SwapDropped)
{
    Records records;
    records.put(ErrorKind::InvalidArgument, nullptr, "outer");
    Record held;
    records.swap(held);
    records.put(ErrorKind::Internal, nullptr, "inner");
    const Crowd crowd(records, kRecords);
    records.swap(held);
    EXPECT_EQ(records.get().kind, ErrorKind::InvalidArgument);
    EXPECT_STREQ(records.get().text, "outer");
}
