#include "io/nl/reader/suffixes.hpp"

#include "core/env/error.hpp"
#include "core/events/stream.hpp"
#include "core/numbers.hpp"
#include "core/text/text.hpp"
#include "io/nl/header.hpp"
#include "io/nl/reader/sets.hpp"
#include "io/nl/source/source.hpp"
#include "io/utils/records.hpp"
#include "io/utils/report.hpp"
#include "model/model.hpp"

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace biyos::nl
{

namespace
{

constexpr std::array<std::string_view, 4> kSuffixTargets{"variable",
                                                         "constraint",
                                                         "objective",
                                                         "problem"};
constexpr Offset kSuffixKinds = 8;
constexpr Offset kRealSuffix = 4;
constexpr std::string_view kNumbers = "sosno";
constexpr std::string_view kWeights = "ref";

void add_sets(const Source &source, Model &model, const Sets &sets)
{
    try
    {
        model.add_special_ordered_sets(sets.kinds, sets.starts, sets.members);
    }
    catch (const Error &error)
    {
        if (error.kind() != ErrorKind::InvalidArgument)
        {
            throw;
        }
        source.refuse(error.object() + ": " + error.message());
    }
}

}

Suffixes::Suffixes(Stream stream, Source &source, const Header &header)
    : stream_(stream), source_(source), header_(header)
{
}

void Suffixes::read()
{
    source_.expect(3);
    const Offset kind = source_.integer();
    if (kind < 0 || kind >= kSuffixKinds)
    {
        source_.bad("suffix kind " + quote(source_.last_field()) + " is unknown");
    }
    const Index n = source_.count();
    const std::string name = source_.name();
    const bool on_variables = kind % kRealSuffix == 0;
    if (on_variables && name == kNumbers)
    {
        read_numbers(kind, n);
        return;
    }
    if (on_variables && name == kWeights)
    {
        read_weights(kind, n);
        return;
    }
    source_.inform(stream_, "suffix " + quote(name) + " is skipped");
    skip(kind, n);
}

void Suffixes::add_to(Model &model) const
{
    if (!numbers_)
    {
        if (weights_)
        {
            inform(stream_, weights_->place, "suffix " + quote(kWeights) + " is skipped");
        }
        return;
    }
    const std::vector<std::optional<Real>> none(to_size(header_.variables));
    add_sets(source_, model, sets_of(*numbers_, weights_ ? weights_->values : none));
}

void Suffixes::read_numbers(Offset kind, Index n)
{
    if (kind >= kRealSuffix)
    {
        source_.refuse("real values of suffix " + quote(kNumbers));
    }
    if (numbers_)
    {
        source_.bad("suffix " + quote(kNumbers) + " is repeated");
    }
    numbers_.emplace(to_size(header_.variables), 0);
    std::vector<bool> seen(to_size(header_.variables), false);
    for (Index k = 0; k < n; ++k)
    {
        const Index variable = read_variable(seen);
        (*numbers_)[to_size(variable)] = source_.integer();
    }
}

void Suffixes::read_weights(Offset kind, Index n)
{
    if (weights_)
    {
        source_.bad("suffix " + quote(kWeights) + " is repeated");
    }
    weights_ = Weights{.place = source_.place(), .values = {}};
    weights_->values.resize(to_size(header_.variables));
    std::vector<bool> seen(to_size(header_.variables), false);
    for (Index k = 0; k < n; ++k)
    {
        const Index variable = read_variable(seen);
        const bool real = kind >= kRealSuffix;
        weights_->values[to_size(variable)] =
            real ? source_.real() : static_cast<Real>(source_.integer());
    }
}

void Suffixes::skip(Offset kind, Index n)
{
    const std::string_view noun = kSuffixTargets[to_size(kind % kRealSuffix)];
    for (Index k = 0; k < n; ++k)
    {
        source_.line("a suffix");
        source_.expect(2);
        source_.index(size_of(kind), noun);
        if (kind >= kRealSuffix)
        {
            source_.real();
        }
        else
        {
            source_.integer();
        }
    }
}

Index Suffixes::read_variable(std::vector<bool> &seen)
{
    source_.line("a suffix");
    source_.expect(2);
    const Index variable = source_.index(header_.variables, "variable");
    if (seen[to_size(variable)])
    {
        source_.bad("variable " + text_of(variable) + " is repeated");
    }
    seen[to_size(variable)] = true;
    return variable;
}

Index Suffixes::size_of(Offset kind) const noexcept
{
    const std::array sizes{header_.variables, header_.constraints, header_.objectives, Index{1}};
    return sizes[to_size(kind % kRealSuffix)];
}

}
