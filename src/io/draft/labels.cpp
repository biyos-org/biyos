#include "io/draft/labels.hpp"

#include "core/error.hpp"
#include "core/numbers.hpp"
#include "core/text/text.hpp"
#include "model/checks.hpp"
#include "model/model.hpp"

#include <initializer_list>
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
        const std::string reason = "name '" + std::string(name) + "' cannot be written in free MPS";
        throw Error(ErrorKind::Unsupported, object, reason);
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

std::string label_of(const Model &model,
                     Family family,
                     Index index,
                     const std::string &base,
                     const Taken &taken)
{
    const std::string *name = model.name_of(family, index);
    if (name == nullptr)
    {
        return fresh(base, taken);
    }
    check_label(item(noun_of(family), index), *name, family != Family::Variable);
    return *name;
}

}
