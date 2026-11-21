#include "model/model.hpp"

#include "core/index.hpp"
#include "expr/graph.hpp"
#include "model/checks.hpp"
#include "model/nodes.hpp"
#include "model/rollback.hpp"

#include <cstddef>
#include <span>

namespace biyos
{

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

}
