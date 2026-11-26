#pragma once

#include <optional>
#include <string_view>

namespace biyos
{

enum class Section
{
    None,
    Name,
    Sense,
    Rows,
    Columns,
    Rhs,
    Ranges,
    Bounds,
    Sets,
    Indicators,
    End,
};

std::optional<Section> section_of(std::string_view word) noexcept;
std::string_view word_of(Section section) noexcept;

}
