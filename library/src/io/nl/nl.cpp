#include "io/nl/nl.hpp"

#include "core/events/stream.hpp"
#include "core/numbers.hpp"
#include "core/text/text.hpp"
#include "expr/forms/terms.hpp"
#include "io/lines.hpp"
#include "io/nl/bodies.hpp"
#include "io/nl/expressions.hpp"
#include "io/nl/header.hpp"
#include "io/nl/tokens.hpp"
#include "io/records.hpp"
#include "io/report.hpp"
#include "model/model.hpp"

#include <algorithm>
#include <ios>
#include <istream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace biyos::nl
{

namespace
{

bool same_variable(const LinearTerm &a, const LinearTerm &b) noexcept
{
    return a.variable == b.variable;
}

ObjectiveSense sense_of(const Lines &lines, std::string_view field)
{
    const Offset code = integer_of(lines, field);
    if (code != 0 && code != 1)
    {
        bad_file(lines, "objective sense " + quote(field) + " is unknown");
    }
    return code == 0 ? ObjectiveSense::Minimize : ObjectiveSense::Maximize;
}

Offset size_of(std::istream &input)
{
    const std::streampos start = input.tellg();
    input.seekg(0, std::ios::end);
    const std::streamoff size = input.tellg() - start;
    input.seekg(start);
    return size;
}

class NlReader
{
  public:
    NlReader(Stream stream, std::istream &input, const std::string &file, Offset bytes);

    Model read();

  private:
    void segment(const Entry &entry);
    void once(char key);
    bool seen(char key) const noexcept;
    void read_defined(const Entry &entry);
    void read_constraint(const Entry &entry);
    void read_objective(const Entry &entry);
    void read_ranges(const Entry &entry);
    void read_bounds(const Entry &entry);
    void read_columns(const Entry &entry);
    void read_linear(const Entry &entry, Index size, Index offset, std::string_view noun);
    void skip_guesses(const Entry &entry,
                      Index size,
                      std::string_view noun,
                      std::string_view notice);
    std::vector<LinearTerm> read_terms(std::string_view field);
    Bounds read_bound(std::string_view part);
    void finish();
    void check_bodies(Index first, Index n, std::string_view noun);

    Stream stream_;
    Lines lines_;
    Header header_;
    Model model_;
    Expressions expressions_;
    Bodies bodies_;
    std::vector<Bounds> ranges_;
    std::vector<ObjectiveSense> senses_;
    std::string seen_;
};

NlReader::NlReader(Stream stream, std::istream &input, const std::string &file, Offset bytes)
    : stream_(stream), lines_(input, file), header_(read_header(lines_, bytes)),
      expressions_(lines_, model_, header_.variables, header_.defined),
      bodies_(header_.constraints + header_.objectives),
      senses_(to_size(header_.objectives), ObjectiveSense::Minimize)
{
}

Model NlReader::read()
{
    std::vector<Variable> variables;
    for (const VariableKind kind : header_.kinds)
    {
        variables.push_back(widest(kind));
    }
    model_.add_variables(variables);
    while (advance(lines_))
    {
        segment(Entry(lines_));
    }
    finish();
    return std::move(model_);
}

void NlReader::segment(const Entry &entry)
{
    switch (entry.key())
    {
    case 'F':
        unsupported(lines_, "imported functions");
    case 'S':
        unsupported(lines_, "suffixes");
    case 'L':
        unsupported(lines_, "logical constraints");
    case 'V':
        read_defined(entry);
        return;
    case 'C':
        read_constraint(entry);
        return;
    case 'O':
        read_objective(entry);
        return;
    case 'd':
        skip_guesses(entry, header_.constraints, "constraint", "dual guesses are skipped");
        return;
    case 'x':
        skip_guesses(entry, header_.variables, "variable", "starting point is skipped");
        return;
    case 'r':
        once('r');
        read_ranges(entry);
        return;
    case 'b':
        once('b');
        read_bounds(entry);
        return;
    case 'k':
        once('k');
        read_columns(entry);
        return;
    case 'J':
        read_linear(entry, header_.constraints, 0, "constraint");
        return;
    case 'G':
        read_linear(entry, header_.objectives, header_.constraints, "objective");
        return;
    default:
        break;
    }
    bad_file(lines_, "segment " + quote(fields_of(lines_).front()) + " is unknown");
}

void NlReader::once(char key)
{
    if (seen(key))
    {
        bad_file(lines_, "segment " + quote(std::string(1, key)) + " is repeated");
    }
    seen_.push_back(key);
}

bool NlReader::seen(char key) const noexcept
{
    return seen_.find(key) != std::string::npos;
}

void NlReader::read_defined(const Entry &entry)
{
    check_size(lines_, entry.size(), 3);
    const Index total = header_.variables + header_.defined;
    const Index variable = index_in(lines_, entry[0], total, "variable");
    if (variable < header_.variables)
    {
        bad_file(lines_, "variable " + text_of(variable) + " is not a defined variable");
    }
    if (expressions_.has(variable))
    {
        bad_file(lines_, "defined variable " + text_of(variable) + " is repeated");
    }
    integer_of(lines_, entry[2]);
    const std::vector<LinearTerm> terms = read_terms(entry[1]);
    expressions_.define(variable, terms, expressions_.read());
}

void NlReader::read_constraint(const Entry &entry)
{
    check_size(lines_, entry.size(), 1);
    const Index constraint = index_in(lines_, entry[0], header_.constraints, "constraint");
    if (bodies_.has_body(constraint))
    {
        bad_file(lines_, "constraint " + text_of(constraint) + " is repeated");
    }
    bodies_.set_body(constraint, expressions_.read());
}

void NlReader::read_objective(const Entry &entry)
{
    check_size(lines_, entry.size(), 2);
    const Index objective = index_in(lines_, entry[0], header_.objectives, "objective");
    const Index function = header_.constraints + objective;
    if (bodies_.has_body(function))
    {
        bad_file(lines_, "objective " + text_of(objective) + " is repeated");
    }
    senses_[to_size(objective)] = sense_of(lines_, entry[1]);
    bodies_.set_body(function, expressions_.read());
}

void NlReader::read_ranges(const Entry &entry)
{
    check_size(lines_, entry.size(), 0);
    for (Index k = 0; k < header_.constraints; ++k)
    {
        ranges_.push_back(read_bound("the constraint bounds"));
    }
}

void NlReader::read_bounds(const Entry &entry)
{
    check_size(lines_, entry.size(), 0);
    for (Index k = 0; k < header_.variables; ++k)
    {
        const Bounds bounds = read_bound("the variable bounds");
        Variable variable = model_.variable(k);
        variable.lower = bounds.lower;
        variable.upper = bounds.upper;
        checked(lines_, [&] { model_.set_variables(std::span(&k, 1), std::span(&variable, 1)); });
    }
}

void NlReader::read_columns(const Entry &entry)
{
    check_size(lines_, entry.size(), 1);
    const Index n = count_of(lines_, entry[0]);
    const Index wanted = (std::max)(header_.variables - 1, Index{0});
    if (n != wanted)
    {
        bad_file(lines_, "the k segment holds " + text_of(n) + " counts, not " + text_of(wanted));
    }
    for (Index k = 0; k < n; ++k)
    {
        advance_in(lines_, "the column counts");
        const Fields fields = fields_of(lines_);
        check_size(lines_, fields.size(), 1);
        count_of(lines_, fields[0]);
    }
}

void NlReader::read_linear(const Entry &entry, Index size, Index offset, std::string_view noun)
{
    check_size(lines_, entry.size(), 2);
    const Index index = index_in(lines_, entry[0], size, noun);
    const Index function = offset + index;
    if (bodies_.has_terms(function))
    {
        bad_file(lines_,
                 "the linear part of " + std::string(noun) + " " + text_of(index) + " is repeated");
    }
    std::vector<LinearTerm> terms = read_terms(entry[1]);
    std::ranges::sort(terms, {}, &LinearTerm::variable);
    const auto repeat = std::ranges::adjacent_find(terms, same_variable);
    if (repeat != terms.end())
    {
        bad_file(lines_, "variable " + text_of(repeat->variable) + " is repeated");
    }
    bodies_.set_terms(function, std::move(terms));
}

void NlReader::skip_guesses(const Entry &entry,
                            Index size,
                            std::string_view noun,
                            std::string_view notice)
{
    once(entry.key());
    inform(stream_, lines_, "the file's " + std::string(notice));
    check_size(lines_, entry.size(), 1);
    const Index n = count_of(lines_, entry[0]);
    for (Index k = 0; k < n; ++k)
    {
        advance_in(lines_, "a list of values");
        const Fields fields = fields_of(lines_);
        check_size(lines_, fields.size(), 2);
        index_in(lines_, fields[0], size, noun);
        value_of(lines_, fields[1]);
    }
}

std::vector<LinearTerm> NlReader::read_terms(std::string_view field)
{
    const Index n = count_of(lines_, field);
    std::vector<LinearTerm> terms;
    for (Index k = 0; k < n; ++k)
    {
        advance_in(lines_, "a linear part");
        const Fields fields = fields_of(lines_);
        check_size(lines_, fields.size(), 2);
        const Index variable = index_in(lines_, fields[0], header_.variables, "variable");
        terms.push_back({.variable = variable, .coefficient = value_of(lines_, fields[1])});
    }
    return terms;
}

Bounds NlReader::read_bound(std::string_view part)
{
    advance_in(lines_, part);
    const std::optional<Bounds> bounds = bounds_of(lines_);
    if (!bounds)
    {
        unsupported(lines_, "complementarity constraints");
    }
    return *bounds;
}

void NlReader::finish()
{
    if (header_.constraints > 0 && !seen('r'))
    {
        bad_file(lines_, "the file has no constraint bounds");
    }
    if (header_.variables > 0 && !seen('b'))
    {
        bad_file(lines_, "the file has no variable bounds");
    }
    check_bodies(0, header_.constraints, "constraint");
    check_bodies(header_.constraints, header_.objectives, "objective");
    bodies_.add_to(lines_, model_);
    std::vector<Constraint> constraints;
    for (Index k = 0; k < header_.constraints; ++k)
    {
        const Bounds &bounds = ranges_[to_size(k)];
        constraints.push_back({.function = k, .lower = bounds.lower, .upper = bounds.upper});
    }
    checked(lines_, [&] { model_.add_constraints(constraints); });
    std::vector<Objective> objectives;
    for (Index k = 0; k < header_.objectives; ++k)
    {
        objectives.push_back(default_objective(header_.constraints + k, k));
        objectives.back().sense = senses_[to_size(k)];
    }
    checked(lines_, [&] { model_.add_objectives(objectives); });
}

void NlReader::check_bodies(Index first, Index n, std::string_view noun)
{
    for (Index k = 0; k < n; ++k)
    {
        if (!bodies_.has_body(first + k))
        {
            bad_file(lines_, std::string(noun) + " " + text_of(k) + " has no expression");
        }
    }
}

}

}

namespace biyos
{

Model read_nl(Stream stream, std::istream &input, const std::string &file)
{
    const Offset bytes = nl::size_of(input);
    return nl::NlReader(stream, input, file, bytes).read();
}

}
