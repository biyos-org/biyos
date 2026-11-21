#include "core/check.hpp"

#include <string>

namespace biyos
{

namespace
{

std::string msg_of(std::string_view invariant, std::string_view values)
{
    std::string msg = "check '" + std::string(invariant) + "' failed";
    if (!values.empty())
    {
        msg += ": ";
        msg += values;
    }
    return msg;
}

void describe(Event &evt,
              std::string_view module,
              const Error &error,
              std::string_view invariant,
              std::string_view values)
{
    evt.module.assign(module);
    evt.text = error.what();
    evt.fields.push_back(Field{.name = "invariant", .value = std::string(invariant)});
    evt.fields.push_back(Field{.name = "object", .value = error.object()});
    evt.fields.push_back(Field{.name = "values", .value = std::string(values)});
}

}

Checker::Checker(Env &env, std::string_view module)
    : env_(env), module_(module), level_(env.options().get_enum<CheckLevel>(Option::CheckLevel))
{
}

void Checker::fail(ErrorKind knd,
                   std::string_view invariant,
                   std::string_view object,
                   std::string_view values) const
{
    const Error error(knd, std::string(object), msg_of(invariant, values));
    const auto fill = [&](Event &evt) { describe(evt, module_, error, invariant, values); };
    Events &evts = env_.events();
    if (evts.filling())
    {
        throw error;
    }
    env_.record_error(error.what());
    if (!evts.delivering())
    {
        evts.emit(EventKind::Check, EventLevel::Error, fill);
    }
    throw error;
}

}
