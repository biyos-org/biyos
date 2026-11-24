#include "model/model.hpp"

#include "core/error.hpp"
#include "core/index.hpp"
#include "expr/graph/graph.hpp"
#include "model/checks.hpp"
#include "model/compressed.hpp"
#include "model/expressions/nodes.hpp"
#include "model/expressions/postfix.hpp"
#include "model/rollback.hpp"

#include <cstddef>
#include <span>
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

}

const Graph &Model::graph() const noexcept
{
    return graph_;
}

std::span<const Offset> Model::roots(Index start, Index size) const
{
    check_span("function", start, size, n_functions());
    const auto offset = static_cast<std::size_t>(start);
    return std::span(roots_).subspan(offset, static_cast<std::size_t>(size));
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
