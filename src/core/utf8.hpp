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

// The character a nonempty text starts with, by Unicode's table of well-formed UTF-8 byte
// sequences. An ill-formed start gives its maximal subpart, the longest prefix of a well-formed
// sequence or else one byte, which a converter replaces with one U+FFFD.
Sequence first_sequence(std::string_view text) noexcept;

}
