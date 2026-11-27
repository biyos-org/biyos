#include "io/draft/labels.hpp"

#include "core/env/error.hpp"
#include "core/numbers.hpp"
#include "core/text/text.hpp"
#include "io/draft/draft.hpp"
#include "io/utils/directory.hpp"
#include "model/model.hpp"
#include "model/utils/checks.hpp"

#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>

namespace biyos
{

namespace
{

constexpr std::string_view kSeparators = " \t\r\f\v\n";

bool carried(std::string_view name, bool row) noexcept
{
    const bool split = name.find_first_of(kSeparators) != std::string_view::npos;
    return !split && !name.starts_with('$') && !(row && name == "'MARKER'");
}

void check_label(const std::string &object, std::string_view name, bool row)
{
    if (!carried(name, row))
    {
        unwritable(object, "name '" + std::string(name) + "'");
    }
}

}

Taken user_names(const Model &model, std::initializer_list<Family> families)
{
    Taken taken;
    for (const Family family : families)
    {
        for (Index k = 0; k < model.n_of(family); ++k)
        {
            if (const std::string *name = model.name_of(family, k))
            {
                taken.insert(*name);
            }
        }
    }
    return taken;
}

std::string fresh(const std::string &base, const Taken &taken)
{
    std::string label = base;
    for (Index k = 1; taken.contains(label); ++k)
    {
        label = base + "_" + text_of(k);
    }
    return label;
}

void enter(Directory &directory, const std::string &label, Family family, Index index)
{
    if (!directory.add(label))
    {
        const std::string reason = "name '" + label + "' is repeated";
        throw Error(ErrorKind::Unsupported, item(noun_of(family), index), reason);
    }
}

std::optional<std::string> kept_label(const Model &model, Family family, Index index)
{
    const std::string *name = model.name_of(family, index);
    if (name == nullptr)
    {
        return std::nullopt;
    }
    const bool row = family == Family::Objective || family == Family::Constraint;
    check_label(item(noun_of(family), index), *name, row);
    return *name;
}

std::string label_of(const Model &model,
                     Family family,
                     Index index,
                     const std::string &base,
                     const Taken &taken)
{
    const std::optional<std::string> kept = kept_label(model, family, index);
    return kept ? *kept : fresh(base, taken);
}

}
