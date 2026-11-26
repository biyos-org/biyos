#include "model/expressions/patterns.hpp"

#include "core/error.hpp"
#include "core/memory/room.hpp"
#include "core/numbers.hpp"
#include "core/text/text.hpp"
#include "model/expressions/callbacks.hpp"
#include "model/utils/checks.hpp"

#include <cstddef>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace biyos
{

namespace
{

constexpr std::string_view kNoun = "callback";

void check_sides(std::span<const Index> a, std::span<const Index> b, std::string_view sides)
{
    if (a.size() > room(std::vector<IndexPair>()))
    {
        throw Error(ErrorKind::NoMemory, "model", "no pattern offset left");
    }
    if (a.size() != b.size())
    {
        throw Error(ErrorKind::Internal, "model", std::string(sides) + " differ in number");
    }
}

std::unordered_map<Index, Index> positions(std::span<const Index> covered)
{
    std::unordered_map<Index, Index> found;
    for (std::size_t k = 0; k < covered.size(); ++k)
    {
        found.emplace(covered[k], to_index(k));
    }
    return found;
}

struct Reads
{
    bool dense;
    std::unordered_map<Index, Index> positions;
    Index total;
};

Reads reads_of(const Callback &held, Index total)
{
    return {.dense = held.dense, .positions = positions(held.variables), .total = total};
}

std::string variable_fault(Index variable, const Reads &reads)
{
    if (variable < 0 || variable >= reads.total)
    {
        return item("variable", variable) + " is unknown";
    }
    if (!reads.dense && !reads.positions.contains(variable))
    {
        return item("variable", variable) + " is not listed";
    }
    return "";
}

IndexPair jacobian_pair(Index callback,
                        const std::unordered_map<Index, Index> &found,
                        Index function,
                        Index variable,
                        const Reads &reads)
{
    const auto row = found.find(function);
    if (row == found.end())
    {
        refuse(kNoun, callback, "Jacobian " + item("function", function) + " is not covered");
    }
    const std::string fault = variable_fault(variable, reads);
    if (!fault.empty())
    {
        refuse(kNoun, callback, "Jacobian of " + item("function", function) + ": " + fault);
    }
    return {.row = row->second, .column = variable};
}

std::string pair_name(const IndexPair &pair)
{
    return "pair (" + text_of(pair.row) + ", " + text_of(pair.column) + ")";
}

std::string pair_fault(const IndexPair &pair, const Reads &reads)
{
    for (const Index variable : {pair.row, pair.column})
    {
        const std::string fault = variable_fault(variable, reads);
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

}

std::vector<IndexPair> jacobian_pairs(Index callback,
                                      const Callback &held,
                                      std::span<const Index> functions,
                                      std::span<const Index> variables,
                                      Index total)
{
    check_sides(functions, variables, "functions and variables");
    const std::unordered_map<Index, Index> found = positions(held.covered);
    const Reads reads = reads_of(held, total);
    std::vector<IndexPair> pairs;
    pairs.reserve(functions.size());
    for (std::size_t k = 0; k < functions.size(); ++k)
    {
        pairs.push_back(jacobian_pair(callback, found, functions[k], variables[k], reads));
    }
    return pairs;
}

std::vector<IndexPair> hessian_pairs(Index callback,
                                     const Callback &held,
                                     std::span<const Index> rows,
                                     std::span<const Index> columns,
                                     Index total)
{
    check_sides(rows, columns, "first and second variables");
    const Reads reads = reads_of(held, total);
    std::vector<IndexPair> pairs;
    pairs.reserve(rows.size());
    for (std::size_t k = 0; k < rows.size(); ++k)
    {
        const IndexPair pair{.row = rows[k], .column = columns[k]};
        const std::string fault = pair_fault(pair, reads);
        if (!fault.empty())
        {
            refuse(kNoun, callback, "Hessian " + fault);
        }
        pairs.push_back(pair);
    }
    return pairs;
}

}
