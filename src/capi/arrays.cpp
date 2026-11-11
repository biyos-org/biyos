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

std::span<const Index> listed(int count, const int *indices, std::string_view name)
{
    const std::size_t size = length(count);
    if (size > 0)
    {
        require(indices, name);
    }
    return {indices, size};
}

std::vector<Offset> offsets(std::size_t count, const std::int64_t *starts)
{
    if (starts == nullptr)
    {
        return std::vector<Offset>(count + 1, 0);
    }
    return std::vector<Offset>(starts, starts + count + 1);
}

std::vector<LinearTerm> packed(std::size_t size, const int *variables, const double *values)
{
    if (size > 0)
    {
        require(variables, "variables");
        require(values, "values");
    }
    std::vector<LinearTerm> terms;
    terms.reserve(size);
    for (std::size_t k = 0; k < size; ++k)
    {
        terms.push_back(LinearTerm{.variable = variables[k], .coefficient = values[k]});
    }
    return terms;
}

void unpack(std::span<const LinearTerm> terms, std::size_t size, int *variables, double *values)
{
    const std::size_t written = (std::min)(size, terms.size());
    for (std::size_t k = 0; k < written; ++k)
    {
        if (variables != nullptr)
        {
            variables[k] = terms[k].variable;
        }
        if (values != nullptr)
        {
            values[k] = terms[k].coefficient;
        }
    }
}

}
