#include "core/settings.hpp"

#include "core/error.hpp"
#include "core/option_table.hpp"
#include "core/option_text.hpp"
#include "core/option_value.hpp"

#include <algorithm>
#include <span>
#include <utility>

namespace biyos
{

namespace
{

void check_scope(const OptionSpec &spec)
{
    if (spec.scope == Scope::Model)
    {
        return;
    }
    throw Error(ErrorKind::InvalidArgument, subject(spec.name), "has environment scope");
}

}

Settings::Settings() : table_(option_table()) {}

Settings::Settings(std::span<const OptionSpec> table) : table_(table) {}

void Settings::set(Option option, OptionValue value, Events &events)
{
    check_scope(row_of(table_, option));
    const OptionSpec &spec = checked_row(table_, option, value);
    const auto held = std::ranges::find(entries_, option, &Entry::option);
    if (held != entries_.end())
    {
        held->value = std::move(value);
    }
    else
    {
        entries_.push_back(Entry{.option = option, .value = std::move(value)});
    }
    warn_deprecated(events, table_, spec);
}

const OptionValue *Settings::find(Option option) const noexcept
{
    const auto held = std::ranges::find(entries_, option, &Entry::option);
    return held != entries_.end() ? &held->value : nullptr;
}

void Settings::erase(Option option) noexcept
{
    std::erase_if(entries_, [option](const Entry &entry) { return entry.option == option; });
}

}
