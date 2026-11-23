#include "core/events.hpp"

#include <utility>

namespace biyos
{

Events::Events(const Clock &clock) : clock_(clock), origin_(clock.wall_time()) {}

int Events::add_hook(EventKind kind, EventLevel level, Hook hook)
{
    return hooks_.add_hook(kind, level, std::move(hook));
}

void Events::delete_hook(int id)
{
    hooks_.delete_hook(id);
}

}
