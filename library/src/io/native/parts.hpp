#pragma once

#include <cstddef>
#include <optional>
#include <ostream>
#include <string_view>

namespace biyos::native
{

inline constexpr std::string_view kMagic = "BIYOS";
inline constexpr int kVersion = 1;
inline constexpr std::string_view kUnset = "none";

enum class Part
{
    Name,
    Variables,
    Functions,
    Linear,
    Quadratic,
    Piecewise,
    Expressions,
    Constraints,
    Objectives,
    Indicators,
    Sets,
    Cones,
    Complements,
    Names,
    Tolerances,
    End,
};

std::optional<Part> part_of(std::string_view word) noexcept;
std::string_view word_of(Part part) noexcept;
void print_header(std::ostream &output, Part part, std::size_t count);
void print_header(std::ostream &output, Part part, std::string_view noun, std::size_t count);

}
