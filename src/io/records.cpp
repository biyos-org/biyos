#include "io/records.hpp"

#include "core/index.hpp"
#include "io/directory.hpp"
#include "io/lines.hpp"
#include "io/report.hpp"
#include "model/names.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace biyos
{

Fields uncommented(Fields fields, std::size_t third) noexcept
{
    for (const std::size_t at : {third, third + 2})
    {
        if (at < fields.size() && fields[at].starts_with('$'))
        {
            return fields.first(at);
        }
    }
    return fields;
}

std::string quote(std::string_view text)
{
    return "'" + std::string(text) + "'";
}

double value_of(const Lines &lines, std::string_view field)
{
    const std::optional<double> value = number_of(field);
    if (!value)
    {
        bad_file(lines, quote(field) + " is not a number");
    }
    return *value;
}

void check_name(const Lines &lines, std::string_view name)
{
    const std::string fault = name_fault(name);
    if (!fault.empty())
    {
        bad_file(lines, fault);
    }
}

Index find_in(const Directory &directory,
              const Lines &lines,
              std::string_view noun,
              std::string_view name)
{
    const std::optional<Index> index = directory.find(name);
    if (!index)
    {
        bad_file(lines, "unknown " + std::string(noun) + " " + quote(name));
    }
    return *index;
}

}
