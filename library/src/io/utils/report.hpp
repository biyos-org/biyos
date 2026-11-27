#pragma once

#include "core/env/error.hpp"
#include "core/events/stream.hpp"
#include "io/utils/lines.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace biyos
{

[[noreturn]] void bad_file(const std::string &where, const std::string &message);
[[noreturn]] void bad_file(const Lines &lines, const std::string &message);
[[noreturn]] void unsupported(const std::string &where, const std::string &message);
[[noreturn]] void unsupported(const Lines &lines, const std::string &message);
void warn(Stream stream,
          const std::string &file,
          std::size_t line,
          const std::string &text) noexcept;
void warn(Stream stream, const Lines &lines, const std::string &text) noexcept;
void warn(Stream stream, const std::string &file, const std::string &text) noexcept;
void inform(Stream stream, const Lines &lines, const std::string &text) noexcept;
void inform(Stream stream,
            const std::string &file,
            std::string_view unit,
            std::size_t at,
            const std::string &text) noexcept;
[[noreturn]] void reject(const std::string &where, const Error &error);
[[noreturn]] void reject(const Lines &lines, const Error &error);

template <typename Place, typename Call> auto checked(const Place &place, Call call)
{
    try
    {
        return call();
    }
    catch (const Error &error)
    {
        reject(place, error);
    }
}

}
