#include "core/events.hpp"

#include "core/error.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <limits>
#include <string>
#include <utility>

namespace biyos
{

namespace
{

constexpr std::array kLevels{Level::Debug, Level::Info, Level::Warning, Level::Error};

constexpr bool numbered()
{
    for (std::size_t i = 0; i < kEventKinds.size(); ++i)
    {
        if (static_cast<std::size_t>(kEventKinds[i]) != i + 1)
        {
            return false;
        }
    }
    return true;
}

static_assert(numbered());
static_assert(std::ranges::is_sorted(kLevels));

class Delivery
{
  public:
    explicit Delivery(int &depth) noexcept : depth_(depth) { ++depth_; }

    ~Delivery() { --depth_; }

    Delivery(const Delivery &) = delete;
    Delivery &operator=(const Delivery &) = delete;

  private:
    int &depth_;
};

}

EventKind find_kind(int id)
{
    const auto kind = static_cast<EventKind>(id);
    if (std::ranges::find(kEventKinds, kind) == kEventKinds.end())
    {
        throw Error(ErrorKind::InvalidArgument, "event kind " + std::to_string(id), "unknown");
    }
    return kind;
}

Level find_level(int id)
{
    const auto level = static_cast<Level>(id);
    if (std::ranges::find(kLevels, level) == kLevels.end())
    {
        throw Error(ErrorKind::InvalidArgument, "level " + std::to_string(id), "unknown");
    }
    return level;
}

Events::Events()
{
    lowest_.fill(kNobody);
}

int Events::subscribe(EventKind kind, Level lowest, Subscriber subscriber)
{
    if (!subscriber)
    {
        throw Error(ErrorKind::InvalidArgument, "subscriber", "is empty");
    }
    if (next_id_ == (std::numeric_limits<int>::max)())
    {
        throw Error(ErrorKind::NoMemory, "subscription", "no id left");
    }
    subscriptions_.push_back(Subscription{
        .id = next_id_,
        .kind = kind,
        .lowest = lowest,
        .subscriber = std::move(subscriber),
        .active = true,
    });
    const std::size_t at = slot(kind);
    lowest_[at] = (std::min)(lowest_[at], lowest);
    return next_id_++;
}

void Events::unsubscribe(int id)
{
    const auto found = std::ranges::find(subscriptions_, id, &Subscription::id);
    if (found == subscriptions_.end() || !found->active)
    {
        throw Error(ErrorKind::InvalidArgument, "subscription " + std::to_string(id), "unknown");
    }
    const EventKind kind = found->kind;
    if (depth_ > 0)
    {
        found->active = false;
    }
    else
    {
        subscriptions_.erase(found);
    }
    refresh(kind);
}

bool Events::accepts(const Subscription &subscription, EventKind kind, Level level) noexcept
{
    return subscription.active && subscription.kind == kind && subscription.lowest <= level;
}

void Events::refresh(EventKind kind) noexcept
{
    Level lowest = kNobody;
    for (const Subscription &subscription : subscriptions_)
    {
        if (subscription.active && subscription.kind == kind)
        {
            lowest = (std::min)(lowest, subscription.lowest);
        }
    }
    lowest_[slot(kind)] = lowest;
}

void Events::deliver(const Event &event)
{
    const Delivery delivery(depth_);
    const std::size_t count = subscriptions_.size();
    for (std::size_t i = 0; i < count; ++i)
    {
        const Subscription &subscription = subscriptions_[i];
        if (accepts(subscription, event.kind, event.level))
        {
            subscription.subscriber(event);
        }
    }
    if (depth_ == 1)
    {
        std::erase_if(subscriptions_, std::not_fn(&Subscription::active));
    }
}

}
