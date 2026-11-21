#include "core/events.hpp"

#include "core/error.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace biyos
{

namespace
{

constexpr std::array kLevels{EventLevel::Debug,
                             EventLevel::Info,
                             EventLevel::Warning,
                             EventLevel::Error};

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

void notify(const Subscriber &subscriber, const Event &evt) noexcept
{
    try
    {
        subscriber(evt);
    }
    catch (...)
    {
    }
}

}

EventKind find_kind(int id)
{
    const auto knd = static_cast<EventKind>(id);
    if (std::ranges::find(kEventKinds, knd) == kEventKinds.end())
    {
        throw Error(ErrorKind::InvalidArgument, "event kind " + std::to_string(id), "unknown");
    }
    return knd;
}

EventLevel find_level(int id)
{
    const auto lvl = static_cast<EventLevel>(id);
    if (std::ranges::find(kLevels, lvl) == kLevels.end())
    {
        throw Error(ErrorKind::InvalidArgument, "level " + std::to_string(id), "unknown");
    }
    return lvl;
}

std::string_view name_of(EventKind knd) noexcept
{
    switch (knd)
    {
    case EventKind::Message:
        return "message";
    case EventKind::Check:
        return "check";
    case EventKind::Phase:
        return "phase";
    }
    return "";
}

std::string_view name_of(EventLevel lvl) noexcept
{
    switch (lvl)
    {
    case EventLevel::Debug:
        return "debug";
    case EventLevel::Info:
        return "info";
    case EventLevel::Warning:
        return "warning";
    case EventLevel::Error:
        return "error";
    }
    return "";
}

Events::Events(const Clock &clock) : clock_(clock), origin_(clock.wall_time())
{
    lowest_.fill(kNobody);
}

int Events::subscribe(EventKind knd, EventLevel lowest, Subscriber subscriber)
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
        .kind = knd,
        .lowest = lowest,
        .subscriber = std::make_unique<const Subscriber>(std::move(subscriber)),
        .active = true,
    });
    const std::size_t at = slot(knd);
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

bool Events::accepts(const Subscription &subscription, EventKind knd, EventLevel lvl) noexcept
{
    return subscription.active && subscription.kind == knd && subscription.lowest <= lvl;
}

void Events::refresh(EventKind knd) noexcept
{
    EventLevel lvl = kNobody;
    for (const Subscription &subscription : subscriptions_)
    {
        if (subscription.active && subscription.kind == knd)
        {
            lvl = (std::min)(lvl, subscription.lowest);
        }
    }
    lowest_[slot(knd)] = lvl;
}

void Events::deliver(const Event &evt) noexcept
{
    const Depth delivery(depth_);
    const std::size_t n = subscriptions_.size();
    for (std::size_t i = 0; i < n; ++i)
    {
        const Subscription &subscription = subscriptions_[i];
        if (accepts(subscription, evt.kind, evt.level))
        {
            notify(*subscription.subscriber, evt);
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
