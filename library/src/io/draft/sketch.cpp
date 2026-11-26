#include "io/draft/sketch.hpp"

#include "core/error.hpp"
#include "core/numbers.hpp"
#include "core/text/text.hpp"
#include "expr/forms/terms.hpp"
#include "io/directory.hpp"
#include "io/draft/column_shape.hpp"
#include "io/draft/draft.hpp"
#include "io/draft/labels.hpp"
#include "io/draft/row_shape.hpp"
#include "io/draft/support.hpp"
#include "model/model.hpp"
#include "model/utils/checks.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numeric>
#include <string>
#include <string_view>
#include <vector>

namespace biyos
{

namespace
{

constexpr Index kNoFunction = -1;
constexpr std::string_view kBlanks = " \t\r\f\v";

void name_model(Draft &draft, const Model &model)
{
    const std::string *name = model.name();
    if (name == nullptr)
    {
        return;
    }
    const bool padded = kBlanks.find(name->front()) != std::string_view::npos ||
                        kBlanks.find(name->back()) != std::string_view::npos;
    if (padded || name->find('\n') != std::string::npos)
    {
        const std::string reason = "name '" + *name + "' cannot be written in free MPS";
        throw Error(ErrorKind::Unsupported, "model", reason);
    }
    draft.name = *name;
}

void enter(Directory &directory, const std::string &label, Family family, Index index)
{
    if (!directory.add(label))
    {
        const std::string reason = "name '" + label + "' is repeated";
        throw Error(ErrorKind::Unsupported, item(noun_of(family), index), reason);
    }
}

bool is_free(const Constraint &constraint) noexcept
{
    return std::isinf(constraint.lower) && std::isinf(constraint.upper);
}

bool any_free(const Model &model)
{
    return std::ranges::any_of(model.constraints(0, model.n_constraints()), is_free);
}

void add_objective(Draft &draft, const Model &model, const Taken &taken)
{
    const Objective &objective = model.objective(0);
    draft.sense = objective.sense;
    draft.rows.push_back(free_row(model.constant(objective.function)));
    enter(draft.row_names,
          label_of(model, Family::Objective, 0, "OBJ", taken),
          Family::Objective,
          0);
}

std::vector<Index> add_rows(Draft &draft, const Model &model)
{
    const Taken taken = user_names(model, {Family::Objective, Family::Constraint});
    std::vector<Index> functions;
    functions.reserve(to_size(model.n_constraints()) + 1);
    if (model.n_objectives() > 0)
    {
        add_objective(draft, model, taken);
        functions.push_back(model.objective(0).function);
    }
    else if (any_free(model))
    {
        draft.rows.push_back(free_row(0.0));
        draft.row_names.add(fresh("OBJ", taken));
        functions.push_back(kNoFunction);
    }
    for (Index k = 0; k < model.n_constraints(); ++k)
    {
        const Constraint &constraint = model.constraint(k);
        draft.rows.push_back(row_shape(k, constraint, model.constant(constraint.function)));
        enter(draft.row_names,
              label_of(model, Family::Constraint, k, "R" + text_of(k), taken),
              Family::Constraint,
              k);
        functions.push_back(constraint.function);
    }
    return functions;
}

void add_columns(Draft &draft, const Model &model)
{
    const Taken taken = user_names(model, {Family::Variable});
    draft.columns.reserve(to_size(model.n_variables()));
    for (Index k = 0; k < model.n_variables(); ++k)
    {
        draft.columns.push_back(column_shape(k, model.variable(k)));
        enter(draft.column_names,
              label_of(model, Family::Variable, k, "C" + text_of(k), taken),
              Family::Variable,
              k);
    }
}

std::span<const LinearTerm> terms_of(const Model &model, Index function)
{
    if (function == kNoFunction)
    {
        return {};
    }
    return model.linear_terms(function, function + 1);
}

void add_entries(Draft &draft, const Model &model, const std::vector<Index> &functions)
{
    std::vector<Offset> starts(to_size(model.n_variables()) + 1, 0);
    for (const Index function : functions)
    {
        for (const LinearTerm &term : terms_of(model, function))
        {
            ++starts[to_size(term.variable) + 1];
        }
    }
    std::partial_sum(starts.begin(), starts.end(), starts.begin());
    draft.entries.resize(to_size(starts.back()));
    for (std::size_t row = 0; row < functions.size(); ++row)
    {
        for (const LinearTerm &term : terms_of(model, functions[row]))
        {
            const auto at = to_size(starts[to_size(term.variable)]++);
            draft.entries[at] = DraftEntry{.row = to_index(row),
                                           .column = term.variable,
                                           .value = term.coefficient};
        }
    }
}

}

Draft draft_of(const Model &model)
{
    check_support(model);
    Draft draft;
    name_model(draft, model);
    const std::vector<Index> functions = add_rows(draft, model);
    add_columns(draft, model);
    add_entries(draft, model, functions);
    return draft;
}

}
