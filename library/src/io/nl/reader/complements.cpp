#include "io/nl/reader/complements.hpp"

#include "core/numbers.hpp"
#include "core/text/text.hpp"
#include "expr/forms/terms.hpp"
#include "io/nl/source/bounds.hpp"
#include "io/nl/source/source.hpp"
#include "io/utils/report.hpp"
#include "model/model.hpp"

#include <span>
#include <string>
#include <utility>
#include <vector>

namespace biyos::nl
{

namespace
{

void check_pairings(const Source &source, const Model &model, std::span<const Pairing> pairings)
{
    std::vector<bool> paired(to_size(model.n_variables()), false);
    for (const Pairing &pairing : pairings)
    {
        const std::string variable = "variable " + text_of(pairing.variable);
        if (paired[to_size(pairing.variable)])
        {
            source.refuse(variable + " is complemented twice");
        }
        paired[to_size(pairing.variable)] = true;
        const Variable &bounds = model.variable(pairing.variable);
        if (finite_of(bounds.lower, bounds.upper) != pairing.finite)
        {
            source.bad("complementarity code " + text_of(pairing.finite) +
                       " does not match the bounds of " + variable);
        }
    }
}

}

void add_complements(const Source &source,
                     Model &model,
                     Index first,
                     std::span<const Pairing> pairings)
{
    check_pairings(source, model, pairings);
    const Index start = model.n_functions();
    std::vector<Real> constants;
    std::vector<Offset> starts{0};
    std::vector<LinearTerm> terms;
    std::vector<Complement> complements;
    for (const Pairing &pairing : pairings)
    {
        const Variable &variable = model.variable(pairing.variable);
        const Index function = start + to_index(complements.size());
        complements.push_back({.function = function,
                               .lower = variable.lower,
                               .upper = variable.upper,
                               .multiplier = first + pairing.row});
        constants.push_back(0.0);
        terms.push_back({.variable = pairing.variable, .coefficient = 1.0});
        starts.push_back(to_offset(terms.size()));
    }
    checked(source, [&] { model.add_functions(constants, starts, std::move(terms)); });
    checked(source, [&] { model.add_complements(complements); });
}

}
