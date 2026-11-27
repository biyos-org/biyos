#include "io/nl/writer/writer.hpp"

#include "core/events/stream.hpp"
#include "core/numbers.hpp"
#include "core/numeric/identical.hpp"
#include "core/text/text.hpp"
#include "expr/forms/terms.hpp"
#include "io/nl/source/bounds.hpp"
#include "io/nl/writer/defined.hpp"
#include "io/nl/writer/entries.hpp"
#include "io/nl/writer/layout.hpp"
#include "io/nl/writer/printer.hpp"
#include "io/nl/writer/sets.hpp"
#include "io/nl/writer/usage.hpp"
#include "io/utils/report.hpp"
#include "model/model.hpp"

#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <ostream>
#include <span>
#include <string>
#include <vector>

namespace biyos::nl
{

namespace
{

enum class Bound
{
    Range,
    Upper,
    Lower,
    Free,
    Equal,
};

Bound bound_of(Real lower, Real upper) noexcept
{
    if (identical(lower, upper))
    {
        return Bound::Equal;
    }
    if (std::isinf(lower) && std::isinf(upper))
    {
        return Bound::Free;
    }
    if (std::isinf(lower))
    {
        return Bound::Upper;
    }
    return std::isinf(upper) ? Bound::Lower : Bound::Range;
}

void print_bound(std::ostream &output, Real lower, Real upper)
{
    const Bound bound = bound_of(lower, upper);
    output << static_cast<int>(bound);
    switch (bound)
    {
    case Bound::Range:
        output << ' ' << text_of(lower) << ' ' << text_of(upper);
        break;
    case Bound::Upper:
        output << ' ' << text_of(upper);
        break;
    case Bound::Lower:
    case Bound::Equal:
        output << ' ' << text_of(lower);
        break;
    case Bound::Free:
        break;
    }
    output << '\n';
}

void print_row(std::ostream &output, std::initializer_list<Offset> values)
{
    for (const Offset value : values)
    {
        output << ' ' << value;
    }
    output << '\n';
}

void print_terms(std::ostream &output, std::span<const LinearTerm> terms)
{
    for (const LinearTerm &term : terms)
    {
        output << term.variable << ' ' << text_of(term.coefficient) << '\n';
    }
}

bool named(const Model &model)
{
    for (const Family family : kFamilies)
    {
        for (Index k = 0; k < model.n_of(family); ++k)
        {
            if (model.name_of(family, k) != nullptr)
            {
                return true;
            }
        }
    }
    return model.name() != nullptr;
}

}

Plan::Plan(const Model &model)
    : model_(model), records_(records_of(model)), usage_(model, records_.rows),
      columns_(columns_of(model, usage_)), entries_(model, usage_, columns_.position),
      defined_(model, usage_)
{
}

void Plan::print(std::ostream &output) const
{
    print_header(output);
    Printer printer(model_, defined_, columns_.position, output);
    for (const Definition &definition : defined_.common())
    {
        printer.print_definition(definition);
    }
    print_records(output, printer);
    print_ranges(output);
    print_bounds(output);
    print_columns(output);
    print_entries(output);
    print_sets(output, model_, columns_.position);
}

void Plan::report(Stream stream, const std::string &file) const
{
    if (columns_.integral_binaries)
    {
        warn(stream, file, "NL writes the nonlinear binary variables as integers");
    }
    if (named(model_))
    {
        warn(stream, file, "NL leaves out the names");
    }
    std::vector<bool> paired(to_size(model_.n_functions()), false);
    for (const Complement &complement : model_.complements(0, model_.n_complements()))
    {
        paired[to_size(complement.function)] = true;
    }
    for (Index function = 0; function < model_.n_functions(); ++function)
    {
        if (!reached(usage_.function_reach(function)) && !paired[to_size(function)])
        {
            inform(stream,
                   file,
                   "NL leaves out function " + text_of(function) + ", which no record uses");
        }
    }
}

void Plan::print_header(std::ostream &output) const
{
    Index ranges = 0;
    Index equations = 0;
    for (const Constraint &constraint : model_.constraints(0, model_.n_constraints()))
    {
        const Bound bound = bound_of(constraint.lower, constraint.upper);
        ranges += bound == Bound::Range ? 1 : 0;
        equations += bound == Bound::Equal ? 1 : 0;
    }
    Offset jacobian = 0;
    for (Index m = 0; m < n_rows(); ++m)
    {
        jacobian += to_offset(entries_.of(function_of(m)).size());
    }
    Offset gradient = 0;
    for (const Objective &objective : model_.objectives(0, model_.n_objectives()))
    {
        gradient += to_offset(entries_.of(objective.function).size());
    }
    const Counts &counts = columns_.counts;
    const auto defined = defined_.counts();
    output << "g3 1 1 0\n";
    print_row(output, {model_.n_variables(), n_rows(), model_.n_objectives(), ranges, equations});
    print_nonlinear(output);
    print_row(output, {0, 0});
    print_row(output, {counts.in_constraints, counts.in_objectives, counts.in_both});
    print_row(output, {0, 0, 0, 1});
    print_row(output,
              {counts.binaries,
               counts.integers,
               counts.both_integers,
               counts.constraint_integers,
               counts.objective_integers});
    print_row(output, {jacobian, gradient});
    print_row(output, {0, 0});
    print_row(output, {defined[0], defined[1], defined[2], defined[3], defined[4]});
}

void Plan::print_nonlinear(std::ostream &output) const
{
    const Index rows = records_.nonlinear_rows;
    const Index objectives = records_.nonlinear_objectives;
    const Index complements = model_.n_complements();
    if (complements == 0)
    {
        print_row(output, {rows, objectives});
        return;
    }
    Index two_sided = 0;
    Index shifted = 0;
    for (const Complement &complement : model_.complements(0, complements))
    {
        two_sided += finite_of(complement.lower, complement.upper) == kBothFinite ? 1 : 0;
        shifted += std::isfinite(complement.lower) && !identical(complement.lower, 0.0) ? 1 : 0;
    }
    const Index nonlinear = records_.nonlinear_complements;
    print_row(output, {rows, objectives, complements - nonlinear, nonlinear, two_sided, shifted});
}

void Plan::print_records(std::ostream &output, Printer &printer) const
{
    for (Index m = 0; m < n_rows(); ++m)
    {
        for (const Definition &definition : defined_.of(Scope::Constraint, m))
        {
            printer.print_definition(definition);
        }
        output << 'C' << m << '\n';
        printer.print_top(function_of(m));
    }
    for (Index q = 0; q < model_.n_objectives(); ++q)
    {
        for (const Definition &definition : defined_.of(Scope::Objective, q))
        {
            printer.print_definition(definition);
        }
        const Objective &objective = model_.objective(q);
        const int sense = objective.sense == ObjectiveSense::Maximize ? 1 : 0;
        output << 'O' << q << ' ' << sense << '\n';
        printer.print_top(objective.function);
    }
}

void Plan::print_ranges(std::ostream &output) const
{
    if (records_.rows.empty())
    {
        return;
    }
    output << "r\n";
    for (const Row &row : records_.rows)
    {
        if (row.family == Family::Complement)
        {
            print_pairing(output, model_.complement(row.record));
            continue;
        }
        const Constraint &constraint = model_.constraint(row.record);
        print_bound(output, constraint.lower, constraint.upper);
    }
}

void Plan::print_pairing(std::ostream &output, const Complement &complement) const
{
    const Index variable = columns_.position[to_size(variable_of(model_, complement))];
    const Offset finite = finite_of(complement.lower, complement.upper);
    output << kPairing << ' ' << finite << ' ' << variable + 1 << '\n';
}

void Plan::print_bounds(std::ostream &output) const
{
    if (model_.n_variables() == 0)
    {
        return;
    }
    output << "b\n";
    for (const Index k : columns_.order)
    {
        const Variable &variable = model_.variable(k);
        print_bound(output, variable.lower, variable.upper);
    }
}

void Plan::print_columns(std::ostream &output) const
{
    if (model_.n_variables() == 0)
    {
        return;
    }
    std::vector<Offset> counts(to_size(model_.n_variables()), 0);
    for (Index m = 0; m < n_rows(); ++m)
    {
        for (const LinearTerm &term : entries_.of(function_of(m)))
        {
            ++counts[to_size(term.variable)];
        }
    }
    output << 'k' << model_.n_variables() - 1 << '\n';
    Offset total = 0;
    for (std::size_t k = 0; k + 1 < counts.size(); ++k)
    {
        total += counts[k];
        output << total << '\n';
    }
}

void Plan::print_entries(std::ostream &output) const
{
    for (Index m = 0; m < n_rows(); ++m)
    {
        const auto terms = entries_.of(function_of(m));
        if (!terms.empty())
        {
            output << 'J' << m << ' ' << terms.size() << '\n';
            print_terms(output, terms);
        }
    }
    for (Index q = 0; q < model_.n_objectives(); ++q)
    {
        const auto terms = entries_.of(model_.objective(q).function);
        if (!terms.empty())
        {
            output << 'G' << q << ' ' << terms.size() << '\n';
            print_terms(output, terms);
        }
    }
}

Index Plan::function_of(Index row) const noexcept
{
    return records_.rows[to_size(row)].function;
}

Index Plan::n_rows() const noexcept
{
    return to_index(records_.rows.size());
}

}
