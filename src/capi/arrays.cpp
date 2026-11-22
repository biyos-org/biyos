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

std::span<const Index> listed(int n, const int *idx, std::string_view name)
{
    const std::size_t size = length(n);
    if (size > 0)
    {
        require(idx, name);
    }
    return {idx, size};
}

std::vector<Offset> offsets(std::size_t n, const std::int64_t *starts)
{
    if (starts == nullptr)
    {
        return std::vector<Offset>(n + 1, 0);
    }
    return std::vector<Offset>(starts, starts + n + 1);
}

std::vector<LinearTerm> packed(std::size_t size, const int *vars, const double *values)
{
    if (size > 0)
    {
        require(vars, "varidx");
        require(values, "coef");
    }
    std::vector<LinearTerm> terms;
    terms.reserve(size);
    for (std::size_t k = 0; k < size; ++k)
    {
        terms.push_back(LinearTerm{.variable = vars[k], .coefficient = values[k]});
    }
    return terms;
}

void unpack(std::span<const LinearTerm> terms, std::size_t size, int *vars, double *values)
{
    const std::size_t written = (std::min)(size, terms.size());
    for (std::size_t k = 0; k < written; ++k)
    {
        if (vars != nullptr)
        {
            vars[k] = terms[k].variable;
        }
        if (values != nullptr)
        {
            values[k] = terms[k].coefficient;
        }
    }
}

std::vector<QuadraticTerm> packed(std::size_t size,
                                  const int *firsts,
                                  const int *seconds,
                                  const double *values)
{
    if (size > 0)
    {
        require(firsts, "varidx1");
        require(seconds, "varidx2");
        require(values, "coef");
    }
    std::vector<QuadraticTerm> terms;
    terms.reserve(size);
    for (std::size_t k = 0; k < size; ++k)
    {
        terms.push_back(QuadraticTerm{
            .row = firsts[k],
            .column = seconds[k],
            .coefficient = values[k],
        });
    }
    return terms;
}

void unpack(std::span<const QuadraticTerm> terms,
            std::size_t size,
            int *firsts,
            int *seconds,
            double *values)
{
    const std::size_t written = (std::min)(size, terms.size());
    for (std::size_t k = 0; k < written; ++k)
    {
        if (firsts != nullptr)
        {
            firsts[k] = terms[k].row;
        }
        if (seconds != nullptr)
        {
            seconds[k] = terms[k].column;
        }
        if (values != nullptr)
        {
            values[k] = terms[k].coefficient;
        }
    }
}

}
