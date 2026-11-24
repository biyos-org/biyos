#include "io/lines.hpp"

#include "core/error.hpp"
#include "core/text/text.hpp"

#include <charconv>
#include <cstddef>
#include <istream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace biyos
{

namespace
{

constexpr std::string_view kBlanks = " \t\r\f\v";

bool starts_number(std::string_view body) noexcept
{
    return !body.empty() && (body.front() == '.' || (body.front() >= '0' && body.front() <= '9'));
}

}

Lines::Lines(std::istream &input, std::string file) : input_(input), file_(std::move(file)) {}

bool Lines::next()
{
    fields_.clear();
    if (!std::getline(input_, text_))
    {
        if (input_.bad())
        {
            throw Error(ErrorKind::FileAccess, "file '" + file_ + "'", "cannot be read");
        }
        return false;
    }
    ++number_;
    const std::string_view text = text_;
    std::size_t begin = text.find_first_not_of(kBlanks);
    while (begin != std::string_view::npos)
    {
        const std::size_t end = text.find_first_of(kBlanks, begin);
        fields_.push_back(text.substr(begin, end - begin));
        begin = text.find_first_not_of(kBlanks, end);
    }
    return true;
}

std::size_t Lines::number() const noexcept
{
    return number_;
}

std::string_view Lines::text() const noexcept
{
    return text_;
}

std::span<const std::string_view> Lines::fields() const noexcept
{
    return fields_;
}

const std::string &Lines::file() const noexcept
{
    return file_;
}

std::string Lines::where() const
{
    const std::string file = "file '" + file_ + "'";
    return number_ == 0 ? file : file + " line " + text_of(number_);
}

std::optional<double> number_of(std::string_view field)
{
    const bool plus = field.starts_with('+');
    const std::string_view digits = plus ? field.substr(1) : field;
    const std::string_view body = !plus && digits.starts_with('-') ? digits.substr(1) : digits;
    if (!starts_number(body))
    {
        return std::nullopt;
    }
    double value = 0.0;
    const char *last = digits.data() + digits.size();
    const std::from_chars_result result = std::from_chars(digits.data(), last, value);
    if (result.ec != std::errc() || result.ptr != last)
    {
        return std::nullopt;
    }
    return value;
}

}
