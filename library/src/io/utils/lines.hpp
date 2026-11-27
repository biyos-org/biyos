#pragma once

#include "core/numbers.hpp"

#include <cstddef>
#include <istream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace biyos
{

class Lines
{
  public:
    Lines(std::istream &input, std::string file);

    bool next();
    void split(std::span<const std::size_t> starts);
    std::size_t number() const noexcept;
    std::string_view text() const noexcept;
    std::span<const std::string_view> fields() const noexcept;
    const std::string &file() const noexcept;
    std::string where() const;

  private:
    std::istream &input_;
    std::string file_;
    std::string text_;
    std::vector<std::string_view> fields_;
    std::size_t number_ = 0;
};

std::string_view trimmed(std::string_view text) noexcept;
std::optional<Real> number_of(std::string_view field);

}
