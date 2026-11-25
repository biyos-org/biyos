#include "io/report.hpp"

#include "core/error.hpp"
#include "core/events/event.hpp"
#include "core/events/stream.hpp"
#include "core/text/text.hpp"
#include "io/lines.hpp"

#include <cstddef>
#include <string>

namespace biyos
{

namespace
{

void describe(Event &event, const std::string &file, std::size_t line, const std::string &text)
{
    event.module = "io";
    event.text = "file '" + file + "' line " + text_of(line) + ": " + text;
    event.fields.push_back(Field{.name = "file", .value = file});
    event.fields.push_back(Field{.name = "line", .value = text_of(line)});
}

void describe(Event &event, const std::string &file, const std::string &text)
{
    event.module = "io";
    event.text = "file '" + file + "': " + text;
    event.fields.push_back(Field{.name = "file", .value = file});
}

}

void bad_file(const Lines &lines, const std::string &message)
{
    throw Error(ErrorKind::BadFile, lines.where(), message);
}

void unsupported(const Lines &lines, const std::string &message)
{
    throw Error(ErrorKind::Unsupported, lines.where(), message);
}

void warn(Stream stream,
          const std::string &file,
          std::size_t line,
          const std::string &text) noexcept
{
    const auto fill = [&](Event &event) { describe(event, file, line, text); };
    stream.emit(EventKind::Message, EventLevel::Warning, fill);
}

void warn(Stream stream, const Lines &lines, const std::string &text) noexcept
{
    warn(stream, lines.file(), lines.number(), text);
}

void warn(Stream stream, const std::string &file, const std::string &text) noexcept
{
    const auto fill = [&](Event &event) { describe(event, file, text); };
    stream.emit(EventKind::Message, EventLevel::Warning, fill);
}

}
