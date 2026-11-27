#include "core/options/settings.hpp"

#include "core/env/error.hpp"
#include "core/numbers.hpp"
#include "core/options/table.hpp"
#include "core/options/text.hpp"
#include "core/options/value.hpp"

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

void Settings::set_bool(Option option, bool value, Stream stream)
{
    store(option, value, stream);
}

void Settings::set_int(Option option, int value, Stream stream)
{
    store(option, value, stream);
}

void Settings::set_double(Option option, Real value, Stream stream)
{
    store(option, value, stream);
}

void Settings::set_string(Option option, std::string_view value, Stream stream)
{
    store(option, value, stream);
}

void Settings::set_flags(Option option, Masks value, Stream stream)
{
    store(option, value, stream);
}

void Settings::store(Option option, const OptionValue &value, Stream stream)
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
    warn_deprecated(stream, table_, spec);
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

bool Settings::empty() const noexcept
{
    return entries_.empty();
}

}
