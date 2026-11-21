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

void notify(const Hook &hk, const Event &evt) noexcept
{
    try
    {
        hk(evt);
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

int Events::add_hook(EventKind knd, EventLevel lvl, Hook hk)
{
    if (!hk)
    {
        throw Error(ErrorKind::InvalidArgument, "subscriber", "is empty");
    }
    if (next_id_ == (std::numeric_limits<int>::max)())
    {
        throw Error(ErrorKind::NoMemory, "subscription", "no id left");
    }
    hooks_.push_back(Entry{
        .id = next_id_,
        .kind = knd,
        .lowest = lvl,
        .hook = std::make_unique<const Hook>(std::move(hk)),
        .active = true,
    });
    const std::size_t at = slot(knd);
    lowest_[at] = (std::min)(lowest_[at], lvl);
    return next_id_++;
}

void Events::del_hook(int id)
{
    const auto found = std::ranges::find(hooks_, id, &Entry::id);
    if (found == hooks_.end() || !found->active)
    {
        throw Error(ErrorKind::InvalidArgument, "subscription " + std::to_string(id), "unknown");
    }
    found->active = false;
    refresh(found->kind);
    if (depth_ == 0)
    {
        const std::unique_ptr<const Hook> removed = std::move(found->hook);
        hooks_.erase(found);
    }
}

bool Events::accepts(const Entry &entry, EventKind knd, EventLevel lvl) noexcept
{
    return entry.active && entry.kind == knd && entry.lowest <= lvl;
}

void Events::refresh(EventKind knd) noexcept
{
    EventLevel lvl = kNobody;
    for (const Entry &entry : hooks_)
    {
        if (entry.active && entry.kind == knd)
        {
            lvl = (std::min)(lvl, entry.lowest);
        }
    }
    lowest_[slot(knd)] = lvl;
}

void Events::deliver(const Event &evt) noexcept
{
    const Depth delivery(depth_);
    const std::size_t n = hooks_.size();
    for (std::size_t i = 0; i < n; ++i)
    {
        const Entry &entry = hooks_[i];
        if (accepts(entry, evt.kind, evt.level))
        {
            notify(*entry.hook, evt);
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
    for (std::size_t i = 0; i < hooks_.size(); ++i)
    {
        if (hooks_[i].active)
        {
            std::swap(hooks_[kept], hooks_[i]);
            ++kept;
        }
    }
    while (!hooks_.empty() && !hooks_.back().active)
    {
        const std::unique_ptr<const Hook> removed = std::move(hooks_.back().hook);
        hooks_.pop_back();
    }
}

}
