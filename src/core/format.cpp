#include "core/format.hpp"

#include "core/events.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

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

void append_quoted(std::string &line, std::string_view text)
{
    line += '"';
    for (const char c : text)
    {
        if (c == '"' || c == '\\')
        {
            line += '\\';
            line += c;
        }
        else if (control(c))
        {
            append_control(line, c);
        }
        else
        {
            line += c;
        }
    }
    line += '"';
}

void append_key(std::string &line, std::string_view name)
{
    if (line.back() != '{')
    {
        line += ',';
    }
    append_quoted(line, name);
    line += ':';
}

void append_member(std::string &line, std::string_view name, std::string_view value)
{
    append_key(line, name);
    append_quoted(line, value);
}

void append_fields(std::string &line, const std::vector<Field> &fields)
{
    append_key(line, "fields");
    line += '{';
    for (const Field &field : fields)
    {
        append_member(line, field.name, field.value);
    }
    line += '}';
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

std::string format_json(const Event &event)
{
    std::string line = "{";
    append_member(line, "kind", name_of(event.kind));
    append_member(line, "level", name_of(event.level));
    append_member(line, "module", event.module);
    append_member(line, "text", event.text);
    append_fields(line, event.fields);
    line += '}';
    return line;
}

}
