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

void notify(const Hook &hook, const Event &event) noexcept
{
    try
    {
        hook(event);
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

EventLevel find_level(int id)
{
    const auto level = static_cast<EventLevel>(id);
    if (std::ranges::find(kLevels, level) == kLevels.end())
    {
        throw Error(ErrorKind::InvalidArgument, "level " + std::to_string(id), "unknown");
    }
    return level;
}

std::string_view name_of(EventKind kind) noexcept
{
    switch (kind)
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

std::string_view name_of(EventLevel level) noexcept
{
    switch (level)
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

Events::Turn::Turn(Events &events) noexcept : events_(events), nested_(events.mine())
{
    if (!nested_)
    {
        events_.turn_.lock();
        events_.owner_.store(this_thread());
    }
}

Events::Turn::~Turn()
{
    if (!nested_)
    {
        events_.owner_.store(ThreadId{});
        events_.turn_.unlock();
    }
}

Events::Events(const Clock &clock) : clock_(clock), origin_(clock.wall_time())
{
    lowest_.fill(kNobody);
}

int Events::add_hook(EventKind kind, EventLevel level, Hook hook)
{
    if (!hook)
    {
        throw Error(ErrorKind::InvalidArgument, "hook", "is empty");
    }
    if (next_id_ == (std::numeric_limits<int>::max)())
    {
        throw Error(ErrorKind::NoMemory, "hook", "no id left");
    }
    hooks_.push_back(Entry{
        .id = next_id_,
        .kind = kind,
        .lowest = level,
        .hook = std::make_unique<const Hook>(std::move(hook)),
        .active = true,
    });
    const std::size_t at = slot(kind);
    lowest_[at] = (std::min)(lowest_[at], level);
    return next_id_++;
}

void Events::delete_hook(int id)
{
    const auto found = std::ranges::find(hooks_, id, &Entry::id);
    if (found == hooks_.end() || !found->active)
    {
        throw Error(ErrorKind::InvalidArgument, "hook " + std::to_string(id), "unknown");
    }
    found->active = false;
    refresh(found->kind);
    if (!delivering())
    {
        const std::unique_ptr<const Hook> removed = std::move(found->hook);
        hooks_.erase(found);
    }
}

bool Events::accepts(const Entry &entry, EventKind kind, EventLevel level) noexcept
{
    return entry.active && entry.kind == kind && entry.lowest <= level;
}

void Events::refresh(EventKind kind) noexcept
{
    EventLevel level = kNobody;
    for (const Entry &entry : hooks_)
    {
        if (entry.active && entry.kind == kind)
        {
            level = (std::min)(level, entry.lowest);
        }
    }
    lowest_[slot(kind)] = level;
}

void Events::deliver(const Event &event) noexcept
{
    const Depth delivery(depth_);
    const std::size_t n = hooks_.size();
    for (std::size_t i = 0; i < n; ++i)
    {
        const Entry &entry = hooks_[i];
        if (accepts(entry, event.kind, event.level))
        {
            notify(*entry.hook, event);
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
