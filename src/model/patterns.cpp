#include "model/patterns.hpp"

#include "core/error.hpp"
#include "core/index.hpp"
#include "core/text/text.hpp"
#include "model/callbacks.hpp"
#include "model/checks.hpp"
#include "model/compressed.hpp"

#include <cstddef>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace biyos
{

namespace
{

constexpr std::string_view kNoun = "callback";

template <typename T> void check_entries(std::size_t entries)
{
    if (entries > room(std::vector<T>()))
    {
        throw Error(ErrorKind::NoMemory, "model", "no pattern offset left");
    }
}

std::string variable_fault(Index variable, Index total)
{
    return variable < 0 || variable >= total ? item("variable", variable) + " is unknown" : "";
}

std::string order_fault(Index variable, Index previous)
{
    if (variable == previous)
    {
        return item("variable", variable) + " is repeated";
    }
    if (variable < previous)
    {
        return item("variable", variable) + " is listed after " + item("variable", previous);
    }
    return "";
}

std::string row_fault(std::span<const Index> variables, Index total)
{
    for (std::size_t k = 0; k < variables.size(); ++k)
    {
        std::string fault = variable_fault(variables[k], total);
        if (fault.empty() && k > 0)
        {
            fault = order_fault(variables[k], variables[k - 1]);
        }
        if (!fault.empty())
        {
            return fault;
        }
    }
    return "";
}

std::string pair_name(const IndexPair &pair)
{
    return "pair (" + text_of(pair.row) + ", " + text_of(pair.column) + ")";
}

std::string pair_fault(const IndexPair &pair, Index total)
{
    for (const Index variable : {pair.row, pair.column})
    {
        const std::string fault = variable_fault(variable, total);
        if (!fault.empty())
        {
            return fault;
        }
    }
    if (pair.column < pair.row)
    {
        return pair_name(pair) + " has its second variable below its first";
    }
    return "";
}

std::string pair_order(const IndexPair &pair, const IndexPair &previous)
{
    if (pair == previous)
    {
        return pair_name(pair) + " is repeated";
    }
    if (pair < previous)
    {
        return pair_name(pair) + " is listed after " + pair_name(previous);
    }
    return "";
}

}

void check_jacobian(Index callback,
                    std::span<const Index> functions,
                    std::span<const Offset> starts,
                    std::span<const Index> variables,
                    Index total)
{
    check_entries<Index>(variables.size());
    check_shape(functions.size(), starts, variables.size());
    for (std::size_t k = 0; k < functions.size(); ++k)
    {
        const std::string fault = row_fault(slice(variables, starts, k), total);
        if (!fault.empty())
        {
            refuse(kNoun, callback, "Jacobian of " + item("function", functions[k]) + ": " + fault);
        }
    }
}

std::vector<IndexPair> hessian_pairs(Index callback,
                                     std::span<const Index> first,
                                     std::span<const Index> second,
                                     Index total)
{
    check_entries<IndexPair>(first.size());
    if (first.size() != second.size())
    {
        throw Error(ErrorKind::Internal, "model", "first and second variables differ in number");
    }
    std::vector<IndexPair> pairs;
    pairs.reserve(first.size());
    for (std::size_t k = 0; k < first.size(); ++k)
    {
        const IndexPair pair{.row = first[k], .column = second[k]};
        std::string fault = pair_fault(pair, total);
        if (fault.empty() && k > 0)
        {
            fault = pair_order(pair, pairs.back());
        }
        if (!fault.empty())
        {
            refuse(kNoun, callback, "Hessian " + fault);
        }
        pairs.push_back(pair);
    }
    return pairs;
}

}
