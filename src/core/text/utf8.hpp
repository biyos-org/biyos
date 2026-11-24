#pragma once

#include <cstddef>
#include <string_view>

namespace biyos::utf8
{

struct Sequence
{
    std::size_t size;
    bool valid;
};

Sequence first_sequence(std::string_view text) noexcept;

}
