#include "core/format.hpp"

#include "core/events.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace biyos
{

namespace
{

constexpr std::string_view kHex = "0123456789abcdef";

bool control(char c)
{
    return static_cast<unsigned char>(c) < 0x20;
}

void append_control(std::string &line, char c)
{
    if (c == '\n')
    {
        line += "\\n";
    }
    else if (c == '\t')
    {
        line += "\\t";
    }
    else
    {
        const auto byte = static_cast<unsigned char>(c);
        line += "\\u00";
        line += kHex[byte / 16U];
        line += kHex[byte % 16U];
    }
}

void append_plain(std::string &line, std::string_view text)
{
    for (const char c : text)
    {
        if (control(c))
        {
            append_control(line, c);
        }
        else
        {
            line += c;
        }
    }
}

}

std::string format_text(const Event &event)
{
    std::string line(name_of(event.level));
    line += ' ';
    append_plain(line, event.module);
    line += ':';
    if (!event.text.empty())
    {
        line += ' ';
        append_plain(line, event.text);
    }
    for (const Field &field : event.fields)
    {
        line += ' ';
        append_plain(line, field.name);
        line += '=';
        append_plain(line, field.value);
    }
    return line;
}

}
