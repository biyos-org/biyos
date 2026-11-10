#include "core/option_warning.hpp"

#include "core/option_table.hpp"
#include "core/option_text.hpp"
#include "core/version.hpp"

#include <span>
#include <string>
#include <string_view>

namespace biyos
{

namespace
{

void describe(Event &event, std::span<const OptionSpec> table, const OptionSpec &spec)
{
    const Version deprecated = *spec.versions.deprecated;
    const std::string_view replacement = row_of(table, *spec.replacement).name;
    event.module = "core";
    event.text = subject(spec.name) + ": " + notice("deprecated", deprecated, replacement);
    event.fields.push_back(Field{.name = "option", .value = std::string(spec.name)});
    event.fields.push_back(Field{.name = "deprecated", .value = dotted(deprecated)});
    event.fields.push_back(Field{.name = "replacement", .value = std::string(replacement)});
}

}

void warn_deprecated(Events &events, std::span<const OptionSpec> table,
                     const OptionSpec &spec) noexcept
{
    if (!spec.versions.deprecated)
    {
        return;
    }
    const auto fill = [&](Event &event) { describe(event, table, spec); };
    events.emit(EventKind::Message, Level::Warning, fill);
}

}
