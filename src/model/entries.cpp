#include "model/entries.hpp"

#include "core/error.hpp"
#include "core/index.hpp"
#include "core/text.hpp"
#include "model/checks.hpp"
#include "model/model.hpp"

#include <algorithm>
#include <span>
#include <string>
#include <string_view>

namespace biyos
{

namespace
{

[[noreturn]] void refuse(const std::string &object, const std::string &reason)
{
    throw Error(ErrorKind::InvalidArgument, object, reason);
}

std::string column_name(Index column)
{
    return "column " + text_of(column);
}

void check_column(const std::string &object, Index column, Index variables)
{
    if (column < 0 || column >= variables)
    {
        refuse(object, column_name(column) + " is unknown");
    }
}

void check_value(const std::string &object, const Entry &entry)
{
    const std::string_view fault = value_fault(entry.value);
    if (!fault.empty())
    {
        refuse(object, "coefficient of " + column_name(entry.column) + " " + std::string(fault));
    }
}

}

void check_entries(const std::string &object, std::span<const Entry> entries, Index variables)
{
    for (const Entry &entry : entries)
    {
        check_column(object, entry.column, variables);
        check_value(object, entry);
    }
}

void sort_entries(const std::string &object, std::span<Entry> entries)
{
    std::ranges::sort(entries, {}, &Entry::column);
    const auto repeat = std::ranges::adjacent_find(entries, {}, &Entry::column);
    if (repeat != entries.end())
    {
        refuse(object, column_name(repeat->column) + " is repeated");
    }
}

}
