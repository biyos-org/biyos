#include "io/codes.hpp"

#include "model/model.hpp"

#include <algorithm>
#include <array>
#include <optional>
#include <string_view>

namespace biyos
{

namespace
{

struct SetCode
{
    std::string_view code;
    SpecialOrderedSetKind kind;
};

constexpr std::array kSetCodes{
    SetCode{"S1", SpecialOrderedSetKind::One},
    SetCode{"S2", SpecialOrderedSetKind::Two},
};

}

std::optional<SpecialOrderedSetKind> set_kind(std::string_view code) noexcept
{
    const auto found = std::ranges::find(kSetCodes, code, &SetCode::code);
    if (found == kSetCodes.end())
    {
        return std::nullopt;
    }
    return found->kind;
}

std::string_view code_of(SpecialOrderedSetKind kind) noexcept
{
    const auto found = std::ranges::find(kSetCodes, kind, &SetCode::kind);
    return found == kSetCodes.end() ? std::string_view() : found->code;
}

}
