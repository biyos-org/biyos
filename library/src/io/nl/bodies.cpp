#include "io/nl/bodies.hpp"

#include "core/numbers.hpp"
#include "expr/forms/terms.hpp"
#include "io/nl/expressions.hpp"
#include "io/nl/source/source.hpp"
#include "io/utils/report.hpp"
#include "model/model.hpp"

#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

namespace biyos::nl
{

Bodies::Bodies(Index functions) : bodies_(to_size(functions)), terms_(to_size(functions)) {}

bool Bodies::has_body(Index function) const noexcept
{
    return bodies_[to_size(function)].has_value();
}

void Bodies::set_body(Index function, const Operand &body)
{
    bodies_[to_size(function)] = body;
}

bool Bodies::has_terms(Index function) const noexcept
{
    return terms_[to_size(function)].has_value();
}

void Bodies::set_terms(Index function, std::vector<LinearTerm> terms)
{
    terms_[to_size(function)] = std::move(terms);
}

void Bodies::add_to(const Source &source, Model &model) const
{
    std::vector<Real> constants;
    std::vector<Offset> starts{0};
    std::vector<LinearTerm> terms;
    std::vector<Index> functions;
    std::vector<Offset> nodes;
    for (std::size_t k = 0; k < bodies_.size(); ++k)
    {
        const Operand body = bodies_[k].value_or(Operand{.node = kNoRoot, .value = 0.0});
        constants.push_back(body.value.value_or(0.0));
        if (!body.value)
        {
            functions.push_back(to_index(k));
            nodes.push_back(body.node);
        }
        if (terms_[k])
        {
            terms.insert(terms.end(), terms_[k]->begin(), terms_[k]->end());
        }
        starts.push_back(to_offset(terms.size()));
    }
    checked(source, [&] { model.add_functions(constants, starts, std::move(terms)); });
    checked(source, [&] { model.attach_expressions(functions, nodes); });
}

}
