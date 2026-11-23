#include "core/events.hpp"

#include <utility>

namespace biyos
{

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

Events::Events(const Clock &clock) : clock_(clock), origin_(clock.wall_time()) {}

int Events::add_hook(EventKind kind, EventLevel level, Hook hook)
{
    const Turn turn(*this);
    return hooks_.add_hook(kind, level, std::move(hook));
}

void Events::delete_hook(int id)
{
    const Turn turn(*this);
    hooks_.delete_hook(id);
}

}
