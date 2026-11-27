#include "io/reproducer/header.hpp"

#include "core/env/version.hpp"
#include "core/events/stream.hpp"
#include "core/numbers.hpp"
#include "core/options/lookup.hpp"
#include "core/options/options.hpp"
#include "core/options/table.hpp"
#include "core/options/text.hpp"
#include "core/options/value.hpp"
#include "core/text/strings.hpp"
#include "io/native/utils/body.hpp"
#include "io/native/utils/record.hpp"
#include "io/reproducer/values.hpp"
#include "io/utils/lines.hpp"
#include "io/utils/records.hpp"
#include "io/utils/report.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <optional>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace biyos::reproducer
{

namespace
{

using native::Body;
using native::Record;

constexpr std::string_view kMagic = "REPRO";
constexpr int kFormat = 1;
constexpr std::string_view kVersionWord = "VERSION";
constexpr std::string_view kOptionsWord = "OPTIONS";

struct SourceWord
{
    OptionSource source;
    std::string_view word;
};

constexpr std::array kSources{
    SourceWord{.source = OptionSource::Model, .word = "model"},
    SourceWord{.source = OptionSource::Environment, .word = "environment"},
    SourceWord{.source = OptionSource::Default, .word = "default"},
};

std::string_view word_of(OptionSource source)
{
    return std::ranges::find(kSources, source, &SourceWord::source)->word;
}

bool printed(const OptionSpec &spec)
{
    return !spec.versions.retired;
}

Found in_force(const Lookup &lookup, const OptionSpec &spec)
{
    const Options &options = lookup.options();
    switch (spec.scope)
    {
    case Scope::Model:
        return lookup.find(spec.id);
    case Scope::Environment:
        return Found{.value = options.value_of(spec.id), .source = options.source_of(spec.id)};
    }
    return lookup.find(spec.id);
}

void print_options(std::ostream &output, const Lookup &lookup)
{
    const std::span<const OptionSpec> table = lookup.options().table();
    output << kOptionsWord << ' ' << std::ranges::count_if(table, printed) << '\n';
    for (const OptionSpec &spec : table)
    {
        if (printed(spec))
        {
            const Found found = in_force(lookup, spec);
            output << spec.name << ' ' << word_of(found.source);
            if (found.source != OptionSource::Default)
            {
                print_value(output, spec, found.value);
            }
            output << '\n';
        }
    }
}

Record record_of(Lines &lines, std::string_view word)
{
    if (!native::next_line(lines))
    {
        bad_file(lines, "the file ends before " + std::string(word));
    }
    Record record(lines);
    if (record.word() != word)
    {
        bad_file(lines, "the line does not start with " + std::string(word));
    }
    return record;
}

std::optional<Version> version_of(std::string_view field) noexcept
{
    std::array<int, 3> parts{};
    const char *next = field.data();
    const char *last = next + field.size();
    for (std::size_t k = 0; k < parts.size(); ++k)
    {
        if (k > 0 && (next == last || *next++ != '.'))
        {
            return std::nullopt;
        }
        const std::from_chars_result result = std::from_chars(next, last, parts[k]);
        if (result.ec != std::errc())
        {
            return std::nullopt;
        }
        next = result.ptr;
    }
    const Version found{.major = parts[0], .minor = parts[1], .patch = parts[2]};
    if (next != last || !numbered(found))
    {
        return std::nullopt;
    }
    return found;
}

void read_version(Stream stream, Lines &lines)
{
    Record record = record_of(lines, kVersionWord);
    const std::string_view field = record.word();
    record.end();
    const std::optional<Version> found = version_of(field);
    if (!found)
    {
        record.refuse(quote(field) + " is not a version");
    }
    if (*found != version())
    {
        warn(stream,
             lines,
             "the file is from biyos " + dotted(*found) + ", read by " + dotted(version()));
    }
}

const OptionSpec *known(Stream stream,
                        const Lines &lines,
                        std::span<const OptionSpec> table,
                        std::string_view name)
{
    const auto found = std::ranges::find(table, name, &OptionSpec::name);
    if (found == table.end())
    {
        warn(stream, lines, subject(name) + " is skipped: unknown to biyos " + dotted(version()));
        return nullptr;
    }
    if (found->versions.retired)
    {
        warn(stream, lines, subject(name) + " is skipped: " + retired_notice(table, *found));
        return nullptr;
    }
    return &*found;
}

OptionSource source_of(Record &record, const OptionSpec &spec)
{
    const std::string_view field = record.word();
    const auto found = std::ranges::find(kSources, field, &SourceWord::word);
    if (found == kSources.end())
    {
        record.refuse(quote(field) + " is not model, environment or default");
    }
    if (found->source == OptionSource::Model && spec.scope == Scope::Environment)
    {
        record.refuse(subject(spec.name) + " has environment scope");
    }
    return found->source;
}

std::vector<Assignment> read_options(Stream stream,
                                     Lines &lines,
                                     std::span<const OptionSpec> table,
                                     Strings &strings)
{
    Record header = record_of(lines, kOptionsWord);
    const Offset count = header.count();
    header.end();
    Body body(lines, std::string(kOptionsWord), count);
    std::vector<Option> named;
    std::vector<Assignment> assignments;
    while (body.next())
    {
        Record record = body.record();
        const std::string_view name = record.word();
        const OptionSpec *spec = known(stream, lines, table, name);
        if (spec == nullptr)
        {
            continue;
        }
        if (std::ranges::find(named, spec->id) != named.end())
        {
            record.refuse(subject(name) + " is set twice");
        }
        named.push_back(spec->id);
        const OptionSource source = source_of(record, *spec);
        if (source == OptionSource::Default)
        {
            record.end();
            continue;
        }
        const OptionValue value = read_value(record, *spec, strings);
        record.end();
        checked(lines, [&] { checked_row(table, spec->id, value); });
        assignments.push_back(Assignment{.option = spec->id, .source = source, .value = value});
    }
    return assignments;
}

}

void print_header(std::ostream &output, const Lookup &lookup)
{
    output << kMagic << ' ' << kFormat << '\n';
    output << kVersionWord << ' ' << dotted(version()) << '\n';
    print_options(output, lookup);
}

std::vector<Assignment> read_header(Stream stream,
                                    Lines &lines,
                                    std::span<const OptionSpec> table,
                                    Strings &strings)
{
    native::read_start(lines, "the file", kMagic, kFormat);
    read_version(stream, lines);
    return read_options(stream, lines, table, strings);
}

}
