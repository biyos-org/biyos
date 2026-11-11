#include "model/names.hpp"

#include "core/text.hpp"
#include "core/utf8.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace biyos
{

namespace
{

std::string at_byte(std::string_view fault, std::size_t at)
{
    return "name " + std::string(fault) + " at byte " + text_of(at);
}

}

std::string name_fault(std::string_view name)
{
    if (name.empty())
    {
        return "name is empty";
    }
    std::size_t at = 0;
    while (at < name.size())
    {
        const utf8::Sequence sequence = utf8::first_sequence(name.substr(at));
        if (name[at] == '\0')
        {
            return at_byte("holds a NUL", at);
        }
        if (!sequence.valid)
        {
            return at_byte("is not valid UTF-8", at);
        }
        at += sequence.size;
    }
    return "";
}

}
