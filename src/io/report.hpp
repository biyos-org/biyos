#pragma once

#include "core/events/stream.hpp"
#include "io/lines.hpp"

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

}
