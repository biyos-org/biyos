#include "core/events.hpp"

#include "core/error.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <memory>
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

void notify(const Subscriber &subscriber, const Event &event) noexcept
{
    try
    {
        subscriber(event);
    }
    catch (...)
    {
    }
}

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

Events::Events(const Clock &clock) : clock_(clock), origin_(clock.wall_time())
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
        .subscriber = std::make_unique<const Subscriber>(std::move(subscriber)),
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
    found->active = false;
    refresh(found->kind);
    if (depth_ == 0)
    {
        const std::unique_ptr<const Subscriber> removed = std::move(found->subscriber);
        subscriptions_.erase(found);
    }
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

void Events::deliver(const Event &event) noexcept
{
    const Depth delivery(depth_);
    const std::size_t count = subscriptions_.size();
    for (std::size_t i = 0; i < count; ++i)
    {
        const Subscription &subscription = subscriptions_[i];
        if (accepts(subscription, event.kind, event.level))
        {
            notify(*subscription.subscriber, event);
        }
    }
    if (depth_ == 1)
    {
        prune();
    }
}

void Events::prune() noexcept
{
    std::size_t kept = 0;
    for (std::size_t i = 0; i < subscriptions_.size(); ++i)
    {
        if (subscriptions_[i].active)
        {
            std::swap(subscriptions_[kept], subscriptions_[i]);
            ++kept;
        }
    }
    while (!subscriptions_.empty() && !subscriptions_.back().active)
    {
        const std::unique_ptr<const Subscriber> removed =
            std::move(subscriptions_.back().subscriber);
        subscriptions_.pop_back();
    }
}

}
