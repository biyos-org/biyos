#include "io/native/writers/functions.hpp"

#include "core/numbers.hpp"
#include "core/text/text.hpp"
#include "expr/forms/terms.hpp"
#include "expr/graph/points.hpp"
#include "io/native/parts.hpp"
#include "io/native/utils/words.hpp"
#include "model/expressions/nodes.hpp"
#include "model/expressions/postfix.hpp"
#include "model/model.hpp"

#include <cstddef>
#include <ostream>
#include <span>
#include <vector>

namespace biyos::native
{

namespace
{

struct Roots
{
    std::vector<Index> functions;
    std::vector<Offset> nodes;
};

Roots roots_of(const Model &model)
{
    Roots roots;
    const std::span<const Offset> stored = model.roots(0, model.n_functions());
    for (std::size_t k = 0; k < stored.size(); ++k)
    {
        if (stored[k] != kNoRoot)
        {
            roots.functions.push_back(to_index(k));
            roots.nodes.push_back(stored[k]);
        }
    }
    return roots;
}

std::size_t print_entry(std::ostream &output,
                        const Encoding &encoding,
                        std::size_t entry,
                        std::size_t at)
{
    const int code = encoding.codes[entry];
    const Offset argument = encoding.arguments[entry];
    output << word_of(code, kOperatorWords) << ' ' << argument;
    const std::size_t end = at + to_size(n_values(code, argument));
    for (; at < end; ++at)
    {
        output << ' ' << text_of(encoding.values[at]);
    }
    output << '\n';
    return end;
}

}

void print_functions(const Model &model, std::ostream &output)
{
    const std::span<const Real> constants = model.constants(0, model.n_functions());
    if (constants.empty())
    {
        return;
    }
    print_header(output, Part::Functions, constants.size());
    for (const Real constant : constants)
    {
        output << text_of(constant) << '\n';
    }
}

void print_linear(const Model &model, std::ostream &output)
{
    const Index n = model.n_functions();
    const std::span<const LinearTerm> all = model.linear_terms(0, n);
    if (all.empty())
    {
        return;
    }
    print_header(output, Part::Linear, all.size());
    for (Index function = 0; function < n; ++function)
    {
        for (const LinearTerm &term : model.linear_terms(function, function + 1))
        {
            output << function << ' ' << term.variable << ' ' << text_of(term.coefficient) << '\n';
        }
    }
}

void print_quadratic(const Model &model, std::ostream &output)
{
    const Index n = model.n_functions();
    const std::span<const QuadraticTerm> all = model.quadratic_terms(0, n);
    if (all.empty())
    {
        return;
    }
    print_header(output, Part::Quadratic, all.size());
    for (Index function = 0; function < n; ++function)
    {
        for (const QuadraticTerm &term : model.quadratic_terms(function, function + 1))
        {
            output << function << ' ' << term.row << ' ' << term.column << ' '
                   << text_of(term.coefficient) << '\n';
        }
    }
}

void print_piecewise(const Model &model, std::ostream &output)
{
    const Points &points = model.graph().points();
    if (points.size() == 0)
    {
        return;
    }
    print_header(output, Part::Piecewise, to_size(points.size()));
    for (Offset k = 0; k < points.size(); ++k)
    {
        const PiecewiseLinear pwl = points.piecewise_linear(k);
        output << pwl.x.size();
        for (std::size_t point = 0; point < pwl.x.size(); ++point)
        {
            output << ' ' << text_of(pwl.x[point]) << ' ' << text_of(pwl.y[point]);
        }
        output << '\n';
    }
}

void print_expressions(const Model &model, std::ostream &output)
{
    const Roots roots = roots_of(model);
    if (roots.functions.empty())
    {
        return;
    }
    const Encoding encoding = encode(model.graph(), roots.nodes);
    print_header(output, Part::Expressions, roots.functions.size());
    std::size_t at = 0;
    for (std::size_t k = 0; k < roots.functions.size(); ++k)
    {
        const std::size_t begin = to_size(encoding.starts[k]);
        const std::size_t end = to_size(encoding.starts[k + 1]);
        output << roots.functions[k] << ' ' << end - begin << '\n';
        for (std::size_t entry = begin; entry < end; ++entry)
        {
            at = print_entry(output, encoding, entry, at);
        }
    }
}

}
