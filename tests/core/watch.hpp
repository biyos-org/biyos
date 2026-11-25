#pragma once

#include "core/events/event.hpp"
#include "core/events/events.hpp"

#include <atomic>
#include <thread>

namespace biyos::test
{

class Watch
{
  public:
    void operator()(const Event &)
    {
        if (inside_.exchange(true))
        {
            ++overlaps_;
        }
        std::this_thread::yield();
        ++seen_;
        inside_.store(false);
    }

    int overlaps() const { return overlaps_.load(); }
    int seen() const { return seen_; }

  private:
    std::atomic<bool> inside_{false};
    std::atomic<int> overlaps_{0};
    int seen_ = 0;
};

using Read = bool (Events::*)() const noexcept;

inline bool elsewhere(const Events &events, Read read)
{
    bool seen = true;
    std::thread([&] { seen = (events.*read)(); }).join();
    return seen;
}

class Peek
{
  public:
    explicit Peek(const Events &events) : events_(events) {}

    void hook(const Event &) { delivering = elsewhere(events_, &Events::delivering); }
    void fill(Event &) { filling = elsewhere(events_, &Events::filling); }

    bool delivering = true;
    bool filling = true;

  private:
    const Events &events_;
};

}
