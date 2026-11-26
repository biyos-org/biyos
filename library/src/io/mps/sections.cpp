#include "io/mps/sections.hpp"

#include <algorithm>
#include <array>
#include <optional>
#include <string_view>

namespace biyos
{

namespace
{

struct Header
{
    std::string_view word;
    Section section;
};

constexpr std::array kHeaders{
    Header{"NAME", Section::Name},
    Header{"OBJSENSE", Section::Sense},
    Header{"OBJSEN", Section::Sense},
    Header{"ROWS", Section::Rows},
    Header{"COLUMNS", Section::Columns},
    Header{"RHS", Section::Rhs},
    Header{"RANGES", Section::Ranges},
    Header{"BOUNDS", Section::Bounds},
    Header{"SOS", Section::Sets},
    Header{"INDICATORS", Section::Indicators},
    Header{"ENDATA", Section::End},
};

}

std::optional<Section> section_of(std::string_view word) noexcept
{
    const auto found = std::ranges::find(kHeaders, word, &Header::word);
    if (found == kHeaders.end())
    {
        return std::nullopt;
    }
    return found->section;
}

std::string_view word_of(Section section) noexcept
{
    const auto found = std::ranges::find(kHeaders, section, &Header::section);
    return found == kHeaders.end() ? std::string_view() : found->word;
}

}
