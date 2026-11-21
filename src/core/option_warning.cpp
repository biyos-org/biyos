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

void describe(Event &evt, std::span<const OptionSpec> table, const OptionSpec &spec)
{
    const Version deprecated = *spec.versions.deprecated;
    const std::string_view replacement = row_of(table, *spec.replacement).name;
    evt.module = "core";
    evt.text = subject(spec.name) + ": " + notice("deprecated", deprecated, replacement);
    evt.fields.push_back(Field{.name = "option", .value = std::string(spec.name)});
    evt.fields.push_back(Field{.name = "deprecated", .value = dotted(deprecated)});
    evt.fields.push_back(Field{.name = "replacement", .value = std::string(replacement)});
}

}

void warn_deprecated(Events &evts,
                     std::span<const OptionSpec> table,
                     const OptionSpec &spec) noexcept
{
    if (!spec.versions.deprecated)
    {
        return;
    }
    const auto fill = [&](Event &evt) { describe(evt, table, spec); };
    evts.emit(EventKind::Message, EventLevel::Warning, fill);
}

}
