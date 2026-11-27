#include "io/nl/reader/suffixes.hpp"

#include "core/events/stream.hpp"
#include "core/numbers.hpp"
#include "io/nl/header.hpp"
#include "io/nl/source/source.hpp"
#include "io/utils/records.hpp"

#include <algorithm>
#include <array>
#include <string>
#include <string_view>

namespace biyos::nl
{

namespace
{

constexpr std::array<std::string_view, 2> kSetSuffixes{"sosno", "ref"};
constexpr std::array<std::string_view, 4> kSuffixTargets{"variable",
                                                         "constraint",
                                                         "objective",
                                                         "problem"};
constexpr Offset kSuffixKinds = 8;
constexpr Offset kRealSuffix = 4;

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
    const std::string text = source_.name();
    const std::string name = quote(text);
    if (std::ranges::find(kSetSuffixes, text) != kSetSuffixes.end())
    {
        source_.refuse("suffix " + name);
    }
    source_.inform(stream_, "suffix " + name + " is skipped");
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

Index Suffixes::size_of(Offset kind) const noexcept
{
    const std::array sizes{header_.variables, header_.constraints, header_.objectives, Index{1}};
    return sizes[to_size(kind % kRealSuffix)];
}

}
