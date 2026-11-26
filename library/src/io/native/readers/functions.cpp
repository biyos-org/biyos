#include "io/native/readers/functions.hpp"

#include "core/numbers.hpp"
#include "expr/forms/terms.hpp"
#include "io/native/utils/body.hpp"
#include "io/native/utils/record.hpp"
#include "model/expressions/nodes.hpp"
#include "model/expressions/postfix.hpp"
#include "model/model.hpp"

#include <map>
#include <utility>
#include <vector>

namespace biyos::native
{

namespace
{

template <typename Term> using Terms = std::map<Index, std::vector<Term>>;

template <typename Term> struct Grouped
{
    std::vector<Index> functions;
    std::vector<Offset> starts{0};
    std::vector<Term> terms;
};

template <typename Term> Grouped<Term> grouped(const Terms<Term> &by_function)
{
    Grouped<Term> found;
    for (const auto &[function, terms] : by_function)
    {
        found.functions.push_back(function);
        found.terms.insert(found.terms.end(), terms.begin(), terms.end());
        found.starts.push_back(to_offset(found.terms.size()));
    }
    return found;
}

void add_linear(Record record, Terms<LinearTerm> &terms)
{
    const Index function = record.index();
    const LinearTerm term{.variable = record.index(), .coefficient = record.real()};
    record.end();
    terms[function].push_back(term);
}

void add_quadratic(Record record, Terms<QuadraticTerm> &terms)
{
    const Index function = record.index();
    const QuadraticTerm term{
        .row = record.index(),
        .column = record.index(),
        .coefficient = record.real(),
    };
    record.end();
    terms[function].push_back(term);
}

struct Points
{
    std::vector<Offset> starts{0};
    std::vector<Real> x;
    std::vector<Real> y;
};

void add_points(Record record, Points &points)
{
    const Offset count = record.entries(2);
    for (Offset k = 0; k < count; ++k)
    {
        points.x.push_back(record.real());
        points.y.push_back(record.real());
    }
    record.end();
    points.starts.push_back(to_offset(points.x.size()));
}

struct Program
{
    std::vector<Index> functions;
    Encoding encoding{.starts = {0}, .codes = {}, .arguments = {}, .values = {}};
};

void add_entry(Record record, Encoding &encoding)
{
    const int code = record.integer();
    const Offset argument = record.count();
    const Offset values = n_values(code, argument);
    encoding.codes.push_back(code);
    encoding.arguments.push_back(argument);
    for (Offset k = 0; k < values; ++k)
    {
        encoding.values.push_back(record.real());
    }
    record.end();
}

void add_expression(Body &body, Program &program)
{
    Record record = body.record();
    program.functions.push_back(record.index());
    const Offset entries = record.count();
    record.end();
    for (Offset k = 0; k < entries; ++k)
    {
        body.follow();
        add_entry(body.record(), program.encoding);
    }
    program.encoding.starts.push_back(to_offset(program.encoding.codes.size()));
}

Postfix postfix_of(const Encoding &encoding) noexcept
{
    return Postfix{
        .starts = encoding.starts,
        .codes = encoding.codes,
        .arguments = encoding.arguments,
        .values = encoding.values,
    };
}

}

void read_functions(Body &body, Model &model)
{
    std::vector<Real> constants;
    while (body.next())
    {
        Record record = body.record();
        constants.push_back(record.real());
        record.end();
    }
    const std::vector<Offset> starts(constants.size() + 1, 0);
    model.add_functions(constants, starts, {});
}

void read_linear(Body &body, Model &model)
{
    Terms<LinearTerm> terms;
    while (body.next())
    {
        add_linear(body.record(), terms);
    }
    Grouped<LinearTerm> found = grouped(terms);
    model.add_linear_terms(found.functions, found.starts, std::move(found.terms));
}

void read_quadratic(Body &body, Model &model)
{
    Terms<QuadraticTerm> terms;
    while (body.next())
    {
        add_quadratic(body.record(), terms);
    }
    Grouped<QuadraticTerm> found = grouped(terms);
    model.add_quadratic_terms(found.functions, found.starts, std::move(found.terms));
}

void read_piecewise(Body &body, Model &model)
{
    Points points;
    while (body.next())
    {
        add_points(body.record(), points);
    }
    model.add_piecewise_linears(points.starts, points.x, points.y);
}

void read_expressions(Body &body, Model &model)
{
    Program program;
    while (body.next())
    {
        add_expression(body, program);
    }
    const std::vector<Offset> tops = model.add_expressions(postfix_of(program.encoding));
    model.attach_expressions(program.functions, tops);
}

}
