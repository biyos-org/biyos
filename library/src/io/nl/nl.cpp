#include "io/nl/nl.hpp"

#include "core/events/stream.hpp"
#include "core/numbers.hpp"
#include "core/text/text.hpp"
#include "expr/forms/terms.hpp"
#include "io/nl/header.hpp"
#include "io/nl/reader/bodies.hpp"
#include "io/nl/reader/complements.hpp"
#include "io/nl/reader/expressions.hpp"
#include "io/nl/reader/suffixes.hpp"
#include "io/nl/source/bounds.hpp"
#include "io/nl/source/source.hpp"
#include "io/utils/records.hpp"
#include "io/utils/report.hpp"
#include "model/model.hpp"

#include <algorithm>
#include <istream>
#include <memory>
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

ObjectiveSense sense_of(Source &source)
{
    const Offset code = source.integer();
    if (code != 0 && code != 1)
    {
        source.bad("objective sense " + quote(source.last_field()) + " is unknown");
    }
    return code == 0 ? ObjectiveSense::Minimize : ObjectiveSense::Maximize;
}

class NlReader
{
  public:
    NlReader(Stream stream, std::istream &input, const std::string &file, Offset bytes);

    Model read();

  private:
    void segment();
    void once(char key);
    bool seen(char key) const noexcept;
    void read_defined();
    void read_constraint();
    void read_objective();
    void read_ranges();
    void read_bounds();
    void read_columns();
    void read_linear(Index size, Index offset, std::string_view noun);
    void skip_guesses(Index size, std::string_view noun, std::string_view notice);
    std::vector<LinearTerm> read_terms(Index n);
    Bounds read_bound();
    void finish();
    void check_bodies(Index first, Index n, std::string_view noun);

    Stream stream_;
    Lines lines_;
    Header header_;
    std::unique_ptr<Source> owned_;
    Source &source_;
    Model model_;
    Expressions expressions_;
    Bodies bodies_;
    Suffixes suffixes_;
    std::vector<std::optional<Bounds>> ranges_;
    std::vector<Pairing> pairings_;
    std::vector<ObjectiveSense> senses_;
    std::string seen_;
};

NlReader::NlReader(Stream stream, std::istream &input, const std::string &file, Offset bytes)
    : stream_(stream), lines_(input, file), header_(read_header(lines_, bytes)),
      owned_(source_of(lines_, input, header_.encoding, bytes)), source_(*owned_),
      expressions_(source_, model_, header_.variables, header_.defined),
      bodies_(header_.constraints + header_.objectives), suffixes_(stream, source_, header_),
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
    while (source_.next())
    {
        segment();
    }
    finish();
    return std::move(model_);
}

void NlReader::segment()
{
    switch (source_.key())
    {
    case 'F':
        source_.refuse("imported functions");
    case 'S':
        suffixes_.read();
        return;
    case 'L':
        source_.refuse("logical constraints");
    case 'V':
        read_defined();
        return;
    case 'C':
        read_constraint();
        return;
    case 'O':
        read_objective();
        return;
    case 'd':
        skip_guesses(header_.constraints, "constraint", "dual guesses are skipped");
        return;
    case 'x':
        skip_guesses(header_.variables, "variable", "starting point is skipped");
        return;
    case 'r':
        once('r');
        read_ranges();
        return;
    case 'b':
        once('b');
        read_bounds();
        return;
    case 'k':
        once('k');
        read_columns();
        return;
    case 'J':
        read_linear(header_.constraints, 0, "constraint");
        return;
    case 'G':
        read_linear(header_.objectives, header_.constraints, "objective");
        return;
    default:
        break;
    }
    source_.bad("segment " + quote(source_.key_text()) + " is unknown");
}

void NlReader::once(char key)
{
    if (seen(key))
    {
        source_.bad("segment " + quote(std::string(1, key)) + " is repeated");
    }
    seen_.push_back(key);
}

bool NlReader::seen(char key) const noexcept
{
    return seen_.find(key) != std::string::npos;
}

void NlReader::read_defined()
{
    source_.expect(3);
    const Index total = header_.variables + header_.defined;
    const Index variable = source_.index(total, "variable");
    if (variable < header_.variables)
    {
        source_.bad("variable " + text_of(variable) + " is not a defined variable");
    }
    if (expressions_.has(variable))
    {
        source_.bad("defined variable " + text_of(variable) + " is repeated");
    }
    const Index n = source_.count();
    source_.integer();
    const std::vector<LinearTerm> terms = read_terms(n);
    expressions_.define(variable, terms, expressions_.read());
}

void NlReader::read_constraint()
{
    source_.expect(1);
    const Index constraint = source_.index(header_.constraints, "constraint");
    if (bodies_.has_body(constraint))
    {
        source_.bad("constraint " + text_of(constraint) + " is repeated");
    }
    bodies_.set_body(constraint, expressions_.read());
}

