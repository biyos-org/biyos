#include "model/model.hpp"

#include "core/error.hpp"
#include "core/index.hpp"
#include "core/text/text.hpp"
#include "expr/forms/terms.hpp"
#include "expr/graph/graph.hpp"
#include "model/checks.hpp"
#include "model/renumber.hpp"

#include <algorithm>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace biyos
{

namespace
{

constexpr std::string_view kNoun = "variable";

std::size_t at(Index index) noexcept
{
    return static_cast<std::size_t>(index);
}

[[noreturn]] void refuse_use(Index variable, const std::string &user)
{
    throw Error(ErrorKind::InUse, item(kNoun, variable), "used by " + user);
}

void check_nodes(const Graph &graph, std::span<const Index> map)
{
    for (std::size_t k = 0; k < map.size(); ++k)
    {
        const Offset node = graph.node_of(static_cast<Index>(k));
        if (map[k] == kDeleted && node >= 0)
        {
            refuse_use(static_cast<Index>(k), "node " + text_of(node));
        }
    }
}

void check_members(std::span<const Offset> ends,
                   std::span<const Member> members,
                   std::span<const Index> map)
{
    std::size_t begin = 0;
    for (std::size_t sos = 0; sos < ends.size(); ++sos)
    {
        const auto end = static_cast<std::size_t>(ends[sos]);
        for (const Member &member : members.subspan(begin, end - begin))
        {
            if (map[at(member.variable)] == kDeleted)
            {
                refuse_use(member.variable, item("set", static_cast<Index>(sos)));
            }
        }
        begin = end;
    }
}

void check_indicators(std::span<const Indicator> indicators, std::span<const Index> map)
{
    for (std::size_t k = 0; k < indicators.size(); ++k)
    {
        if (map[at(indicators[k].variable)] == kDeleted)
        {
            refuse_use(indicators[k].variable, item("indicator", static_cast<Index>(k)));
        }
    }
}

void check_callbacks(bool held, std::span<const Index> map)
{
    const auto deleted = std::ranges::find(map, kDeleted);
    if (held && deleted != map.end())
    {
        refuse_use(static_cast<Index>(deleted - map.begin()), item("callback", 0));
    }
}

bool renumbered(LinearTerm &term, std::span<const Index> map) noexcept
{
    term.variable = map[at(term.variable)];
    return term.variable != kDeleted;
}

bool renumbered(QuadraticTerm &term, std::span<const Index> map) noexcept
{
    term.row = map[at(term.row)];
    term.column = map[at(term.column)];
    return term.row != kDeleted && term.column != kDeleted;
}

template <typename Term>
void renumber_terms(std::vector<Offset> &ends,
                    std::vector<Term> &terms,
                    std::span<const Index> map) noexcept
{
    std::size_t read = 0;
    std::size_t write = 0;
    for (Offset &end : ends)
    {
        for (; read < static_cast<std::size_t>(end); ++read)
        {
            Term term = terms[read];
            if (renumbered(term, map))
            {
                terms[write] = term;
                ++write;
            }
        }
        end = static_cast<Offset>(write);
    }
    terms.resize(write);
}

}

std::vector<Index> Model::delete_variables(std::span<const Index> indices)
{
    std::vector<Index> map = renumbering(kNoun, n_variables(), indices);
    check_nodes(graph_, map);
    check_members(special_ordered_set_ends_, members_, map);
    check_indicators(indicators_, map);
    check_callbacks(n_callbacks() > 0, map);
    keep(variables_, map);
    renumber_tolerances(Family::Variable, map);
    renumber_terms(linear_ends_, linear_terms_, map);
    renumber_terms(quadratic_ends_, quadratic_terms_, map);
    renumber_variables(map);
    renumber_names(Family::Variable, map);
    return map;
}

void Model::renumber_variables(std::span<const Index> map) noexcept
{
    for (Member &member : members_)
    {
        member.variable = map[at(member.variable)];
    }
    for (Indicator &indicator : indicators_)
    {
        indicator.variable = map[at(indicator.variable)];
    }
    graph_.renumber_variables(map);
}

}
