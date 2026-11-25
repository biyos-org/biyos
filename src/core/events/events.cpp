#include "core/events/events.hpp"

#include <utility>

namespace biyos
{

int Events::add_hook(EventKind kind, EventLevel level, Hook hook)
{
    return hooks_.add_hook(kind, level, std::move(hook));
}

void Events::delete_hook(int id)
{
    hooks_.delete_hook(id);
}

}
