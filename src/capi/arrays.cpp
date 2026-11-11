#include "capi/arrays.hpp"

#include "capi/entry.hpp"
#include "core/error.hpp"
#include "core/index.hpp"
#include "model/model.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace biyos::capi
{

std::size_t length(std::int64_t count, std::string_view name)
{
    if (count < 0)
    {
        const std::string reason = std::to_string(count) + " is negative";
        const std::string argument = "argument '" + std::string(name) + "'";
        throw biyos::Error(biyos::ErrorKind::InvalidArgument, argument, reason);
    }
    return static_cast<std::size_t>(count);
}

std::span<const Index> listed(int count, const int *indices)
{
    const std::size_t size = length(count);
    if (size > 0)
    {
        require(indices, "indices");
    }
    return {indices, size};
}

std::vector<Entry> packed(std::size_t size, const int *columns, const double *values)
{
    if (size > 0)
    {
        require(columns, "columns");
        require(values, "values");
    }
    std::vector<Entry> entries;
    entries.reserve(size);
    for (std::size_t k = 0; k < size; ++k)
    {
        entries.push_back(Entry{.column = columns[k], .value = values[k]});
    }
    return entries;
}

void unpack(std::span<const Entry> entries, std::size_t size, int *columns, double *values)
{
    const std::size_t written = (std::min)(size, entries.size());
    for (std::size_t k = 0; k < written; ++k)
    {
        if (columns != nullptr)
        {
            columns[k] = entries[k].column;
        }
        if (values != nullptr)
        {
            values[k] = entries[k].value;
        }
    }
}

}
