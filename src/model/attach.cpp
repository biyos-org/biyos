#include "model/model.hpp"

#include "core/error.hpp"
#include "core/index.hpp"
#include "core/text/text.hpp"
#include "expr/canonical.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/uses.hpp"
#include "expr/terms.hpp"
#include "model/checks.hpp"
#include "model/compressed.hpp"
#include "model/merge.hpp"
#include "model/rollback.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace biyos
{

namespace
{

constexpr std::string_view kNoun = "function";

std::size_t at(Index function) noexcept
{
    return static_cast<std::size_t>(function);
}

void check_nodes(std::span<const Index> functions, std::span<const Offset> nodes, Offset size)
{
    if (functions.size() != nodes.size())
    {
        throw Error(ErrorKind::Internal, "model", "functions and nodes differ in number");
    }
    for (std::size_t k = 0; k < nodes.size(); ++k)
    {
        if (nodes[k] < 0 || nodes[k] >= size)
        {
            refuse(kNoun, functions[k], "node " + text_of(nodes[k]) + " is unknown");
        }
    }
}

std::vector<Parts> split_all(Graph &graph, const Uses &uses, std::span<const Offset> nodes)
{
    std::vector<Parts> parts;
    parts.reserve(nodes.size());
    for (const Offset node : nodes)
    {
        parts.push_back(split(graph, uses, node));
    }
    return parts;
}

double added_to(double held, double added) noexcept
{
    const bool nothing = std::fpclassify(added) == FP_ZERO && !std::signbit(added);
    return nothing ? held : held + added;
}

std::vector<double> constants_of(std::span<const Index> functions,
                                 std::span<const Parts> parts,
                                 std::span<const double> held)
{
    std::vector<double> sums;
    sums.reserve(functions.size());
    for (std::size_t k = 0; k < functions.size(); ++k)
    {
        const double sum = added_to(held[at(functions[k])], parts[k].constant);
        const std::string_view fault = value_fault(sum);
        if (!fault.empty())
        {
            refuse(kNoun, functions[k], "sum of constants " + std::string(fault));
        }
        sums.push_back(sum);
    }
    return sums;
}

Offset joined(Graph &graph, Offset root, Offset remainder)
{
    if (remainder == kNoRemainder)
    {
        return root;
    }
    if (root == kNoRoot)
    {
        return remainder;
    }
    const std::array children{root, remainder};
    const std::array weights{1.0, 1.0};
    return graph.add_sum(0.0, children, weights);
}

std::vector<Offset> roots_of(Graph &graph,
                             std::span<const Index> functions,
                             std::span<const Parts> parts,
                             std::span<const Offset> held)
{
    std::vector<Offset> roots;
    roots.reserve(functions.size());
    for (std::size_t k = 0; k < functions.size(); ++k)
    {
        roots.push_back(joined(graph, held[at(functions[k])], parts[k].remainder));
    }
    return roots;
}

template <typename Term>
std::vector<Addition<Term>> additions_of(std::span<const Index> functions,
                                         std::span<const Parts> parts,
                                         std::vector<Term> Parts::*terms)
{
    std::vector<Addition<Term>> additions;
    additions.reserve(functions.size());
    for (std::size_t k = 0; k < functions.size(); ++k)
    {
        additions.push_back(Addition<Term>{.function = functions[k], .terms = parts[k].*terms});
    }
    return additions;
}

std::vector<Index> distinct(std::span<const Index> functions, Index held)
{
    for (const Index function : functions)
    {
        check_index(kNoun, function, held);
    }
    std::vector<Index> sorted(functions.begin(), functions.end());
    std::ranges::sort(sorted);
    const auto repeated = std::ranges::unique(sorted);
    sorted.erase(repeated.begin(), repeated.end());
    return sorted;
}

}

void Model::attach_expressions(std::span<const Index> functions, std::span<const Offset> nodes)
{
    check_functions(functions, n_functions(), "idx");
    check_nodes(functions, nodes, graph_.size());
    Rollback rollback(graph_);
    const std::vector<Parts> parts = split_all(graph_, uses_, nodes);
    const std::vector<double> constants = constants_of(functions, parts, constants_);
    const std::vector<Offset> roots = roots_of(graph_, functions, parts, roots_);
    const std::vector<AddedTerms> linear = additions_of(functions, parts, &Parts::linear);
    const std::vector<AddedPairs> pairs = additions_of(functions, parts, &Parts::quadratic);
    Merge<LinearTerm> linear_merge(linear_ends_, linear_terms_, linear);
    Merge<QuadraticTerm> pair_merge(quadratic_ends_, quadratic_terms_, pairs);
    uses_.extend(graph_);
    linear_merge.apply();
    pair_merge.apply();
    for (std::size_t k = 0; k < functions.size(); ++k)
    {
        constants_[at(functions[k])] = constants[k];
        roots_[at(functions[k])] = roots[k];
    }
    rollback.keep();
}

void Model::clear_functions(std::span<const Index> functions)
{
    const std::vector<Index> cleared = distinct(functions, n_functions());
    clear_parts(linear_ends_, linear_terms_, cleared);
    clear_parts(quadratic_ends_, quadratic_terms_, cleared);
    for (const Index function : cleared)
    {
        constants_[at(function)] = 0.0;
        roots_[at(function)] = kNoRoot;
    }
}

}
