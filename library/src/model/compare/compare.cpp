#include "model/compare/compare.hpp"

#include "core/events/event.hpp"
#include "core/events/stream.hpp"
#include "model/model.hpp"

#include <string>

namespace biyos
{

namespace
{

void describe(Event &event, const std::string &found)
{
    event.module = kModule.text();
    event.text = found;
}

}

bool compare(Stream stream, const Model &a, const Model &b)
{
    const std::string found = difference(a, b);
    if (found.empty())
    {
        return true;
    }
    const auto fill = [&](Event &event) { describe(event, found); };
    stream.emit(EventKind::Message, EventLevel::Info, fill);
    return false;
}

}
