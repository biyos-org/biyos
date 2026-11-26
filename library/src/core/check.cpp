#include "core/check.hpp"

#include <string>

namespace biyos
{

namespace
{

std::string message_of(std::string_view invariant, std::string_view values)
{
    std::string message = "check '" + std::string(invariant) + "' failed";
    if (!values.empty())
    {
        message += ": ";
        message += values;
    }
    return message;
}

void describe(Event &event,
              std::string_view module,
              const Error &error,
              std::string_view invariant,
              std::string_view values)
{
    event.module.assign(module);
    event.text = error.what();
    event.fields.push_back(Field{.name = "invariant", .value = std::string(invariant)});
    event.fields.push_back(Field{.name = "object", .value = error.object()});
    event.fields.push_back(Field{.name = "values", .value = std::string(values)});
}

}

Checker::Checker(Stream stream, const Lookup &lookup, StaticName module)
    : stream_(stream), module_(module.text()),
      checking_(lookup.get_enum<Checking>(Option::Checking))
{
}

void Checker::fail(ErrorKind kind,
                   std::string_view invariant,
                   std::string_view object,
                   std::string_view values) const
{
    const Error error(kind, std::string(object), message_of(invariant, values));
    const auto fill = [&](Event &event) { describe(event, module_, error, invariant, values); };
    if (stream_.filling())
    {
        throw error;
    }
    stream_.record(kind, error.what());
    if (!stream_.delivering())
    {
        stream_.emit(EventKind::Invariant, EventLevel::Error, fill);
    }
    throw error;
}

}