void NlReader::read_objective()
{
    source_.expect(2);
    const Index objective = source_.index(header_.objectives, "objective");
    const Index function = header_.constraints + objective;
    if (bodies_.has_body(function))
    {
        source_.bad("objective " + text_of(objective) + " is repeated");
    }
    senses_[to_size(objective)] = sense_of(source_);
    bodies_.set_body(function, expressions_.read());
}

void NlReader::read_ranges()
{
    source_.expect(0);
    for (Index k = 0; k < header_.constraints; ++k)
    {
        source_.line("the constraint bounds");
        const std::optional<Bounds> bounds = bounds_of(source_);
        if (!bounds)
        {
            pairings_.push_back(pairing_of(source_, k, header_.variables));
        }
        ranges_.push_back(bounds);
    }
    if (to_index(pairings_.size()) != header_.complements)
    {
        source_.bad("the header counts " + text_of(header_.complements) +
                    " complements, the segment holds " + text_of(to_index(pairings_.size())));
    }
}

void NlReader::read_bounds()
{
    source_.expect(0);
    for (Index k = 0; k < header_.variables; ++k)
    {
        const Bounds bounds = read_bound();
        Variable variable = model_.variable(k);
        variable.lower = bounds.lower;
        variable.upper = bounds.upper;
        checked(source_, [&] { model_.set_variables(std::span(&k, 1), std::span(&variable, 1)); });
    }
}

void NlReader::read_columns()
{
    source_.expect(1);
    const Index n = source_.count();
    const Index wanted = (std::max)(header_.variables - 1, Index{0});
    if (n != wanted)
    {
        source_.bad("the k segment holds " + text_of(n) + " counts, not " + text_of(wanted));
    }
    for (Index k = 0; k < n; ++k)
    {
        source_.line("the column counts");
        source_.expect(1);
        source_.count();
    }
}

void NlReader::read_linear(Index size, Index offset, std::string_view noun)
{
    source_.expect(2);
    const Index index = source_.index(size, noun);
    const Index function = offset + index;
    if (bodies_.has_terms(function))
    {
        source_.bad("the linear part of " + std::string(noun) + " " + text_of(index) +
                    " is repeated");
    }
    std::vector<LinearTerm> terms = read_terms(source_.count());
    std::ranges::sort(terms, {}, &LinearTerm::variable);
    const auto repeat = std::ranges::adjacent_find(terms, same_variable);
    if (repeat != terms.end())
    {
        source_.bad("variable " + text_of(repeat->variable) + " is repeated");
    }
    bodies_.set_terms(function, std::move(terms));
}

void NlReader::skip_guesses(Index size, std::string_view noun, std::string_view notice)
{
    once(source_.key());
    source_.inform(stream_, "the file's " + std::string(notice));
    source_.expect(1);
    const Index n = source_.count();
    for (Index k = 0; k < n; ++k)
    {
        source_.line("a list of values");
        source_.expect(2);
        source_.index(size, noun);
        source_.real();
    }
}

std::vector<LinearTerm> NlReader::read_terms(Index n)
{
    std::vector<LinearTerm> terms;
    for (Index k = 0; k < n; ++k)
    {
        source_.line("a linear part");
        source_.expect(2);
        const Index variable = source_.index(header_.variables, "variable");
        terms.push_back({.variable = variable, .coefficient = source_.real()});
    }
    return terms;
}

Bounds NlReader::read_bound()
{
    source_.line("the variable bounds");
    const std::optional<Bounds> bounds = bounds_of(source_);
    if (!bounds)
    {
        source_.bad("a variable bound cannot be a complementarity");
    }
    return *bounds;
}

void NlReader::finish()
{
    if (header_.constraints > 0 && !seen('r'))
    {
        source_.bad("the file has no constraint bounds");
    }
    if (header_.variables > 0 && !seen('b'))
    {
        source_.bad("the file has no variable bounds");
    }
    check_bodies(0, header_.constraints, "constraint");
    check_bodies(header_.constraints, header_.objectives, "objective");
    const Index first = bodies_.add_to(source_, model_);
    std::vector<Constraint> constraints;
    for (Index k = 0; k < header_.constraints; ++k)
    {
        const std::optional<Bounds> &bounds = ranges_[to_size(k)];
        if (bounds)
        {
            constraints.push_back(
                {.function = first + k, .lower = bounds->lower, .upper = bounds->upper});
        }
    }
    checked(source_, [&] { model_.add_constraints(constraints); });
    std::vector<Objective> objectives;
    for (Index k = 0; k < header_.objectives; ++k)
    {
        objectives.push_back(default_objective(first + header_.constraints + k, k));
        objectives.back().sense = senses_[to_size(k)];
    }
    checked(source_, [&] { model_.add_objectives(objectives); });
    add_complements(source_, model_, first, pairings_);
    suffixes_.add_to(model_);
}

void NlReader::check_bodies(Index first, Index n, std::string_view noun)
{
    for (Index k = 0; k < n; ++k)
    {
        if (!bodies_.has_body(first + k))
        {
            source_.bad(std::string(noun) + " " + text_of(k) + " has no expression");
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
