#pragma once

#include "core/numbers.hpp"
#include "io/utils/lines.hpp"
#include "io/utils/records.hpp"

#include <cstddef>
#include <limits>
#include <optional>
#include <string_view>

namespace biyos::nl
{

inline constexpr Offset kMostIndex = (std::numeric_limits<Index>::max)();

struct Bounds
{
    Real lower;
    Real upper;
};

class Entry
{
  public:
    explicit Entry(const Lines &lines) noexcept;

    char key() const noexcept;
    std::size_t size() const noexcept;
    std::string_view operator[](std::size_t k) const noexcept;

  private:
    Fields fields_;
    std::string_view glued_;
};

Fields fields_of(const Lines &lines) noexcept;
bool advance(Lines &lines);
void advance_in(Lines &lines, std::string_view part);
void check_size(const Lines &lines, std::size_t held, std::size_t wanted);
Offset integer_of(const Lines &lines, std::string_view field);
Index count_of(const Lines &lines, std::string_view field);
Index index_in(const Lines &lines, std::string_view field, Index size, std::string_view noun);
std::optional<Bounds> bounds_of(const Lines &lines);

}
