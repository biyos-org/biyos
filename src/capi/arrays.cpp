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

std::size_t length(std::int64_t n, std::string_view name)
{
    if (n < 0)
    {
        const std::string reason = std::to_string(n) + " is negative";
        const std::string argument = "argument '" + std::string(name) + "'";
        throw biyos::Error(biyos::ErrorKind::InvalidArgument, argument, reason);
    }
    return static_cast<std::size_t>(n);
}

std::span<const Index> listed(int n, const int *indices, std::string_view name)
{
    const std::size_t size = length(n);
    if (size > 0)
    {
        require(indices, name);
    }
    return {indices, size};
}

std::vector<Offset> offsets(std::size_t n, const std::int64_t *starts)
{
    if (starts == nullptr)
    {
        return std::vector<Offset>(n + 1, 0);
    }
    return std::vector<Offset>(starts, starts + n + 1);
}

std::vector<LinearTerm> packed(std::size_t size, const int *variables, const double *values)
{
    if (size > 0)
    {
        require(variables, "varidx");
        require(values, "coef");
    }
    std::vector<LinearTerm> terms;
    terms.reserve(size);
    for (std::size_t k = 0; k < size; ++k)
    {
        terms.push_back(LinearTerm{.variable = variables[k], .coefficient = values[k]});
    }
    return terms;
}

void unpack(std::span<const LinearTerm> terms, std::size_t capacity, int *variables, double *values)
{
    const std::size_t written = (std::min)(capacity, terms.size());
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

std::vector<QuadraticTerm> packed(std::size_t size,
                                  const int *rows,
                                  const int *columns,
                                  const double *values)
{
    if (size > 0)
    {
        require(rows, "varidx1");
        require(columns, "varidx2");
        require(values, "coef");
    }
    std::vector<QuadraticTerm> terms;
    terms.reserve(size);
    for (std::size_t k = 0; k < size; ++k)
    {
        terms.push_back(QuadraticTerm{
            .row = rows[k],
            .column = columns[k],
            .coefficient = values[k],
        });
    }
    return terms;
}

void unpack(std::span<const QuadraticTerm> terms,
            std::size_t capacity,
            int *rows,
            int *columns,
            double *values)
{
    const std::size_t written = (std::min)(capacity, terms.size());
    for (std::size_t k = 0; k < written; ++k)
    {
        if (rows != nullptr)
        {
            rows[k] = terms[k].row;
        }
        if (columns != nullptr)
        {
            columns[k] = terms[k].column;
        }
        if (values != nullptr)
        {
            values[k] = terms[k].coefficient;
        }
    }
}

}
