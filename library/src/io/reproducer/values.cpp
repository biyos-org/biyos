#include "io/reproducer/values.hpp"

#include "core/env/error.hpp"
#include "core/numbers.hpp"
#include "core/options/options.hpp"
#include "core/options/text.hpp"
#include "core/options/value.hpp"
#include "core/text/strings.hpp"
#include "core/text/text.hpp"
#include "io/native/utils/escape.hpp"
#include "io/native/utils/record.hpp"
#include "io/utils/records.hpp"

#include <algorithm>
#include <ostream>
#include <string_view>
#include <variant>

namespace biyos::reproducer
{

namespace
{

using native::Record;

constexpr std::string_view kTrue = "true";
constexpr std::string_view kFalse = "false";

void print_fields(std::ostream &output,
                  const OptionSpec &spec,
                  const BoolRule &,
                  const OptionValue &value)
{
    output << ' ' << (bool_of(spec, value) ? kTrue : kFalse);
}

void print_fields(std::ostream &output,
                  const OptionSpec &spec,
                  const IntRule &,
                  const OptionValue &value)
{
    output << ' ' << text_of(int_of(spec, value));
}

void print_fields(std::ostream &output,
                  const OptionSpec &spec,
                  const DoubleRule &,
                  const OptionValue &value)
{
    output << ' ' << text_of(double_of(spec, value));
}

void print_fields(std::ostream &output,
                  const OptionSpec &spec,
                  const StringRule &,
                  const OptionValue &value)
{
    const std::string_view text = string_of(spec, value);
    if (!text.empty())
    {
        output << ' ' << native::escaped(text);
    }
}

void print_fields(std::ostream &output,
                  const OptionSpec &spec,
                  const EnumRule &rule,
                  const OptionValue &value)
{
    const int number = enum_of(spec, value);
    const auto found = std::ranges::find(rule.values, number, &EnumValue::number);
    if (found == rule.values.end())
    {
        throw Error(ErrorKind::Internal, subject(spec.name), "holds a value outside its list");
    }
    output << ' ' << found->name;
}

void print_fields(std::ostream &output,
                  const OptionSpec &spec,
                  const FlagsRule &rule,
                  const OptionValue &value)
{
    const Masks masks = flags_of(spec, value);
    for (const FlagPart &part : rule.parts)
    {
        if ((masks.on & part.bit) != 0)
        {
            output << " +" << part.name;
        }
        if ((masks.off & part.bit) != 0)
        {
            output << " -" << part.name;
        }
    }
}

OptionValue read_fields(Record &record, const BoolRule &, Strings &)
{
    const std::string_view field = record.word();
    if (field != kTrue && field != kFalse)
    {
        record.refuse(quote(field) + " is not true or false");
    }
    return field == kTrue;
}

OptionValue read_fields(Record &record, const IntRule &, Strings &)
{
    return record.integer();
}

OptionValue read_fields(Record &record, const DoubleRule &, Strings &)
{
    return record.real();
}

OptionValue read_fields(Record &record, const StringRule &, Strings &strings)
{
    if (record.ended())
    {
        return std::string_view();
    }
    return strings.keep(record.name());
}

OptionValue read_fields(Record &record, const EnumRule &rule, Strings &)
{
    const std::string_view field = record.word();
    const auto found = std::ranges::find(rule.values, field, &EnumValue::name);
    if (found == rule.values.end())
    {
        record.refuse(quote(field) + " is not one of its values");
    }
    return found->number;
}

Mask &side_of(Record &record, Masks &masks, std::string_view field)
{
    if (field.front() == '+')
    {
        return masks.on;
    }
    if (field.front() == '-')
    {
        return masks.off;
    }
    record.refuse(quote(field) + " does not start with + or -");
}

Mask bit_of(Record &record, const FlagsRule &rule, std::string_view field)
{
    const std::string_view name = field.substr(1);
    const auto found = std::ranges::find(rule.parts, name, &FlagPart::name);
    if (found == rule.parts.end())
    {
        record.refuse(quote(name) + " is not one of its parts");
    }
    return found->bit;
}

OptionValue read_fields(Record &record, const FlagsRule &rule, Strings &)
{
    Masks masks;
    while (!record.ended())
    {
        const std::string_view field = record.word();
        Mask &side = side_of(record, masks, field);
        side |= bit_of(record, rule, field);
    }
    return masks;
}

}

void print_value(std::ostream &output, const OptionSpec &spec, const OptionValue &value)
{
    const auto print = [&](const auto &rule) { print_fields(output, spec, rule, value); };
    std::visit(print, spec.rule);
}

OptionValue read_value(Record &record, const OptionSpec &spec, Strings &strings)
{
    const auto read = [&](const auto &rule) { return read_fields(record, rule, strings); };
    return std::visit(read, spec.rule);
}

}
