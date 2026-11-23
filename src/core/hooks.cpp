#include "core/hooks.hpp"

#include "core/depth.hpp"
#include "core/error.hpp"
#include "core/thread.hpp"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <limits>
#include <memory>
#include <string>
#include <utility>

namespace biyos
{

namespace
{

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

Hooks::Turn::Turn(Hooks &hooks) noexcept : hooks_(hooks), nested_(hooks.mine())
{
    if (!nested_)
    {
        hooks_.turn_.lock();
        hooks_.owner_.store(this_thread());
    }
}

Hooks::Turn::~Turn()
{
    if (!nested_)
    {
        hooks_.owner_.store(ThreadId{});
        hooks_.turn_.unlock();
    }
}

Hooks::Hooks()
{
    for (std::atomic<EventLevel> &lowest : lowest_)
    {
        lowest.store(kNobody);
    }
}

int Hooks::add_hook(EventKind kind, EventLevel level, Hook hook)
{
    const Turn turn(*this);
    if (!hook)
    {
        throw Error(ErrorKind::InvalidArgument, "hook", "is empty");
    }
    if (next_id_ == (std::numeric_limits<int>::max)())
    {
        throw Error(ErrorKind::NoMemory, "hook", "no id left");
    }
    entries_.push_back(Entry{
        .id = next_id_,
        .kind = kind,
        .lowest = level,
        .hook = std::make_unique<const Hook>(std::move(hook)),
        .active = true,
    });
    std::atomic<EventLevel> &lowest = lowest_[slot(kind)];
    lowest.store((std::min)(lowest.load(), level));
    return next_id_++;
}

void Hooks::delete_hook(int id)
{
    const Turn turn(*this);
    const auto found = std::ranges::find(entries_, id, &Entry::id);
    if (found == entries_.end() || !found->active)
    {
        throw Error(ErrorKind::InvalidArgument, "hook " + std::to_string(id), "unknown");
    }
    found->active = false;
    refresh(found->kind);
    if (!delivering())
    {
        const std::unique_ptr<const Hook> removed = std::move(found->hook);
        entries_.erase(found);
    }
}

bool Hooks::accepts(const Entry &entry, EventKind kind, EventLevel level) noexcept
{
    return entry.active && entry.kind == kind && entry.lowest <= level;
}

void Hooks::refresh(EventKind kind) noexcept
{
    EventLevel level = kNobody;
    for (const Entry &entry : entries_)
    {
        if (entry.active && entry.kind == kind)
        {
            level = (std::min)(level, entry.lowest);
        }
    }
    lowest_[slot(kind)].store(level);
}

void Hooks::deliver(const Event &event) noexcept
{
    const Turn turn(*this);
    const Depth delivery(depth_);
    const std::size_t n = entries_.size();
    for (std::size_t i = 0; i < n; ++i)
    {
        const Entry &entry = entries_[i];
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

void Hooks::prune() noexcept
{
    std::size_t kept = 0;
    for (std::size_t i = 0; i < entries_.size(); ++i)
    {
        if (entries_[i].active)
        {
            std::swap(entries_[kept], entries_[i]);
            ++kept;
        }
    }
    while (!entries_.empty() && !entries_.back().active)
    {
        const std::unique_ptr<const Hook> removed = std::move(entries_.back().hook);
        entries_.pop_back();
    }
}

}
