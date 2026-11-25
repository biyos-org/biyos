#include "model/model.hpp"

#include "core/error.hpp"
#include "core/numbers.hpp"
#include "core/text/text.hpp"
#include "expr/graph/graph.hpp"
#include "model/checks.hpp"
#include "model/compressed.hpp"
#include "model/expressions/nodes.hpp"
#include "model/expressions/postfix.hpp"
#include "model/rollback.hpp"

#include <cstddef>
#include <span>
#include <string>
#include <vector>

namespace biyos
{

namespace
{

void check_postfix(const Postfix &postfix)
{
    if (postfix.starts.empty() || postfix.arguments.size() != postfix.codes.size())
    {
        throw Error(ErrorKind::Internal, "model", "postfix arrays differ in length");
    }
    check_shape(postfix.starts.size() - 1, postfix.starts, postfix.codes.size());
}

void check_table(std::span<const Offset> starts,
                 std::span<const double> x,
                 std::span<const double> y)
{
    if (starts.empty() || x.size() != y.size())
    {
        throw Error(ErrorKind::Internal, "model", "pwl arrays differ in length");
    }
    check_shape(starts.size() - 1, starts, x.size());
}

void add_numbered(Graph &graph, std::span<const double> x, std::span<const double> y)
{
    const Offset pwl = graph.points().size();
    try
    {
        graph.add_piecewise_linear(x, y);
    }
    catch (const Error &error)
    {
        throw Error(error.kind(), "pwl " + text_of(pwl), error.message());
    }
}

}

const Graph &Model::graph() const noexcept
{
    return graph_;
}

std::span<const Offset> Model::roots(Index start, Index stop) const
{
    check_span("function", start, stop, n_functions());
    const auto offset = to_size(start);
    return std::span(roots_).subspan(offset, to_size(stop - start));
}

Offset Model::add_variable_node(Index variable)
{
    check_index("variable", variable, n_variables());
    Rollback rollback(graph_);
    const Offset node = graph_.add_variable(variable);
    uses_.extend(graph_);
    rollback.keep();
    return node;
}

Offset Model::add_node(int op, std::span<const Offset> children, std::span<const double> values)
{
    Rollback rollback(graph_);
    const Offset node = append_node(graph_, op, children, values);
    uses_.extend(graph_);
    rollback.keep();
    return node;
}

Offset Model::add_piecewise_linear(std::span<const double> x, std::span<const double> y)
{
    return graph_.add_piecewise_linear(x, y);
}

Offset Model::add_piecewise_linears(std::span<const Offset> starts,
                                    std::span<const double> x,
                                    std::span<const double> y)
{
    check_table(starts, x, y);
    Rollback rollback(graph_);
    const Offset first = graph_.points().size();
    for (std::size_t k = 0; k + 1 < starts.size(); ++k)
    {
        add_numbered(graph_, slice(x, starts, k), slice(y, starts, k));
    }
    rollback.keep();
    return first;
}

Offset Model::add_piecewise(Offset pwl, Offset child)
{
    Rollback rollback(graph_);
    const Offset node = graph_.add_piecewise(pwl, std::span(&child, 1));
    uses_.extend(graph_);
    rollback.keep();
    return node;
}

std::vector<Offset> Model::add_expressions(const Postfix &postfix)
{
    check_postfix(postfix);
    Rollback rollback(graph_);
    std::vector<Offset> tops = decode(graph_, n_variables(), postfix);
    uses_.extend(graph_);
    rollback.keep();
    return tops;
}

}
