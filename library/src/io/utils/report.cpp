#include "io/utils/report.hpp"

#include "core/env/error.hpp"
#include "core/events/event.hpp"
#include "core/events/stream.hpp"
#include "core/text/text.hpp"
#include "io/utils/lines.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace biyos
{

namespace
{

void describe(Event &event,
              const std::string &file,
              std::string_view unit,
              std::size_t at,
              const std::string &text)
{
    const std::string name(unit);
    event.module = "io";
    event.text = "file '" + file + "' " + name + " " + text_of(at) + ": " + text;
    event.fields.push_back(Field{.name = "file", .value = file});
    event.fields.push_back(Field{.name = name, .value = text_of(at)});
}

void describe(Event &event, const std::string &file, const std::string &text)
{
    event.module = "io";
    event.text = "file '" + file + "': " + text;
    event.fields.push_back(Field{.name = "file", .value = file});
}

}

void bad_file(const std::string &where, const std::string &message)
{
    throw Error(ErrorKind::BadFile, where, message);
}

void bad_file(const Lines &lines, const std::string &message)
{
    bad_file(lines.where(), message);
}

void unsupported(const std::string &where, const std::string &message)
{
    throw Error(ErrorKind::Unsupported, where, message);
}

void unsupported(const Lines &lines, const std::string &message)
{
    unsupported(lines.where(), message);
}

void warn(Stream stream,
          const std::string &file,
          std::size_t line,
          const std::string &text) noexcept
{
    const auto fill = [&](Event &event) { describe(event, file, "line", line, text); };
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

void inform(Stream stream, const Lines &lines, const std::string &text) noexcept
{
    inform(stream, lines.file(), "line", lines.number(), text);
}

void inform(Stream stream, const std::string &file, const std::string &text) noexcept
{
    const auto fill = [&](Event &event) { describe(event, file, text); };
    stream.emit(EventKind::Message, EventLevel::Info, fill);
}

void inform(Stream stream,
            const std::string &file,
            std::string_view unit,
            std::size_t at,
            const std::string &text) noexcept
{
    const auto fill = [&](Event &event) { describe(event, file, unit, at, text); };
    stream.emit(EventKind::Message, EventLevel::Info, fill);
}

void reject(const std::string &where, const Error &error)
{
    if (error.kind() != ErrorKind::InvalidArgument)
    {
        throw error;
    }
    if (error.object().empty())
    {
        bad_file(where, error.message());
    }
    bad_file(where, error.object() + ": " + error.message());
}

void reject(const Lines &lines, const Error &error)
{
    reject(lines.where(), error);
}

}
