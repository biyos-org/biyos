#pragma once

#include "model/model.hpp"

#include <optional>
#include <string_view>

namespace biyos
{

inline constexpr std::string_view kIndicatorCode = "IF";

std::optional<SpecialOrderedSetKind> set_kind(std::string_view code) noexcept;
std::string_view code_of(SpecialOrderedSetKind kind) noexcept;

}
