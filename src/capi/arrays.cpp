#include "capi/arrays.hpp"

#include "capi/entry.hpp"
#include "core/error.hpp"
#include "core/index.hpp"
#include "model/model.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace biyos::capi
{

namespace
{

[[noreturn]] void refuse_count(std::string_view name, const std::string &reason, std::size_t entry)
{
    const std::string argument = "argument '" + std::string(name) + "'";
    const std::string message = reason + " at entry " + std::to_string(entry);
    throw biyos::Error(biyos::ErrorKind::InvalidArgument, argument, message);
}

}

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

void check_capacity(std::int64_t capacity,
                    std::size_t needed,
                    std::string_view name,
                    std::string_view items)
{
    if (length(capacity, name) < needed)
    {
        const std::string held = "holds " + std::to_string(capacity) + " of the ";
        const std::string reason = held + std::to_string(needed) + " " + std::string(items);
        const std::string argument = "argument '" + std::string(name) + "'";
        throw biyos::Error(biyos::ErrorKind::InvalidArgument, argument, reason);
    }
}

void put_term(const Coordinates &out, std::size_t k, Index function, const LinearTerm &term)
{
    if (out.functions != nullptr)
    {
        out.functions[k] = function;
    }
    if (out.variables != nullptr)
    {
        out.variables[k] = term.variable;
    }
    if (out.values != nullptr)
    {
        out.values[k] = term.coefficient;
    }
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

std::vector<Offset> starts_of(std::size_t n, const std::int64_t *counts, std::string_view name)
{
    std::vector<Offset> starts(n + 1, 0);
    if (counts == nullptr)
    {
        return starts;
    }
    for (std::size_t k = 0; k < n; ++k)
    {
        if (counts[k] < 0)
        {
            refuse_count(name, std::to_string(counts[k]) + " is negative", k);
        }
        if (counts[k] > (std::numeric_limits<Offset>::max)() - starts[k])
        {
            refuse_count(name, "sum overflows", k);
        }
        starts[k + 1] = starts[k] + counts[k];
    }
    return starts;
}

void write_counts(std::span<const Offset> starts, std::int64_t *counts)
{
    if (counts == nullptr)
    {
        return;
    }
    for (std::size_t j = 0; j + 1 < starts.size(); ++j)
    {
        counts[j] = starts[j + 1] - starts[j];
    }
}

std::vector<LinearTerm> packed(std::size_t size, const int *variables, const double *values)
{
    if (size > 0)
    {
        require(variables, "var");
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
        require(rows, "var1");
        require(columns, "var2");
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
