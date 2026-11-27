#include "io/native/parts.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <ostream>
#include <string_view>

namespace biyos::native
{

namespace
{

struct Header
{
    std::string_view word;
    Part part;
};

constexpr std::array kHeaders{
    Header{"NAME", Part::Name},
    Header{"VARIABLES", Part::Variables},
    Header{"FUNCTIONS", Part::Functions},
    Header{"LINEAR", Part::Linear},
    Header{"QUADRATIC", Part::Quadratic},
    Header{"PIECEWISE", Part::Piecewise},
    Header{"EXPRESSIONS", Part::Expressions},
    Header{"CONSTRAINTS", Part::Constraints},
    Header{"OBJECTIVES", Part::Objectives},
    Header{"INDICATORS", Part::Indicators},
    Header{"SETS", Part::Sets},
    Header{"CONES", Part::Cones},
    Header{"COMPLEMENTS", Part::Complements},
    Header{"NAMES", Part::Names},
    Header{"TOLERANCES", Part::Tolerances},
    Header{"END", Part::End},
};

}

std::optional<Part> part_of(std::string_view word) noexcept
{
    const auto found = std::ranges::find(kHeaders, word, &Header::word);
    if (found == kHeaders.end())
    {
        return std::nullopt;
    }
    return found->part;
}

std::string_view word_of(Part part) noexcept
{
    const auto found = std::ranges::find(kHeaders, part, &Header::part);
    return found == kHeaders.end() ? std::string_view() : found->word;
}

void print_header(std::ostream &output, Part part, std::size_t count)
{
    output << word_of(part) << ' ' << count << '\n';
}

void print_header(std::ostream &output, Part part, std::string_view noun, std::size_t count)
{
    output << word_of(part) << ' ' << noun << ' ' << count << '\n';
}

}
