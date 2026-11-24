#include "core/options/lookup.hpp"

#include "core/options/table.hpp"
#include "core/options/value.hpp"

#include <string_view>

namespace biyos
{

Lookup::Lookup(const Settings &settings, const Options &options) noexcept
    : settings_(&settings), options_(options)
{
}

Lookup::Lookup(const Options &options) noexcept : settings_(nullptr), options_(options) {}

Found Lookup::find(Option option) const
{
    const OptionValue *own = settings_ != nullptr ? settings_->find(option) : nullptr;
    if (own != nullptr)
    {
        return Found{.value = *own, .source = OptionSource::Model};
    }
    return Found{.value = options_.value_of(option), .source = options_.source_of(option)};
}

bool Lookup::get_bool(Option option) const
{
    const OptionSpec &spec = row_of(options_.table(), option);
    return bool_of(spec, find(option).value);
}

int Lookup::get_int(Option option) const
{
    const OptionSpec &spec = row_of(options_.table(), option);
    return int_of(spec, find(option).value);
}

double Lookup::get_double(Option option) const
{
    const OptionSpec &spec = row_of(options_.table(), option);
    return double_of(spec, find(option).value);
}

std::string_view Lookup::get_string(Option option) const
{
    const OptionSpec &spec = row_of(options_.table(), option);
    return string_of(spec, find(option).value);
}

Masks Lookup::get_flags(Option option) const
{
    const OptionSpec &spec = row_of(options_.table(), option);
    return flags_of(spec, find(option).value);
}

PartState Lookup::part_state(Option option, Mask part) const
{
    const OptionSpec &spec = row_of(options_.table(), option);
    return state_of(spec, find(option).value, part);
}

int Lookup::enum_value(Option option) const
{
    const OptionSpec &spec = row_of(options_.table(), option);
    return enum_of(spec, find(option).value);
}

}
