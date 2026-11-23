#include "core/settings.hpp"

#include "core/error.hpp"
#include "core/option_table.hpp"
#include "core/option_text.hpp"
#include "core/option_value.hpp"

#include <algorithm>
#include <span>
#include <string_view>

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

Settings::Settings(const Settings &other) : table_(other.table_)
{
    entries_.reserve(other.entries_.size());
    for (const Entry &entry : other.entries_)
    {
        entries_.push_back(Entry{.option = entry.option, .value = kept(strings_, entry.value)});
    }
}

void Settings::set_bool(Option option,
                        bool value,
                        Events &events,
                        const BIYOS_model *model,
                        Hooks *hooks)
{
    store(option, value, events, model, hooks);
}

void Settings::set_int(Option option,
                       int value,
                       Events &events,
                       const BIYOS_model *model,
                       Hooks *hooks)
{
    store(option, value, events, model, hooks);
}

void Settings::set_double(Option option,
                          double value,
                          Events &events,
                          const BIYOS_model *model,
                          Hooks *hooks)
{
    store(option, value, events, model, hooks);
}

void Settings::set_string(Option option,
                          std::string_view value,
                          Events &events,
                          const BIYOS_model *model,
                          Hooks *hooks)
{
    store(option, value, events, model, hooks);
}

void Settings::set_flags(Option option,
                         Masks value,
                         Events &events,
                         const BIYOS_model *model,
                         Hooks *hooks)
{
    store(option, value, events, model, hooks);
}

void Settings::store(Option option,
                     const OptionValue &value,
                     Events &events,
                     const BIYOS_model *model,
                     Hooks *hooks)
{
    check_scope(row_of(table_, option));
    const OptionSpec &spec = checked_row(table_, option, value);
    const OptionValue own = kept(strings_, value);
    const auto held = std::ranges::find(entries_, option, &Entry::option);
    if (held != entries_.end())
    {
        held->value = own;
    }
    else
    {
        entries_.push_back(Entry{.option = option, .value = own});
    }
    warn_deprecated(events, table_, spec, model, hooks);
}

const OptionValue *Settings::find(Option option) const
{
    check_scope(row_of(table_, option));
    const auto held = std::ranges::find(entries_, option, &Entry::option);
    return held != entries_.end() ? &held->value : nullptr;
}

void Settings::erase(Option option)
{
    check_scope(row_of(table_, option));
    std::erase_if(entries_, [option](const Entry &entry) { return entry.option == option; });
}

}
