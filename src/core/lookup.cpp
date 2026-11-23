#include "core/lookup.hpp"

#include "core/option_table.hpp"
#include "core/option_value.hpp"

#include <string>

namespace biyos
{

namespace
{

const Settings &no_settings()
{
    static const Settings kNone;
    return kNone;
}

}

Lookup::Lookup(const Settings &settings, const Options &options) noexcept
    : settings_(settings), options_(options)
{
}

Lookup::Lookup(const Options &options) noexcept : Lookup(no_settings(), options) {}

Found Lookup::find(Option option) const
{
    if (const OptionValue *value = settings_.find(option))
    {
        return Found{.value = *value, .source = Source::Model};
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

const std::string &Lookup::get_string(Option option) const
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
