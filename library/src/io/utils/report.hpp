#pragma once

#include "core/env/error.hpp"
#include "core/events/stream.hpp"
#include "io/utils/lines.hpp"

#include <cstddef>
#include <string>

namespace biyos
{

[[noreturn]] void bad_file(const Lines &lines, const std::string &message);
[[noreturn]] void unsupported(const Lines &lines, const std::string &message);
void warn(Stream stream,
          const std::string &file,
          std::size_t line,
          const std::string &text) noexcept;
void warn(Stream stream, const Lines &lines, const std::string &text) noexcept;
void warn(Stream stream, const std::string &file, const std::string &text) noexcept;
void inform(Stream stream, const Lines &lines, const std::string &text) noexcept;
[[noreturn]] void reject(const std::string &where, const Error &error);
[[noreturn]] void reject(const Lines &lines, const Error &error);

template <typename Call> auto checked(const Lines &lines, Call call)
{
    try
    {
        return call();
    }
    catch (const Error &error)
    {
        reject(lines, error);
    }
}

}
