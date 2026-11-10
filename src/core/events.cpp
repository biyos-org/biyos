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

constexpr std::array kKinds{EventKind::Message};
constexpr std::array kLevels{Level::Debug, Level::Info, Level::Warning, Level::Error};

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
    if (std::ranges::find(kKinds, kind) == kKinds.end())
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

int Events::subscribe(EventKind kind, Level lowest, Subscriber subscriber)
{
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
    return next_id_++;
}

void Events::unsubscribe(int id)
{
    const auto found = std::ranges::find(subscriptions_, id, &Subscription::id);
    if (found == subscriptions_.end() || !found->active)
    {
        throw Error(ErrorKind::InvalidArgument, "subscription " + std::to_string(id), "unknown");
    }
    if (depth_ > 0)
    {
        found->active = false;
        return;
    }
    subscriptions_.erase(found);
}

bool Events::wanted(EventKind kind, Level level) const noexcept
{
    for (const Subscription &subscription : subscriptions_)
    {
        if (accepts(subscription, kind, level))
        {
            return true;
        }
    }
    return false;
}

bool Events::accepts(const Subscription &subscription, EventKind kind, Level level) noexcept
{
    return subscription.active && subscription.kind == kind && subscription.lowest <= level;
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
