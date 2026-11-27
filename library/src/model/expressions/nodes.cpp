#include "model/expressions/nodes.hpp"

#include "core/env/error.hpp"
#include "core/numbers.hpp"
#include "core/text/text.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/node.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <optional>
#include <span>
#include <string>

namespace biyos
{

namespace
{

constexpr Offset kMost = (std::numeric_limits<Offset>::max)();

Offset sum_values(Offset n) noexcept
{
    return n < 0 ? 0 : (std::min)(n, kMost - 1) + 1;
}

std::span<const Real> taken(std::span<const Real> values, Offset n)
{
    const auto held = to_offset(values.size());
    if (held < n)
    {
        const std::string reason = "holds " + n_text(held, "value", "values");
        throw Error(ErrorKind::InvalidArgument, "argument 'val'", reason + ", not " + text_of(n));
    }
    return values.first(to_size(n));
}

}

Operator to_operator(int op)
{
    const std::optional<Operator> found = operator_of(op);
    if (!found)
    {
        const std::string reason = "operator " + text_of(op) + " is unknown";
        throw Error(ErrorKind::InvalidArgument, "expression", reason);
    }
    return *found;
}

Offset n_values(int op, Offset n)
{
    const std::optional<Operator> found = operator_of(op);
    if (!found)
    {
        return 0;
    }
    switch (payload_of(*found))
    {
    case Payload::None:
    case Payload::Variable:
    case Payload::Points:
        return 0;
    case Payload::Constant:
        return 1;
    case Payload::Coefficients:
        return sum_values(n);
    }
    return 0;
}

Offset append_node(Graph &graph,
                   int op,
                   std::span<const Offset> children,
                   std::span<const Real> values)
{
    const Operator found = to_operator(op);
    const auto n = to_offset(children.size());
    switch (payload_of(found))
    {
    case Payload::None:
    case Payload::Variable:
    case Payload::Points:
        return graph.add(found, children);
    case Payload::Constant:
        return graph.add(found, taken(values, 1).front(), children);
    case Payload::Coefficients:
        break;
    }
    const std::span<const Real> read = taken(values, sum_values(n));
    return graph.add_sum(read.front(), children, read.subspan(1));
}

}
