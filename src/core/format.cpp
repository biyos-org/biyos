#include "core/format.hpp"

#include "core/events.hpp"
#include "core/utf8.hpp"

#include <array>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace biyos
{

namespace
{

constexpr std::string_view kHex = "0123456789abcdef";
constexpr std::string_view kReplacement = "\xef\xbf\xbd";
constexpr std::size_t kDecimals = 6;

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

void append_digits(std::string &line, long long value, std::size_t width)
{
    std::array<char, 24> digits{};
    const std::to_chars_result result =
        std::to_chars(digits.data(), digits.data() + digits.size(), value);
    const auto count = static_cast<std::size_t>(result.ptr - digits.data());
    if (count < width)
    {
        line.append(width - count, '0');
    }
    line.append(digits.data(), count);
}

void append_seconds(std::string &line, std::chrono::nanoseconds time)
{
    const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(time);
    const std::chrono::microseconds magnitude = std::chrono::abs(micros);
    const auto whole = std::chrono::duration_cast<std::chrono::seconds>(magnitude);
    if (micros.count() < 0)
    {
        line += '-';
    }
    append_digits(line, whole.count(), 1);
    line += '.';
    append_digits(line, (magnitude - whole).count(), kDecimals);
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

std::size_t append_char(std::string &line, std::string_view rest)
{
    const char c = rest.front();
    if (c == '"' || c == '\\')
    {
        line += '\\';
        line += c;
        return 1;
    }
    if (control(c))
    {
        append_control(line, c);
        return 1;
    }
    const utf8::Sequence sequence = utf8::first_sequence(rest);
    line += sequence.valid ? rest.substr(0, sequence.size) : kReplacement;
    return sequence.size;
}

void append_quoted(std::string &line, std::string_view text)
{
    line += '"';
    std::size_t at = 0;
    while (at < text.size())
    {
        at += append_char(line, text.substr(at));
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
    std::string line;
    append_seconds(line, event.time);
    line += ' ';
    line += name_of(event.level);
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
    append_key(line, "time");
    append_seconds(line, event.time);
    append_member(line, "kind", name_of(event.kind));
    append_member(line, "level", name_of(event.level));
    append_member(line, "module", event.module);
    append_member(line, "text", event.text);
    append_fields(line, event.fields);
    line += '}';
    return line;
}

}
